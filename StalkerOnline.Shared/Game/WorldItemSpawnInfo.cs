namespace StalkerOnline.Shared.Game;

public sealed class WorldItemSpawnInfo
{
    public int WorldObjectId { get; set; }

    public string ItemTemplateId { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;

    public int Quantity { get; set; }

    public NetVector3 Position { get; set; }
    public NetVector3 Rotation { get; set; }
}

public sealed class WorldItemDespawnInfo
{
    public int WorldObjectId { get; set; }
}