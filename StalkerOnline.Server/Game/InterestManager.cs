using System.Collections.Generic;

namespace StalkerOnline.Server.Game;

public sealed class InterestManager
{
    private readonly object _lock = new();

    private readonly Dictionary<int, HashSet<int>> _visiblePlayersByObserverSessionId = new();
    private readonly Dictionary<int, HashSet<int>> _visibleWorldItemsByObserverSessionId = new();

    public void RegisterPlayer(int sessionId)
    {
        lock (_lock)
        {
            if (!_visiblePlayersByObserverSessionId.ContainsKey(sessionId))
                _visiblePlayersByObserverSessionId[sessionId] = new HashSet<int>();

            if (!_visibleWorldItemsByObserverSessionId.ContainsKey(sessionId))
                _visibleWorldItemsByObserverSessionId[sessionId] = new HashSet<int>();
        }
    }

    public void UnregisterPlayer(int sessionId)
    {
        lock (_lock)
        {
            _visiblePlayersByObserverSessionId.Remove(sessionId);
            _visibleWorldItemsByObserverSessionId.Remove(sessionId);

            foreach (HashSet<int> visiblePlayers in _visiblePlayersByObserverSessionId.Values)
            {
                visiblePlayers.Remove(sessionId);
            }
        }
    }

    public InterestVisibilityChanges RefreshVisiblePlayers(
        int observerSessionId,
        IEnumerable<int> currentVisibleSessionIds)
    {
        lock (_lock)
        {
            if (!_visiblePlayersByObserverSessionId.TryGetValue(
                    observerSessionId,
                    out HashSet<int>? oldVisiblePlayers))
            {
                oldVisiblePlayers = new HashSet<int>();
                _visiblePlayersByObserverSessionId[observerSessionId] = oldVisiblePlayers;
            }

            HashSet<int> newVisiblePlayers = new(currentVisibleSessionIds);
            newVisiblePlayers.Remove(observerSessionId);

            List<int> playersToSpawn = new();
            List<int> playersToDespawn = new();

            foreach (int sessionId in newVisiblePlayers)
            {
                if (!oldVisiblePlayers.Contains(sessionId))
                    playersToSpawn.Add(sessionId);
            }

            foreach (int sessionId in oldVisiblePlayers)
            {
                if (!newVisiblePlayers.Contains(sessionId))
                    playersToDespawn.Add(sessionId);
            }

            oldVisiblePlayers.Clear();

            foreach (int sessionId in newVisiblePlayers)
            {
                oldVisiblePlayers.Add(sessionId);
            }

            return new InterestVisibilityChanges(playersToSpawn, playersToDespawn);
        }
    }

    public InterestWorldItemVisibilityChanges RefreshVisibleWorldItems(
        int observerSessionId,
        IEnumerable<int> currentVisibleWorldItemIds)
    {
        lock (_lock)
        {
            if (!_visibleWorldItemsByObserverSessionId.TryGetValue(
                    observerSessionId,
                    out HashSet<int>? oldVisibleWorldItems))
            {
                oldVisibleWorldItems = new HashSet<int>();
                _visibleWorldItemsByObserverSessionId[observerSessionId] = oldVisibleWorldItems;
            }

            HashSet<int> newVisibleWorldItems = new(currentVisibleWorldItemIds);

            List<int> itemsToSpawn = new();
            List<int> itemsToDespawn = new();

            foreach (int worldObjectId in newVisibleWorldItems)
            {
                if (!oldVisibleWorldItems.Contains(worldObjectId))
                    itemsToSpawn.Add(worldObjectId);
            }

            foreach (int worldObjectId in oldVisibleWorldItems)
            {
                if (!newVisibleWorldItems.Contains(worldObjectId))
                    itemsToDespawn.Add(worldObjectId);
            }

            oldVisibleWorldItems.Clear();

            foreach (int worldObjectId in newVisibleWorldItems)
            {
                oldVisibleWorldItems.Add(worldObjectId);
            }

            return new InterestWorldItemVisibilityChanges(itemsToSpawn, itemsToDespawn);
        }
    }

    public bool AddVisiblePlayer(int observerSessionId, int targetSessionId)
    {
        if (observerSessionId == targetSessionId)
            return false;

        lock (_lock)
        {
            if (!_visiblePlayersByObserverSessionId.TryGetValue(
                    observerSessionId,
                    out HashSet<int>? visiblePlayers))
            {
                visiblePlayers = new HashSet<int>();
                _visiblePlayersByObserverSessionId[observerSessionId] = visiblePlayers;
            }

            return visiblePlayers.Add(targetSessionId);
        }
    }

    public bool RemoveVisiblePlayer(int observerSessionId, int targetSessionId)
    {
        lock (_lock)
        {
            if (!_visiblePlayersByObserverSessionId.TryGetValue(
                    observerSessionId,
                    out HashSet<int>? visiblePlayers))
            {
                return false;
            }

            return visiblePlayers.Remove(targetSessionId);
        }
    }

    public List<int> GetVisiblePlayers(int observerSessionId)
    {
        lock (_lock)
        {
            if (!_visiblePlayersByObserverSessionId.TryGetValue(
                    observerSessionId,
                    out HashSet<int>? visiblePlayers))
            {
                return new List<int>();
            }

            return visiblePlayers.ToList();
        }
    }

    public List<int> GetObserversSeeingPlayer(int targetSessionId)
    {
        lock (_lock)
        {
            List<int> result = new();

            foreach (KeyValuePair<int, HashSet<int>> pair in _visiblePlayersByObserverSessionId)
            {
                int observerSessionId = pair.Key;
                HashSet<int> visiblePlayers = pair.Value;

                if (visiblePlayers.Contains(targetSessionId))
                    result.Add(observerSessionId);
            }

            return result;
        }
    }

    public List<int> RemoveVisibleWorldItemFromAll(int worldObjectId)
    {
        lock (_lock)
        {
            List<int> observers = new();

            foreach (KeyValuePair<int, HashSet<int>> pair in _visibleWorldItemsByObserverSessionId)
            {
                int observerSessionId = pair.Key;
                HashSet<int> visibleWorldItems = pair.Value;

                if (visibleWorldItems.Remove(worldObjectId))
                    observers.Add(observerSessionId);
            }

            return observers;
        }
    }

    public bool CanSee(int observerSessionId, int targetSessionId)
    {
        lock (_lock)
        {
            if (!_visiblePlayersByObserverSessionId.TryGetValue(
                    observerSessionId,
                    out HashSet<int>? visiblePlayers))
            {
                return false;
            }

            return visiblePlayers.Contains(targetSessionId);
        }
    }

    public void Clear()
    {
        lock (_lock)
        {
            _visiblePlayersByObserverSessionId.Clear();
            _visibleWorldItemsByObserverSessionId.Clear();
        }
    }
}

public sealed class InterestVisibilityChanges
{
    public List<int> PlayersToSpawn { get; }
    public List<int> PlayersToDespawn { get; }

    public bool HasChanges => PlayersToSpawn.Count > 0 || PlayersToDespawn.Count > 0;

    public InterestVisibilityChanges(
        List<int> playersToSpawn,
        List<int> playersToDespawn)
    {
        PlayersToSpawn = playersToSpawn;
        PlayersToDespawn = playersToDespawn;
    }
}

public sealed class InterestWorldItemVisibilityChanges
{
    public List<int> ItemsToSpawn { get; }
    public List<int> ItemsToDespawn { get; }

    public bool HasChanges => ItemsToSpawn.Count > 0 || ItemsToDespawn.Count > 0;

    public InterestWorldItemVisibilityChanges(
        List<int> itemsToSpawn,
        List<int> itemsToDespawn)
    {
        ItemsToSpawn = itemsToSpawn;
        ItemsToDespawn = itemsToDespawn;
    }
}