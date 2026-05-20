using MySqlConnector;

namespace StalkerOnline.Server.Database;

public static class DatabaseInitializer
{
    public static void Initialize(DatabaseConnectionFactory connectionFactory)
    {
        EnsureDatabaseExists(connectionFactory);
        EnsureTablesExist(connectionFactory);

        Console.WriteLine("[DATABASE] Ready.");
    }

    private static void EnsureDatabaseExists(DatabaseConnectionFactory connectionFactory)
    {
        using MySqlConnection connection = connectionFactory.CreateServerConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            $"""
            CREATE DATABASE IF NOT EXISTS `{connectionFactory.DatabaseName}`
            CHARACTER SET utf8mb4
            COLLATE utf8mb4_unicode_ci;
            """;

        command.ExecuteNonQuery();

        Console.WriteLine($"[DATABASE] Database checked: {connectionFactory.DatabaseName}");
    }

    private static void EnsureTablesExist(DatabaseConnectionFactory connectionFactory)
    {
        using MySqlConnection connection = connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            CREATE TABLE IF NOT EXISTS accounts
            (
                account_id INT NOT NULL AUTO_INCREMENT,
                login VARCHAR(32) NOT NULL,
                email VARCHAR(128) NOT NULL,
                password_hash VARCHAR(512) NOT NULL,

                created_at_utc DATETIME(6) NOT NULL,
                last_login_at_utc DATETIME(6) NULL,

                is_banned TINYINT(1) NOT NULL DEFAULT 0,
                is_admin TINYINT(1) NOT NULL DEFAULT 0,

                PRIMARY KEY (account_id),
                UNIQUE KEY uq_accounts_login (login),
                UNIQUE KEY uq_accounts_email (email)
            )
            ENGINE=InnoDB
            DEFAULT CHARACTER SET utf8mb4
            COLLATE utf8mb4_unicode_ci;
            """;

        command.ExecuteNonQuery();

        Console.WriteLine("[DATABASE] Table checked: accounts");
    }
}