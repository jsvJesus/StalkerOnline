using StalkerOnline.Shared.Network;

namespace StalkerOnline.Server.Core;

public sealed class PacketRateLimitResult
{
    public bool IsAllowed { get; }
    public bool ShouldDisconnect { get; }
    public string Reason { get; }

    private PacketRateLimitResult(
        bool isAllowed,
        bool shouldDisconnect,
        string reason)
    {
        IsAllowed = isAllowed;
        ShouldDisconnect = shouldDisconnect;
        Reason = reason;
    }

    public static PacketRateLimitResult Allowed()
    {
        return new PacketRateLimitResult(true, false, string.Empty);
    }

    public static PacketRateLimitResult Rejected(string reason, bool shouldDisconnect)
    {
        return new PacketRateLimitResult(false, shouldDisconnect, reason);
    }
}

public sealed class PacketRateRule
{
    public PacketType PacketType { get; }
    public int MaxPackets { get; }
    public TimeSpan Window { get; }
    public bool DisconnectOnLimit { get; }

    public PacketRateRule(
        PacketType packetType,
        int maxPackets,
        TimeSpan window,
        bool disconnectOnLimit)
    {
        PacketType = packetType;
        MaxPackets = maxPackets;
        Window = window;
        DisconnectOnLimit = disconnectOnLimit;
    }
}