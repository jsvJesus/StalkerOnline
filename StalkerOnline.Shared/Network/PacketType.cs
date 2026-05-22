namespace StalkerOnline.Shared.Network;

public enum PacketType : ushort
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
}