namespace StalkerOnline.Shared.Game;

public sealed class PlayerMovementInput
{
    public NetVector3 MoveDirection { get; set; } = NetVector3.Zero;
    public NetVector3 Rotation { get; set; } = NetVector3.Zero;

    // В секундах. Клиент говорит серверу, сколько времени прошло для этого движения.
    public float DeltaTime { get; set; }
}

public sealed class PlayerPositionUpdate
{
    public int CharacterId { get; set; }

    public NetVector3 Position { get; set; }
    public NetVector3 Rotation { get; set; }
}