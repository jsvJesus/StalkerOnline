namespace StalkerOnline.Shared.Network;

public sealed class PacketMessage
{
    public PacketType Type { get; }
    public byte[] Payload { get; }

    public PacketMessage(PacketType type, byte[] payload)
    {
        Type = type;
        Payload = payload;
    }
}