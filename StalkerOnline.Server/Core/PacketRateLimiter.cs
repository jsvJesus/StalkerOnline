using StalkerOnline.Shared.Network;

namespace StalkerOnline.Server.Core;

public sealed class PacketRateLimiter
{
    private sealed class PacketCounter
    {
        public DateTime WindowStartedAtUtc { get; set; }
        public int Count { get; set; }
    }

    private static readonly TimeSpan GlobalWindow = TimeSpan.FromSeconds(3);
    private const int GlobalMaxPackets = 120;

    private readonly object _lock = new();

    private readonly Dictionary<PacketType, PacketRateRule> _rules = new()
    {
        {
            PacketType.RegisterRequest,
            new PacketRateRule(
                PacketType.RegisterRequest,
                maxPackets: 3,
                window: TimeSpan.FromSeconds(30),
                disconnectOnLimit: true)
        },
        {
            PacketType.LoginRequest,
            new PacketRateRule(
                PacketType.LoginRequest,
                maxPackets: 5,
                window: TimeSpan.FromSeconds(10),
                disconnectOnLimit: true)
        },
        {
            PacketType.MoveRequest,
            new PacketRateRule(
                PacketType.MoveRequest,
                maxPackets: 40,
                window: TimeSpan.FromSeconds(1),
                disconnectOnLimit: false)
        },
        {
            PacketType.PickupItemRequest,
            new PacketRateRule(
                PacketType.PickupItemRequest,
                maxPackets: 10,
                window: TimeSpan.FromSeconds(1),
                disconnectOnLimit: false)
        },
        {
            PacketType.DropItemRequest,
            new PacketRateRule(
                PacketType.DropItemRequest,
                maxPackets: 10,
                window: TimeSpan.FromSeconds(1),
                disconnectOnLimit: false)
        },
        {
            PacketType.Ping,
            new PacketRateRule(
                PacketType.Ping,
                maxPackets: 10,
                window: TimeSpan.FromSeconds(10),
                disconnectOnLimit: true)
        },
        {
            PacketType.Pong,
            new PacketRateRule(
                PacketType.Pong,
                maxPackets: 10,
                window: TimeSpan.FromSeconds(10),
                disconnectOnLimit: true)
        },
        {
            PacketType.Disconnect,
            new PacketRateRule(
                PacketType.Disconnect,
                maxPackets: 5,
                window: TimeSpan.FromSeconds(10),
                disconnectOnLimit: true)
        }
    };

    private readonly Dictionary<PacketType, PacketCounter> _packetCounters = new();

    private DateTime _globalWindowStartedAtUtc = DateTime.UtcNow;
    private int _globalPacketCount;
    private int _violationCount;

    public PacketRateLimitResult Check(PacketType packetType)
    {
        lock (_lock)
        {
            DateTime now = DateTime.UtcNow;

            PacketRateLimitResult globalResult = CheckGlobalLimit(now);

            if (!globalResult.IsAllowed)
                return globalResult;

            PacketRateRule rule = GetRule(packetType);
            PacketCounter counter = GetOrCreateCounter(packetType, now);

            if (now - counter.WindowStartedAtUtc > rule.Window)
            {
                counter.WindowStartedAtUtc = now;
                counter.Count = 0;
            }

            counter.Count++;

            if (counter.Count <= rule.MaxPackets)
                return PacketRateLimitResult.Allowed();

            _violationCount++;

            bool shouldDisconnect = rule.DisconnectOnLimit || _violationCount >= 5;

            string reason =
                $"Packet={packetType}, Count={counter.Count}, Limit={rule.MaxPackets}, WindowSeconds={rule.Window.TotalSeconds:0.0}, Violations={_violationCount}";

            return PacketRateLimitResult.Rejected(reason, shouldDisconnect);
        }
    }

    private PacketRateLimitResult CheckGlobalLimit(DateTime now)
    {
        if (now - _globalWindowStartedAtUtc > GlobalWindow)
        {
            _globalWindowStartedAtUtc = now;
            _globalPacketCount = 0;
        }

        _globalPacketCount++;

        if (_globalPacketCount <= GlobalMaxPackets)
            return PacketRateLimitResult.Allowed();

        _violationCount++;

        string reason =
            $"Global packet spam, Count={_globalPacketCount}, Limit={GlobalMaxPackets}, WindowSeconds={GlobalWindow.TotalSeconds:0.0}, Violations={_violationCount}";

        return PacketRateLimitResult.Rejected(reason, shouldDisconnect: true);
    }

    private PacketCounter GetOrCreateCounter(PacketType packetType, DateTime now)
    {
        if (_packetCounters.TryGetValue(packetType, out PacketCounter? counter))
            return counter;

        counter = new PacketCounter
        {
            WindowStartedAtUtc = now,
            Count = 0
        };

        _packetCounters.Add(packetType, counter);

        return counter;
    }

    private PacketRateRule GetRule(PacketType packetType)
    {
        if (_rules.TryGetValue(packetType, out PacketRateRule? rule))
            return rule;

        return new PacketRateRule(
            packetType,
            maxPackets: 30,
            window: TimeSpan.FromSeconds(5),
            disconnectOnLimit: false);
    }
}