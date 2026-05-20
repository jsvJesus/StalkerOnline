using System.Net.Sockets;
using StalkerOnline.Shared.Network;

Console.Title = "Stalker Online Client";
Console.OutputEncoding = System.Text.Encoding.UTF8;

const string host = "26.163.92.76"; // Local IP
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

    PacketMessage? response = await PacketProtocol.ReceiveAsync(stream);

    if (response == null)
    {
        Console.WriteLine("Server closed connection.");
        return;
    }

    if (response.Type != PacketType.LoginResponse)
    {
        Console.WriteLine($"Unexpected packet: {response.Type}");
        return;
    }

    PacketReader reader = new(response.Payload);

    bool success = reader.ReadBool();
    string message = reader.ReadString();

    Console.WriteLine("-----------------------------------");
    Console.WriteLine($"Login success: {success}");
    Console.WriteLine($"Message: {message}");
    Console.WriteLine("-----------------------------------");
}
catch (Exception ex)
{
    Console.WriteLine($"Client error: {ex.Message}");
}

Console.WriteLine("Press ENTER to exit.");
Console.ReadLine();