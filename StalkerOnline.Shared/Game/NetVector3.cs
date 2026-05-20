namespace StalkerOnline.Shared.Game;

public struct NetVector3
{
    public float X;
    public float Y;
    public float Z;

    public NetVector3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public static NetVector3 Zero => new(0f, 0f, 0f);

    public override string ToString()
    {
        return $"X={X:0.00}, Y={Y:0.00}, Z={Z:0.00}";
    }
}