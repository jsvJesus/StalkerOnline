#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

enum class PacketType : uint16_t
{
    None = 0,

    Ping = 10,
    Pong = 11,

    ServerMessage = 20,
    ErrorMessage = 21,

    LoginRequest = 100,
    LoginResponse = 101,

    RegisterRequest = 102,
    RegisterResponse = 103,

    PlayerStateSnapshot = 200,

    MoveRequest = 300,
    PlayerPositionUpdate = 301,
    PlayerPositionBroadcast = 302,

    PlayerSpawn = 400,
    PlayerDespawn = 401,

    WorldItemSpawn = 500,
    WorldItemDespawn = 501,

    InventorySnapshot = 600,

    PickupItemRequest = 610,
    PickupItemResponse = 611,

    DropItemRequest = 620,
    DropItemResponse = 621,

    InventoryMoveRequest = 630,
    InventoryMoveResponse = 631,

    Disconnect = 900
};

struct PacketMessage
{
    PacketType Type = PacketType::None;
    std::vector<uint8_t> Payload;
};

class PacketWriter
{
public:
    void WriteBool(bool value)
    {
        _buffer.push_back(value ? 1 : 0);
    }

    void WriteInt32(int32_t value)
    {
        _buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        _buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        _buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        _buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    void WriteUInt16(uint16_t value)
    {
        _buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        _buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    void WriteFloat(float value)
    {
        uint32_t bits = 0;
        static_assert(sizeof(float) == sizeof(uint32_t));
        std::memcpy(&bits, &value, sizeof(float));

        _buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
        _buffer.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
        _buffer.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
        _buffer.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
    }

    void WriteString(const std::string& value)
    {
        WriteInt32(static_cast<int32_t>(value.size()));

        for (char c : value)
            _buffer.push_back(static_cast<uint8_t>(c));
    }

    const std::vector<uint8_t>& Data() const
    {
        return _buffer;
    }

private:
    std::vector<uint8_t> _buffer;
};

class PacketReader
{
public:
    explicit PacketReader(const std::vector<uint8_t>& buffer)
        : _buffer(buffer)
    {
    }

    bool ReadBool()
    {
        EnsureAvailable(1);
        return _buffer[_offset++] != 0;
    }

    int32_t ReadInt32()
    {
        EnsureAvailable(4);

        int32_t value =
            static_cast<int32_t>(_buffer[_offset]) |
            (static_cast<int32_t>(_buffer[_offset + 1]) << 8) |
            (static_cast<int32_t>(_buffer[_offset + 2]) << 16) |
            (static_cast<int32_t>(_buffer[_offset + 3]) << 24);

        _offset += 4;
        return value;
    }

    uint16_t ReadUInt16()
    {
        EnsureAvailable(2);

        uint16_t value =
            static_cast<uint16_t>(_buffer[_offset]) |
            static_cast<uint16_t>(_buffer[_offset + 1] << 8);

        _offset += 2;
        return value;
    }

    float ReadFloat()
    {
        EnsureAvailable(4);

        uint32_t bits =
            static_cast<uint32_t>(_buffer[_offset]) |
            (static_cast<uint32_t>(_buffer[_offset + 1]) << 8) |
            (static_cast<uint32_t>(_buffer[_offset + 2]) << 16) |
            (static_cast<uint32_t>(_buffer[_offset + 3]) << 24);

        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(float));

        _offset += 4;
        return value;
    }

    std::string ReadString()
    {
        int32_t length = ReadInt32();

        if (length < 0)
            throw std::runtime_error("String length is negative.");

        EnsureAvailable(static_cast<size_t>(length));

        std::string value;
        value.resize(static_cast<size_t>(length));

        if (length > 0)
            std::memcpy(value.data(), _buffer.data() + _offset, static_cast<size_t>(length));

        _offset += static_cast<size_t>(length);
        return value;
    }

private:
    void EnsureAvailable(size_t count)
    {
        if (_offset + count > _buffer.size())
            throw std::runtime_error("Packet payload ended unexpectedly.");
    }

private:
    const std::vector<uint8_t>& _buffer;
    size_t _offset = 0;
};