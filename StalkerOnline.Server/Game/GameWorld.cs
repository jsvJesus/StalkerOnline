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
            (existingSessionId, oldPlayer) =>
            {
                _objectsByWorldObjectId.TryRemove(
                    oldPlayer.WorldObjectId,
                    out WorldObject? removedObject);

                return player;
            });

        _objectsByWorldObjectId.AddOrUpdate(
            player.WorldObjectId,
            player,
            (existingWorldObjectId, oldObject) => player);

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
    
    public WorldItem SpawnWorldItem(
        string itemTemplateId,
        string displayName,
        int quantity,
        NetVector3 position,
        NetVector3 rotation,
        int maxStack = 1,
        float weightPerItem = 0f)
    {
        int worldObjectId = CreateWorldObjectId();

        WorldItem item = new(
            worldObjectId,
            itemTemplateId,
            displayName,
            quantity,
            maxStack,
            weightPerItem,
            position,
            rotation);

        _objectsByWorldObjectId.AddOrUpdate(
            item.WorldObjectId,
            item,
            (existingWorldObjectId, oldObject) => item);

        Console.WriteLine(
            $"[WORLD ITEM SPAWNED] WorldObjectId={item.WorldObjectId}, TemplateId={item.ItemTemplateId}, Name={item.DisplayName}, Quantity={item.Quantity}, MaxStack={item.MaxStack}, Weight={item.WeightPerItem:0.00}, Position={item.GetPosition()}, Objects={WorldObjectCount}");

        return item;
    }

    public bool RemoveWorldItem(int worldObjectId)
    {
        bool removed = _objectsByWorldObjectId.TryRemove(worldObjectId, out WorldObject? worldObject);

        if (removed && worldObject is WorldItem item)
        {
            item.SetActive(false);

            Console.WriteLine(
                $"[WORLD ITEM REMOVED] WorldObjectId={item.WorldObjectId}, TemplateId={item.ItemTemplateId}, Name={item.DisplayName}, Objects={WorldObjectCount}");

            return true;
        }

        return false;
    }

    public WorldItem? GetWorldItem(int worldObjectId)
    {
        if (!_objectsByWorldObjectId.TryGetValue(worldObjectId, out WorldObject? worldObject))
            return null;

        return worldObject as WorldItem;
    }

    public List<WorldItem> GetNearbyWorldItemsForPlayer(
        int sourceSessionId,
        float radius)
    {
        List<WorldItem> result = new();

        List<WorldObject> nearbyObjects = GetNearbyWorldObjectsForPlayer(
            sourceSessionId,
            radius,
            WorldObjectType.Item);

        foreach (WorldObject worldObject in nearbyObjects)
        {
            if (worldObject is WorldItem item)
                result.Add(item);
        }

        return result;
    }
    
    public WorldItemPickupResult TryPickupWorldItem(
    int pickerSessionId,
    int worldObjectId,
    float maxPickupDistance)
    {
        if (!_playersBySessionId.TryGetValue(pickerSessionId, out WorldPlayer? player))
        {
            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                "Player was not found in world.");
        }

        if (!player.State.IsAlive)
        {
            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                "Dead player cannot pickup items.");
        }

        if (!_objectsByWorldObjectId.TryGetValue(worldObjectId, out WorldObject? worldObject))
        {
            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                "World item was not found.");
        }

        if (worldObject is not WorldItem item)
        {
            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                "World object is not an item.");
        }

        if (!item.IsActive)
        {
            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                "World item is not active.");
        }

        if (float.IsNaN(maxPickupDistance) || float.IsInfinity(maxPickupDistance) || maxPickupDistance <= 0f)
            maxPickupDistance = 2.0f;

        NetVector3 playerPosition = player.GetPosition();
        NetVector3 itemPosition = item.GetPosition();

        float maxDistanceSquared = maxPickupDistance * maxPickupDistance;
        float distanceSquared = GetDistanceSquared(playerPosition, itemPosition);

        if (distanceSquared > maxDistanceSquared)
        {
            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                $"Too far from item. MaxDistance={maxPickupDistance:0.00}");
        }

        int itemQuantity = item.GetQuantitySnapshot();

        if (itemQuantity <= 0)
        {
            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                "World item is empty.");
        }

        bool canAddToInventory = player.Inventory.CanFullyAddItem(
            item.ItemTemplateId,
            item.DisplayName,
            itemQuantity,
            item.MaxStack,
            item.WeightPerItem);

        if (!canAddToInventory)
        {
            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                "Inventory is full.");
        }

        if (!item.TryTakeAll(out int takenQuantity))
        {
            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                "Failed to pickup item.");
        }

        InventoryAddResult addResult = player.Inventory.AddItem(
            item.ItemTemplateId,
            item.DisplayName,
            takenQuantity,
            item.MaxStack,
            item.WeightPerItem);

        if (!addResult.IsSuccess || addResult.AddedQuantity != takenQuantity)
        {
            item.RestoreQuantity(takenQuantity);

            return WorldItemPickupResult.Fail(
                pickerSessionId,
                worldObjectId,
                $"Failed to add item to inventory. {addResult.Message}");
        }

        _objectsByWorldObjectId.TryRemove(worldObjectId, out _);

        Console.WriteLine(
            $"[WORLD ITEM PICKED UP] SessionId={player.SessionId}, CharacterId={player.CharacterId}, WorldObjectId={item.WorldObjectId}, TemplateId={item.ItemTemplateId}, Name={item.DisplayName}, Quantity={takenQuantity}");

        return new WorldItemPickupResult
        {
            IsSuccess = true,

            PickerSessionId = player.SessionId,
            PickerCharacterId = player.CharacterId,

            WorldObjectId = item.WorldObjectId,

            ItemTemplateId = item.ItemTemplateId,
            DisplayName = item.DisplayName,

            Quantity = takenQuantity,

            Message = "Item picked up."
        };
    }
    
    public InventoryAddResult? AddItemToPlayer(
        int sessionId,
        string itemTemplateId,
        string displayName,
        int quantity,
        int maxStack,
        float weightPerItem)
    {
        if (!_playersBySessionId.TryGetValue(sessionId, out WorldPlayer? player))
            return null;

        return player.Inventory.AddItem(
            itemTemplateId,
            displayName,
            quantity,
            maxStack,
            weightPerItem);
    }

    public InventorySnapshot? CreateInventorySnapshot(int sessionId)
    {
        if (!_playersBySessionId.TryGetValue(sessionId, out WorldPlayer? player))
            return null;

        return player.CreateInventorySnapshot();
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