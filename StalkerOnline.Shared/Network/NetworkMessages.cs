namespace StalkerOnline.Shared.Network;

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