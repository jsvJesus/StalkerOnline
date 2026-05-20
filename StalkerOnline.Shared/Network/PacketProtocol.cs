using System.Buffers.Binary;
using System.Net.Sockets;

namespace StalkerOnline.Shared.Network;

public static class PacketProtocol
{
    public const int MaxPacketSize = 1024 * 1024;

    public static async Task SendAsync(
        NetworkStream stream,
        PacketType type,
        byte[] payload,
        CancellationToken cancellationToken = default)
    {
        int bodyLength = 2 + payload.Length;

        if (bodyLength > MaxPacketSize)
            throw new InvalidDataException($"Packet is too large: {bodyLength} bytes.");

        byte[] header = new byte[4];
        BinaryPrimitives.WriteInt32LittleEndian(header, bodyLength);

        byte[] body = new byte[bodyLength];
        BinaryPrimitives.WriteUInt16LittleEndian(body.AsSpan(0, 2), (ushort)type);
        payload.CopyTo(body.AsSpan(2));

        await stream.WriteAsync(header, cancellationToken);
        await stream.WriteAsync(body, cancellationToken);
        await stream.FlushAsync(cancellationToken);
    }

    public static async Task<PacketMessage?> ReceiveAsync(
        NetworkStream stream,
        CancellationToken cancellationToken = default)
    {
        byte[] header = new byte[4];

        bool headerRead = await ReadExactAsync(stream, header, cancellationToken);

        if (!headerRead)
            return null;

        int bodyLength = BinaryPrimitives.ReadInt32LittleEndian(header);

        if (bodyLength < 2)
            throw new InvalidDataException("Invalid packet length.");

        if (bodyLength > MaxPacketSize)
            throw new InvalidDataException($"Packet is too large: {bodyLength} bytes.");

        byte[] body = new byte[bodyLength];

        bool bodyRead = await ReadExactAsync(stream, body, cancellationToken);

        if (!bodyRead)
            return null;

        PacketType type = (PacketType)BinaryPrimitives.ReadUInt16LittleEndian(body.AsSpan(0, 2));
        byte[] payload = body[2..];

        return new PacketMessage(type, payload);
    }

    private static async Task<bool> ReadExactAsync(
        NetworkStream stream,
        byte[] buffer,
        CancellationToken cancellationToken)
    {
        int offset = 0;

        while (offset < buffer.Length)
        {
            int read = await stream.ReadAsync(
                buffer.AsMemory(offset, buffer.Length - offset),
                cancellationToken);

            if (read == 0)
                return false;

            offset += read;
        }

        return true;
    }
}