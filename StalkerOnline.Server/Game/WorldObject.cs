using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Game;

public abstract class WorldObject
{
    private readonly object _transformLock = new();

    private NetVector3 _position;
    private NetVector3 _rotation;

    public int WorldObjectId { get; }
    public WorldObjectType ObjectType { get; }

    public bool IsActive { get; private set; } = true;

    public DateTime CreatedAtUtc { get; }
    public DateTime UpdatedAtUtc { get; private set; }

    protected WorldObject(
        int worldObjectId,
        WorldObjectType objectType,
        NetVector3 position,
        NetVector3 rotation)
    {
        WorldObjectId = worldObjectId;
        ObjectType = objectType;

        _position = position;
        _rotation = rotation;

        CreatedAtUtc = DateTime.UtcNow;
        UpdatedAtUtc = CreatedAtUtc;
    }

    public NetVector3 GetPosition()
    {
        lock (_transformLock)
        {
            return _position;
        }
    }

    public NetVector3 GetRotation()
    {
        lock (_transformLock)
        {
            return _rotation;
        }
    }

    public void SetPosition(NetVector3 position)
    {
        lock (_transformLock)
        {
            _position = SanitizeVector(position);
            UpdatedAtUtc = DateTime.UtcNow;
        }
    }

    public void SetRotation(NetVector3 rotation)
    {
        lock (_transformLock)
        {
            _rotation = SanitizeVector(rotation);
            UpdatedAtUtc = DateTime.UtcNow;
        }
    }

    public void SetTransform(NetVector3 position, NetVector3 rotation)
    {
        lock (_transformLock)
        {
            _position = SanitizeVector(position);
            _rotation = SanitizeVector(rotation);
            UpdatedAtUtc = DateTime.UtcNow;
        }
    }

    public void SetActive(bool isActive)
    {
        IsActive = isActive;
        UpdatedAtUtc = DateTime.UtcNow;
    }

    public WorldObjectSnapshot CreateWorldObjectSnapshot()
    {
        lock (_transformLock)
        {
            return new WorldObjectSnapshot
            {
                WorldObjectId = WorldObjectId,
                ObjectType = ObjectType,

                Position = _position,
                Rotation = _rotation,

                IsActive = IsActive,

                CreatedAtUtc = CreatedAtUtc,
                UpdatedAtUtc = UpdatedAtUtc
            };
        }
    }

    private static NetVector3 SanitizeVector(NetVector3 vector)
    {
        return new NetVector3(
            SanitizeFloat(vector.X),
            SanitizeFloat(vector.Y),
            SanitizeFloat(vector.Z));
    }

    private static float SanitizeFloat(float value)
    {
        if (float.IsNaN(value) || float.IsInfinity(value))
            return 0f;

        return value;
    }
}