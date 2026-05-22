namespace StalkerOnline.Shared.Game;

public sealed class InventoryItem
{
    public int SlotIndex { get; set; }

    public string ItemTemplateId { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;

    public int Quantity { get; set; }
    public int MaxStack { get; set; } = 1;

    public float WeightPerItem { get; set; }

    public float TotalWeight => Math.Max(0, Quantity) * Math.Max(0f, WeightPerItem);

    public bool IsStackable => MaxStack > 1;

    public bool CanStackWith(string itemTemplateId, int maxStack)
    {
        if (!IsStackable)
            return false;

        if (Quantity >= MaxStack)
            return false;

        return string.Equals(
                   ItemTemplateId,
                   itemTemplateId,
                   StringComparison.OrdinalIgnoreCase) &&
               MaxStack == maxStack;
    }

    public InventoryItem Clone()
    {
        return new InventoryItem
        {
            SlotIndex = SlotIndex,
            ItemTemplateId = ItemTemplateId,
            DisplayName = DisplayName,
            Quantity = Quantity,
            MaxStack = MaxStack,
            WeightPerItem = WeightPerItem
        };
    }
}