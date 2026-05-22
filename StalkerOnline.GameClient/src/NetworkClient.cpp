#include "NetworkClient.h"

#include <ws2tcpip.h>
#include <windows.h>

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

NetworkClient::NetworkClient(LogCallback logCallback)
    : _logCallback(std::move(logCallback))
{
    WSADATA wsaData{};

    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != 0)
        Log(L"[NET] WSAStartup failed.");
}

NetworkClient::~NetworkClient()
{
    Stop();
    WSACleanup();
}

bool NetworkClient::Connect(const std::string& host, uint16_t port)
{
    if (_connected)
        return true;

    std::lock_guard lock(_socketMutex);

    if (_connected)
        return true;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string portText = std::to_string(port);

    addrinfo* result = nullptr;

    int getAddrResult = getaddrinfo(
        host.c_str(),
        portText.c_str(),
        &hints,
        &result);

    if (getAddrResult != 0 || result == nullptr)
    {
        Log(L"[NET] getaddrinfo failed.");
        return false;
    }

    SOCKET newSocket = INVALID_SOCKET;

    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
    {
        newSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        if (newSocket == INVALID_SOCKET)
            continue;

        if (connect(newSocket, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == SOCKET_ERROR)
        {
            closesocket(newSocket);
            newSocket = INVALID_SOCKET;
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (newSocket == INVALID_SOCKET)
    {
        Log(L"[NET] Failed to connect to server.");
        return false;
    }

    _socket = newSocket;
    _connected = true;

    std::wstringstream ss;
    ss << L"[NET] Connected to " << Utf8ToWide(host) << L":" << port;
    Log(ss.str());

    return true;
}

LoginResult NetworkClient::Login(
    const std::string& login,
    const std::string& password)
{
    LoginResult result{};

    if (!_connected)
    {
        result.Message = "Not connected.";
        return result;
    }

    PacketWriter writer;
    writer.WriteString(login);
    writer.WriteString(password);

    if (!SendPacket(PacketType::LoginRequest, writer.Data()))
    {
        result.Message = "Failed to send LoginRequest.";
        return result;
    }

    Log(L"[AUTH] LoginRequest sent.");

    while (_connected)
    {
        PacketMessage message;

        if (!ReceivePacket(message))
        {
            result.Message = "Connection closed.";
            return result;
        }

        if (message.Type == PacketType::LoginResponse)
        {
            PacketReader reader(message.Payload);

            result.Success = reader.ReadBool();
            result.Message = reader.ReadString();

            return result;
        }

        HandlePacket(message);
    }

    result.Message = "Disconnected.";
    return result;
}

RegisterResult NetworkClient::Register(
    const std::string& login,
    const std::string& email,
    const std::string& password)
{
    RegisterResult result{};

    if (!_connected)
    {
        result.Message = "Not connected.";
        return result;
    }

    PacketWriter writer;
    writer.WriteString(login);
    writer.WriteString(email);
    writer.WriteString(password);

    if (!SendPacket(PacketType::RegisterRequest, writer.Data()))
    {
        result.Message = "Failed to send RegisterRequest.";
        return result;
    }

    Log(L"[AUTH] RegisterRequest sent.");

    while (_connected)
    {
        PacketMessage message;

        if (!ReceivePacket(message))
        {
            result.Message = "Connection closed.";
            return result;
        }

        if (message.Type == PacketType::RegisterResponse)
        {
            PacketReader reader(message.Payload);

            result.Success = reader.ReadBool();
            result.AccountId = reader.ReadInt32();
            result.Login = reader.ReadString();
            result.Message = reader.ReadString();

            return result;
        }

        HandlePacket(message);
    }

    result.Message = "Disconnected.";
    return result;
}

void NetworkClient::StartReceiveLoop()
{
    if (!_connected)
        return;

    if (_receiveLoopRunning)
        return;

    _receiveLoopRunning = true;

    _receiveThread = std::thread([this]()
    {
        ReceiveLoop();
    });
}

void NetworkClient::Stop()
{
    _receiveLoopRunning = false;
    _connected = false;

    CloseSocket();

    if (_receiveThread.joinable())
    {
        if (std::this_thread::get_id() != _receiveThread.get_id())
            _receiveThread.join();
    }
}

bool NetworkClient::IsConnected() const
{
    return _connected;
}

bool NetworkClient::SendPacket(PacketType type, const std::vector<uint8_t>& payload)
{
    if (!_connected || _socket == INVALID_SOCKET)
        return false;

    std::lock_guard lock(_sendMutex);

    int32_t bodyLength = static_cast<int32_t>(2 + payload.size());

    std::vector<uint8_t> packet;
    packet.reserve(static_cast<size_t>(4 + bodyLength));

    packet.push_back(static_cast<uint8_t>(bodyLength & 0xFF));
    packet.push_back(static_cast<uint8_t>((bodyLength >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>((bodyLength >> 16) & 0xFF));
    packet.push_back(static_cast<uint8_t>((bodyLength >> 24) & 0xFF));

    uint16_t typeValue = static_cast<uint16_t>(type);

    packet.push_back(static_cast<uint8_t>(typeValue & 0xFF));
    packet.push_back(static_cast<uint8_t>((typeValue >> 8) & 0xFF));

    packet.insert(packet.end(), payload.begin(), payload.end());

    return SendAll(packet.data(), static_cast<int>(packet.size()));
}

bool NetworkClient::ReceivePacket(PacketMessage& message)
{
    uint8_t header[4]{};

    if (!ReceiveAll(header, 4))
        return false;

    int32_t bodyLength =
        static_cast<int32_t>(header[0]) |
        (static_cast<int32_t>(header[1]) << 8) |
        (static_cast<int32_t>(header[2]) << 16) |
        (static_cast<int32_t>(header[3]) << 24);

    if (bodyLength < 2 || bodyLength > 1024 * 1024)
        throw std::runtime_error("Invalid packet size.");

    std::vector<uint8_t> body;
    body.resize(static_cast<size_t>(bodyLength));

    if (!ReceiveAll(body.data(), bodyLength))
        return false;

    uint16_t typeValue =
        static_cast<uint16_t>(body[0]) |
        static_cast<uint16_t>(body[1] << 8);

    message.Type = static_cast<PacketType>(typeValue);
    message.Payload.assign(body.begin() + 2, body.end());

    return true;
}

bool NetworkClient::SendAll(const uint8_t* data, int length)
{
    int sentTotal = 0;

    while (sentTotal < length)
    {
        int sent = send(
            _socket,
            reinterpret_cast<const char*>(data + sentTotal),
            length - sentTotal,
            0);

        if (sent == SOCKET_ERROR || sent == 0)
        {
            Log(L"[NET] Send failed.");
            _connected = false;
            CloseSocket();
            return false;
        }

        sentTotal += sent;
    }

    return true;
}

bool NetworkClient::ReceiveAll(uint8_t* data, int length)
{
    int receivedTotal = 0;

    while (receivedTotal < length)
    {
        int received = recv(
            _socket,
            reinterpret_cast<char*>(data + receivedTotal),
            length - receivedTotal,
            0);

        if (received == SOCKET_ERROR || received == 0)
        {
            _connected = false;
            return false;
        }

        receivedTotal += received;
    }

    return true;
}

void NetworkClient::ReceiveLoop()
{
    Log(L"[NET] Receive loop started.");

    try
    {
        while (_receiveLoopRunning && _connected)
        {
            PacketMessage message;

            if (!ReceivePacket(message))
                break;

            HandlePacket(message);
        }
    }
    catch (const std::exception& ex)
    {
        Log(L"[NET ERROR] " + Utf8ToWide(ex.what()));
    }

    _connected = false;
    _receiveLoopRunning = false;

    Log(L"[NET] Receive loop stopped.");
}

void NetworkClient::HandlePacket(const PacketMessage& message)
{
    switch (message.Type)
    {
        case PacketType::Ping:
        {
            SendPacket(PacketType::Pong, {});
            Log(L"[PING] -> [PONG]");
            break;
        }

        case PacketType::Pong:
        {
            Log(L"[PONG]");
            break;
        }

        case PacketType::ServerMessage:
        {
            PacketReader reader(message.Payload);
            std::string text = reader.ReadString();

            Log(L"[SERVER] " + Utf8ToWide(text));
            break;
        }

        case PacketType::ErrorMessage:
        {
            PacketReader reader(message.Payload);

            std::string code = reader.ReadString();
            std::string text = reader.ReadString();
            bool shouldDisconnect = reader.ReadBool();

            Log(
                L"[ERROR] Code=" + Utf8ToWide(code) +
                L", Message=" + Utf8ToWide(text) +
                L", Disconnect=" + std::wstring(shouldDisconnect ? L"true" : L"false"));

            if (shouldDisconnect)
                Stop();

            break;
        }

        case PacketType::PlayerStateSnapshot:
        {
            PacketReader reader(message.Payload);

            int32_t accountId = reader.ReadInt32();
            int32_t characterId = reader.ReadInt32();
            std::string nickname = reader.ReadString();

            float posX = reader.ReadFloat();
            float posY = reader.ReadFloat();
            float posZ = reader.ReadFloat();

            float rotX = reader.ReadFloat();
            float rotY = reader.ReadFloat();
            float rotZ = reader.ReadFloat();

            float health = reader.ReadFloat();
            float maxHealth = reader.ReadFloat();

            float stamina = reader.ReadFloat();
            float maxStamina = reader.ReadFloat();

            float hunger = reader.ReadFloat();
            float thirst = reader.ReadFloat();

            float radiation = reader.ReadFloat();
            float toxicity = reader.ReadFloat();

            bool isAlive = reader.ReadBool();

            std::wstringstream ss;
            ss
                << L"[PLAYER] AccountId=" << accountId
                << L", CharacterId=" << characterId
                << L", Nickname=" << Utf8ToWide(nickname)
                << L", Pos=(" << posX << L", " << posY << L", " << posZ << L")"
                << L", Rot=(" << rotX << L", " << rotY << L", " << rotZ << L")"
                << L", HP=" << health << L"/" << maxHealth
                << L", ST=" << stamina << L"/" << maxStamina
                << L", Hunger=" << hunger
                << L", Thirst=" << thirst
                << L", Rad=" << radiation
                << L", Toxic=" << toxicity
                << L", Alive=" << (isAlive ? L"true" : L"false");

            Log(ss.str());
            break;
        }

        case PacketType::InventorySnapshot:
        {
            PacketReader reader(message.Payload);

            int32_t characterId = reader.ReadInt32();
            int32_t capacity = reader.ReadInt32();
            float totalWeight = reader.ReadFloat();
            int32_t itemCount = reader.ReadInt32();

            std::wstringstream ss;
            ss
                << L"[INVENTORY] CharacterId=" << characterId
                << L", Items=" << itemCount << L"/" << capacity
                << L", Weight=" << totalWeight;

            Log(ss.str());

            for (int32_t i = 0; i < itemCount; ++i)
            {
                int32_t slotIndex = reader.ReadInt32();
                std::string itemTemplateId = reader.ReadString();
                std::string displayName = reader.ReadString();
                int32_t quantity = reader.ReadInt32();
                int32_t maxStack = reader.ReadInt32();
                float weightPerItem = reader.ReadFloat();

                std::wstringstream itemStream;
                itemStream
                    << L"  Slot " << slotIndex
                    << L": " << Utf8ToWide(displayName)
                    << L" [" << Utf8ToWide(itemTemplateId) << L"]"
                    << L" x" << quantity << L"/" << maxStack
                    << L", WeightPerItem=" << weightPerItem;

                Log(itemStream.str());
            }

            break;
        }

        case PacketType::WorldItemSpawn:
        {
            PacketReader reader(message.Payload);

            int32_t worldObjectId = reader.ReadInt32();
            std::string itemTemplateId = reader.ReadString();
            std::string displayName = reader.ReadString();
            int32_t quantity = reader.ReadInt32();

            float posX = reader.ReadFloat();
            float posY = reader.ReadFloat();
            float posZ = reader.ReadFloat();

            float rotX = reader.ReadFloat();
            float rotY = reader.ReadFloat();
            float rotZ = reader.ReadFloat();

            std::wstringstream ss;
            ss
                << L"[WORLD ITEM SPAWN] WorldObjectId=" << worldObjectId
                << L", " << Utf8ToWide(displayName)
                << L" [" << Utf8ToWide(itemTemplateId) << L"]"
                << L" x" << quantity
                << L", Pos=(" << posX << L", " << posY << L", " << posZ << L")"
                << L", Rot=(" << rotX << L", " << rotY << L", " << rotZ << L")";

            Log(ss.str());
            break;
        }

        case PacketType::WorldItemDespawn:
        {
            PacketReader reader(message.Payload);
            int32_t worldObjectId = reader.ReadInt32();

            std::wstringstream ss;
            ss << L"[WORLD ITEM DESPAWN] WorldObjectId=" << worldObjectId;
            Log(ss.str());
            break;
        }

        default:
        {
            Log(L"[PACKET] Type=" + PacketTypeToText(message.Type));
            break;
        }
    }
}

void NetworkClient::Log(const std::wstring& message)
{
    if (_logCallback)
        _logCallback(message);
}

void NetworkClient::CloseSocket()
{
    std::lock_guard lock(_socketMutex);

    if (_socket != INVALID_SOCKET)
    {
        shutdown(_socket, SD_BOTH);
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
}

std::wstring NetworkClient::Utf8ToWide(const std::string& value)
{
    if (value.empty())
        return L"";

    int requiredSize = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);

    if (requiredSize <= 0)
        return L"";

    std::wstring result;
    result.resize(static_cast<size_t>(requiredSize));

    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        requiredSize);

    return result;
}

std::wstring NetworkClient::PacketTypeToText(PacketType type)
{
    std::wstringstream ss;
    ss << static_cast<uint16_t>(type);
    return ss.str();
}