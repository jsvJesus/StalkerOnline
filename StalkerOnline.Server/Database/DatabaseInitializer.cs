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

        command.CommandText =
            """
            CREATE TABLE IF NOT EXISTS characters
            (
                character_id INT NOT NULL AUTO_INCREMENT,
                account_id INT NOT NULL,

                nickname VARCHAR(32) NOT NULL,

                position_x FLOAT NOT NULL DEFAULT 0,
                position_y FLOAT NOT NULL DEFAULT 0,
                position_z FLOAT NOT NULL DEFAULT 0,

                rotation_x FLOAT NOT NULL DEFAULT 0,
                rotation_y FLOAT NOT NULL DEFAULT 0,
                rotation_z FLOAT NOT NULL DEFAULT 0,

                health FLOAT NOT NULL DEFAULT 100,
                max_health FLOAT NOT NULL DEFAULT 100,

                stamina FLOAT NOT NULL DEFAULT 100,
                max_stamina FLOAT NOT NULL DEFAULT 100,

                hunger FLOAT NOT NULL DEFAULT 0,
                thirst FLOAT NOT NULL DEFAULT 0,

                radiation FLOAT NOT NULL DEFAULT 0,
                toxicity FLOAT NOT NULL DEFAULT 0,

                is_alive TINYINT(1) NOT NULL DEFAULT 1,

                created_at_utc DATETIME(6) NOT NULL,
                last_save_at_utc DATETIME(6) NOT NULL,

                PRIMARY KEY (character_id),
                UNIQUE KEY uq_characters_account_id (account_id),
                KEY ix_characters_nickname (nickname),

                CONSTRAINT fk_characters_account_id
                    FOREIGN KEY (account_id)
                    REFERENCES accounts(account_id)
                    ON DELETE CASCADE
            )
            ENGINE=InnoDB
            DEFAULT CHARACTER SET utf8mb4
            COLLATE utf8mb4_unicode_ci;
            """;

        command.ExecuteNonQuery();

        Console.WriteLine("[DATABASE] Table checked: characters");
    }
}