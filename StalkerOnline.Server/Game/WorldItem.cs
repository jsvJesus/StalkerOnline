using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Game;

public sealed class WorldItem : WorldObject
{
    private readonly object _lock = new();

    public string ItemTemplateId { get; }
    public string DisplayName { get; }

    public int Quantity { get; private set; }
    public int MaxStack { get; }
    public float WeightPerItem { get; }

    public DateTime SpawnedAtUtc { get; }

    public WorldItem(
        int worldObjectId,
        string itemTemplateId,
        string displayName,
        int quantity,
        int maxStack,
        float weightPerItem,
        NetVector3 position,
        NetVector3 rotation)
        : base(
            worldObjectId,
            WorldObjectType.Item,
            position,
            rotation)
    {
        ItemTemplateId = (itemTemplateId ?? string.Empty).Trim();
        DisplayName = (displayName ?? string.Empty).Trim();

        if (string.IsNullOrWhiteSpace(DisplayName))
            DisplayName = ItemTemplateId;

        Quantity = Math.Max(1, quantity);
        MaxStack = Math.Max(1, maxStack);

        if (float.IsNaN(weightPerItem) || float.IsInfinity(weightPerItem) || weightPerItem < 0f)
            WeightPerItem = 0f;
        else
            WeightPerItem = weightPerItem;

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

    public int GetQuantitySnapshot()
    {
        lock (_lock)
        {
            return Quantity;
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

    public void RestoreQuantity(int quantity)
    {
        if (quantity <= 0)
            return;

        lock (_lock)
        {
            Quantity += quantity;
            SetActive(true);
        }
    }
}