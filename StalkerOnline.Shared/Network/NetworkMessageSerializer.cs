namespace StalkerOnline.Shared.Network;

public static class NetworkMessageSerializer
{
    public static void WriteServerMessage(PacketWriter writer, ServerMessage message)
    {
        writer.WriteString(message.Message);
    }

    public static ServerMessage ReadServerMessage(PacketReader reader)
    {
        return new ServerMessage
        {
            Message = reader.ReadString()
        };
    }

    public static void WriteErrorMessage(PacketWriter writer, ErrorMessage message)
    {
        writer.WriteString(message.Code);
        writer.WriteString(message.Message);
        writer.WriteBool(message.ShouldDisconnect);
    }

    public static ErrorMessage ReadErrorMessage(PacketReader reader)
    {
        return new ErrorMessage
        {
            Code = reader.ReadString(),
            Message = reader.ReadString(),
            ShouldDisconnect = reader.ReadBool()
        };
    }
}