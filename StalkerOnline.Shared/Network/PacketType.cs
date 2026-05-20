namespace StalkerOnline.Shared.Network;

public enum PacketType : ushort
{
    None = 0,

    LoginRequest = 100,
    LoginResponse = 101,

    Disconnect = 900
}