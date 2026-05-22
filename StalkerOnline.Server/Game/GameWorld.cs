using System.Collections.Concurrent;
using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Game;

public sealed class GameWorld
{
    private readonly ConcurrentDictionary<int, WorldPlayer> _playersBySessionId = new();
    private readonly ConcurrentDictionary<int, WorldObject> _objectsByWorldObjectId = new();

    private readonly float _moveSpeed;
    private readonly float _defaultDeltaTime;
    private readonly float _maxDeltaTime;

    private int _nextWorldObjectId;

    public int PlayerCount => _playersBySessionId.Count;
    public int WorldObjectCount => _objectsByWorldObjectId.Count;

    public GameWorld(
        float moveSpeed,
        float defaultDeltaTime,
        float maxDeltaTime)
    {
        _moveSpeed = moveSpeed;
        _defaultDeltaTime = defaultDeltaTime;
        _maxDeltaTime = maxDeltaTime;
    }

    public WorldPlayer AddPlayer(PlayerConnection connection)
    {
        int worldObjectId = CreateWorldObjectId();

        WorldPlayer player = new(
            worldObjectId,
            connection,
            _moveSpeed,
            _defaultDeltaTime,
            _maxDeltaTime);

        _playersBySessionId.AddOrUpdate(
            connection.SessionId,
            player,
            static (_, oldPlayer) =>
            {
                return oldPlayer;
            });

        if (_playersBySessionId.TryGetValue(connection.SessionId, out WorldPlayer? existingPlayer))
        {
            if (existingPlayer.WorldObjectId != player.WorldObjectId)
                _objectsByWorldObjectId.TryRemove(existingPlayer.WorldObjectId, out WorldObject? removedOldObject);
        }

        _playersBySessionId[connection.SessionId] = player;
        _objectsByWorldObjectId[player.WorldObjectId] = player;

        Console.WriteLine(
            $"[WORLD PLAYER ADDED] SessionId={player.SessionId}, WorldObjectId={player.WorldObjectId}, CharacterId={player.CharacterId}, Nickname={player.Nickname}, Players={PlayerCount}, Objects={WorldObjectCount}");

        return player;
    }

    public bool RemovePlayer(int sessionId)
    {
        bool removed = _playersBySessionId.TryRemove(sessionId, out WorldPlayer? player);

        if (removed && player != null)
        {
            player.SetActive(false);
            _objectsByWorldObjectId.TryRemove(player.WorldObjectId, out _);

            Console.WriteLine(
                $"[WORLD PLAYER REMOVED] SessionId={player.SessionId}, WorldObjectId={player.WorldObjectId}, CharacterId={player.CharacterId}, Nickname={player.Nickname}, Players={PlayerCount}, Objects={WorldObjectCount}");
        }

        return removed;
    }

    public PlayerPositionUpdate? ApplyMovement(int sessionId, PlayerMovementInput input)
    {
        if (!_playersBySessionId.TryGetValue(sessionId, out WorldPlayer? player))
            return null;

        return player.ApplyMovement(input);
    }

    public List<PlayerState> CreatePlayerStateSnapshots()
    {
        List<PlayerState> snapshots = new();

        foreach (WorldPlayer player in _playersBySessionId.Values)
        {
            snapshots.Add(player.CreateStateSnapshot());
        }

        return snapshots;
    }

    public List<WorldObjectSnapshot> CreateWorldObjectSnapshots()
    {
        List<WorldObjectSnapshot> snapshots = new();

        foreach (WorldObject worldObject in _objectsByWorldObjectId.Values)
        {
            snapshots.Add(worldObject.CreateWorldObjectSnapshot());
        }

        return snapshots;
    }

    public List<int> GetNearbyPlayerSessionIds(int sourceSessionId, float radius)
    {
        List<int> result = new();

        if (!_playersBySessionId.TryGetValue(sourceSessionId, out WorldPlayer? sourcePlayer))
            return result;

        NetVector3 sourcePosition = sourcePlayer.GetPosition();

        float radiusSquared = radius * radius;

        foreach (KeyValuePair<int, WorldPlayer> pair in _playersBySessionId)
        {
            int targetSessionId = pair.Key;

            if (targetSessionId == sourceSessionId)
                continue;

            WorldPlayer targetPlayer = pair.Value;
            NetVector3 targetPosition = targetPlayer.GetPosition();

            float distanceSquared = GetDistanceSquared(sourcePosition, targetPosition);

            if (distanceSquared <= radiusSquared)
                result.Add(targetSessionId);
        }

        return result;
    }

    public List<WorldPlayer> GetNearbyPlayers(int sourceSessionId, float radius)
    {
        List<WorldPlayer> result = new();

        if (!_playersBySessionId.TryGetValue(sourceSessionId, out WorldPlayer? sourcePlayer))
            return result;

        NetVector3 sourcePosition = sourcePlayer.GetPosition();

        float radiusSquared = radius * radius;

        foreach (KeyValuePair<int, WorldPlayer> pair in _playersBySessionId)
        {
            int targetSessionId = pair.Key;

            if (targetSessionId == sourceSessionId)
                continue;

            WorldPlayer targetPlayer = pair.Value;
            NetVector3 targetPosition = targetPlayer.GetPosition();

            float distanceSquared = GetDistanceSquared(sourcePosition, targetPosition);

            if (distanceSquared <= radiusSquared)
                result.Add(targetPlayer);
        }

        return result;
    }

    public List<WorldObject> GetNearbyWorldObjects(
        NetVector3 sourcePosition,
        float radius,
        WorldObjectType objectType = WorldObjectType.None)
    {
        List<WorldObject> result = new();

        float radiusSquared = radius * radius;

        foreach (WorldObject worldObject in _objectsByWorldObjectId.Values)
        {
            if (!worldObject.IsActive)
                continue;

            if (objectType != WorldObjectType.None && worldObject.ObjectType != objectType)
                continue;

            NetVector3 targetPosition = worldObject.GetPosition();

            float distanceSquared = GetDistanceSquared(sourcePosition, targetPosition);

            if (distanceSquared <= radiusSquared)
                result.Add(worldObject);
        }

        return result;
    }

    public List<WorldObject> GetNearbyWorldObjectsForPlayer(
        int sourceSessionId,
        float radius,
        WorldObjectType objectType = WorldObjectType.None)
    {
        if (!_playersBySessionId.TryGetValue(sourceSessionId, out WorldPlayer? sourcePlayer))
            return new List<WorldObject>();

        return GetNearbyWorldObjects(
            sourcePlayer.GetPosition(),
            radius,
            objectType);
    }

    public WorldPlayer? GetPlayer(int sessionId)
    {
        _playersBySessionId.TryGetValue(sessionId, out WorldPlayer? player);
        return player;
    }

    public WorldObject? GetWorldObject(int worldObjectId)
    {
        _objectsByWorldObjectId.TryGetValue(worldObjectId, out WorldObject? worldObject);
        return worldObject;
    }

    public List<WorldObject> GetWorldObjectsByType(WorldObjectType objectType)
    {
        List<WorldObject> result = new();

        foreach (WorldObject worldObject in _objectsByWorldObjectId.Values)
        {
            if (!worldObject.IsActive)
                continue;

            if (worldObject.ObjectType == objectType)
                result.Add(worldObject);
        }

        return result;
    }

    public void Clear()
    {
        _playersBySessionId.Clear();
        _objectsByWorldObjectId.Clear();

        Console.WriteLine("[WORLD CLEARED]");
    }

    private int CreateWorldObjectId()
    {
        return Interlocked.Increment(ref _nextWorldObjectId);
    }

    private static float GetDistanceSquared(NetVector3 a, NetVector3 b)
    {
        float dx = a.X - b.X;
        float dy = a.Y - b.Y;
        float dz = a.Z - b.Z;

        return dx * dx + dy * dy + dz * dz;
    }
}