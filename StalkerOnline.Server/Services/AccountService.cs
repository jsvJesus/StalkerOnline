namespace StalkerOnline.Server.Services;

public sealed class AccountService
{
    public LoginResult ValidateLogin(string login, string password)
    {
        login = login.Trim();

        if (login.Length < 3)
            return LoginResult.Fail("Login must contain at least 3 characters.");

        if (password.Length < 3)
            return LoginResult.Fail("Password must contain at least 3 characters.");

        // Временная проверка.
        // Потом тут будет база данных: PostgreSQL / MySQL / SQLite.
        return LoginResult.Success(
            accountId: 1,
            login: login,
            message: "Login accepted. Welcome to Stalker Online.");
    }
}

public sealed class LoginResult
{
    public bool IsSuccess { get; }
    public int AccountId { get; }
    public string Login { get; }
    public string Message { get; }

    private LoginResult(bool isSuccess, int accountId, string login, string message)
    {
        IsSuccess = isSuccess;
        AccountId = accountId;
        Login = login;
        Message = message;
    }

    public static LoginResult Success(int accountId, string login, string message)
    {
        return new LoginResult(true, accountId, login, message);
    }

    public static LoginResult Fail(string message)
    {
        return new LoginResult(false, 0, string.Empty, message);
    }
}