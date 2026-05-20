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
}
catch (Exception ex)
{
    Console.WriteLine($"Client error: {ex.Message}");
}

Console.WriteLine("Press ENTER to exit.");
Console.ReadLine();