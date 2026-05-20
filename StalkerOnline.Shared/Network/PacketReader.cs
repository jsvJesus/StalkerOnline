using System.Buffers.Binary;
using System.Text;

namespace StalkerOnline.Shared.Network;

public sealed class PacketReader
{
    private readonly byte[] _buffer;
    private int _offset;

    public PacketReader(byte[] buffer)
    {
        _buffer = buffer;
        _offset = 0;
    }

    public bool ReadBool()
    {
        EnsureAvailable(1);

        byte value = _buffer[_offset];
        _offset += 1;

        return value != 0;
    }

    public int ReadInt32()
    {
        EnsureAvailable(4);

        int value = BinaryPrimitives.ReadInt32LittleEndian(_buffer.AsSpan(_offset, 4));
        _offset += 4;

        return value;
    }

    public ushort ReadUInt16()
    {
        EnsureAvailable(2);

        ushort value = BinaryPrimitives.ReadUInt16LittleEndian(_buffer.AsSpan(_offset, 2));
        _offset += 2;

        return value;
    }

    public string ReadString()
    {
        int length = ReadInt32();

        if (length < 0)
            throw new InvalidDataException("String length is negative.");

        EnsureAvailable(length);

        string value = Encoding.UTF8.GetString(_buffer, _offset, length);
        _offset += length;

        return value;
    }

    private void EnsureAvailable(int count)
    {
        if (_offset + count > _buffer.Length)
            throw new EndOfStreamException("Packet payload ended unexpectedly.");
    }
}