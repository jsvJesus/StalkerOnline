using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Game;

public sealed class WorldPlayer
{
    private const float MoveSpeed = 4.5f;
    private const float DefaultDeltaTime = 0.05f;
    private const float MaxDeltaTime = 0.10f;

    private readonly object _lock = new();

    public int SessionId { get; }
    public int AccountId { get; }
    public int CharacterId { get; }
    public string Login { get; }
    public string Nickname { get; }

    public PlayerState State { get; }

    public DateTime SpawnedAtUtc { get; }
    public DateTime LastMovementAtUtc { get; private set; }

    public WorldPlayer(PlayerConnection connection)
    {
        SessionId = connection.SessionId;
        AccountId = connection.AccountId;
        Login = connection.Login;

        State = connection.State;

        CharacterId = State.CharacterId;
        Nickname = State.Nickname;

        SpawnedAtUtc = DateTime.UtcNow;
        LastMovementAtUtc = SpawnedAtUtc;
    }

    public PlayerPositionUpdate ApplyMovement(PlayerMovementInput input)
    {
        lock (_lock)
        {
            if (!State.IsAlive)
                return CreatePositionUpdate();

            float deltaTime = SanitizeDeltaTime(input.DeltaTime);
            NetVector3 direction = SanitizeMoveDirection(input.MoveDirection);
            NetVector3 rotation = SanitizeRotation(input.Rotation);

            float distance = MoveSpeed * deltaTime;

            State.Position = new NetVector3(
                State.Position.X + direction.X * distance,
                State.Position.Y + direction.Y * distance,
                State.Position.Z);

            State.Rotation = rotation;

            LastMovementAtUtc = DateTime.UtcNow;

            return CreatePositionUpdate();
        }
    }

    public NetVector3 GetPosition()
    {
        lock (_lock)
        {
            return State.Position;
        }
    }

    public PlayerPositionUpdate CreatePositionUpdate()
    {
        lock (_lock)
        {
            return new PlayerPositionUpdate
            {
                CharacterId = State.CharacterId,
                Position = State.Position,
                Rotation = State.Rotation
            };
        }
    }

    public PlayerSpawnInfo CreateSpawnInfo()
    {
        lock (_lock)
        {
            return new PlayerSpawnInfo
            {
                CharacterId = State.CharacterId,

                Nickname = State.Nickname,

                Position = State.Position,
                Rotation = State.Rotation,

                Health = State.Health,
                MaxHealth = State.MaxHealth,

                IsAlive = State.IsAlive
            };
        }
    }

    private static float SanitizeDeltaTime(float deltaTime)
    {
        if (float.IsNaN(deltaTime) || float.IsInfinity(deltaTime))
            return DefaultDeltaTime;

        if (deltaTime <= 0f)
            return DefaultDeltaTime;

        return Math.Clamp(deltaTime, 0.001f, MaxDeltaTime);
    }

    private static NetVector3 SanitizeMoveDirection(NetVector3 direction)
    {
        float x = SanitizeFloat(direction.X);
        float y = SanitizeFloat(direction.Y);

        float length = MathF.Sqrt(x * x + y * y);

        if (length <= 0.0001f)
            return NetVector3.Zero;

        if (length > 1f)
        {
            x /= length;
            y /= length;
        }

        return new NetVector3(x, y, 0f);
    }

    private static NetVector3 SanitizeRotation(NetVector3 rotation)
    {
        return new NetVector3(
            SanitizeFloat(rotation.X),
            SanitizeFloat(rotation.Y),
            SanitizeFloat(rotation.Z));
    }

    private static float SanitizeFloat(float value)
    {
        if (float.IsNaN(value) || float.IsInfinity(value))
            return 0f;

        return value;
    }
}