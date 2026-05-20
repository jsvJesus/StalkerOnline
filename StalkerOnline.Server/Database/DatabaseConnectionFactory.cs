using MySqlConnector;

namespace StalkerOnline.Server.Database;

public sealed class DatabaseConnectionFactory
{
    private readonly DatabaseConfig _config;

    public DatabaseConnectionFactory(DatabaseConfig config)
    {
        _config = config;
    }

    public MySqlConnection CreateServerConnection()
    {
        MySqlConnectionStringBuilder builder = CreateBaseBuilder();

        return new MySqlConnection(builder.ConnectionString);
    }

    public MySqlConnection CreateDatabaseConnection()
    {
        MySqlConnectionStringBuilder builder = CreateBaseBuilder();
        builder.Database = _config.Database;

        return new MySqlConnection(builder.ConnectionString);
    }

    public string DatabaseName => _config.Database;

    private MySqlConnectionStringBuilder CreateBaseBuilder()
    {
        MySqlConnectionStringBuilder builder = new()
        {
            Server = _config.Host,
            Port = _config.Port,
            UserID = _config.User,
            Password = _config.Password,
            CharacterSet = "utf8mb4",
            SslMode = ParseSslMode(_config.SslMode)
        };

        return builder;
    }

    private static MySqlSslMode ParseSslMode(string value)
    {
        if (Enum.TryParse(value, ignoreCase: true, out MySqlSslMode sslMode))
            return sslMode;

        return MySqlSslMode.None;
    }
}