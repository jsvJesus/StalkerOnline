#pragma once

#include "Packet.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <winsock2.h>

struct LoginResult
{
    bool Success = false;
    std::string Message;
};

struct RegisterResult
{
    bool Success = false;
    int32_t AccountId = 0;
    std::string Login;
    std::string Message;
};

class NetworkClient
{
public:
    using LogCallback = std::function<void(const std::wstring&)>;

    explicit NetworkClient(LogCallback logCallback);
    ~NetworkClient();

    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    bool Connect(const std::string& host, uint16_t port);

    LoginResult Login(
        const std::string& login,
        const std::string& password);

    RegisterResult Register(
        const std::string& login,
        const std::string& email,
        const std::string& password);

    void StartReceiveLoop();
    void Stop();

    bool IsConnected() const;

private:
    bool SendPacket(PacketType type, const std::vector<uint8_t>& payload);
    bool ReceivePacket(PacketMessage& message);

    bool SendAll(const uint8_t* data, int length);
    bool ReceiveAll(uint8_t* data, int length);

    void ReceiveLoop();
    void HandlePacket(const PacketMessage& message);

    void Log(const std::wstring& message);
    void CloseSocket();

    static std::wstring Utf8ToWide(const std::string& value);
    static std::wstring PacketTypeToText(PacketType type);

private:
    LogCallback _logCallback;

    SOCKET _socket = INVALID_SOCKET;

    std::mutex _socketMutex;
    std::mutex _sendMutex;

    std::atomic_bool _connected = false;
    std::atomic_bool _receiveLoopRunning = false;

    std::thread _receiveThread;
};