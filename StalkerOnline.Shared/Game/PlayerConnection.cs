using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Game;

public sealed class PlayerConnection
{
    public int SessionId { get; }
    public int AccountId { get; }
    public string Login { get; }

    public PlayerState State { get; }

    public DateTime ConnectedAtUtc { get; }
    public DateTime LastPacketAtUtc { get; private set; }

    public PlayerConnection(
        int sessionId,
        int accountId,
        string login,
        PlayerState state)
    {
        SessionId = sessionId;
        AccountId = accountId;
        Login = login;
        State = state;

        ConnectedAtUtc = DateTime.UtcNow;
        LastPacketAtUtc = ConnectedAtUtc;
    }

    public void Touch()
    {
        LastPacketAtUtc = DateTime.UtcNow;
    }

    public void SetPosition(float x, float y, float z)
    {
        State.Position = new NetVector3(x, y, z);
        Touch();
    }

    public void SetRotation(float x, float y, float z)
    {
        State.Rotation = new NetVector3(x, y, z);
        Touch();
    }

    public void Damage(float amount)
    {
        if (!State.IsAlive)
            return;

        if (amount <= 0f)
            return;

        State.Health -= amount;

        if (State.Health <= 0f)
        {
            State.Health = 0f;
            State.IsAlive = false;
        }

        Touch();
    }

    public void Heal(float amount)
    {
        if (!State.IsAlive)
            return;

        if (amount <= 0f)
            return;

        State.Health += amount;

        if (State.Health > State.MaxHealth)
            State.Health = State.MaxHealth;

        Touch();
    }
}