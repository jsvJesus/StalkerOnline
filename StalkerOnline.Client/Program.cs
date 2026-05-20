using System.Net.Sockets;
using StalkerOnline.Shared.Game;
using StalkerOnline.Shared.Network;

Console.Title = "Stalker Online Client";
Console.OutputEncoding = System.Text.Encoding.UTF8;

const string host = "26.163.92.76";
const int port = 7777;

Console.WriteLine("===================================");
Console.WriteLine(" Stalker Online Client");
Console.WriteLine("===================================");

Console.Write("Login: ");
string login = Console.ReadLine() ?? string.Empty;

Console.Write("Password: ");
string password = Console.ReadLine() ?? string.Empty;

try
{
    using TcpClient client = new();

    Console.WriteLine($"Connecting to {host}:{port}...");

    await client.ConnectAsync(host, port);

    Console.WriteLine("Connected.");

    await using NetworkStream stream = client.GetStream();

    PacketWriter loginWriter = new();
    loginWriter.WriteString(login);
    loginWriter.WriteString(password);

    await PacketProtocol.SendAsync(
        stream,
        PacketType.LoginRequest,
        loginWriter.ToArray());

    Console.WriteLine("LoginRequest sent.");

    PacketMessage? loginResponse = await PacketProtocol.ReceiveAsync(stream);

    if (loginResponse == null)
    {
        Console.WriteLine("Server closed connection.");
        return;
    }

    if (loginResponse.Type != PacketType.LoginResponse)
    {
        Console.WriteLine($"Unexpected packet: {loginResponse.Type}");
        return;
    }

    PacketReader loginReader = new(loginResponse.Payload);

    bool success = loginReader.ReadBool();
    string message = loginReader.ReadString();

    Console.WriteLine("-----------------------------------");
    Console.WriteLine($"Login success: {success}");
    Console.WriteLine($"Message: {message}");
    Console.WriteLine("-----------------------------------");

    if (!success)
        return;

    PacketMessage? playerStatePacket = await PacketProtocol.ReceiveAsync(stream);

    if (playerStatePacket == null)
    {
        Console.WriteLine("Server closed connection before PlayerStateSnapshot.");
        return;
    }

    if (playerStatePacket.Type != PacketType.PlayerStateSnapshot)
    {
        Console.WriteLine($"Unexpected packet: {playerStatePacket.Type}");
        return;
    }

    PacketReader playerStateReader = new(playerStatePacket.Payload);
    PlayerState playerState = PlayerStateSerializer.Read(playerStateReader);

    Console.WriteLine("=========== PLAYER STATE ===========");
    Console.WriteLine($"AccountId:   {playerState.AccountId}");
    Console.WriteLine($"CharacterId: {playerState.CharacterId}");
    Console.WriteLine($"Nickname:    {playerState.Nickname}");
    Console.WriteLine($"Position:    {playerState.Position}");
    Console.WriteLine($"Rotation:    {playerState.Rotation}");
    Console.WriteLine($"Health:      {playerState.Health}/{playerState.MaxHealth}");
    Console.WriteLine($"Stamina:     {playerState.Stamina}/{playerState.MaxStamina}");
    Console.WriteLine($"Hunger:      {playerState.Hunger}");
    Console.WriteLine($"Thirst:      {playerState.Thirst}");
    Console.WriteLine($"Radiation:   {playerState.Radiation}");
    Console.WriteLine($"Toxicity:    {playerState.Toxicity}");
    Console.WriteLine($"IsAlive:     {playerState.IsAlive}");
    Console.WriteLine("====================================");

    Dictionary<int, PlayerSpawnInfo> remotePlayers = new();

    using CancellationTokenSource networkCts = new();

    Task receiveTask = ReceiveLoopAsync(
        stream,
        playerState.CharacterId,
        remotePlayers,
        networkCts.Token);

    float rotationZ = playerState.Rotation.Z;

    Console.WriteLine();
    Console.WriteLine("Movement controls:");
    Console.WriteLine("W/A/S/D - move");
    Console.WriteLine("Q/E     - rotate");
    Console.WriteLine("ESC     - disconnect");
    Console.WriteLine();

    while (!networkCts.IsCancellationRequested)
    {
        ConsoleKeyInfo keyInfo = Console.ReadKey(true);

        if (keyInfo.Key == ConsoleKey.Escape)
        {
            await PacketProtocol.SendAsync(
                stream,
                PacketType.Disconnect,
                Array.Empty<byte>());

            networkCts.Cancel();

            Console.WriteLine("[CLIENT] Disconnect sent.");
            break;
        }

        PlayerMovementInput? input = BuildMovementInput(keyInfo, ref rotationZ);

        if (input == null)
            continue;

        PacketWriter movementWriter = new();
        PlayerMovementSerializer.WriteMoveRequest(movementWriter, input);

        await PacketProtocol.SendAsync(
            stream,
            PacketType.MoveRequest,
            movementWriter.ToArray(),
            networkCts.Token);
    }

    try
    {
        await receiveTask;
    }
    catch
    {
        // ignore on shutdown
    }
}
catch (Exception ex)
{
    Console.WriteLine($"Client error: {ex.Message}");
}

Console.WriteLine("Press ENTER to exit.");
Console.ReadLine();

static PlayerMovementInput? BuildMovementInput(ConsoleKeyInfo keyInfo, ref float rotationZ)
{
    NetVector3 direction = NetVector3.Zero;
    bool shouldSend = true;

    switch (keyInfo.Key)
    {
        case ConsoleKey.W:
            direction = new NetVector3(0f, 1f, 0f);
            break;

        case ConsoleKey.S:
            direction = new NetVector3(0f, -1f, 0f);
            break;

        case ConsoleKey.A:
            direction = new NetVector3(-1f, 0f, 0f);
            break;

        case ConsoleKey.D:
            direction = new NetVector3(1f, 0f, 0f);
            break;

        case ConsoleKey.Q:
            rotationZ -= 15f;
            break;

        case ConsoleKey.E:
            rotationZ += 15f;
            break;

        default:
            shouldSend = false;
            break;
    }

    if (!shouldSend)
        return null;

    return new PlayerMovementInput
    {
        MoveDirection = direction,
        Rotation = new NetVector3(0f, 0f, rotationZ),
        DeltaTime = 0.10f
    };
}

static async Task ReceiveLoopAsync(
    NetworkStream stream,
    int localCharacterId,
    Dictionary<int, PlayerSpawnInfo> remotePlayers,
    CancellationToken cancellationToken)
{
    try
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            PacketMessage? packet = await PacketProtocol.ReceiveAsync(stream, cancellationToken);

            if (packet == null)
            {
                Console.WriteLine("[SERVER] Connection closed.");
                break;
            }

            switch (packet.Type)
            {
                case PacketType.PlayerSpawn:
                {
                    PacketReader reader = new(packet.Payload);
                    PlayerSpawnInfo spawnInfo = PlayerWorldSerializer.ReadSpawnInfo(reader);

                    if (spawnInfo.CharacterId == localCharacterId)
                        break;

                    remotePlayers[spawnInfo.CharacterId] = spawnInfo;

                    Console.WriteLine(
                        $"[PLAYER SPAWN] CharacterId={spawnInfo.CharacterId}, Nickname={spawnInfo.Nickname}, Position={spawnInfo.Position}, RemotePlayers={remotePlayers.Count}");

                    break;
                }

                case PacketType.PlayerDespawn:
                {
                    PacketReader reader = new(packet.Payload);
                    PlayerDespawnInfo despawnInfo = PlayerWorldSerializer.ReadDespawnInfo(reader);

                    bool removed = remotePlayers.Remove(despawnInfo.CharacterId);

                    Console.WriteLine(
                        $"[PLAYER DESPAWN] CharacterId={despawnInfo.CharacterId}, Removed={removed}, RemotePlayers={remotePlayers.Count}");

                    break;
                }

                case PacketType.PlayerPositionUpdate:
                {
                    PacketReader reader = new(packet.Payload);
                    PlayerPositionUpdate update = PlayerMovementSerializer.ReadPositionUpdate(reader);

                    Console.WriteLine(
                        $"[POSITION UPDATE] CharacterId={update.CharacterId}, Position={update.Position}, Rotation={update.Rotation}");

                    break;
                }

                case PacketType.PlayerPositionBroadcast:
                {
                    PacketReader reader = new(packet.Payload);
                    PlayerPositionUpdate update = PlayerMovementSerializer.ReadPositionUpdate(reader);

                    if (remotePlayers.TryGetValue(update.CharacterId, out PlayerSpawnInfo? remotePlayer))
                    {
                        remotePlayer.Position = update.Position;
                        remotePlayer.Rotation = update.Rotation;
                    }

                    Console.WriteLine(
                        $"[POSITION BROADCAST] CharacterId={update.CharacterId}, Position={update.Position}, Rotation={update.Rotation}");

                    break;
                }

                case PacketType.PlayerStateSnapshot:
                {
                    PacketReader reader = new(packet.Payload);
                    PlayerState state = PlayerStateSerializer.Read(reader);

                    Console.WriteLine(
                        $"[STATE SNAPSHOT] CharacterId={state.CharacterId}, Position={state.Position}, Rotation={state.Rotation}");

                    break;
                }

                default:
                    Console.WriteLine($"[SERVER PACKET] Type={packet.Type}, Payload={packet.Payload.Length} bytes");
                    break;
            }
        }
    }
    catch (OperationCanceledException)
    {
        // normal shutdown
    }
    catch (IOException)
    {
        Console.WriteLine("[SERVER] Network stream closed.");
    }
    catch (ObjectDisposedException)
    {
        Console.WriteLine("[SERVER] Network stream disposed.");
    }
    catch (Exception ex)
    {
        Console.WriteLine($"[CLIENT RECEIVE ERROR] {ex.Message}");
    }
}