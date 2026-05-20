using MySqlConnector;
using StalkerOnline.Server.Database;
using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Services;

public sealed class CharacterRepository
{
    private readonly DatabaseConnectionFactory _connectionFactory;

    public CharacterRepository(DatabaseConnectionFactory connectionFactory)
    {
        _connectionFactory = connectionFactory;
    }

    public PlayerState? FindByAccountId(int accountId)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                character_id,
                account_id,
                nickname,

                position_x,
                position_y,
                position_z,

                rotation_x,
                rotation_y,
                rotation_z,

                health,
                max_health,

                stamina,
                max_stamina,

                hunger,
                thirst,

                radiation,
                toxicity,

                is_alive
            FROM characters
            WHERE account_id = @account_id
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@account_id", accountId);

        using MySqlDataReader reader = command.ExecuteReader();

        if (!reader.Read())
            return null;

        return ReadPlayerState(reader);
    }

    public PlayerState CreateDefault(int accountId, string nickname)
    {
        PlayerState state = PlayerState.CreateDefault(accountId, nickname);

        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            INSERT INTO characters
            (
                account_id,
                nickname,

                position_x,
                position_y,
                position_z,

                rotation_x,
                rotation_y,
                rotation_z,

                health,
                max_health,

                stamina,
                max_stamina,

                hunger,
                thirst,

                radiation,
                toxicity,

                is_alive,

                created_at_utc,
                last_save_at_utc
            )
            VALUES
            (
                @account_id,
                @nickname,

                @position_x,
                @position_y,
                @position_z,

                @rotation_x,
                @rotation_y,
                @rotation_z,

                @health,
                @max_health,

                @stamina,
                @max_stamina,

                @hunger,
                @thirst,

                @radiation,
                @toxicity,

                @is_alive,

                @created_at_utc,
                @last_save_at_utc
            );
            """;

        DateTime now = DateTime.UtcNow;

        command.Parameters.AddWithValue("@account_id", state.AccountId);
        command.Parameters.AddWithValue("@nickname", state.Nickname);

        command.Parameters.AddWithValue("@position_x", state.Position.X);
        command.Parameters.AddWithValue("@position_y", state.Position.Y);
        command.Parameters.AddWithValue("@position_z", state.Position.Z);

        command.Parameters.AddWithValue("@rotation_x", state.Rotation.X);
        command.Parameters.AddWithValue("@rotation_y", state.Rotation.Y);
        command.Parameters.AddWithValue("@rotation_z", state.Rotation.Z);

        command.Parameters.AddWithValue("@health", state.Health);
        command.Parameters.AddWithValue("@max_health", state.MaxHealth);

        command.Parameters.AddWithValue("@stamina", state.Stamina);
        command.Parameters.AddWithValue("@max_stamina", state.MaxStamina);

        command.Parameters.AddWithValue("@hunger", state.Hunger);
        command.Parameters.AddWithValue("@thirst", state.Thirst);

        command.Parameters.AddWithValue("@radiation", state.Radiation);
        command.Parameters.AddWithValue("@toxicity", state.Toxicity);

        command.Parameters.AddWithValue("@is_alive", state.IsAlive);

        command.Parameters.AddWithValue("@created_at_utc", now);
        command.Parameters.AddWithValue("@last_save_at_utc", now);

        command.ExecuteNonQuery();

        state.CharacterId = (int)command.LastInsertedId;

        return state;
    }

    public void Save(PlayerState state)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            UPDATE characters
            SET
                nickname = @nickname,

                position_x = @position_x,
                position_y = @position_y,
                position_z = @position_z,

                rotation_x = @rotation_x,
                rotation_y = @rotation_y,
                rotation_z = @rotation_z,

                health = @health,
                max_health = @max_health,

                stamina = @stamina,
                max_stamina = @max_stamina,

                hunger = @hunger,
                thirst = @thirst,

                radiation = @radiation,
                toxicity = @toxicity,

                is_alive = @is_alive,

                last_save_at_utc = @last_save_at_utc
            WHERE character_id = @character_id
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@character_id", state.CharacterId);
        command.Parameters.AddWithValue("@nickname", state.Nickname);

        command.Parameters.AddWithValue("@position_x", state.Position.X);
        command.Parameters.AddWithValue("@position_y", state.Position.Y);
        command.Parameters.AddWithValue("@position_z", state.Position.Z);

        command.Parameters.AddWithValue("@rotation_x", state.Rotation.X);
        command.Parameters.AddWithValue("@rotation_y", state.Rotation.Y);
        command.Parameters.AddWithValue("@rotation_z", state.Rotation.Z);

        command.Parameters.AddWithValue("@health", state.Health);
        command.Parameters.AddWithValue("@max_health", state.MaxHealth);

        command.Parameters.AddWithValue("@stamina", state.Stamina);
        command.Parameters.AddWithValue("@max_stamina", state.MaxStamina);

        command.Parameters.AddWithValue("@hunger", state.Hunger);
        command.Parameters.AddWithValue("@thirst", state.Thirst);

        command.Parameters.AddWithValue("@radiation", state.Radiation);
        command.Parameters.AddWithValue("@toxicity", state.Toxicity);

        command.Parameters.AddWithValue("@is_alive", state.IsAlive);
        command.Parameters.AddWithValue("@last_save_at_utc", DateTime.UtcNow);

        command.ExecuteNonQuery();
    }

    private static PlayerState ReadPlayerState(MySqlDataReader reader)
    {
        return new PlayerState
        {
            CharacterId = reader.GetInt32(reader.GetOrdinal("character_id")),
            AccountId = reader.GetInt32(reader.GetOrdinal("account_id")),
            Nickname = reader.GetString(reader.GetOrdinal("nickname")),

            Position = new NetVector3(
                reader.GetFloat(reader.GetOrdinal("position_x")),
                reader.GetFloat(reader.GetOrdinal("position_y")),
                reader.GetFloat(reader.GetOrdinal("position_z"))),

            Rotation = new NetVector3(
                reader.GetFloat(reader.GetOrdinal("rotation_x")),
                reader.GetFloat(reader.GetOrdinal("rotation_y")),
                reader.GetFloat(reader.GetOrdinal("rotation_z"))),

            Health = reader.GetFloat(reader.GetOrdinal("health")),
            MaxHealth = reader.GetFloat(reader.GetOrdinal("max_health")),

            Stamina = reader.GetFloat(reader.GetOrdinal("stamina")),
            MaxStamina = reader.GetFloat(reader.GetOrdinal("max_stamina")),

            Hunger = reader.GetFloat(reader.GetOrdinal("hunger")),
            Thirst = reader.GetFloat(reader.GetOrdinal("thirst")),

            Radiation = reader.GetFloat(reader.GetOrdinal("radiation")),
            Toxicity = reader.GetFloat(reader.GetOrdinal("toxicity")),

            IsAlive = reader.GetBoolean(reader.GetOrdinal("is_alive"))
        };
    }
}