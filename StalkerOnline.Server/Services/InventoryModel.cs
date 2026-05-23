namespace StalkerOnline.Server.Services;

public sealed class InventoryModel
{
    public int InventoryId { get; set; }
    public int CharacterId { get; set; }

    public string InventoryType { get; set; } = "Main";

    public int Width { get; set; } = 10;
    public int Height { get; set; } = 6;

    public float MaxWeight { get; set; } = 50f;

    public DateTime CreatedAtUtc { get; set; }
    public DateTime UpdatedAtUtc { get; set; }
}