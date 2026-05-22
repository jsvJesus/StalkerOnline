namespace StalkerOnline.Shared.Game;

public sealed class PlayerInventory
{
    private readonly object _lock = new();
    private readonly List<InventoryItem> _items = new();

    public int Capacity { get; }

    public PlayerInventory(int capacity)
    {
        if (capacity <= 0)
            throw new ArgumentOutOfRangeException(nameof(capacity), "Inventory capacity must be greater than zero.");

        Capacity = capacity;
    }

    public int Count
    {
        get
        {
            lock (_lock)
            {
                return _items.Count;
            }
        }
    }

    public bool IsFull
    {
        get
        {
            lock (_lock)
            {
                return _items.Count >= Capacity;
            }
        }
    }

    public InventoryAddResult AddItem(
        string itemTemplateId,
        string displayName,
        int quantity,
        int maxStack,
        float weightPerItem)
    {
        itemTemplateId = (itemTemplateId ?? string.Empty).Trim();
        displayName = (displayName ?? string.Empty).Trim();

        if (string.IsNullOrWhiteSpace(itemTemplateId))
        {
            return new InventoryAddResult
            {
                IsSuccess = false,
                ItemTemplateId = itemTemplateId,
                RequestedQuantity = quantity,
                AddedQuantity = 0,
                RemainingQuantity = quantity,
                Message = "ItemTemplateId is empty."
            };
        }

        if (string.IsNullOrWhiteSpace(displayName))
            displayName = itemTemplateId;

        if (quantity <= 0)
        {
            return new InventoryAddResult
            {
                IsSuccess = false,
                ItemTemplateId = itemTemplateId,
                RequestedQuantity = quantity,
                AddedQuantity = 0,
                RemainingQuantity = quantity,
                Message = "Quantity must be greater than zero."
            };
        }

        maxStack = Math.Max(1, maxStack);

        if (float.IsNaN(weightPerItem) || float.IsInfinity(weightPerItem) || weightPerItem < 0f)
            weightPerItem = 0f;

        int requested = quantity;
        int remaining = quantity;
        int added = 0;

        lock (_lock)
        {
            foreach (InventoryItem item in _items.OrderBy(x => x.SlotIndex))
            {
                if (remaining <= 0)
                    break;

                if (!item.CanStackWith(itemTemplateId, maxStack))
                    continue;

                int freeInStack = item.MaxStack - item.Quantity;

                if (freeInStack <= 0)
                    continue;

                int addToStack = Math.Min(freeInStack, remaining);

                item.Quantity += addToStack;
                remaining -= addToStack;
                added += addToStack;
            }

            while (remaining > 0)
            {
                int freeSlot = FindFreeSlotUnsafe();

                if (freeSlot < 0)
                    break;

                int stackQuantity = Math.Min(maxStack, remaining);

                InventoryItem newItem = new()
                {
                    SlotIndex = freeSlot,
                    ItemTemplateId = itemTemplateId,
                    DisplayName = displayName,
                    Quantity = stackQuantity,
                    MaxStack = maxStack,
                    WeightPerItem = weightPerItem
                };

                _items.Add(newItem);

                remaining -= stackQuantity;
                added += stackQuantity;
            }
        }

        bool success = remaining == 0;

        return new InventoryAddResult
        {
            IsSuccess = success,
            ItemTemplateId = itemTemplateId,
            RequestedQuantity = requested,
            AddedQuantity = added,
            RemainingQuantity = remaining,
            Message = success
                ? "Item added."
                : added > 0
                    ? "Item partially added. Inventory is full."
                    : "Item was not added. Inventory is full."
        };
    }
    
    public bool CanFullyAddItem(
        string itemTemplateId,
        string displayName,
        int quantity,
        int maxStack,
        float weightPerItem)
    {
        itemTemplateId = (itemTemplateId ?? string.Empty).Trim();

        if (string.IsNullOrWhiteSpace(itemTemplateId))
            return false;

        if (quantity <= 0)
            return false;

        maxStack = Math.Max(1, maxStack);

        int remaining = quantity;

        lock (_lock)
        {
            foreach (InventoryItem item in _items.OrderBy(x => x.SlotIndex))
            {
                if (remaining <= 0)
                    break;

                if (!item.CanStackWith(itemTemplateId, maxStack))
                    continue;

                int freeInStack = item.MaxStack - item.Quantity;

                if (freeInStack <= 0)
                    continue;

                int addToStack = Math.Min(freeInStack, remaining);
                remaining -= addToStack;
            }

            if (remaining <= 0)
                return true;

            int usedSlots = _items.Count;
            int freeSlots = Capacity - usedSlots;

            if (freeSlots <= 0)
                return false;

            int requiredNewSlots = (remaining + maxStack - 1) / maxStack;

            return requiredNewSlots <= freeSlots;
        }
    }

    public InventorySnapshot CreateSnapshot(int characterId)
    {
        lock (_lock)
        {
            InventorySnapshot snapshot = new()
            {
                CharacterId = characterId,
                Capacity = Capacity,
                TotalWeight = _items.Sum(x => x.TotalWeight)
            };

            foreach (InventoryItem item in _items.OrderBy(x => x.SlotIndex))
            {
                snapshot.Items.Add(item.Clone());
            }

            return snapshot;
        }
    }

    public List<InventoryItem> GetItemsSnapshot()
    {
        lock (_lock)
        {
            return _items
                .OrderBy(x => x.SlotIndex)
                .Select(x => x.Clone())
                .ToList();
        }
    }

    private int FindFreeSlotUnsafe()
    {
        for (int slot = 0; slot < Capacity; slot++)
        {
            bool used = false;

            foreach (InventoryItem item in _items)
            {
                if (item.SlotIndex == slot)
                {
                    used = true;
                    break;
                }
            }

            if (!used)
                return slot;
        }

        return -1;
    }
}