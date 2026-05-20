namespace StalkerOnline.Shared.Network;

public sealed class RegisterRequest
{
    public string Login { get; set; } = string.Empty;
    public string Email { get; set; } = string.Empty;
    public string Password { get; set; } = string.Empty;
}

public sealed class RegisterResponse
{
    public bool IsSuccess { get; set; }
    public int AccountId { get; set; }
    public string Login { get; set; } = string.Empty;
    public string Message { get; set; } = string.Empty;
}

public sealed class ServerMessage
{
    public string Message { get; set; } = string.Empty;
}

public sealed class ErrorMessage
{
    public string Code { get; set; } = string.Empty;
    public string Message { get; set; } = string.Empty;
    public bool ShouldDisconnect { get; set; }
}