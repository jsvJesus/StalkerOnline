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
    private const float SpawnBroadcastRadius = 100f;

    private static readonly TimeSpan ClientPingInterval = TimeSpan.FromSeconds(10);
    private static readonly TimeSpan ClientTimeout = TimeSpan.FromSeconds(30);

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

        _ = Task.Run(
            () => MonitorSessionsAsync(cancellationToken),
            cancellationToken);

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
                HandlePlayerJoinedWorldAsync,
                BroadcastPlayerPositionAsync,
                BroadcastPlayerDespawnAsync,
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

    private async Task MonitorSessionsAsync(CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                await Task.Delay(ClientPingInterval, cancellationToken);

                DateTime now = DateTime.UtcNow;

                foreach (ClientSession session in _sessions.Values)
                {
                    if (session.IsClosed)
                        continue;

                    TimeSpan idleTime = now - session.LastPacketAtUtc;

                    if (idleTime > ClientTimeout)
                    {
                        Console.WriteLine(
                            $"[CLIENT TIMEOUT] SessionId={session.SessionId}, IdleSeconds={idleTime.TotalSeconds:0.0}");

                        session.Close();
                        continue;
                    }

                    await session.SendPingAsync();

                    Console.WriteLine($"[PING] SessionId={session.SessionId}");
                }
            }
        }
        catch (OperationCanceledException)
        {
            // normal shutdown
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[SESSION MONITOR ERROR] {ex.Message}");
        }
    }

    private async Task HandlePlayerJoinedWorldAsync(ClientSession sourceSession)
    {
        WorldPlayer? sourcePlayer = _gameWorld.GetPlayer(sourceSession.SessionId);

        if (sourcePlayer == null)
            return;

        List<WorldPlayer> nearbyPlayers = _gameWorld.GetNearbyPlayers(
            sourceSession.SessionId,
            SpawnBroadcastRadius);

        await SendExistingPlayersToNewPlayerAsync(sourceSession, nearbyPlayers);
        await BroadcastPlayerSpawnAsync(sourceSession, sourcePlayer, nearbyPlayers);
    }

    private static async Task SendExistingPlayersToNewPlayerAsync(
        ClientSession newSession,
        List<WorldPlayer> existingPlayers)
    {
        int sent = 0;

        foreach (WorldPlayer existingPlayer in existingPlayers)
        {
            PlayerSpawnInfo spawnInfo = existingPlayer.CreateSpawnInfo();

            PacketWriter writer = new();
            PlayerWorldSerializer.WriteSpawnInfo(writer, spawnInfo);

            await newSession.SendPacketAsync(PacketType.PlayerSpawn, writer.ToArray());

            sent++;
        }

        if (sent > 0)
        {
            Console.WriteLine(
                $"[SPAWN LIST SENT] SessionId={newSession.SessionId}, Players={sent}");
        }
    }

    private async Task BroadcastPlayerSpawnAsync(
        ClientSession sourceSession,
        WorldPlayer sourcePlayer,
        List<WorldPlayer> nearbyPlayers)
    {
        if (nearbyPlayers.Count == 0)
            return;

        PlayerSpawnInfo spawnInfo = sourcePlayer.CreateSpawnInfo();

        PacketWriter writer = new();
        PlayerWorldSerializer.WriteSpawnInfo(writer, spawnInfo);

        byte[] payload = writer.ToArray();

        List<Task> sendTasks = new();

        foreach (WorldPlayer nearbyPlayer in nearbyPlayers)
        {
            if (!_sessions.TryGetValue(nearbyPlayer.SessionId, out ClientSession? targetSession))
                continue;

            if (!targetSession.IsAuthorized || targetSession.PlayerConnection == null)
                continue;

            sendTasks.Add(targetSession.SendPacketAsync(PacketType.PlayerSpawn, payload));
        }

        if (sendTasks.Count > 0)
        {
            await Task.WhenAll(sendTasks);

            Console.WriteLine(
                $"[PLAYER SPAWN BROADCAST] CharacterId={spawnInfo.CharacterId}, Nickname={spawnInfo.Nickname}, Targets={sendTasks.Count}");
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

    private async Task BroadcastPlayerDespawnAsync(ClientSession sourceSession)
    {
        if (sourceSession.PlayerConnection == null)
            return;

        List<int> nearbySessionIds = _gameWorld.GetNearbyPlayerSessionIds(
            sourceSession.SessionId,
            SpawnBroadcastRadius);

        if (nearbySessionIds.Count == 0)
            return;

        PlayerDespawnInfo despawnInfo = new()
        {
            CharacterId = sourceSession.PlayerConnection.State.CharacterId
        };

        PacketWriter writer = new();
        PlayerWorldSerializer.WriteDespawnInfo(writer, despawnInfo);

        byte[] payload = writer.ToArray();

        List<Task> sendTasks = new();

        foreach (int targetSessionId in nearbySessionIds)
        {
            if (!_sessions.TryGetValue(targetSessionId, out ClientSession? targetSession))
                continue;

            if (!targetSession.IsAuthorized || targetSession.PlayerConnection == null)
                continue;

            sendTasks.Add(targetSession.SendPacketAsync(PacketType.PlayerDespawn, payload));
        }

        if (sendTasks.Count > 0)
        {
            await Task.WhenAll(sendTasks);

            Console.WriteLine(
                $"[PLAYER DESPAWN BROADCAST] CharacterId={despawnInfo.CharacterId}, Targets={sendTasks.Count}");
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