namespace StalkerOnline.Shared.Game;

public sealed class PlayerState
{
    public int AccountId { get; set; }
    public int CharacterId { get; set; }

    public string Nickname { get; set; } = string.Empty;

    public NetVector3 Position { get; set; }
    public NetVector3 Rotation { get; set; }

    public float Health { get; set; }
    public float MaxHealth { get; set; }

    public float Stamina { get; set; }
    public float MaxStamina { get; set; }

    public float Hunger { get; set; }
    public float Thirst { get; set; }

    public float Radiation { get; set; }
    public float Toxicity { get; set; }

    public bool IsAlive { get; set; }

    public static PlayerState CreateDefault(int accountId, string nickname)
    {
        return new PlayerState
        {
            AccountId = accountId,
            CharacterId = accountId,

            Nickname = nickname,

            Position = new NetVector3(0f, 0f, 0f),
            Rotation = new NetVector3(0f, 0f, 0f),

            Health = 100f,
            MaxHealth = 100f,

            Stamina = 100f,
            MaxStamina = 100f,

            Hunger = 0f,
            Thirst = 0f,

            Radiation = 0f,
            Toxicity = 0f,

            IsAlive = true
        };
    }
}