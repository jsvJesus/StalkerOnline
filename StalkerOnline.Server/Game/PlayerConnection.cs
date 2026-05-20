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
}