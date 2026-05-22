using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Game;

public sealed class WorldPlayer : WorldObject
{
    private readonly object _lock = new();

    private readonly float _moveSpeed;
    private readonly float _defaultDeltaTime;
    private readonly float _maxDeltaTime;

    public int SessionId { get; }
    public int AccountId { get; }
    public int CharacterId { get; }
    public string Login { get; }
    public string Nickname { get; }

    public PlayerState State { get; }
    public PlayerInventory Inventory { get; }

    public DateTime SpawnedAtUtc { get; }
    public DateTime LastMovementAtUtc { get; private set; }

    public WorldPlayer(
        int worldObjectId,
        PlayerConnection connection,
        float moveSpeed,
        float defaultDeltaTime,
        float maxDeltaTime)
        : base(
            worldObjectId,
            WorldObjectType.Player,
            connection.State.Position,
            connection.State.Rotation)
    {
        _moveSpeed = moveSpeed;
        _defaultDeltaTime = defaultDeltaTime;
        _maxDeltaTime = maxDeltaTime;

        SessionId = connection.SessionId;
        AccountId = connection.AccountId;
        Login = connection.Login;

        State = connection.State;

        CharacterId = State.CharacterId;
        Nickname = State.Nickname;

        Inventory = new PlayerInventory(capacity: 30);

        SpawnedAtUtc = DateTime.UtcNow;
        LastMovementAtUtc = SpawnedAtUtc;

        AddStarterItems();
    }

    public PlayerPositionUpdate ApplyMovement(PlayerMovementInput input)
    {
        lock (_lock)
        {
            if (!State.IsAlive)
                return CreatePositionUpdateUnsafe();

            float deltaTime = SanitizeDeltaTime(input.DeltaTime);
            NetVector3 direction = SanitizeMoveDirection(input.MoveDirection);
            NetVector3 rotation = SanitizeRotation(input.Rotation);

            float distance = _moveSpeed * deltaTime;

            State.Position = new NetVector3(
                State.Position.X + direction.X * distance,
                State.Position.Y + direction.Y * distance,
                State.Position.Z);

            State.Rotation = rotation;

            SetTransform(State.Position, State.Rotation);

            LastMovementAtUtc = DateTime.UtcNow;

            return CreatePositionUpdateUnsafe();
        }
    }

    public PlayerState CreateStateSnapshot()
    {
        lock (_lock)
        {
            return new PlayerState
            {
                AccountId = State.AccountId,
                CharacterId = State.CharacterId,
                Nickname = State.Nickname,

                Position = State.Position,
                Rotation = State.Rotation,

                Health = State.Health,
                MaxHealth = State.MaxHealth,

                Stamina = State.Stamina,
                MaxStamina = State.MaxStamina,

                Hunger = State.Hunger,
                Thirst = State.Thirst,

                Radiation = State.Radiation,
                Toxicity = State.Toxicity,

                IsAlive = State.IsAlive
            };
        }
    }

    public InventorySnapshot CreateInventorySnapshot()
    {
        return Inventory.CreateSnapshot(CharacterId);
    }

    public PlayerPositionUpdate CreatePositionUpdate()
    {
        lock (_lock)
        {
            return CreatePositionUpdateUnsafe();
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

    private void AddStarterItems()
    {
        Inventory.AddItem(
            itemTemplateId: "bandage",
            displayName: "Bandage",
            quantity: 3,
            maxStack: 10,
            weightPerItem: 0.05f);

        Inventory.AddItem(
            itemTemplateId: "medkit_basic",
            displayName: "Basic Medkit",
            quantity: 2,
            maxStack: 5,
            weightPerItem: 0.35f);

        Inventory.AddItem(
            itemTemplateId: "ammo_9x18",
            displayName: "9x18 Ammo",
            quantity: 125,
            maxStack: 60,
            weightPerItem: 0.01f);

        Inventory.AddItem(
            itemTemplateId: "water_bottle",
            displayName: "Bottle of Water",
            quantity: 2,
            maxStack: 5,
            weightPerItem: 0.50f);
    }

    private PlayerPositionUpdate CreatePositionUpdateUnsafe()
    {
        return new PlayerPositionUpdate
        {
            CharacterId = State.CharacterId,
            Position = State.Position,
            Rotation = State.Rotation
        };
    }

    private float SanitizeDeltaTime(float deltaTime)
    {
        if (float.IsNaN(deltaTime) || float.IsInfinity(deltaTime))
            return _defaultDeltaTime;

        if (deltaTime <= 0f)
            return _defaultDeltaTime;

        return Math.Clamp(deltaTime, 0.001f, _maxDeltaTime);
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