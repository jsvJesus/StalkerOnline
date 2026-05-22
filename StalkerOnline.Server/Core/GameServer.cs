using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using StalkerOnline.Server.Config;
using StalkerOnline.Server.Database;
using StalkerOnline.Server.Game;
using StalkerOnline.Server.Services;
using StalkerOnline.Shared.Game;
using StalkerOnline.Shared.Network;

namespace StalkerOnline.Server.Core;

public sealed class GameServer
{
    private readonly ServerConfig _serverConfig;

    private readonly IPAddress _ipAddress;
    private readonly int _port;

    private readonly DatabaseConfig _databaseConfig;
    private readonly DatabaseConnectionFactory _databaseConnectionFactory;
    private readonly AccountRepository _accountRepository;
    private readonly AccountService _accountService;

    private readonly CharacterRepository _characterRepository;
    private readonly CharacterService _characterService;

    private readonly GameWorld _gameWorld;
    private readonly InterestManager _interestManager = new();

    private readonly ConcurrentDictionary<int, ClientSession> _sessions = new();

    private TcpListener? _listener;
    private int _nextSessionId;

    public GameServer(ServerConfig serverConfig)
    {
        _serverConfig = serverConfig;

        _ipAddress = _serverConfig.GetIPAddress();
        _port = _serverConfig.Port;

        _databaseConfig = DatabaseConfig.Load("database.json");
        _databaseConnectionFactory = new DatabaseConnectionFactory(_databaseConfig);

        _accountRepository = new AccountRepository(_databaseConnectionFactory);
        _accountService = new AccountService(_accountRepository);

        _characterRepository = new CharacterRepository(_databaseConnectionFactory);
        _characterService = new CharacterService(_characterRepository);

        _gameWorld = new GameWorld(
            _serverConfig.MoveSpeed,
            _serverConfig.DefaultDeltaTime,
            _serverConfig.MaxDeltaTime);
    }

    public async Task StartAsync(CancellationToken cancellationToken = default)
    {
        DatabaseInitializer.Initialize(_databaseConnectionFactory);
        
        SpawnTestWorldItems();

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

        _ = Task.Run(
            () => AutoSaveOnlinePlayersAsync(cancellationToken),
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
                _serverConfig.ItemPickupDistance,
                HandlePlayerJoinedWorldAsync,
                BroadcastPlayerPositionAsync,
                BroadcastWorldItemPickedUpAsync,
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
                await Task.Delay(_serverConfig.ClientPingInterval, cancellationToken);

                DateTime now = DateTime.UtcNow;

                foreach (ClientSession session in _sessions.Values)
                {
                    if (session.IsClosed)
                        continue;

                    TimeSpan idleTime = now - session.LastPacketAtUtc;

                    if (idleTime > _serverConfig.ClientTimeout)
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

    private async Task AutoSaveOnlinePlayersAsync(CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                await Task.Delay(_serverConfig.AutoSaveInterval, cancellationToken);

                SaveOnlinePlayers("AUTO_SAVE");
            }
        }
        catch (OperationCanceledException)
        {
            // normal shutdown
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[AUTO SAVE ERROR] {ex.Message}");
        }
    }

    private void SaveOnlinePlayers(string reason)
    {
        List<PlayerState> snapshots = _gameWorld.CreatePlayerStateSnapshots();

        if (snapshots.Count == 0)
            return;

        int saved = 0;

        foreach (PlayerState snapshot in snapshots)
        {
            try
            {
                _characterService.SavePlayerState(snapshot);
                saved++;
            }
            catch (Exception ex)
            {
                Console.WriteLine(
                    $"[SAVE PLAYER ERROR] Reason={reason}, AccountId={snapshot.AccountId}, CharacterId={snapshot.CharacterId}, Message={ex.Message}");
            }
        }

        Console.WriteLine($"[WORLD SAVE] Reason={reason}, SavedPlayers={saved}/{snapshots.Count}");
    }

    private async Task HandlePlayerJoinedWorldAsync(ClientSession sourceSession)
    {
        WorldPlayer? sourcePlayer = _gameWorld.GetPlayer(sourceSession.SessionId);

        if (sourcePlayer == null)
            return;

        _interestManager.RegisterPlayer(sourceSession.SessionId);

        await UpdatePlayerInterestAsync(sourceSession);
        await UpdateWorldItemInterestAsync(sourceSession);

        Console.WriteLine(
            $"[INTEREST JOIN] SessionId={sourceSession.SessionId}, CharacterId={sourcePlayer.CharacterId}");
    }

    private async Task BroadcastPlayerPositionAsync(
        ClientSession sourceSession,
        PlayerPositionUpdate positionUpdate)
    {
        await UpdatePlayerInterestAsync(sourceSession);
        await UpdateWorldItemInterestAsync(sourceSession);

        List<int> observerSessionIds = _interestManager.GetObserversSeeingPlayer(sourceSession.SessionId);

        if (observerSessionIds.Count == 0)
            return;

        PacketWriter writer = new();
        PlayerMovementSerializer.WritePositionUpdate(writer, positionUpdate);

        byte[] payload = writer.ToArray();

        List<Task> sendTasks = new();

        foreach (int observerSessionId in observerSessionIds)
        {
            if (!_sessions.TryGetValue(observerSessionId, out ClientSession? observerSession))
                continue;

            if (!observerSession.IsAuthorized || observerSession.PlayerConnection == null)
                continue;

            sendTasks.Add(observerSession.SendPacketAsync(PacketType.PlayerPositionBroadcast, payload));
        }

        if (sendTasks.Count > 0)
        {
            await Task.WhenAll(sendTasks);

            Console.WriteLine(
                $"[POSITION BROADCAST] CharacterId={positionUpdate.CharacterId}, Targets={sendTasks.Count}, Position={positionUpdate.Position}");
        }
    }

    private async Task UpdatePlayerInterestAsync(ClientSession sourceSession)
    {
        if (sourceSession.PlayerConnection == null)
            return;

        WorldPlayer? sourcePlayer = _gameWorld.GetPlayer(sourceSession.SessionId);

        if (sourcePlayer == null)
            return;

        List<int> nearbySessionIds = _gameWorld.GetNearbyPlayerSessionIds(
            sourceSession.SessionId,
            _serverConfig.SpawnBroadcastRadius);

        InterestVisibilityChanges sourceChanges = _interestManager.RefreshVisiblePlayers(
            sourceSession.SessionId,
            nearbySessionIds);

        if (sourceChanges.PlayersToSpawn.Count > 0)
            await SendSpawnPacketsToObserverAsync(sourceSession, sourceChanges.PlayersToSpawn);

        if (sourceChanges.PlayersToDespawn.Count > 0)
            await SendDespawnPacketsToObserverAsync(sourceSession, sourceChanges.PlayersToDespawn);

        await UpdateOtherPlayersInterestForSourceAsync(
            sourceSession,
            sourcePlayer,
            nearbySessionIds);
    }

    private async Task UpdateOtherPlayersInterestForSourceAsync(
        ClientSession sourceSession,
        WorldPlayer sourcePlayer,
        List<int> nearbySessionIds)
    {
        HashSet<int> nearbySessionSet = new(nearbySessionIds);

        List<Task> sendTasks = new();

        foreach (int nearbySessionId in nearbySessionSet)
        {
            if (!_sessions.TryGetValue(nearbySessionId, out ClientSession? nearbySession))
                continue;

            if (!nearbySession.IsAuthorized || nearbySession.PlayerConnection == null)
                continue;

            bool added = _interestManager.AddVisiblePlayer(
                nearbySessionId,
                sourceSession.SessionId);

            if (!added)
                continue;

            sendTasks.Add(SendPlayerSpawnToSessionAsync(nearbySession, sourcePlayer));
        }

        List<int> previousObservers = _interestManager.GetObserversSeeingPlayer(sourceSession.SessionId);

        foreach (int observerSessionId in previousObservers)
        {
            if (nearbySessionSet.Contains(observerSessionId))
                continue;

            if (!_sessions.TryGetValue(observerSessionId, out ClientSession? observerSession))
                continue;

            bool removed = _interestManager.RemoveVisiblePlayer(
                observerSessionId,
                sourceSession.SessionId);

            if (!removed)
                continue;

            sendTasks.Add(SendPlayerDespawnToSessionAsync(
                observerSession,
                sourcePlayer.CharacterId));
        }

        if (sendTasks.Count > 0)
            await Task.WhenAll(sendTasks);
    }

    private async Task SendSpawnPacketsToObserverAsync(
        ClientSession observerSession,
        List<int> targetSessionIds)
    {
        List<Task> sendTasks = new();

        foreach (int targetSessionId in targetSessionIds)
        {
            WorldPlayer? targetPlayer = _gameWorld.GetPlayer(targetSessionId);

            if (targetPlayer == null)
                continue;

            sendTasks.Add(SendPlayerSpawnToSessionAsync(observerSession, targetPlayer));
        }

        if (sendTasks.Count > 0)
        {
            await Task.WhenAll(sendTasks);

            Console.WriteLine(
                $"[INTEREST SPAWN LIST] ObserverSessionId={observerSession.SessionId}, Count={sendTasks.Count}");
        }
    }

    private async Task SendDespawnPacketsToObserverAsync(
        ClientSession observerSession,
        List<int> targetSessionIds)
    {
        List<Task> sendTasks = new();

        foreach (int targetSessionId in targetSessionIds)
        {
            WorldPlayer? targetPlayer = _gameWorld.GetPlayer(targetSessionId);

            if (targetPlayer == null)
                continue;

            sendTasks.Add(SendPlayerDespawnToSessionAsync(
                observerSession,
                targetPlayer.CharacterId));
        }

        if (sendTasks.Count > 0)
        {
            await Task.WhenAll(sendTasks);

            Console.WriteLine(
                $"[INTEREST DESPAWN LIST] ObserverSessionId={observerSession.SessionId}, Count={sendTasks.Count}");
        }
    }

    private async Task SendPlayerSpawnToSessionAsync(
        ClientSession observerSession,
        WorldPlayer targetPlayer)
    {
        if (observerSession.SessionId == targetPlayer.SessionId)
            return;

        PlayerSpawnInfo spawnInfo = targetPlayer.CreateSpawnInfo();

        PacketWriter writer = new();
        PlayerWorldSerializer.WriteSpawnInfo(writer, spawnInfo);

        await observerSession.SendPacketAsync(PacketType.PlayerSpawn, writer.ToArray());

        Console.WriteLine(
            $"[PLAYER SPAWN SEND] ObserverSessionId={observerSession.SessionId}, TargetCharacterId={spawnInfo.CharacterId}, Nickname={spawnInfo.Nickname}");
    }

    private async Task SendPlayerDespawnToSessionAsync(
        ClientSession observerSession,
        int targetCharacterId)
    {
        PlayerDespawnInfo despawnInfo = new()
        {
            CharacterId = targetCharacterId
        };

        PacketWriter writer = new();
        PlayerWorldSerializer.WriteDespawnInfo(writer, despawnInfo);

        await observerSession.SendPacketAsync(PacketType.PlayerDespawn, writer.ToArray());

        Console.WriteLine(
            $"[PLAYER DESPAWN SEND] ObserverSessionId={observerSession.SessionId}, TargetCharacterId={targetCharacterId}");
    }
    
    private void SpawnTestWorldItems()
    {
        _gameWorld.SpawnWorldItem(
            "item_water_bottle",
            "Bottle of Water",
            1,
            new NetVector3(2f, 2f, 0f),
            NetVector3.Zero,
            maxStack: 5,
            weightPerItem: 0.50f);

        _gameWorld.SpawnWorldItem(
            "item_medkit_small",
            "Small Medkit",
            1,
            new NetVector3(5f, 1f, 0f),
            NetVector3.Zero,
            maxStack: 5,
            weightPerItem: 0.35f);

        _gameWorld.SpawnWorldItem(
            "item_ak_ammo_545",
            "5.45x39 Ammo",
            30,
            new NetVector3(-3f, 4f, 0f),
            NetVector3.Zero,
            maxStack: 60,
            weightPerItem: 0.01f);

        Console.WriteLine("[TEST LOOT SPAWNED]");
    }
    
    private async Task UpdateWorldItemInterestAsync(ClientSession observerSession)
    {
        if (observerSession.PlayerConnection == null)
            return;

        List<WorldItem> nearbyItems = _gameWorld.GetNearbyWorldItemsForPlayer(
            observerSession.SessionId,
            _serverConfig.ItemVisibilityRadius);

        List<int> nearbyWorldItemIds = new();

        foreach (WorldItem item in nearbyItems)
        {
            nearbyWorldItemIds.Add(item.WorldObjectId);
        }

        InterestWorldItemVisibilityChanges changes = _interestManager.RefreshVisibleWorldItems(
            observerSession.SessionId,
            nearbyWorldItemIds);

        if (changes.ItemsToSpawn.Count > 0)
            await SendWorldItemSpawnPacketsAsync(observerSession, changes.ItemsToSpawn);

        if (changes.ItemsToDespawn.Count > 0)
            await SendWorldItemDespawnPacketsAsync(observerSession, changes.ItemsToDespawn);
    }
    
    private async Task SendWorldItemSpawnPacketsAsync(
        ClientSession observerSession,
        List<int> worldObjectIds)
    {
        List<Task> sendTasks = new();

        foreach (int worldObjectId in worldObjectIds)
        {
            WorldItem? item = _gameWorld.GetWorldItem(worldObjectId);

            if (item == null)
                continue;

            if (!item.IsActive)
                continue;

            WorldItemSpawnInfo spawnInfo = item.CreateSpawnInfo();

            PacketWriter writer = new();
            WorldItemSerializer.WriteSpawnInfo(writer, spawnInfo);

            sendTasks.Add(observerSession.SendPacketAsync(
                PacketType.WorldItemSpawn,
                writer.ToArray()));

            Console.WriteLine(
                $"[WORLD ITEM SPAWN SEND] ObserverSessionId={observerSession.SessionId}, WorldObjectId={spawnInfo.WorldObjectId}, Name={spawnInfo.DisplayName}, Quantity={spawnInfo.Quantity}");
        }

        if (sendTasks.Count > 0)
            await Task.WhenAll(sendTasks);
    }
    
    private async Task SendWorldItemDespawnPacketsAsync(
        ClientSession observerSession,
        List<int> worldObjectIds)
    {
        List<Task> sendTasks = new();

        foreach (int worldObjectId in worldObjectIds)
        {
            WorldItemDespawnInfo despawnInfo = new()
            {
                WorldObjectId = worldObjectId
            };

            PacketWriter writer = new();
            WorldItemSerializer.WriteDespawnInfo(writer, despawnInfo);

            sendTasks.Add(observerSession.SendPacketAsync(
                PacketType.WorldItemDespawn,
                writer.ToArray()));

            Console.WriteLine(
                $"[WORLD ITEM DESPAWN SEND] ObserverSessionId={observerSession.SessionId}, WorldObjectId={worldObjectId}");
        }

        if (sendTasks.Count > 0)
            await Task.WhenAll(sendTasks);
    }
    
    private async Task BroadcastWorldItemPickedUpAsync(
        ClientSession pickerSession,
        WorldItemPickupResult pickupResult)
    {
        List<int> observerSessionIds = _interestManager.RemoveVisibleWorldItemFromAll(
            pickupResult.WorldObjectId);

        if (observerSessionIds.Count == 0)
            return;

        List<Task> sendTasks = new();

        foreach (int observerSessionId in observerSessionIds)
        {
            if (!_sessions.TryGetValue(observerSessionId, out ClientSession? observerSession))
                continue;

            if (!observerSession.IsAuthorized || observerSession.PlayerConnection == null)
                continue;

            sendTasks.Add(SendWorldItemDespawnPacketsAsync(
                observerSession,
                new List<int> { pickupResult.WorldObjectId }));
        }

        if (sendTasks.Count > 0)
        {
            await Task.WhenAll(sendTasks);

            Console.WriteLine(
                $"[WORLD ITEM PICKUP BROADCAST] PickerSessionId={pickerSession.SessionId}, WorldObjectId={pickupResult.WorldObjectId}, Observers={sendTasks.Count}");
        }
    }

    private async Task BroadcastPlayerDespawnAsync(ClientSession sourceSession)
    {
        if (sourceSession.PlayerConnection == null)
            return;

        int sourceSessionId = sourceSession.SessionId;
        int sourceCharacterId = sourceSession.PlayerConnection.State.CharacterId;

        List<int> observerSessionIds = _interestManager.GetObserversSeeingPlayer(sourceSessionId);

        List<Task> sendTasks = new();

        foreach (int observerSessionId in observerSessionIds)
        {
            if (!_sessions.TryGetValue(observerSessionId, out ClientSession? observerSession))
                continue;

            if (!observerSession.IsAuthorized || observerSession.PlayerConnection == null)
                continue;

            sendTasks.Add(SendPlayerDespawnToSessionAsync(
                observerSession,
                sourceCharacterId));
        }

        if (sendTasks.Count > 0)
        {
            await Task.WhenAll(sendTasks);

            Console.WriteLine(
                $"[PLAYER DESPAWN BROADCAST] CharacterId={sourceCharacterId}, Targets={sendTasks.Count}");
        }

        _interestManager.UnregisterPlayer(sourceSessionId);
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

        SaveOnlinePlayers("SERVER_STOP");

        foreach (ClientSession session in _sessions.Values)
        {
            session.Close();
        }

        _sessions.Clear();
        _interestManager.Clear();
        _gameWorld.Clear();

        _listener?.Stop();

        Console.WriteLine("[SERVER STOPPED]");
    }
}