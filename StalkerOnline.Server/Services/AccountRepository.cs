using MySqlConnector;
using StalkerOnline.Server.Database;

namespace StalkerOnline.Server.Services;

public sealed class AccountRepository
{
    private readonly DatabaseConnectionFactory _connectionFactory;

    public AccountRepository(DatabaseConnectionFactory connectionFactory)
    {
        _connectionFactory = connectionFactory;
    }

    public AccountModel? FindByLogin(string login)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                account_id,
                login,
                email,
                password_hash,
                created_at_utc,
                last_login_at_utc,
                is_banned,
                is_admin
            FROM accounts
            WHERE login = @login
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@login", login);

        using MySqlDataReader reader = command.ExecuteReader();

        if (!reader.Read())
            return null;

        return ReadAccount(reader);
    }

    public AccountModel? FindByEmail(string email)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                account_id,
                login,
                email,
                password_hash,
                created_at_utc,
                last_login_at_utc,
                is_banned,
                is_admin
            FROM accounts
            WHERE email = @email
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@email", email);

        using MySqlDataReader reader = command.ExecuteReader();

        if (!reader.Read())
            return null;

        return ReadAccount(reader);
    }

    public AccountModel Create(string login, string email, string passwordHash)
    {
        try
        {
            using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
            connection.Open();

            using MySqlCommand command = connection.CreateCommand();

            command.CommandText =
                """
                INSERT INTO accounts
                (
                    login,
                    email,
                    password_hash,
                    created_at_utc,
                    last_login_at_utc,
                    is_banned,
                    is_admin
                )
                VALUES
                (
                    @login,
                    @email,
                    @password_hash,
                    @created_at_utc,
                    NULL,
                    0,
                    0
                );
                """;

            command.Parameters.AddWithValue("@login", login);
            command.Parameters.AddWithValue("@email", email);
            command.Parameters.AddWithValue("@password_hash", passwordHash);
            command.Parameters.AddWithValue("@created_at_utc", DateTime.UtcNow);

            command.ExecuteNonQuery();

            int accountId = (int)command.LastInsertedId;

            AccountModel? account = FindById(accountId);

            if (account == null)
                throw new InvalidOperationException("Created account was not found.");

            return account;
        }
        catch (MySqlException ex) when (ex.Number == 1062)
        {
            throw new DuplicateAccountException();
        }
    }

    public AccountModel? FindById(int accountId)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            SELECT
                account_id,
                login,
                email,
                password_hash,
                created_at_utc,
                last_login_at_utc,
                is_banned,
                is_admin
            FROM accounts
            WHERE account_id = @account_id
            LIMIT 1;
            """;

        command.Parameters.AddWithValue("@account_id", accountId);

        using MySqlDataReader reader = command.ExecuteReader();

        if (!reader.Read())
            return null;

        return ReadAccount(reader);
    }

    public void UpdateLastLoginAt(int accountId)
    {
        using MySqlConnection connection = _connectionFactory.CreateDatabaseConnection();
        connection.Open();

        using MySqlCommand command = connection.CreateCommand();

        command.CommandText =
            """
            UPDATE accounts
            SET last_login_at_utc = @last_login_at_utc
            WHERE account_id = @account_id;
            """;

        command.Parameters.AddWithValue("@account_id", accountId);
        command.Parameters.AddWithValue("@last_login_at_utc", DateTime.UtcNow);

        command.ExecuteNonQuery();
    }

    private static AccountModel ReadAccount(MySqlDataReader reader)
    {
        int lastLoginOrdinal = reader.GetOrdinal("last_login_at_utc");

        return new AccountModel
        {
            AccountId = reader.GetInt32("account_id"),
            Login = reader.GetString("login"),
            Email = reader.GetString("email"),
            PasswordHash = reader.GetString("password_hash"),
            CreatedAtUtc = reader.GetDateTime("created_at_utc"),
            LastLoginAtUtc = reader.IsDBNull(lastLoginOrdinal)
                ? null
                : reader.GetDateTime(lastLoginOrdinal),
            IsBanned = reader.GetBoolean("is_banned"),
            IsAdmin = reader.GetBoolean("is_admin")
        };
    }
}

public sealed class DuplicateAccountException : Exception
{
}