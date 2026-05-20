using System.Net;
using System.Net.Sockets;
using StalkerOnline.Shared.Network;

Console.Title = "Stalker Online Server";
Console.OutputEncoding = System.Text.Encoding.UTF8;

const int port = 7777;

TcpListener listener = new(IPAddress.Any, port);

listener.Start();

Console.WriteLine("===================================");
Console.WriteLine(" Stalker Online Server started");
Console.WriteLine($" Port: {port}");
Console.WriteLine("===================================");

while (true)
{
    TcpClient client = await listener.AcceptTcpClientAsync();

    _ = Task.Run(async () =>
    {
        await HandleClientAsync(client);
    });
}

static async Task HandleClientAsync(TcpClient client)
{
    string clientIp = client.Client.RemoteEndPoint?.ToString() ?? "unknown";

    Console.WriteLine($"[CONNECT] {clientIp}");

    try
    {
        using NetworkStream stream = client.GetStream();

        while (client.Connected)
        {
            PacketMessage? packet = await PacketProtocol.ReceiveAsync(stream);

            if (packet == null)
            {
                Console.WriteLine($"[DISCONNECT] {clientIp}");
                break;
            }

            await HandlePacketAsync(stream, clientIp, packet);
        }
    }
    catch (Exception ex)
    {
        Console.WriteLine($"[ERROR] {clientIp}: {ex.Message}");
    }
    finally
    {
        client.Close();
    }
}

static async Task HandlePacketAsync(NetworkStream stream, string clientIp, PacketMessage packet)
{
    switch (packet.Type)
    {
        case PacketType.LoginRequest:
        {
            PacketReader reader = new(packet.Payload);

            string login = reader.ReadString();
            string password = reader.ReadString();

            Console.WriteLine($"[LOGIN REQUEST] IP={clientIp}, Login={login}, Password={password}");

            bool success = login.Length >= 3 && password.Length >= 3;

            PacketWriter writer = new();

            writer.WriteBool(success);

            if (success)
            {
                writer.WriteString("Login accepted. Welcome to Stalker Online.");
                Console.WriteLine($"[LOGIN SUCCESS] {login}");
            }
            else
            {
                writer.WriteString("Login failed. Login and password must contain at least 3 characters.");
                Console.WriteLine($"[LOGIN FAILED] {login}");
            }

            await PacketProtocol.SendAsync(
                stream,
                PacketType.LoginResponse,
                writer.ToArray());

            break;
        }

        default:
        {
            Console.WriteLine($"[UNKNOWN PACKET] Type={packet.Type}");
            break;
        }
    }
}