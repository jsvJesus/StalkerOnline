namespace StalkerOnline.Server.Game;

public sealed class WorldItemDropResult
{
    public bool IsSuccess { get; set; }

    public int DropperSessionId { get; set; }

    public int DropperCharacterId { get; set; }

    public int WorldObjectId { get; set; }

    public int SlotIndex { get; set; }

    public string ItemTemplateId { get; set; } = string.Empty;

    public string DisplayName { get; set; } = string.Empty;

    public int Quantity { get; set; }

    public string Message { get; set; } = string.Empty;

    public static WorldItemDropResult Fail(
        int dropperSessionId,
        int slotIndex,
        string message)
    {
        return new WorldItemDropResult
        {
            IsSuccess = false,
            DropperSessionId = dropperSessionId,
            SlotIndex = slotIndex,
            Message = message
        };
    }
}