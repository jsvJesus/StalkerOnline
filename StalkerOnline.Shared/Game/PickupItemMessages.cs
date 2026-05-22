namespace StalkerOnline.Shared.Game;

public sealed class PickupItemRequest
{
    public int WorldObjectId { get; set; }
}

public sealed class PickupItemResponse
{
    public bool IsSuccess { get; set; }

    public int WorldObjectId { get; set; }

    public string ItemTemplateId { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;

    public int Quantity { get; set; }

    public string Message { get; set; } = string.Empty;
}