namespace StalkerOnline.Server.Services;

public sealed class AccountModel
{
    public int AccountId { get; init; }
    public string Login { get; init; } = string.Empty;
    public string Email { get; init; } = string.Empty;
    public string PasswordHash { get; init; } = string.Empty;

    public DateTime CreatedAtUtc { get; init; }
    public DateTime? LastLoginAtUtc { get; init; }

    public bool IsBanned { get; init; }
    public bool IsAdmin { get; init; }
}