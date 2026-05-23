using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Services;

public sealed class InventoryService
{
    private const string MainInventoryType = "Main";
    private const int MainInventoryWidth = 10;
    private const int MainInventoryHeight = 3;
    private const float MainInventoryMaxWeight = 50f;

    private readonly InventoryRepository _inventoryRepository;
    private readonly ItemRepository _itemRepository;

    public InventoryService(
        InventoryRepository inventoryRepository,
        ItemRepository itemRepository)
    {
        _inventoryRepository = inventoryRepository;
        _itemRepository = itemRepository;
    }

    public PlayerInventory LoadOrCreatePlayerInventory(int characterId)
    {
        InventoryModel inventoryModel = _inventoryRepository.GetOrCreateInventory(
            characterId,
            MainInventoryType,
            MainInventoryWidth,
            MainInventoryHeight,
            MainInventoryMaxWeight);

        int capacity = Math.Max(1, inventoryModel.Width * inventoryModel.Height);

        PlayerInventory inventory = new(capacity);

        List<InventoryItemModel> savedItems = _inventoryRepository.LoadItems(inventoryModel.InventoryId);

        foreach (InventoryItemModel savedItem in savedItems)
        {
            int slotIndex = savedItem.SlotY * inventoryModel.Width + savedItem.SlotX;

            bool loaded = inventory.TryAddItemToSlot(
                slotIndex,
                savedItem.ItemKey,
                savedItem.DisplayName,
                savedItem.Quantity,
                savedItem.MaxStack,
                savedItem.WeightPerItem);

            if (!loaded)
            {
                Console.WriteLine(
                    $"[INVENTORY LOAD ITEM SKIPPED] CharacterId={characterId}, Item={savedItem.ItemKey}, Slot={slotIndex}");
            }
        }

        if (savedItems.Count == 0)
        {
            AddStarterItems(inventory);
            SaveInventorySnapshot(inventory.CreateSnapshot(characterId));

            Console.WriteLine($"[STARTER INVENTORY CREATED] CharacterId={characterId}");
        }

        Console.WriteLine(
            $"[INVENTORY LOADED] CharacterId={characterId}, Items={inventory.Count}, Capacity={capacity}");

        return inventory;
    }

    public void SaveInventorySnapshot(InventorySnapshot snapshot)
    {
        InventoryModel inventoryModel = _inventoryRepository.GetOrCreateInventory(
            snapshot.CharacterId,
            MainInventoryType,
            MainInventoryWidth,
            MainInventoryHeight,
            MainInventoryMaxWeight);

        _inventoryRepository.ClearInventory(inventoryModel.InventoryId);

        foreach (InventoryItem item in snapshot.Items)
        {
            if (item.Quantity <= 0)
                continue;

            if (item.SlotIndex < 0)
                continue;

            int slotX = item.SlotIndex % inventoryModel.Width;
            int slotY = item.SlotIndex / inventoryModel.Width;

            if (slotY >= inventoryModel.Height)
            {
                Console.WriteLine(
                    $"[INVENTORY SAVE ITEM SKIPPED] CharacterId={snapshot.CharacterId}, Item={item.ItemTemplateId}, Slot={item.SlotIndex}");
                continue;
            }

            ItemTemplateModel? template = EnsureItemTemplate(item);

            if (template == null)
                continue;

            _inventoryRepository.AddItem(
                inventoryModel.InventoryId,
                template.ItemTemplateId,
                item.Quantity,
                slotX,
                slotY,
                width: 1,
                height: 1,
                durability: template.MaxDurability);
        }

        Console.WriteLine(
            $"[INVENTORY SAVED] CharacterId={snapshot.CharacterId}, Items={snapshot.Items.Count}, Weight={snapshot.TotalWeight:0.00}");
    }

    private void AddStarterItems(PlayerInventory inventory)
    {
        AddStarterItem(inventory, "item_water_bottle", 2);
        AddStarterItem(inventory, "item_medkit_small", 1);
        AddStarterItem(inventory, "item_ak_ammo_545", 30);
    }

    private void AddStarterItem(PlayerInventory inventory, string itemKey, int quantity)
    {
        ItemTemplateModel? template = _itemRepository.FindByKey(itemKey);

        if (template == null)
        {
            Console.WriteLine($"[STARTER ITEM MISSING] ItemKey={itemKey}");
            return;
        }

        InventoryAddResult result = inventory.AddItem(
            template.ItemKey,
            template.DisplayName,
            quantity,
            template.MaxStack,
            template.WeightPerItem);

        if (!result.IsSuccess)
        {
            Console.WriteLine(
                $"[STARTER ITEM FAILED] ItemKey={itemKey}, Quantity={quantity}, Reason={result.Message}");
        }
    }

    private ItemTemplateModel? EnsureItemTemplate(InventoryItem item)
    {
        string itemKey = (item.ItemTemplateId ?? string.Empty).Trim();

        if (string.IsNullOrWhiteSpace(itemKey))
            return null;

        ItemTemplateModel? existing = _itemRepository.FindByKey(itemKey);

        if (existing != null)
            return existing;

        string displayName = string.IsNullOrWhiteSpace(item.DisplayName)
            ? itemKey
            : item.DisplayName;

        return _itemRepository.CreateOrUpdate(new ItemTemplateModel
        {
            ItemKey = itemKey,
            DisplayName = displayName,
            ItemType = "Misc",
            Rarity = "Common",
            MaxStack = Math.Max(1, item.MaxStack),
            WeightPerItem = Math.Max(0f, item.WeightPerItem),
            MaxDurability = 100f,
            FoodValue = 0f,
            WaterValue = 0f,
            RadiationProtection = 0f
        });
    }
}