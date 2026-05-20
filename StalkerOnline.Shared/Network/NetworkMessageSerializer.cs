namespace StalkerOnline.Shared.Network;

public static class NetworkMessageSerializer
{
    public static void WriteRegisterRequest(PacketWriter writer, RegisterRequest request)
    {
        writer.WriteString(request.Login);
        writer.WriteString(request.Email);
        writer.WriteString(request.Password);
    }

    public static RegisterRequest ReadRegisterRequest(PacketReader reader)
    {
        return new RegisterRequest
        {
            Login = reader.ReadString(),
            Email = reader.ReadString(),
            Password = reader.ReadString()
        };
    }

    public static void WriteRegisterResponse(PacketWriter writer, RegisterResponse response)
    {
        writer.WriteBool(response.IsSuccess);
        writer.WriteInt32(response.AccountId);
        writer.WriteString(response.Login);
        writer.WriteString(response.Message);
    }

    public static RegisterResponse ReadRegisterResponse(PacketReader reader)
    {
        return new RegisterResponse
        {
            IsSuccess = reader.ReadBool(),
            AccountId = reader.ReadInt32(),
            Login = reader.ReadString(),
            Message = reader.ReadString()
        };
    }

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