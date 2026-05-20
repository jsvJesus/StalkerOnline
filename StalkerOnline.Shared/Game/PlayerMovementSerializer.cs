using StalkerOnline.Shared.Network;

namespace StalkerOnline.Shared.Game;

public static class PlayerMovementSerializer
{
    public static void WriteMoveRequest(PacketWriter writer, PlayerMovementInput input)
    {
        WriteVector3(writer, input.MoveDirection);
        WriteVector3(writer, input.Rotation);

        writer.WriteFloat(input.DeltaTime);
    }

    public static PlayerMovementInput ReadMoveRequest(PacketReader reader)
    {
        return new PlayerMovementInput
        {
            MoveDirection = ReadVector3(reader),
            Rotation = ReadVector3(reader),
            DeltaTime = reader.ReadFloat()
        };
    }

    public static void WritePositionUpdate(PacketWriter writer, PlayerPositionUpdate update)
    {
        writer.WriteInt32(update.CharacterId);

        WriteVector3(writer, update.Position);
        WriteVector3(writer, update.Rotation);
    }

    public static PlayerPositionUpdate ReadPositionUpdate(PacketReader reader)
    {
        return new PlayerPositionUpdate
        {
            CharacterId = reader.ReadInt32(),
            Position = ReadVector3(reader),
            Rotation = ReadVector3(reader)
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