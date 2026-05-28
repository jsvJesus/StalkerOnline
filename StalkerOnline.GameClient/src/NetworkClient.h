#pragma once

#include "Packet.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

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

struct PlayerSnapshot
{
    bool Valid = false;

    int32_t AccountId = 0;
    int32_t CharacterId = 0;

    std::string Nickname;

    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;

    float RotationX = 0.0f;
    float RotationY = 0.0f;
    float RotationZ = 0.0f;

    float Health = 0.0f;
    float MaxHealth = 0.0f;

    float Stamina = 0.0f;
    float MaxStamina = 0.0f;

    float Hunger = 0.0f;
    float Thirst = 0.0f;

    float Radiation = 0.0f;
    float Toxicity = 0.0f;

    bool IsAlive = false;
};

struct InventoryItemView
{
    int32_t SlotIndex = 0;

    std::string ItemTemplateId;
    std::string DisplayName;

    int32_t Quantity = 0;
    int32_t MaxStack = 1;

    float WeightPerItem = 0.0f;
};

struct InventorySnapshotView
{
    bool Valid = false;

    int32_t CharacterId = 0;
    int32_t Capacity = 0;

    float TotalWeight = 0.0f;

    std::vector<InventoryItemView> Items;
};

struct WorldItemView
{
    int32_t WorldObjectId = 0;

    std::string ItemTemplateId;
    std::string DisplayName;

    int32_t Quantity = 0;

    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;

    float RotationX = 0.0f;
    float RotationY = 0.0f;
    float RotationZ = 0.0f;
};

struct RemotePlayerView
{
    int32_t CharacterId = 0;

    std::string Nickname;

    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;

    float RotationX = 0.0f;
    float RotationY = 0.0f;
    float RotationZ = 0.0f;

    float Health = 0.0f;
    float MaxHealth = 0.0f;

    bool IsAlive = false;
};

class NetworkClient
{
public:
    using LogCallback = std::function<void(const std::wstring&)>;
    using StateChangedCallback = std::function<void()>;

    NetworkClient(
        LogCallback logCallback,
        StateChangedCallback stateChangedCallback);

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

    bool SendMoveRequest(
        float directionX,
        float directionY,
        float rotationZ,
        float deltaTime = 0.10f);

    bool SendPickupItemRequest(int32_t worldObjectId);
    bool SendDropItemRequest(int32_t slotIndex, int32_t quantity);

    void StartReceiveLoop();
    void Stop();

    bool IsConnected() const;

    PlayerSnapshot GetPlayerSnapshot() const;
    InventorySnapshotView GetInventorySnapshot() const;
    std::vector<WorldItemView> GetWorldItemsSnapshot() const;
    std::vector<RemotePlayerView> GetRemotePlayersSnapshot() const;

private:
    bool SendPacket(PacketType type, const std::vector<uint8_t>& payload);
    bool ReceivePacket(PacketMessage& message);

    bool SendAll(const uint8_t* data, int length);
    bool ReceiveAll(uint8_t* data, int length);

    void ReceiveLoop();
    void HandlePacket(const PacketMessage& message);

    void HandlePlayerStateSnapshot(const PacketMessage& message);
    void HandlePlayerPositionUpdate(const PacketMessage& message);
    void HandlePlayerPositionBroadcast(const PacketMessage& message);
    void HandlePlayerSpawn(const PacketMessage& message);
    void HandlePlayerDespawn(const PacketMessage& message);
    void HandleInventorySnapshot(const PacketMessage& message);
    void HandleWorldItemSpawn(const PacketMessage& message);
    void HandleWorldItemDespawn(const PacketMessage& message);
    void HandlePickupItemResponse(const PacketMessage& message);
    void HandleDropItemResponse(const PacketMessage& message);

    void NotifyStateChanged();

    void Log(const std::wstring& message);
    void CloseSocket();

    static std::wstring Utf8ToWide(const std::string& value);
    static std::wstring PacketTypeToText(PacketType type);

private:
    LogCallback _logCallback;
    StateChangedCallback _stateChangedCallback;

    SOCKET _socket = INVALID_SOCKET;

    mutable std::mutex _stateMutex;
    std::mutex _socketMutex;
    std::mutex _sendMutex;

    PlayerSnapshot _playerSnapshot;
    InventorySnapshotView _inventorySnapshot;
    std::unordered_map<int32_t, WorldItemView> _worldItemsById;
    std::unordered_map<int32_t, RemotePlayerView> _remotePlayersByCharacterId;

    std::atomic_bool _connected = false;
    std::atomic_bool _receiveLoopRunning = false;

    std::thread _receiveThread;
};
