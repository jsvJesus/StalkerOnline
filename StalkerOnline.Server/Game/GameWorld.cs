using System.Collections.Concurrent;
using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Game;

public sealed class GameWorld
{
    private readonly ConcurrentDictionary<int, WorldPlayer> _playersBySessionId = new();

    public int PlayerCount => _playersBySessionId.Count;

    public WorldPlayer AddPlayer(PlayerConnection connection)
    {
        WorldPlayer player = new(connection);

        _playersBySessionId.AddOrUpdate(
            connection.SessionId,
            player,
            (_, _) => player);

        Console.WriteLine(
            $"[WORLD PLAYER ADDED] SessionId={player.SessionId}, CharacterId={player.CharacterId}, Nickname={player.Nickname}, Players={PlayerCount}");

        return player;
    }

    public bool RemovePlayer(int sessionId)
    {
        bool removed = _playersBySessionId.TryRemove(sessionId, out WorldPlayer? player);

        if (removed && player != null)
        {
            Console.WriteLine(
                $"[WORLD PLAYER REMOVED] SessionId={player.SessionId}, CharacterId={player.CharacterId}, Nickname={player.Nickname}, Players={PlayerCount}");
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

    public WorldPlayer? GetPlayer(int sessionId)
    {
        _playersBySessionId.TryGetValue(sessionId, out WorldPlayer? player);
        return player;
    }

    public void Clear()
    {
        _playersBySessionId.Clear();

        Console.WriteLine("[WORLD CLEARED]");
    }

    private static float GetDistanceSquared(NetVector3 a, NetVector3 b)
    {
        float dx = a.X - b.X;
        float dy = a.Y - b.Y;
        float dz = a.Z - b.Z;

        return dx * dx + dy * dy + dz * dz;
    }
}