using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using StalkerOnline.Server.Game;
using StalkerOnline.Server.Services;
using StalkerOnline.Shared.Game;
using StalkerOnline.Shared.Network;

namespace StalkerOnline.Server.Core;

public sealed class GameServer
{
    private const float PositionBroadcastRadius = 100f;

    private readonly IPAddress _ipAddress;
    private readonly int _port;

    private readonly AccountService _accountService = new();
    private readonly CharacterService _characterService = new();
    private readonly GameWorld _gameWorld = new();

    private readonly ConcurrentDictionary<int, ClientSession> _sessions = new();

    private TcpListener? _listener;
    private int _nextSessionId;

    public GameServer(IPAddress ipAddress, int port)
    {
        _ipAddress = ipAddress;
        _port = port;
    }

    public async Task StartAsync(CancellationToken cancellationToken = default)
    {
        _listener = new TcpListener(_ipAddress, _port);
        _listener.Start();

        Console.WriteLine("===================================");
        Console.WriteLine(" Stalker Online Server started");
        Console.WriteLine($" Address: {_ipAddress}");
        Console.WriteLine($" Port: {_port}");
        Console.WriteLine("===================================");

        while (!cancellationToken.IsCancellationRequested)
        {
            TcpClient tcpClient = await _listener.AcceptTcpClientAsync(cancellationToken);

            int sessionId = Interlocked.Increment(ref _nextSessionId);

            ClientSession session = new(
                sessionId,
                tcpClient,
                _accountService,
                _characterService,
                _gameWorld,
                BroadcastPlayerPositionAsync,
                RemoveSession);

            if (!_sessions.TryAdd(sessionId, session))
            {
                Console.WriteLine($"[SESSION ERROR] Failed to add session: {sessionId}");
                tcpClient.Close();
                continue;
            }

            Console.WriteLine($"[SESSION ADDED] SessionId={sessionId}, Online={_sessions.Count}");

            _ = Task.Run(session.RunAsync, cancellationToken);
        }
    }

    private async Task BroadcastPlayerPositionAsync(
        ClientSession sourceSession,
        PlayerPositionUpdate positionUpdate)
    {
        List<int> nearbySessionIds = _gameWorld.GetNearbyPlayerSessionIds(
            sourceSession.SessionId,
            PositionBroadcastRadius);

        if (nearbySessionIds.Count == 0)
            return;

        PacketWriter writer = new();
        PlayerMovementSerializer.WritePositionUpdate(writer, positionUpdate);

        byte[] payload = writer.ToArray();

        List<Task> sendTasks = new();

        foreach (int targetSessionId in nearbySessionIds)
        {
            if (!_sessions.TryGetValue(targetSessionId, out ClientSession? targetSession))
                continue;

            if (!targetSession.IsAuthorized || targetSession.PlayerConnection == null)
                continue;

            sendTasks.Add(targetSession.SendPacketAsync(PacketType.PlayerPositionBroadcast, payload));
        }

        if (sendTasks.Count > 0)
        {
            await Task.WhenAll(sendTasks);

            Console.WriteLine(
                $"[POSITION BROADCAST] CharacterId={positionUpdate.CharacterId}, Targets={sendTasks.Count}, Position={positionUpdate.Position}");
        }
    }

    private void RemoveSession(ClientSession session)
    {
        if (_sessions.TryRemove(session.SessionId, out _))
        {
            Console.WriteLine($"[SESSION REMOVED] SessionId={session.SessionId}, Online={_sessions.Count}");
        }
    }

    public void Stop()
    {
        Console.WriteLine("[SERVER STOPPING]");

        foreach (ClientSession session in _sessions.Values)
        {
            session.Close();
        }

        _sessions.Clear();
        _gameWorld.Clear();

        _listener?.Stop();

        Console.WriteLine("[SERVER STOPPED]");
    }
}