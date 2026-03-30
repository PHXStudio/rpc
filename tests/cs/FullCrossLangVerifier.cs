using System;
using System.Globalization;
using System.IO;
using System.Text;
using bin;

internal static class HexUtil
{
    public static byte[] Parse(string hex)
    {
        if (string.IsNullOrEmpty(hex))
            return Array.Empty<byte>();
        if ((hex.Length & 1) != 0)
            throw new ArgumentException("hex length must be even");
        var b = new byte[hex.Length / 2];
        for (int i = 0; i < b.Length; i++)
            b[i] = byte.Parse(hex.AsSpan(i * 2, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture);
        return b;
    }
}

internal static class Program
{
    private static int Usage()
    {
        Console.Error.WriteLine(
            "Usage:\n" +
            "  FullCrossLangVerifier verify-read <file> <i32> <u32> <bool> <color> <utf8StrHex> <bytesHex> <i32ArrayHex>\n" +
            "  FullCrossLangVerifier write <file> <i32> <u32> <bool> <color> <utf8StrHex> <bytesHex> <i32ArrayHex>");
        return 2;
    }

    private static int VerifyRead(string path, int expectedI32, uint expectedU32,
        bool expectedBool, int expectedColor, string strHex, string bytesHex, string i32ArrayHex)
    {
        byte[] raw = File.ReadAllBytes(path);
        var r = new TestMemReader(raw);
        var p = new FullCrossLangPayload();
        if (!p.deserialize(r))
        {
            Console.Error.WriteLine("deserialize failed");
            return 4;
        }

        string expectedS = Encoding.UTF8.GetString(HexUtil.Parse(strHex));
        byte[] expectedB = HexUtil.Parse(bytesHex);
        byte[] arrBytes = HexUtil.Parse(i32ArrayHex);
        int[] expectedArr = new int[arrBytes.Length / 4];
        Buffer.BlockCopy(arrBytes, 0, expectedArr, 0, arrBytes.Length);

        if (p.i32_ != expectedI32)
        {
            Console.Error.WriteLine("i32 mismatch: got {0} expected {1}", p.i32_, expectedI32);
            return 5;
        }
        if (p.u32_ != expectedU32)
        {
            Console.Error.WriteLine("u32 mismatch: got {0} expected {1}", p.u32_, expectedU32);
            return 6;
        }
        if (p.bool_ != expectedBool)
        {
            Console.Error.WriteLine("bool mismatch: got {0} expected {1}", p.bool_, expectedBool);
            return 7;
        }
        if ((int)p.color_ != expectedColor)
        {
            Console.Error.WriteLine("color mismatch: got {0} expected {1}", (int)p.color_, expectedColor);
            return 8;
        }
        if (p.s_ != expectedS)
        {
            Console.Error.WriteLine("string mismatch");
            return 9;
        }
        if (p.b_ == null || p.b_.Length != expectedB.Length)
        {
            Console.Error.WriteLine("bytes length mismatch");
            return 10;
        }
        for (int i = 0; i < expectedB.Length; i++)
        {
            if (p.b_[i] != expectedB[i])
            {
                Console.Error.WriteLine("bytes mismatch at {0}", i);
                return 11;
            }
        }
        if (p.i32Array_ == null || p.i32Array_.Length != expectedArr.Length)
        {
            Console.Error.WriteLine("i32Array length mismatch");
            return 12;
        }
        for (int i = 0; i < expectedArr.Length; i++)
        {
            if (p.i32Array_[i] != expectedArr[i])
            {
                Console.Error.WriteLine("i32Array mismatch at {0}", i);
                return 13;
            }
        }
        return 0;
    }

    private static int WriteFile(string path, int i32, uint u32,
        bool boolVal, int color, string strHex, string bytesHex, string i32ArrayHex)
    {
        var p = new FullCrossLangPayload
        {
            i32_ = i32,
            u32_ = u32,
            bool_ = boolVal,
            color_ = (Color)color,
            s_ = Encoding.UTF8.GetString(HexUtil.Parse(strHex)),
            b_ = HexUtil.Parse(bytesHex)
        };
        byte[] arrBytes = HexUtil.Parse(i32ArrayHex);
        p.i32Array_ = new int[arrBytes.Length / 4];
        Buffer.BlockCopy(arrBytes, 0, p.i32Array_, 0, arrBytes.Length);

        var w = new TestMemWriter();
        p.serialize(w);
        File.WriteAllBytes(path, w.ToArray());
        return 0;
    }

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length < 9)
                return Usage();
            string cmd = args[0];
            string path = args[1];
            int i32 = int.Parse(args[2], CultureInfo.InvariantCulture);
            uint u32 = uint.Parse(args[3], CultureInfo.InvariantCulture);
            bool boolVal = args[4] != "0";
            int color = int.Parse(args[5], CultureInfo.InvariantCulture);
            string strHex = args[6];
            string bytesHex = args[7];
            string i32ArrayHex = args[8];

            if (cmd == "verify-read")
                return VerifyRead(path, i32, u32, boolVal, color, strHex, bytesHex, i32ArrayHex);
            if (cmd == "write")
                return WriteFile(path, i32, u32, boolVal, color, strHex, bytesHex, i32ArrayHex);
            return Usage();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            return 3;
        }
    }
}
