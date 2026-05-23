using MySqlConnector;

namespace StalkerOnline.Server.Database;

public static class DatabaseInitializer
{
    public static void Initialize(DatabaseConnectionFactory connectionFactory)
    {
        EnsureDatabaseExists(connectionFactory);

        using MySqlConnection connection = connectionFactory.CreateDatabaseConnection();
        connection.Open();

        EnsureMigrationTable(connection);

        ApplyMigration(connection, 1, "create_accounts_and_characters", command =>
        {
            ExecuteNonQuery(command,
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
                """);

            ExecuteNonQuery(command,
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
                """);
        });

        ApplyMigration(connection, 2, "create_items_and_inventories", command =>
        {
            ExecuteNonQuery(command,
                """
                CREATE TABLE IF NOT EXISTS item_templates
                (
                    item_template_id INT NOT NULL AUTO_INCREMENT,

                    item_key VARCHAR(64) NOT NULL,
                    display_name VARCHAR(128) NOT NULL,

                    item_type VARCHAR(32) NOT NULL DEFAULT 'Misc',
                    rarity VARCHAR(32) NOT NULL DEFAULT 'Common',

                    max_stack INT NOT NULL DEFAULT 1,
                    weight_per_item FLOAT NOT NULL DEFAULT 0,

                    max_durability FLOAT NOT NULL DEFAULT 100,

                    food_value FLOAT NOT NULL DEFAULT 0,
                    water_value FLOAT NOT NULL DEFAULT 0,
                    radiation_protection FLOAT NOT NULL DEFAULT 0,

                    created_at_utc DATETIME(6) NOT NULL,
                    updated_at_utc DATETIME(6) NOT NULL,

                    PRIMARY KEY (item_template_id),
                    UNIQUE KEY uq_item_templates_item_key (item_key)
                )
                ENGINE=InnoDB
                DEFAULT CHARACTER SET utf8mb4
                COLLATE utf8mb4_unicode_ci;
                """);

            ExecuteNonQuery(command,
                """
                CREATE TABLE IF NOT EXISTS inventories
                (
                    inventory_id INT NOT NULL AUTO_INCREMENT,
                    character_id INT NOT NULL,

                    inventory_type VARCHAR(32) NOT NULL DEFAULT 'Main',

                    width INT NOT NULL DEFAULT 10,
                    height INT NOT NULL DEFAULT 6,
                    max_weight FLOAT NOT NULL DEFAULT 50,

                    created_at_utc DATETIME(6) NOT NULL,
                    updated_at_utc DATETIME(6) NOT NULL,

                    PRIMARY KEY (inventory_id),
                    UNIQUE KEY uq_inventories_character_type (character_id, inventory_type),

                    CONSTRAINT fk_inventories_character_id
                        FOREIGN KEY (character_id)
                        REFERENCES characters(character_id)
                        ON DELETE CASCADE
                )
                ENGINE=InnoDB
                DEFAULT CHARACTER SET utf8mb4
                COLLATE utf8mb4_unicode_ci;
                """);

            ExecuteNonQuery(command,
                """
                CREATE TABLE IF NOT EXISTS inventory_items
                (
                    inventory_item_id BIGINT NOT NULL AUTO_INCREMENT,

                    inventory_id INT NOT NULL,
                    item_template_id INT NOT NULL,

                    quantity INT NOT NULL DEFAULT 1,

                    slot_x INT NOT NULL DEFAULT 0,
                    slot_y INT NOT NULL DEFAULT 0,

                    width INT NOT NULL DEFAULT 1,
                    height INT NOT NULL DEFAULT 1,

                    durability FLOAT NOT NULL DEFAULT 100,

                    created_at_utc DATETIME(6) NOT NULL,
                    updated_at_utc DATETIME(6) NOT NULL,

                    PRIMARY KEY (inventory_item_id),

                    KEY ix_inventory_items_inventory_id (inventory_id),
                    KEY ix_inventory_items_item_template_id (item_template_id),
                    UNIQUE KEY uq_inventory_items_slot (inventory_id, slot_x, slot_y),

                    CONSTRAINT fk_inventory_items_inventory_id
                        FOREIGN KEY (inventory_id)
                        REFERENCES inventories(inventory_id)
                        ON DELETE CASCADE,

                    CONSTRAINT fk_inventory_items_item_template_id
                        FOREIGN KEY (item_template_id)
                        REFERENCES item_templates(item_template_id)
                        ON DELETE RESTRICT
                )
                ENGINE=InnoDB
                DEFAULT CHARACTER SET utf8mb4
                COLLATE utf8mb4_unicode_ci;
                """);

            ExecuteNonQuery(command,
                """
                INSERT INTO item_templates
                (
                    item_key,
                    display_name,
                    item_type,
                    rarity,
                    max_stack,
                    weight_per_item,
                    max_durability,
                    food_value,
                    water_value,
                    radiation_protection,
                    created_at_utc,
                    updated_at_utc
                )
                VALUES
                (
                    'item_water_bottle',
                    'Bottle of Water',
                    'Consumable',
                    'Common',
                    5,
                    0.50,
                    1,
                    0,
                    35,
                    0,
                    UTC_TIMESTAMP(6),
                    UTC_TIMESTAMP(6)
                ),
                (
                    'item_medkit_small',
                    'Small Medkit',
                    'Consumable',
                    'Common',
                    5,
                    0.35,
                    1,
                    0,
                    0,
                    0,
                    UTC_TIMESTAMP(6),
                    UTC_TIMESTAMP(6)
                ),
                (
                    'item_ak_ammo_545',
                    '5.45x39 Ammo',
                    'Ammo',
                    'Common',
                    60,
                    0.01,
                    1,
                    0,
                    0,
                    0,
                    UTC_TIMESTAMP(6),
                    UTC_TIMESTAMP(6)
                )
                ON DUPLICATE KEY UPDATE
                    display_name = VALUES(display_name),
                    item_type = VALUES(item_type),
                    rarity = VALUES(rarity),
                    max_stack = VALUES(max_stack),
                    weight_per_item = VALUES(weight_per_item),
                    max_durability = VALUES(max_durability),
                    food_value = VALUES(food_value),
                    water_value = VALUES(water_value),
                    radiation_protection = VALUES(radiation_protection),
                    updated_at_utc = UTC_TIMESTAMP(6);
                """);
        });

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

    private static void EnsureMigrationTable(MySqlConnection connection)
    {
        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            CREATE TABLE IF NOT EXISTS schema_migrations
            (
                migration_id INT NOT NULL,
                name VARCHAR(128) NOT NULL,
                applied_at_utc DATETIME(6) NOT NULL,

                PRIMARY KEY (migration_id)
            )
            ENGINE=InnoDB
            DEFAULT CHARACTER SET utf8mb4
            COLLATE utf8mb4_unicode_ci;
            """;

        command.ExecuteNonQuery();

        Console.WriteLine("[DATABASE] Table checked: schema_migrations");
    }

    private static void ApplyMigration(
        MySqlConnection connection,
        int migrationId,
        string migrationName,
        Action<MySqlCommand> migrationAction)
    {
        if (IsMigrationApplied(connection, migrationId))
        {
            Console.WriteLine($"[DATABASE MIGRATION] Skipped {migrationId}: {migrationName}");
            return;
        }

        using MySqlCommand command = connection.CreateCommand();

        migrationAction(command);
        RecordMigration(command, migrationId, migrationName);

        Console.WriteLine($"[DATABASE MIGRATION] Applied {migrationId}: {migrationName}");
    }

    private static bool IsMigrationApplied(MySqlConnection connection, int migrationId)
    {
        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT COUNT(*)
            FROM schema_migrations
            WHERE migration_id = @migration_id;
            """;

        command.Parameters.AddWithValue("@migration_id", migrationId);

        object? result = command.ExecuteScalar();

        return Convert.ToInt32(result) > 0;
    }

    private static void RecordMigration(MySqlCommand command, int migrationId, string migrationName)
    {
        command.Parameters.Clear();

        command.CommandText =
            """
            INSERT INTO schema_migrations
            (
                migration_id,
                name,
                applied_at_utc
            )
            VALUES
            (
                @migration_id,
                @name,
                @applied_at_utc
            );
            """;

        command.Parameters.AddWithValue("@migration_id", migrationId);
        command.Parameters.AddWithValue("@name", migrationName);
        command.Parameters.AddWithValue("@applied_at_utc", DateTime.UtcNow);

        command.ExecuteNonQuery();
    }

    private static void ExecuteNonQuery(MySqlCommand command, string sql)
    {
        command.Parameters.Clear();
        command.CommandText = sql;
        command.ExecuteNonQuery();
    }
}