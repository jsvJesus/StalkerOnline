namespace StalkerOnline.Shared.Game;

public sealed class PlayerSpawnInfo
{
    public int CharacterId { get; set; }

    public string Nickname { get; set; } = string.Empty;

    public NetVector3 Position { get; set; }
    public NetVector3 Rotation { get; set; }

    public float Health { get; set; }
    public float MaxHealth { get; set; }

    public bool IsAlive { get; set; }
}

public sealed class PlayerDespawnInfo
{
    public int CharacterId { get; set; }
}