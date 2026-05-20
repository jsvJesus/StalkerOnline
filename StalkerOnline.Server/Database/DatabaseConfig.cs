using System.Text.Json;

namespace StalkerOnline.Server.Database;

public sealed class DatabaseConfig
{
    public string Host { get; set; } = "127.0.0.1";
    public uint Port { get; set; } = 3307;
    public string Database { get; set; } = "stalker_online";
    public string User { get; set; } = "root";
    public string Password { get; set; } = string.Empty;
    public string SslMode { get; set; } = "None";

    public static DatabaseConfig Load(string fileName)
    {
        string path = Path.Combine(Environment.CurrentDirectory, fileName);

        if (!File.Exists(path))
        {
            DatabaseConfig defaultConfig = new();

            string defaultJson = JsonSerializer.Serialize(
                defaultConfig,
                new JsonSerializerOptions
                {
                    WriteIndented = true
                });

            File.WriteAllText(path, defaultJson);

            Console.WriteLine($"[DATABASE CONFIG CREATED] {path}");
            Console.WriteLine("[DATABASE CONFIG] Edit database.json and restart server.");

            return defaultConfig;
        }

        string json = File.ReadAllText(path);

        DatabaseConfig? config = JsonSerializer.Deserialize<DatabaseConfig>(
            json,
            new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            });

        if (config == null)
            throw new InvalidOperationException("Failed to read database.json.");

        config.Validate();

        Console.WriteLine($"[DATABASE CONFIG LOADED] {path}");

        return config;
    }

    public void Validate()
    {
        if (string.IsNullOrWhiteSpace(Host))
            throw new InvalidOperationException("Database Host is empty.");

        if (Port == 0)
            throw new InvalidOperationException("Database Port is invalid.");

        if (string.IsNullOrWhiteSpace(Database))
            throw new InvalidOperationException("Database name is empty.");

        if (string.IsNullOrWhiteSpace(User))
            throw new InvalidOperationException("Database User is empty.");

        if (!IsSafeDatabaseName(Database))
            throw new InvalidOperationException("Database name can contain only letters, digits and underscore.");
    }

    private static bool IsSafeDatabaseName(string value)
    {
        foreach (char c in value)
        {
            if (char.IsLetterOrDigit(c) || c == '_')
                continue;

            return false;
        }

        return true;
    }
}