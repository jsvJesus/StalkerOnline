using System.Net;
using System.Net.Sockets;
using System.Text.Json;

namespace StalkerOnline.Server.Config;

public sealed class ServerConfig
{
    public string Host { get; set; } = "0.0.0.0";
    public int Port { get; set; } = 7777;

    public int ClientPingIntervalSeconds { get; set; } = 10;
    public int ClientTimeoutSeconds { get; set; } = 30;
    public int AutoSaveIntervalSeconds { get; set; } = 15;

    public float PositionBroadcastRadius { get; set; } = 100f;
    
    public float ItemVisibilityRadius { get; set; } = 100f;
    public float SpawnBroadcastRadius { get; set; } = 100f;

    public float MoveSpeed { get; set; } = 4.5f;
    public float DefaultDeltaTime { get; set; } = 0.05f;
    public float MaxDeltaTime { get; set; } = 0.10f;

    public TimeSpan ClientPingInterval => TimeSpan.FromSeconds(ClientPingIntervalSeconds);
    public TimeSpan ClientTimeout => TimeSpan.FromSeconds(ClientTimeoutSeconds);
    public TimeSpan AutoSaveInterval => TimeSpan.FromSeconds(AutoSaveIntervalSeconds);

    public static ServerConfig Load(string fileName)
    {
        string path = Path.Combine(Environment.CurrentDirectory, fileName);

        if (!File.Exists(path))
        {
            ServerConfig defaultConfig = new();

            string defaultJson = JsonSerializer.Serialize(
                defaultConfig,
                new JsonSerializerOptions
                {
                    WriteIndented = true
                });

            File.WriteAllText(path, defaultJson);

            Console.WriteLine($"[SERVER CONFIG CREATED] {path}");
            Console.WriteLine("[SERVER CONFIG] Edit server.json and restart server.");

            return defaultConfig;
        }

        string json = File.ReadAllText(path);

        ServerConfig? config = JsonSerializer.Deserialize<ServerConfig>(
            json,
            new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            });

        if (config == null)
            throw new InvalidOperationException("Failed to read server.json.");

        config.Validate();

        Console.WriteLine($"[SERVER CONFIG LOADED] {path}");
        Console.WriteLine($"[SERVER CONFIG] Host={config.Host}, Port={config.Port}");
        Console.WriteLine($"[SERVER CONFIG] Ping={config.ClientPingIntervalSeconds}s, Timeout={config.ClientTimeoutSeconds}s, AutoSave={config.AutoSaveIntervalSeconds}s");
        Console.WriteLine($"[SERVER CONFIG] PositionRadius={config.PositionBroadcastRadius}, SpawnRadius={config.SpawnBroadcastRadius}");
        Console.WriteLine($"[SERVER CONFIG] ItemVisibilityRadius={config.ItemVisibilityRadius}");
        Console.WriteLine($"[SERVER CONFIG] MoveSpeed={config.MoveSpeed}, DeltaTime={config.DefaultDeltaTime}-{config.MaxDeltaTime}");

        return config;
    }

    public IPAddress GetIPAddress()
    {
        string host = Host.Trim();

        if (host == "*" || host == "0.0.0.0" || host.Equals("any", StringComparison.OrdinalIgnoreCase))
            return IPAddress.Any;

        if (host.Equals("localhost", StringComparison.OrdinalIgnoreCase))
            return IPAddress.Loopback;

        if (IPAddress.TryParse(host, out IPAddress? ipAddress))
            return ipAddress;

        IPAddress[] addresses = Dns.GetHostAddresses(host);

        foreach (IPAddress address in addresses)
        {
            if (address.AddressFamily == AddressFamily.InterNetwork)
                return address;
        }

        throw new InvalidOperationException($"Could not resolve server host: {Host}");
    }

    private void Validate()
    {
        if (string.IsNullOrWhiteSpace(Host))
            throw new InvalidOperationException("Server Host is empty.");

        if (Port <= 0 || Port > 65535)
            throw new InvalidOperationException("Server Port must be between 1 and 65535.");

        if (ClientPingIntervalSeconds <= 0)
            throw new InvalidOperationException("ClientPingIntervalSeconds must be greater than 0.");

        if (ClientTimeoutSeconds <= 0)
            throw new InvalidOperationException("ClientTimeoutSeconds must be greater than 0.");

        if (ClientTimeoutSeconds <= ClientPingIntervalSeconds)
            throw new InvalidOperationException("ClientTimeoutSeconds must be greater than ClientPingIntervalSeconds.");

        if (AutoSaveIntervalSeconds <= 0)
            throw new InvalidOperationException("AutoSaveIntervalSeconds must be greater than 0.");

        if (PositionBroadcastRadius <= 0f)
            throw new InvalidOperationException("PositionBroadcastRadius must be greater than 0.");

        if (SpawnBroadcastRadius <= 0f)
            throw new InvalidOperationException("SpawnBroadcastRadius must be greater than 0.");
        
        if (ItemVisibilityRadius <= 0f)
            throw new InvalidOperationException("ItemVisibilityRadius must be greater than 0.");

        if (MoveSpeed <= 0f)
            throw new InvalidOperationException("MoveSpeed must be greater than 0.");

        if (DefaultDeltaTime <= 0f)
            throw new InvalidOperationException("DefaultDeltaTime must be greater than 0.");

        if (MaxDeltaTime <= 0f)
            throw new InvalidOperationException("MaxDeltaTime must be greater than 0.");

        if (MaxDeltaTime < DefaultDeltaTime)
            throw new InvalidOperationException("MaxDeltaTime must be greater than or equal to DefaultDeltaTime.");
    }
}