using System.Net.Sockets;
using StalkerOnline.Server.Game;
using StalkerOnline.Server.Services;
using StalkerOnline.Shared.Game;
using StalkerOnline.Shared.Network;

namespace StalkerOnline.Server.Core;

public sealed class ClientSession
{
    private static readonly TimeSpan RateLimitErrorMessageCooldown = TimeSpan.FromSeconds(3);

    private readonly TcpClient _client;
    private readonly AccountService _accountService;
    private readonly CharacterService _characterService;
    private readonly GameWorld _gameWorld;
    private readonly float _itemPickupDistance;
    private readonly Func<ClientSession, WorldItemPickupResult, Task> _onWorldItemPickedUp;
    private readonly Func<ClientSession, Task> _onPlayerJoinedWorld;
    private readonly Func<ClientSession, PlayerPositionUpdate, Task> _onPlayerPositionChanged;
    private readonly Func<ClientSession, Task> _onPlayerLeavingWorld;
    private readonly Action<ClientSession> _onDisconnected;

    private readonly SemaphoreSlim _sendLock = new(1, 1);
    private readonly PacketRateLimiter _rateLimiter = new();

    private NetworkStream? _stream;
    private bool _removedFromWorld;
    private int _closed;

    private DateTime _lastRateLimitErrorSentAtUtc = DateTime.MinValue;

    public int SessionId { get; }
    public string RemoteAddress { get; }

    public DateTime ConnectedAtUtc { get; }
    public DateTime LastPacketAtUtc { get; private set; }

    public bool IsClosed => Volatile.Read(ref _closed) == 1;

    public bool IsAuthorized { get; private set; }
    public int AccountId { get; private set; }
    public string Login { get; private set; } = string.Empty;

    public PlayerConnection? PlayerConnection { get; private set; }

    public ClientSession(
        int sessionId,
        TcpClient client,
        AccountService accountService,
        CharacterService characterService,
        GameWorld gameWorld,
        float itemPickupDistance,
        Func<ClientSession, Task> onPlayerJoinedWorld,
        Func<ClientSession, PlayerPositionUpdate, Task> onPlayerPositionChanged,
        Func<ClientSession, WorldItemPickupResult, Task> onWorldItemPickedUp,
        Func<ClientSession, Task> onPlayerLeavingWorld,
        Action<ClientSession> onDisconnected)
    {
        SessionId = sessionId;
        _client = client;
        _accountService = accountService;
        _characterService = characterService;
        _gameWorld = gameWorld;
        _itemPickupDistance = itemPickupDistance;
        _onPlayerJoinedWorld = onPlayerJoinedWorld;
        _onPlayerPositionChanged = onPlayerPositionChanged;
        _onWorldItemPickedUp = onWorldItemPickedUp;
        _onPlayerLeavingWorld = onPlayerLeavingWorld;
        _onDisconnected = onDisconnected;

        ConnectedAtUtc = DateTime.UtcNow;
        LastPacketAtUtc = ConnectedAtUtc;

        RemoteAddress = _client.Client.RemoteEndPoint?.ToString() ?? "unknown";
    }

    public async Task RunAsync()
    {
        Console.WriteLine($"[CONNECT] SessionId={SessionId}, IP={RemoteAddress}");

        try
        {
            _stream = _client.GetStream();

            while (_client.Connected && !IsClosed)
            {
                PacketMessage? packet = await PacketProtocol.ReceiveAsync(_stream);

                if (packet == null)
                {
                    Console.WriteLine($"[DISCONNECT] SessionId={SessionId}, IP={RemoteAddress}");
                    break;
                }

                PacketRateLimitResult rateLimitResult = _rateLimiter.Check(packet.Type);

                if (!rateLimitResult.IsAllowed)
                {
                    Console.WriteLine(
                        $"[RATE LIMIT] SessionId={SessionId}, IP={RemoteAddress}, Reason={rateLimitResult.Reason}");

                    if (rateLimitResult.ShouldDisconnect)
                    {
                        await SendErrorMessageAsync(
                            "RATE_LIMIT_DISCONNECT",
                            "Too many packets. Connection closed.",
                            shouldDisconnect: true);

                        Console.WriteLine(
                            $"[RATE LIMIT DISCONNECT] SessionId={SessionId}, IP={RemoteAddress}");

                        Close();
                        break;
                    }

                    await SendRateLimitErrorIfAllowedAsync(rateLimitResult.Reason);
                    continue;
                }

                Touch();

                await HandlePacketAsync(packet);
            }
        }
        catch (IOException ex)
        {
            Console.WriteLine($"[NETWORK ERROR] SessionId={SessionId}, Message={ex.Message}");
        }
        catch (SocketException ex)
        {
            Console.WriteLine($"[SOCKET ERROR] SessionId={SessionId}, Message={ex.Message}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[SESSION ERROR] SessionId={SessionId}, Message={ex.Message}");
        }
        finally
        {
            await RemoveFromWorldIfNeededAsync();
            Close();
            _onDisconnected(this);
        }
    }

    private async Task HandlePacketAsync(PacketMessage packet)
    {
        switch (packet.Type)
        {
            case PacketType.Ping:
                await SendPacketAsync(PacketType.Pong, Array.Empty<byte>());
                break;

            case PacketType.Pong:
                Console.WriteLine($"[PONG] SessionId={SessionId}");
                break;
            
            case PacketType.RegisterRequest:
                await HandleRegisterRequestAsync(packet);
                break;

            case PacketType.LoginRequest:
                await HandleLoginRequestAsync(packet);
                break;

            case PacketType.MoveRequest:
                await HandleMoveRequestAsync(packet);
                break;
            
            case PacketType.PickupItemRequest:
                await HandlePickupItemRequestAsync(packet);
                break;

            case PacketType.Disconnect:
                Console.WriteLine($"[CLIENT DISCONNECT REQUEST] SessionId={SessionId}");
                await RemoveFromWorldIfNeededAsync();
                Close();
                break;

            default:
                Console.WriteLine($"[UNKNOWN PACKET] SessionId={SessionId}, Type={packet.Type}");

                await SendErrorMessageAsync(
                    "UNKNOWN_PACKET",
                    $"Unknown packet type: {packet.Type}",
                    shouldDisconnect: false);

                break;
        }
    }

    private async Task HandleLoginRequestAsync(PacketMessage packet)
    {
        if (IsAuthorized)
        {
            Console.WriteLine($"[LOGIN IGNORED] SessionId={SessionId}, Reason=Already authorized");

            await SendErrorMessageAsync(
                "ALREADY_AUTHORIZED",
                "You are already authorized.",
                shouldDisconnect: false);

            return;
        }

        PacketReader reader = new(packet.Payload);

        string login = reader.ReadString();
        string password = reader.ReadString();

        Console.WriteLine($"[LOGIN REQUEST] SessionId={SessionId}, Login={login}");

        LoginResult result = _accountService.ValidateLogin(login, password);

        PacketWriter loginResponseWriter = new();
        loginResponseWriter.WriteBool(result.IsSuccess);
        loginResponseWriter.WriteString(result.Message);

        await SendPacketAsync(PacketType.LoginResponse, loginResponseWriter.ToArray());

        if (!result.IsSuccess)
        {
            Console.WriteLine($"[LOGIN FAILED] SessionId={SessionId}, Login={login}, Reason={result.Message}");

            await SendErrorMessageAsync(
                "LOGIN_FAILED",
                result.Message,
                shouldDisconnect: false);

            return;
        }

        IsAuthorized = true;
        AccountId = result.AccountId;
        Login = result.Login;

        PlayerState playerState = _characterService.LoadOrCreatePlayerState(AccountId, Login);

        PlayerConnection = new PlayerConnection(
            SessionId,
            AccountId,
            Login,
            playerState);

        _gameWorld.AddPlayer(PlayerConnection);

        Console.WriteLine($"[LOGIN SUCCESS] SessionId={SessionId}, AccountId={AccountId}, Login={Login}");
        Console.WriteLine($"[PLAYER CREATED] SessionId={SessionId}, CharacterId={playerState.CharacterId}, Nickname={playerState.Nickname}");

        await SendServerMessageAsync($"Welcome to Stalker Online, {Login}.");

        await SendPlayerStateSnapshotAsync();
        await _onPlayerJoinedWorld(this);
    }
    
    private async Task HandleRegisterRequestAsync(PacketMessage packet)
    {
        if (IsAuthorized)
        {
            Console.WriteLine($"[REGISTER IGNORED] SessionId={SessionId}, Reason=Already authorized");

            await SendErrorMessageAsync(
                "ALREADY_AUTHORIZED",
                "You are already authorized.",
                shouldDisconnect: false);

            return;
        }

        PacketReader reader = new(packet.Payload);
        RegisterRequest request = NetworkMessageSerializer.ReadRegisterRequest(reader);

        Console.WriteLine(
            $"[REGISTER REQUEST] SessionId={SessionId}, Login={request.Login}, Email={request.Email}");

        RegisterResult result = _accountService.Register(
            request.Login,
            request.Email,
            request.Password);

        RegisterResponse response = new()
        {
            IsSuccess = result.IsSuccess,
            AccountId = result.AccountId,
            Login = result.Login,
            Message = result.Message
        };

        PacketWriter writer = new();
        NetworkMessageSerializer.WriteRegisterResponse(writer, response);

        await SendPacketAsync(PacketType.RegisterResponse, writer.ToArray());

        if (!result.IsSuccess)
        {
            Console.WriteLine(
                $"[REGISTER FAILED] SessionId={SessionId}, Login={request.Login}, Reason={result.Message}");

            return;
        }

        Console.WriteLine(
            $"[REGISTER SUCCESS] SessionId={SessionId}, AccountId={result.AccountId}, Login={result.Login}");
    }

    private async Task HandleMoveRequestAsync(PacketMessage packet)
    {
        if (!IsAuthorized || PlayerConnection == null)
        {
            Console.WriteLine($"[MOVE IGNORED] SessionId={SessionId}, Reason=Not authorized");

            await SendErrorMessageAsync(
                "NOT_AUTHORIZED",
                "You must login before sending movement.",
                shouldDisconnect: false);

            return;
        }

        PacketReader reader = new(packet.Payload);
        PlayerMovementInput input = PlayerMovementSerializer.ReadMoveRequest(reader);

        PlayerPositionUpdate? positionUpdate = _gameWorld.ApplyMovement(SessionId, input);

        if (positionUpdate == null)
        {
            Console.WriteLine($"[MOVE IGNORED] SessionId={SessionId}, Reason=Player not found in world");

            await SendErrorMessageAsync(
                "PLAYER_NOT_IN_WORLD",
                "Player was not found in world.",
                shouldDisconnect: false);

            return;
        }

        PacketWriter writer = new();
        PlayerMovementSerializer.WritePositionUpdate(writer, positionUpdate);

        await SendPacketAsync(PacketType.PlayerPositionUpdate, writer.ToArray());
        await _onPlayerPositionChanged(this, positionUpdate);

        Console.WriteLine(
            $"[MOVE] SessionId={SessionId}, CharacterId={positionUpdate.CharacterId}, Position={positionUpdate.Position}, Rotation={positionUpdate.Rotation}");
    }
    
    private async Task HandlePickupItemRequestAsync(PacketMessage packet)
    {
        if (!IsAuthorized || PlayerConnection == null)
        {
            Console.WriteLine($"[PICKUP IGNORED] SessionId={SessionId}, Reason=Not authorized");

            PickupItemResponse notAuthorizedResponse = new()
            {
                IsSuccess = false,
                WorldObjectId = 0,
                Message = "You must login before pickup items."
            };

            await SendPickupItemResponseAsync(notAuthorizedResponse);

            return;
        }

        PacketReader reader = new(packet.Payload);
        PickupItemRequest request = PickupItemSerializer.ReadRequest(reader);

        Console.WriteLine(
            $"[PICKUP REQUEST] SessionId={SessionId}, CharacterId={PlayerConnection.State.CharacterId}, WorldObjectId={request.WorldObjectId}");

        WorldItemPickupResult result = _gameWorld.TryPickupWorldItem(
            SessionId,
            request.WorldObjectId,
            _itemPickupDistance);

        PickupItemResponse response = new()
        {
            IsSuccess = result.IsSuccess,

            WorldObjectId = result.WorldObjectId,

            ItemTemplateId = result.ItemTemplateId,
            DisplayName = result.DisplayName,

            Quantity = result.Quantity,

            Message = result.Message
        };

        await SendPickupItemResponseAsync(response);

        if (!result.IsSuccess)
        {
            Console.WriteLine(
                $"[PICKUP FAILED] SessionId={SessionId}, WorldObjectId={request.WorldObjectId}, Reason={result.Message}");

            return;
        }

        await SendServerMessageAsync(
            $"Picked up: {result.DisplayName} x{result.Quantity}");

        await _onWorldItemPickedUp(this, result);
    }

    private async Task SendPlayerStateSnapshotAsync()
    {
        if (PlayerConnection == null)
            return;

        PacketWriter writer = new();

        PlayerStateSerializer.Write(writer, PlayerConnection.State);

        await SendPacketAsync(PacketType.PlayerStateSnapshot, writer.ToArray());

        Console.WriteLine($"[PLAYER STATE SENT] SessionId={SessionId}, CharacterId={PlayerConnection.State.CharacterId}");
    }
    
    private async Task SendPickupItemResponseAsync(PickupItemResponse response)
    {
        PacketWriter writer = new();
        PickupItemSerializer.WriteResponse(writer, response);

        await SendPacketAsync(PacketType.PickupItemResponse, writer.ToArray());
    }

    public async Task SendPingAsync()
    {
        await SendPacketAsync(PacketType.Ping, Array.Empty<byte>());
    }

    public async Task SendServerMessageAsync(string message)
    {
        ServerMessage serverMessage = new()
        {
            Message = message
        };

        PacketWriter writer = new();
        NetworkMessageSerializer.WriteServerMessage(writer, serverMessage);

        await SendPacketAsync(PacketType.ServerMessage, writer.ToArray());
    }

    public async Task SendErrorMessageAsync(
        string code,
        string message,
        bool shouldDisconnect)
    {
        ErrorMessage errorMessage = new()
        {
            Code = code,
            Message = message,
            ShouldDisconnect = shouldDisconnect
        };

        PacketWriter writer = new();
        NetworkMessageSerializer.WriteErrorMessage(writer, errorMessage);

        await SendPacketAsync(PacketType.ErrorMessage, writer.ToArray());
    }

    private async Task SendRateLimitErrorIfAllowedAsync(string reason)
    {
        DateTime now = DateTime.UtcNow;

        if (now - _lastRateLimitErrorSentAtUtc < RateLimitErrorMessageCooldown)
            return;

        _lastRateLimitErrorSentAtUtc = now;

        await SendErrorMessageAsync(
            "RATE_LIMIT",
            $"Too many packets. {reason}",
            shouldDisconnect: false);
    }

    public async Task SendPacketAsync(PacketType type, byte[] payload)
    {
        if (_stream == null || IsClosed)
            return;

        await _sendLock.WaitAsync();

        try
        {
            if (_stream == null || IsClosed)
                return;

            await PacketProtocol.SendAsync(_stream, type, payload);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[SEND ERROR] SessionId={SessionId}, Type={type}, Message={ex.Message}");
            Close();
        }
        finally
        {
            _sendLock.Release();
        }
    }

    private void Touch()
    {
        LastPacketAtUtc = DateTime.UtcNow;
        PlayerConnection?.Touch();
    }

    private async Task RemoveFromWorldIfNeededAsync()
    {
        if (_removedFromWorld)
            return;

        if (PlayerConnection == null)
            return;

        _removedFromWorld = true;

        try
        {
            _characterService.SavePlayerState(PlayerConnection.State);
        }
        catch (Exception ex)
        {
            Console.WriteLine(
                $"[CHARACTER SAVE ERROR] SessionId={SessionId}, AccountId={AccountId}, Message={ex.Message}");
        }

        await _onPlayerLeavingWorld(this);

        _gameWorld.RemovePlayer(SessionId);
    }

    public void Close()
    {
        if (Interlocked.Exchange(ref _closed, 1) == 1)
            return;

        try
        {
            _stream?.Close();
            _client.Close();
        }
        catch
        {
            // ignore
        }
    }
}