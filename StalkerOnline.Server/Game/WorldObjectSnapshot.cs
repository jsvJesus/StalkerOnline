using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Game;

public sealed class WorldObjectSnapshot
{
    public int WorldObjectId { get; set; }
    public WorldObjectType ObjectType { get; set; }

    public NetVector3 Position { get; set; }
    public NetVector3 Rotation { get; set; }

    public bool IsActive { get; set; }

    public DateTime CreatedAtUtc { get; set; }
    public DateTime UpdatedAtUtc { get; set; }
}