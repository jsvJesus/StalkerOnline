namespace StalkerOnline.Shared.Network;

public enum PacketType : ushort
{
    None = 0,

    LoginRequest = 100,
    LoginResponse = 101,

    PlayerStateSnapshot = 200,

    MoveRequest = 300,
    PlayerPositionUpdate = 301,
    PlayerPositionBroadcast = 302,

    Disconnect = 900
}