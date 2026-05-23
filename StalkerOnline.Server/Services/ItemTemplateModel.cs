namespace StalkerOnline.Server.Services;

public sealed class ItemTemplateModel
{
    public int ItemTemplateId { get; set; }

    public string ItemKey { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;

    public string ItemType { get; set; } = "Misc";
    public string Rarity { get; set; } = "Common";

    public int MaxStack { get; set; } = 1;
    public float WeightPerItem { get; set; }

    public float MaxDurability { get; set; } = 100f;

    public float FoodValue { get; set; }
    public float WaterValue { get; set; }
    public float RadiationProtection { get; set; }

    public DateTime CreatedAtUtc { get; set; }
    public DateTime UpdatedAtUtc { get; set; }
}