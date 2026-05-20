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
}