using System.Buffers.Binary;
using System.Text;

namespace StalkerOnline.Shared.Network;

public sealed class PacketWriter
{
    private readonly List<byte> _buffer = new();

    public void WriteBool(bool value)
    {
        _buffer.Add(value ? (byte)1 : (byte)0);
    }

    public void WriteInt32(int value)
    {
        Span<byte> bytes = stackalloc byte[4];
        BinaryPrimitives.WriteInt32LittleEndian(bytes, value);
        _buffer.AddRange(bytes.ToArray());
    }

    public void WriteUInt16(ushort value)
    {
        Span<byte> bytes = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(bytes, value);
        _buffer.AddRange(bytes.ToArray());
    }

    public void WriteFloat(float value)
    {
        Span<byte> bytes = stackalloc byte[4];
        BinaryPrimitives.WriteSingleLittleEndian(bytes, value);
        _buffer.AddRange(bytes.ToArray());
    }

    public void WriteString(string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(value);

        WriteInt32(bytes.Length);
        _buffer.AddRange(bytes);
    }

    public byte[] ToArray()
    {
        return _buffer.ToArray();
    }
}