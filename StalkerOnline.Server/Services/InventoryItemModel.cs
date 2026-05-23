namespace StalkerOnline.Server.Services;

public sealed class InventoryItemModel
{
    public long InventoryItemId { get; set; }

    public int InventoryId { get; set; }
    public int ItemTemplateId { get; set; }

    public string ItemKey { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;

    public int Quantity { get; set; } = 1;

    public int SlotX { get; set; }
    public int SlotY { get; set; }

    public int Width { get; set; } = 1;
    public int Height { get; set; } = 1;

    public float Durability { get; set; } = 100f;

    public DateTime CreatedAtUtc { get; set; }
    public DateTime UpdatedAtUtc { get; set; }
}