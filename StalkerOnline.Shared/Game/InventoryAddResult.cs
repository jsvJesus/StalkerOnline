namespace StalkerOnline.Shared.Game;

public sealed class InventoryAddResult
{
    public bool IsSuccess { get; set; }
    public bool IsPartialSuccess => AddedQuantity > 0 && RemainingQuantity > 0;

    public string ItemTemplateId { get; set; } = string.Empty;

    public int RequestedQuantity { get; set; }
    public int AddedQuantity { get; set; }
    public int RemainingQuantity { get; set; }

    public string Message { get; set; } = string.Empty;
}