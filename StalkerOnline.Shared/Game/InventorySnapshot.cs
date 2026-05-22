namespace StalkerOnline.Shared.Game;

public sealed class InventorySnapshot
{
    public int CharacterId { get; set; }

    public int Capacity { get; set; }
    public float TotalWeight { get; set; }

    public List<InventoryItem> Items { get; } = new();
}