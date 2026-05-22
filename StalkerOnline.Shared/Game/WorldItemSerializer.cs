using StalkerOnline.Shared.Network;

namespace StalkerOnline.Shared.Game;

public static class WorldItemSerializer
{
    public static void WriteSpawnInfo(PacketWriter writer, WorldItemSpawnInfo info)
    {
        writer.WriteInt32(info.WorldObjectId);

        writer.WriteString(info.ItemTemplateId);
        writer.WriteString(info.DisplayName);

        writer.WriteInt32(info.Quantity);

        WriteVector3(writer, info.Position);
        WriteVector3(writer, info.Rotation);
    }

    public static WorldItemSpawnInfo ReadSpawnInfo(PacketReader reader)
    {
        return new WorldItemSpawnInfo
        {
            WorldObjectId = reader.ReadInt32(),

            ItemTemplateId = reader.ReadString(),
            DisplayName = reader.ReadString(),

            Quantity = reader.ReadInt32(),

            Position = ReadVector3(reader),
            Rotation = ReadVector3(reader)
        };
    }

    public static void WriteDespawnInfo(PacketWriter writer, WorldItemDespawnInfo info)
    {
        writer.WriteInt32(info.WorldObjectId);
    }

    public static WorldItemDespawnInfo ReadDespawnInfo(PacketReader reader)
    {
        return new WorldItemDespawnInfo
        {
            WorldObjectId = reader.ReadInt32()
        };
    }

    private static void WriteVector3(PacketWriter writer, NetVector3 vector)
    {
        writer.WriteFloat(vector.X);
        writer.WriteFloat(vector.Y);
        writer.WriteFloat(vector.Z);
    }

    private static NetVector3 ReadVector3(PacketReader reader)
    {
        float x = reader.ReadFloat();
        float y = reader.ReadFloat();
        float z = reader.ReadFloat();

        return new NetVector3(x, y, z);
    }
}