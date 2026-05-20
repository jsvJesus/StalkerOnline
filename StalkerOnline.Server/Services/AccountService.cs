namespace StalkerOnline.Server.Services;

public sealed class AccountService
{
    private readonly object _lock = new();

    private readonly Dictionary<string, AccountRecord> _accountsByLogin =
        new(StringComparer.OrdinalIgnoreCase);

    private readonly Dictionary<string, AccountRecord> _accountsByEmail =
        new(StringComparer.OrdinalIgnoreCase);

    private int _nextAccountId = 1;

    public AccountService()
    {
        // Тестовый аккаунт, чтобы старый логин не умер полностью.
        // Login: test
        // Password: test
        CreateAccountUnsafe("test", "test@local.test", "123");
    }

    public RegisterResult Register(string login, string email, string password)
    {
        login = login.Trim();
        email = email.Trim();

        string? validationError = ValidateRegisterFields(login, email, password);

        if (validationError != null)
            return RegisterResult.Fail(validationError);

        lock (_lock)
        {
            if (_accountsByLogin.ContainsKey(login))
                return RegisterResult.Fail("Login already exists.");

            if (_accountsByEmail.ContainsKey(email))
                return RegisterResult.Fail("Email already exists.");

            AccountRecord account = CreateAccountUnsafe(login, email, password);

            return RegisterResult.Success(
                account.AccountId,
                account.Login,
                "Account registered successfully.");
        }
    }

    public LoginResult ValidateLogin(string login, string password)
    {
        login = login.Trim();

        if (login.Length < 3)
            return LoginResult.Fail("Login must contain at least 3 characters.");

        if (password.Length < 3)
            return LoginResult.Fail("Password must contain at least 3 characters.");

        lock (_lock)
        {
            if (!_accountsByLogin.TryGetValue(login, out AccountRecord? account))
                return LoginResult.Fail("Account not found. Please register first.");

            if (account.Password != password)
                return LoginResult.Fail("Invalid password.");

            account.LastLoginAtUtc = DateTime.UtcNow;

            return LoginResult.Success(
                account.AccountId,
                account.Login,
                "Login accepted. Welcome to Stalker Online.");
        }
    }

    private AccountRecord CreateAccountUnsafe(string login, string email, string password)
    {
        AccountRecord account = new()
        {
            AccountId = _nextAccountId++,
            Login = login,
            Email = email,
            Password = password,
            CreatedAtUtc = DateTime.UtcNow,
            LastLoginAtUtc = null,
            IsBanned = false,
            IsAdmin = false
        };

        _accountsByLogin.Add(account.Login, account);
        _accountsByEmail.Add(account.Email, account);

        return account;
    }

    private static string? ValidateRegisterFields(string login, string email, string password)
    {
        if (login.Length < 3)
            return "Login must contain at least 3 characters.";

        if (login.Length > 32)
            return "Login is too long. Max length is 32 characters.";

        if (!IsValidLogin(login))
            return "Login can contain only letters, digits, underscore, dash and dot.";

        if (!IsValidEmail(email))
            return "Email is invalid.";

        if (password.Length < 3)
            return "Password must contain at least 3 characters.";

        if (password.Length > 64)
            return "Password is too long. Max length is 64 characters.";

        return null;
    }

    private static bool IsValidLogin(string login)
    {
        foreach (char c in login)
        {
            if (char.IsLetterOrDigit(c))
                continue;

            if (c is '_' or '-' or '.')
                continue;

            return false;
        }

        return true;
    }

    private static bool IsValidEmail(string email)
    {
        if (email.Length < 5 || email.Length > 128)
            return false;

        int atIndex = email.IndexOf('@');
        int lastAtIndex = email.LastIndexOf('@');

        if (atIndex <= 0)
            return false;

        if (atIndex != lastAtIndex)
            return false;

        if (atIndex >= email.Length - 2)
            return false;

        string domain = email[(atIndex + 1)..];

        return domain.Contains('.');
    }

    private sealed class AccountRecord
    {
        public int AccountId { get; init; }
        public string Login { get; init; } = string.Empty;
        public string Email { get; init; } = string.Empty;
        public string Password { get; init; } = string.Empty;
        public DateTime CreatedAtUtc { get; init; }
        public DateTime? LastLoginAtUtc { get; set; }
        public bool IsBanned { get; init; }
        public bool IsAdmin { get; init; }
    }
}

public sealed class RegisterResult
{
    public bool IsSuccess { get; }
    public int AccountId { get; }
    public string Login { get; }
    public string Message { get; }

    private RegisterResult(bool isSuccess, int accountId, string login, string message)
    {
        IsSuccess = isSuccess;
        AccountId = accountId;
        Login = login;
        Message = message;
    }

    public static RegisterResult Success(int accountId, string login, string message)
    {
        return new RegisterResult(true, accountId, login, message);
    }

    public static RegisterResult Fail(string message)
    {
        return new RegisterResult(false, 0, string.Empty, message);
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