using StalkerOnline.Shared.Network;

namespace StalkerOnline.Shared.Game;

public static class InventorySerializer
{
    private const int MaxItemsPerSnapshot = 512;

    public static void WriteSnapshot(PacketWriter writer, InventorySnapshot snapshot)
    {
        writer.WriteInt32(snapshot.CharacterId);
        writer.WriteInt32(snapshot.Capacity);
        writer.WriteFloat(snapshot.TotalWeight);

        writer.WriteInt32(snapshot.Items.Count);

        foreach (InventoryItem item in snapshot.Items)
        {
            WriteItem(writer, item);
        }
    }

    public static InventorySnapshot ReadSnapshot(PacketReader reader)
    {
        InventorySnapshot snapshot = new()
        {
            CharacterId = reader.ReadInt32(),
            Capacity = reader.ReadInt32(),
            TotalWeight = reader.ReadFloat()
        };

        int itemCount = reader.ReadInt32();

        if (itemCount < 0 || itemCount > MaxItemsPerSnapshot)
            throw new InvalidDataException($"Invalid inventory item count: {itemCount}");

        for (int i = 0; i < itemCount; i++)
        {
            snapshot.Items.Add(ReadItem(reader));
        }

        return snapshot;
    }

    private static void WriteItem(PacketWriter writer, InventoryItem item)
    {
        writer.WriteInt32(item.SlotIndex);
        writer.WriteString(item.ItemTemplateId);
        writer.WriteString(item.DisplayName);
        writer.WriteInt32(item.Quantity);
        writer.WriteInt32(item.MaxStack);
        writer.WriteFloat(item.WeightPerItem);
    }

    private static InventoryItem ReadItem(PacketReader reader)
    {
        return new InventoryItem
        {
            SlotIndex = reader.ReadInt32(),
            ItemTemplateId = reader.ReadString(),
            DisplayName = reader.ReadString(),
            Quantity = reader.ReadInt32(),
            MaxStack = reader.ReadInt32(),
            WeightPerItem = reader.ReadFloat()
        };
    }
}