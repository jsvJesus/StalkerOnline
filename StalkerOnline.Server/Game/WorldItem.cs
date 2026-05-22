using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Game;

public sealed class WorldItem : WorldObject
{
    private readonly object _lock = new();

    public string ItemTemplateId { get; }
    public string DisplayName { get; }

    public int Quantity { get; private set; }

    public DateTime SpawnedAtUtc { get; }

    public WorldItem(
        int worldObjectId,
        string itemTemplateId,
        string displayName,
        int quantity,
        NetVector3 position,
        NetVector3 rotation)
        : base(
            worldObjectId,
            WorldObjectType.Item,
            position,
            rotation)
    {
        ItemTemplateId = itemTemplateId.Trim();
        DisplayName = displayName.Trim();

        Quantity = Math.Max(1, quantity);

        SpawnedAtUtc = DateTime.UtcNow;
    }

    public WorldItemSpawnInfo CreateSpawnInfo()
    {
        lock (_lock)
        {
            return new WorldItemSpawnInfo
            {
                WorldObjectId = WorldObjectId,

                ItemTemplateId = ItemTemplateId,
                DisplayName = DisplayName,

                Quantity = Quantity,

                Position = GetPosition(),
                Rotation = GetRotation()
            };
        }
    }

    public bool TryTakeAll(out int takenQuantity)
    {
        lock (_lock)
        {
            takenQuantity = 0;

            if (!IsActive)
                return false;

            if (Quantity <= 0)
                return false;

            takenQuantity = Quantity;
            Quantity = 0;

            SetActive(false);

            return true;
        }
    }
}