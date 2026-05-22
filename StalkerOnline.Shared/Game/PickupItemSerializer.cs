using StalkerOnline.Shared.Network;

namespace StalkerOnline.Shared.Game;

public static class PickupItemSerializer
{
    public static void WriteRequest(PacketWriter writer, PickupItemRequest request)
    {
        writer.WriteInt32(request.WorldObjectId);
    }

    public static PickupItemRequest ReadRequest(PacketReader reader)
    {
        return new PickupItemRequest
        {
            WorldObjectId = reader.ReadInt32()
        };
    }

    public static void WriteResponse(PacketWriter writer, PickupItemResponse response)
    {
        writer.WriteBool(response.IsSuccess);

        writer.WriteInt32(response.WorldObjectId);

        writer.WriteString(response.ItemTemplateId);
        writer.WriteString(response.DisplayName);

        writer.WriteInt32(response.Quantity);

        writer.WriteString(response.Message);
    }

    public static PickupItemResponse ReadResponse(PacketReader reader)
    {
        return new PickupItemResponse
        {
            IsSuccess = reader.ReadBool(),

            WorldObjectId = reader.ReadInt32(),

            ItemTemplateId = reader.ReadString(),
            DisplayName = reader.ReadString(),

            Quantity = reader.ReadInt32(),

            Message = reader.ReadString()
        };
    }
}