using StalkerOnline.Shared.Network;

namespace StalkerOnline.Shared.Game;

public static class DropItemSerializer
{
    public static void WriteRequest(PacketWriter writer, DropItemRequest request)
    {
        writer.WriteInt32(request.SlotIndex);
        writer.WriteInt32(request.Quantity);
    }

    public static DropItemRequest ReadRequest(PacketReader reader)
    {
        return new DropItemRequest
        {
            SlotIndex = reader.ReadInt32(),
            Quantity = reader.ReadInt32()
        };
    }

    public static void WriteResponse(PacketWriter writer, DropItemResponse response)
    {
        writer.WriteBool(response.IsSuccess);

        writer.WriteInt32(response.WorldObjectId);
        writer.WriteInt32(response.SlotIndex);

        writer.WriteString(response.ItemTemplateId);
        writer.WriteString(response.DisplayName);

        writer.WriteInt32(response.Quantity);

        writer.WriteString(response.Message);
    }

    public static DropItemResponse ReadResponse(PacketReader reader)
    {
        return new DropItemResponse
        {
            IsSuccess = reader.ReadBool(),

            WorldObjectId = reader.ReadInt32(),
            SlotIndex = reader.ReadInt32(),

            ItemTemplateId = reader.ReadString(),
            DisplayName = reader.ReadString(),

            Quantity = reader.ReadInt32(),

            Message = reader.ReadString()
        };
    }
}