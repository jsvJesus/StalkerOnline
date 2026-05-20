using StalkerOnline.Shared.Network;

namespace StalkerOnline.Shared.Game;

public static class PlayerWorldSerializer
{
    public static void WriteSpawnInfo(PacketWriter writer, PlayerSpawnInfo info)
    {
        writer.WriteInt32(info.CharacterId);

        writer.WriteString(info.Nickname);

        WriteVector3(writer, info.Position);
        WriteVector3(writer, info.Rotation);

        writer.WriteFloat(info.Health);
        writer.WriteFloat(info.MaxHealth);

        writer.WriteBool(info.IsAlive);
    }

    public static PlayerSpawnInfo ReadSpawnInfo(PacketReader reader)
    {
        return new PlayerSpawnInfo
        {
            CharacterId = reader.ReadInt32(),

            Nickname = reader.ReadString(),

            Position = ReadVector3(reader),
            Rotation = ReadVector3(reader),

            Health = reader.ReadFloat(),
            MaxHealth = reader.ReadFloat(),

            IsAlive = reader.ReadBool()
        };
    }

    public static void WriteDespawnInfo(PacketWriter writer, PlayerDespawnInfo info)
    {
        writer.WriteInt32(info.CharacterId);
    }

    public static PlayerDespawnInfo ReadDespawnInfo(PacketReader reader)
    {
        return new PlayerDespawnInfo
        {
            CharacterId = reader.ReadInt32()
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