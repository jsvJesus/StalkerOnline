namespace StalkerOnline.Shared.Game;

public sealed class DropItemRequest
{
    public int SlotIndex { get; set; }

    public int Quantity { get; set; } = 1;
}

public sealed class DropItemResponse
{
    public bool IsSuccess { get; set; }

    public int WorldObjectId { get; set; }

    public int SlotIndex { get; set; }

    public string ItemTemplateId { get; set; } = string.Empty;

    public string DisplayName { get; set; } = string.Empty;

    public int Quantity { get; set; }

    public string Message { get; set; } = string.Empty;
}