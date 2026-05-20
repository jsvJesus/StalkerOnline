using StalkerOnline.Shared.Network;

namespace StalkerOnline.Shared.Game;

public static class PlayerStateSerializer
{
    public static void Write(PacketWriter writer, PlayerState state)
    {
        writer.WriteInt32(state.AccountId);
        writer.WriteInt32(state.CharacterId);

        writer.WriteString(state.Nickname);

        WriteVector3(writer, state.Position);
        WriteVector3(writer, state.Rotation);

        writer.WriteFloat(state.Health);
        writer.WriteFloat(state.MaxHealth);

        writer.WriteFloat(state.Stamina);
        writer.WriteFloat(state.MaxStamina);

        writer.WriteFloat(state.Hunger);
        writer.WriteFloat(state.Thirst);

        writer.WriteFloat(state.Radiation);
        writer.WriteFloat(state.Toxicity);

        writer.WriteBool(state.IsAlive);
    }

    public static PlayerState Read(PacketReader reader)
    {
        PlayerState state = new()
        {
            AccountId = reader.ReadInt32(),
            CharacterId = reader.ReadInt32(),

            Nickname = reader.ReadString(),

            Position = ReadVector3(reader),
            Rotation = ReadVector3(reader),

            Health = reader.ReadFloat(),
            MaxHealth = reader.ReadFloat(),

            Stamina = reader.ReadFloat(),
            MaxStamina = reader.ReadFloat(),

            Hunger = reader.ReadFloat(),
            Thirst = reader.ReadFloat(),

            Radiation = reader.ReadFloat(),
            Toxicity = reader.ReadFloat(),

            IsAlive = reader.ReadBool()
        };

        return state;
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