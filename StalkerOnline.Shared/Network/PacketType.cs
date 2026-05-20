namespace StalkerOnline.Shared.Network;

public enum PacketType : ushort
{
    None = 0,

    Ping = 10,
    Pong = 11,

    LoginRequest = 100,
    LoginResponse = 101,

    PlayerStateSnapshot = 200,

    MoveRequest = 300,
    PlayerPositionUpdate = 301,
    PlayerPositionBroadcast = 302,

    PlayerSpawn = 400,
    PlayerDespawn = 401,

    Disconnect = 900
}