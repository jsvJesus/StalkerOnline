namespace StalkerOnline.Server.Services;

public sealed class AccountService
{
    private readonly AccountRepository _accountRepository;

    public AccountService(AccountRepository accountRepository)
    {
        _accountRepository = accountRepository;
    }

    public RegisterResult Register(string login, string email, string password)
    {
        login = login.Trim();
        email = email.Trim();

        string? validationError = ValidateRegisterFields(login, email, password);

        if (validationError != null)
            return RegisterResult.Fail(validationError);

        try
        {
            if (_accountRepository.FindByLogin(login) != null)
                return RegisterResult.Fail("Login already exists.");

            if (_accountRepository.FindByEmail(email) != null)
                return RegisterResult.Fail("Email already exists.");

            string passwordHash = PasswordHasher.HashPassword(password);

            AccountModel account = _accountRepository.Create(
                login,
                email,
                passwordHash);

            return RegisterResult.Success(
                account.AccountId,
                account.Login,
                "Account registered successfully.");
        }
        catch (DuplicateAccountException)
        {
            return RegisterResult.Fail("Login or email already exists.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[REGISTER DATABASE ERROR] {ex.Message}");
            return RegisterResult.Fail("Database error while creating account.");
        }
    }

    public LoginResult ValidateLogin(string login, string password)
    {
        login = login.Trim();

        if (login.Length < 3)
            return LoginResult.Fail("Login must contain at least 3 characters.");

        if (password.Length < 3)
            return LoginResult.Fail("Password must contain at least 3 characters.");

        try
        {
            AccountModel? account = _accountRepository.FindByLogin(login);

            if (account == null)
                return LoginResult.Fail("Account not found. Please register first.");

            if (account.IsBanned)
                return LoginResult.Fail("Account is banned.");

            if (!PasswordHasher.VerifyPassword(password, account.PasswordHash))
                return LoginResult.Fail("Invalid password.");

            _accountRepository.UpdateLastLoginAt(account.AccountId);

            return LoginResult.Success(
                account.AccountId,
                account.Login,
                "Login accepted. Welcome to Stalker Online.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[LOGIN DATABASE ERROR] {ex.Message}");
            return LoginResult.Fail("Database error while login.");
        }
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