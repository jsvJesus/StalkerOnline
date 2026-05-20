using System.Net.Sockets;
using StalkerOnline.Server.Game;
using StalkerOnline.Server.Services;
using StalkerOnline.Shared.Game;
using StalkerOnline.Shared.Network;

namespace StalkerOnline.Server.Core;

public sealed class ClientSession
{
    private readonly TcpClient _client;
    private readonly AccountService _accountService;
    private readonly CharacterService _characterService;
    private readonly Action<ClientSession> _onDisconnected;

    private NetworkStream? _stream;

    public int SessionId { get; }
    public string RemoteAddress { get; }

    public bool IsAuthorized { get; private set; }
    public int AccountId { get; private set; }
    public string Login { get; private set; } = string.Empty;

    public PlayerConnection? PlayerConnection { get; private set; }

    public ClientSession(
        int sessionId,
        TcpClient client,
        AccountService accountService,
        CharacterService characterService,
        Action<ClientSession> onDisconnected)
    {
        SessionId = sessionId;
        _client = client;
        _accountService = accountService;
        _characterService = characterService;
        _onDisconnected = onDisconnected;

        RemoteAddress = _client.Client.RemoteEndPoint?.ToString() ?? "unknown";
    }

    public async Task RunAsync()
    {
        Console.WriteLine($"[CONNECT] SessionId={SessionId}, IP={RemoteAddress}");

        try
        {
            _stream = _client.GetStream();

            while (_client.Connected)
            {
                PacketMessage? packet = await PacketProtocol.ReceiveAsync(_stream);

                if (packet == null)
                {
                    Console.WriteLine($"[DISCONNECT] SessionId={SessionId}, IP={RemoteAddress}");
                    break;
                }

                PlayerConnection?.Touch();

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
            Close();
            _onDisconnected(this);
        }
    }

    private async Task HandlePacketAsync(PacketMessage packet)
    {
        switch (packet.Type)
        {
            case PacketType.LoginRequest:
                await HandleLoginRequestAsync(packet);
                break;

            case PacketType.Disconnect:
                Console.WriteLine($"[CLIENT DISCONNECT REQUEST] SessionId={SessionId}");
                Close();
                break;

            default:
                Console.WriteLine($"[UNKNOWN PACKET] SessionId={SessionId}, Type={packet.Type}");
                break;
        }
    }

    private async Task HandleLoginRequestAsync(PacketMessage packet)
    {
        if (IsAuthorized)
        {
            Console.WriteLine($"[LOGIN IGNORED] SessionId={SessionId}, Reason=Already authorized");
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

        await SendAsync(PacketType.LoginResponse, loginResponseWriter.ToArray());

        if (!result.IsSuccess)
        {
            Console.WriteLine($"[LOGIN FAILED] SessionId={SessionId}, Login={login}, Reason={result.Message}");
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

        Console.WriteLine($"[LOGIN SUCCESS] SessionId={SessionId}, AccountId={AccountId}, Login={Login}");
        Console.WriteLine($"[PLAYER CREATED] SessionId={SessionId}, CharacterId={playerState.CharacterId}, Nickname={playerState.Nickname}");

        await SendPlayerStateSnapshotAsync();
    }

    private async Task SendPlayerStateSnapshotAsync()
    {
        if (PlayerConnection == null)
            return;

        PacketWriter writer = new();

        PlayerStateSerializer.Write(writer, PlayerConnection.State);

        await SendAsync(PacketType.PlayerStateSnapshot, writer.ToArray());

        Console.WriteLine($"[PLAYER STATE SENT] SessionId={SessionId}, CharacterId={PlayerConnection.State.CharacterId}");
    }

    private async Task SendAsync(PacketType type, byte[] payload)
    {
        if (_stream == null)
            return;

        await PacketProtocol.SendAsync(_stream, type, payload);
    }

    public void Close()
    {
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