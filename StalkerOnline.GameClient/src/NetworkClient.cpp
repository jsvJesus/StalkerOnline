#include "NetworkClient.h"

#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

NetworkClient::NetworkClient(
    LogCallback logCallback,
    StateChangedCallback stateChangedCallback)
    : _logCallback(std::move(logCallback)),
      _stateChangedCallback(std::move(stateChangedCallback))
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

bool NetworkClient::SendMoveRequest(
    float directionX,
    float directionY,
    float rotationZ,
    float deltaTime)
{
    PacketWriter writer;

    writer.WriteFloat(directionX);
    writer.WriteFloat(directionY);
    writer.WriteFloat(0.0f);

    writer.WriteFloat(0.0f);
    writer.WriteFloat(0.0f);
    writer.WriteFloat(rotationZ);

    writer.WriteFloat(deltaTime);

    bool sent = SendPacket(PacketType::MoveRequest, writer.Data());

    if (sent)
    {
        std::wstringstream ss;
        ss
            << L"[MOVE REQUEST] Direction=("
            << directionX << L", "
            << directionY << L"), RotationZ="
            << rotationZ;

        Log(ss.str());
    }

    return sent;
}

bool NetworkClient::SendPickupItemRequest(int32_t worldObjectId)
{
    PacketWriter writer;
    writer.WriteInt32(worldObjectId);

    bool sent = SendPacket(PacketType::PickupItemRequest, writer.Data());

    if (sent)
    {
        std::wstringstream ss;
        ss << L"[PICKUP REQUEST] WorldObjectId=" << worldObjectId;
        Log(ss.str());
    }

    return sent;
}

bool NetworkClient::SendDropItemRequest(int32_t slotIndex, int32_t quantity)
{
    if (slotIndex < 0)
    {
        Log(L"[DROP REQUEST] Invalid slot index.");
        return false;
    }

    if (quantity <= 0)
        quantity = 1;

    PacketWriter writer;
    writer.WriteInt32(slotIndex);
    writer.WriteInt32(quantity);

    bool sent = SendPacket(PacketType::DropItemRequest, writer.Data());

    if (sent)
    {
        std::wstringstream ss;
        ss
            << L"[DROP REQUEST] SlotIndex=" << slotIndex
            << L", Quantity=" << quantity;

        Log(ss.str());
    }

    return sent;
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

    {
        std::lock_guard lock(_stateMutex);
        _worldItemsById.clear();
        _remotePlayersByCharacterId.clear();
    }
}

bool NetworkClient::IsConnected() const
{
    return _connected;
}

PlayerSnapshot NetworkClient::GetPlayerSnapshot() const
{
    std::lock_guard lock(_stateMutex);
    return _playerSnapshot;
}

InventorySnapshotView NetworkClient::GetInventorySnapshot() const
{
    std::lock_guard lock(_stateMutex);
    return _inventorySnapshot;
}

std::vector<WorldItemView> NetworkClient::GetWorldItemsSnapshot() const
{
    std::lock_guard lock(_stateMutex);

    std::vector<WorldItemView> result;
    result.reserve(_worldItemsById.size());

    for (const auto& pair : _worldItemsById)
        result.push_back(pair.second);

    std::sort(
        result.begin(),
        result.end(),
        [](const WorldItemView& a, const WorldItemView& b)
        {
            return a.WorldObjectId < b.WorldObjectId;
        });

    return result;
}

std::vector<RemotePlayerView> NetworkClient::GetRemotePlayersSnapshot() const
{
    std::lock_guard lock(_stateMutex);

    std::vector<RemotePlayerView> result;
    result.reserve(_remotePlayersByCharacterId.size());

    for (const auto& pair : _remotePlayersByCharacterId)
        result.push_back(pair.second);

    std::sort(
        result.begin(),
        result.end(),
        [](const RemotePlayerView& a, const RemotePlayerView& b)
        {
            return a.CharacterId < b.CharacterId;
        });

    return result;
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
    NotifyStateChanged();
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
            HandlePlayerStateSnapshot(message);
            break;
        }

        case PacketType::PlayerPositionUpdate:
        {
            HandlePlayerPositionUpdate(message);
            break;
        }

        case PacketType::PlayerPositionBroadcast:
        {
            HandlePlayerPositionBroadcast(message);
            break;
        }

        case PacketType::PlayerSpawn:
        {
            HandlePlayerSpawn(message);
            break;
        }

        case PacketType::PlayerDespawn:
        {
            HandlePlayerDespawn(message);
            break;
        }

        case PacketType::InventorySnapshot:
        {
            HandleInventorySnapshot(message);
            break;
        }

        case PacketType::WorldItemSpawn:
        {
            HandleWorldItemSpawn(message);
            break;
        }

        case PacketType::WorldItemDespawn:
        {
            HandleWorldItemDespawn(message);
            break;
        }

        case PacketType::PickupItemResponse:
        {
            HandlePickupItemResponse(message);
            break;
        }

        case PacketType::DropItemResponse:
        {
            HandleDropItemResponse(message);
            break;
        }

        default:
        {
            Log(L"[PACKET] Type=" + PacketTypeToText(message.Type));
            break;
        }
    }
}

void NetworkClient::HandlePlayerStateSnapshot(const PacketMessage& message)
{
    PacketReader reader(message.Payload);

    PlayerSnapshot snapshot;
    snapshot.Valid = true;

    snapshot.AccountId = reader.ReadInt32();
    snapshot.CharacterId = reader.ReadInt32();
    snapshot.Nickname = reader.ReadString();

    snapshot.PositionX = reader.ReadFloat();
    snapshot.PositionY = reader.ReadFloat();
    snapshot.PositionZ = reader.ReadFloat();

    snapshot.RotationX = reader.ReadFloat();
    snapshot.RotationY = reader.ReadFloat();
    snapshot.RotationZ = reader.ReadFloat();

    snapshot.Health = reader.ReadFloat();
    snapshot.MaxHealth = reader.ReadFloat();

    snapshot.Stamina = reader.ReadFloat();
    snapshot.MaxStamina = reader.ReadFloat();

    snapshot.Hunger = reader.ReadFloat();
    snapshot.Thirst = reader.ReadFloat();

    snapshot.Radiation = reader.ReadFloat();
    snapshot.Toxicity = reader.ReadFloat();

    snapshot.IsAlive = reader.ReadBool();

    {
        std::lock_guard lock(_stateMutex);
        _playerSnapshot = snapshot;
        _remotePlayersByCharacterId.erase(snapshot.CharacterId);
    }

    std::wstringstream ss;
    ss
        << L"[PLAYER STATE] CharacterId=" << snapshot.CharacterId
        << L", Nickname=" << Utf8ToWide(snapshot.Nickname);

    Log(ss.str());
    NotifyStateChanged();
}

void NetworkClient::HandlePlayerPositionUpdate(const PacketMessage& message)
{
    PacketReader reader(message.Payload);

    int32_t characterId = reader.ReadInt32();

    float posX = reader.ReadFloat();
    float posY = reader.ReadFloat();
    float posZ = reader.ReadFloat();

    float rotX = reader.ReadFloat();
    float rotY = reader.ReadFloat();
    float rotZ = reader.ReadFloat();

    {
        std::lock_guard lock(_stateMutex);

        if (_playerSnapshot.Valid && _playerSnapshot.CharacterId == characterId)
        {
            _playerSnapshot.PositionX = posX;
            _playerSnapshot.PositionY = posY;
            _playerSnapshot.PositionZ = posZ;

            _playerSnapshot.RotationX = rotX;
            _playerSnapshot.RotationY = rotY;
            _playerSnapshot.RotationZ = rotZ;
        }
    }

    NotifyStateChanged();
}

void NetworkClient::HandlePlayerPositionBroadcast(const PacketMessage& message)
{
    PacketReader reader(message.Payload);

    const int32_t characterId = reader.ReadInt32();

    const float posX = reader.ReadFloat();
    const float posY = reader.ReadFloat();
    const float posZ = reader.ReadFloat();

    const float rotX = reader.ReadFloat();
    const float rotY = reader.ReadFloat();
    const float rotZ = reader.ReadFloat();

    {
        std::lock_guard lock(_stateMutex);

        if (_playerSnapshot.Valid && _playerSnapshot.CharacterId == characterId)
        {
            _playerSnapshot.PositionX = posX;
            _playerSnapshot.PositionY = posY;
            _playerSnapshot.PositionZ = posZ;

            _playerSnapshot.RotationX = rotX;
            _playerSnapshot.RotationY = rotY;
            _playerSnapshot.RotationZ = rotZ;
        }
        else
        {
            RemotePlayerView& remotePlayer = _remotePlayersByCharacterId[characterId];
            remotePlayer.CharacterId = characterId;

            remotePlayer.PositionX = posX;
            remotePlayer.PositionY = posY;
            remotePlayer.PositionZ = posZ;

            remotePlayer.RotationX = rotX;
            remotePlayer.RotationY = rotY;
            remotePlayer.RotationZ = rotZ;

            if (!remotePlayer.IsAlive)
                remotePlayer.IsAlive = true;
        }
    }

    NotifyStateChanged();
}

void NetworkClient::HandlePlayerSpawn(const PacketMessage& message)
{
    PacketReader reader(message.Payload);

    RemotePlayerView player;

    player.CharacterId = reader.ReadInt32();
    player.Nickname = reader.ReadString();

    player.PositionX = reader.ReadFloat();
    player.PositionY = reader.ReadFloat();
    player.PositionZ = reader.ReadFloat();

    player.RotationX = reader.ReadFloat();
    player.RotationY = reader.ReadFloat();
    player.RotationZ = reader.ReadFloat();

    player.Health = reader.ReadFloat();
    player.MaxHealth = reader.ReadFloat();

    player.IsAlive = reader.ReadBool();

    {
        std::lock_guard lock(_stateMutex);

        if (!_playerSnapshot.Valid || _playerSnapshot.CharacterId != player.CharacterId)
            _remotePlayersByCharacterId[player.CharacterId] = player;
    }

    std::wstringstream ss;
    ss
        << L"[PLAYER SPAWN] CharacterId=" << player.CharacterId
        << L", Nickname=" << Utf8ToWide(player.Nickname);

    Log(ss.str());
    NotifyStateChanged();
}

void NetworkClient::HandlePlayerDespawn(const PacketMessage& message)
{
    PacketReader reader(message.Payload);
    const int32_t characterId = reader.ReadInt32();

    {
        std::lock_guard lock(_stateMutex);
        _remotePlayersByCharacterId.erase(characterId);
    }

    std::wstringstream ss;
    ss << L"[PLAYER DESPAWN] CharacterId=" << characterId;

    Log(ss.str());
    NotifyStateChanged();
}

void NetworkClient::HandleInventorySnapshot(const PacketMessage& message)
{
    PacketReader reader(message.Payload);

    InventorySnapshotView snapshot;
    snapshot.Valid = true;

    snapshot.CharacterId = reader.ReadInt32();
    snapshot.Capacity = reader.ReadInt32();
    snapshot.TotalWeight = reader.ReadFloat();

    int32_t itemCount = reader.ReadInt32();

    if (itemCount < 0)
        itemCount = 0;

    snapshot.Items.reserve(static_cast<size_t>(itemCount));

    for (int32_t i = 0; i < itemCount; ++i)
    {
        InventoryItemView item;

        item.SlotIndex = reader.ReadInt32();
        item.ItemTemplateId = reader.ReadString();
        item.DisplayName = reader.ReadString();
        item.Quantity = reader.ReadInt32();
        item.MaxStack = reader.ReadInt32();
        item.WeightPerItem = reader.ReadFloat();

        snapshot.Items.push_back(item);
    }

    {
        std::lock_guard lock(_stateMutex);
        _inventorySnapshot = std::move(snapshot);
    }

    Log(L"[INVENTORY SNAPSHOT]");
    NotifyStateChanged();
}

void NetworkClient::HandleWorldItemSpawn(const PacketMessage& message)
{
    PacketReader reader(message.Payload);

    WorldItemView item;

    item.WorldObjectId = reader.ReadInt32();
    item.ItemTemplateId = reader.ReadString();
    item.DisplayName = reader.ReadString();
    item.Quantity = reader.ReadInt32();

    item.PositionX = reader.ReadFloat();
    item.PositionY = reader.ReadFloat();
    item.PositionZ = reader.ReadFloat();

    item.RotationX = reader.ReadFloat();
    item.RotationY = reader.ReadFloat();
    item.RotationZ = reader.ReadFloat();

    {
        std::lock_guard lock(_stateMutex);
        _worldItemsById[item.WorldObjectId] = item;
    }

    std::wstringstream ss;
    ss
        << L"[WORLD ITEM SPAWN] Id=" << item.WorldObjectId
        << L", Name=" << Utf8ToWide(item.DisplayName)
        << L", Qty=" << item.Quantity;

    Log(ss.str());
    NotifyStateChanged();
}

void NetworkClient::HandleWorldItemDespawn(const PacketMessage& message)
{
    PacketReader reader(message.Payload);
    int32_t worldObjectId = reader.ReadInt32();

    {
        std::lock_guard lock(_stateMutex);
        _worldItemsById.erase(worldObjectId);
    }

    std::wstringstream ss;
    ss << L"[WORLD ITEM DESPAWN] Id=" << worldObjectId;

    Log(ss.str());
    NotifyStateChanged();
}

void NetworkClient::HandlePickupItemResponse(const PacketMessage& message)
{
    PacketReader reader(message.Payload);

    bool success = reader.ReadBool();
    int32_t worldObjectId = reader.ReadInt32();

    std::string itemTemplateId = reader.ReadString();
    std::string displayName = reader.ReadString();

    int32_t quantity = reader.ReadInt32();
    std::string responseMessage = reader.ReadString();

    if (success)
    {
        std::lock_guard lock(_stateMutex);
        _worldItemsById.erase(worldObjectId);
    }

    std::wstringstream ss;
    ss
        << L"[PICKUP RESPONSE] Success=" << (success ? L"true" : L"false")
        << L", Id=" << worldObjectId
        << L", Name=" << Utf8ToWide(displayName)
        << L", Qty=" << quantity
        << L", Message=" << Utf8ToWide(responseMessage);

    Log(ss.str());
    NotifyStateChanged();
}

void NetworkClient::HandleDropItemResponse(const PacketMessage& message)
{
    PacketReader reader(message.Payload);

    bool success = reader.ReadBool();

    int32_t worldObjectId = reader.ReadInt32();
    int32_t slotIndex = reader.ReadInt32();

    std::string itemTemplateId = reader.ReadString();
    std::string displayName = reader.ReadString();

    int32_t quantity = reader.ReadInt32();
    std::string responseMessage = reader.ReadString();

    std::wstringstream ss;
    ss
        << L"[DROP RESPONSE] Success=" << (success ? L"true" : L"false")
        << L", WorldObjectId=" << worldObjectId
        << L", Slot=" << slotIndex
        << L", Name=" << Utf8ToWide(displayName)
        << L", Qty=" << quantity
        << L", Message=" << Utf8ToWide(responseMessage);

    Log(ss.str());
    NotifyStateChanged();
}

void NetworkClient::NotifyStateChanged()
{
    if (_stateChangedCallback)
        _stateChangedCallback();
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
