namespace StalkerOnline.Server.Game;

public sealed class WorldItemPickupResult
{
    public bool IsSuccess { get; set; }

    public int PickerSessionId { get; set; }
    public int PickerCharacterId { get; set; }

    public int WorldObjectId { get; set; }

    public string ItemTemplateId { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;

    public int Quantity { get; set; }

    public string Message { get; set; } = string.Empty;

    public static WorldItemPickupResult Fail(
        int pickerSessionId,
        int worldObjectId,
        string message)
    {
        return new WorldItemPickupResult
        {
            IsSuccess = false,
            PickerSessionId = pickerSessionId,
            WorldObjectId = worldObjectId,
            Message = message
        };
    }
}