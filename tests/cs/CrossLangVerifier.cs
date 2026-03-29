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
			"  CrossLangVerifier verify-read <file> <i32> <utf8StrHex> <bytesHex>\n" +
			"  CrossLangVerifier write <file> <i32> <utf8StrHex> <bytesHex>");
		return 2;
	}

	private static int VerifyRead(string path, int expectedI32, string strHex, string bytesHex)
	{
		byte[] raw = File.ReadAllBytes(path);
		var r = new TestMemReader(raw);
		var p = new CrossLangPayload();
		if (!p.deserialize(r))
		{
			Console.Error.WriteLine("deserialize failed");
			return 4;
		}

		string expectedS = Encoding.UTF8.GetString(HexUtil.Parse(strHex));
		byte[] expectedB = HexUtil.Parse(bytesHex);

		if (p.i32_ != expectedI32)
		{
			Console.Error.WriteLine("i32 mismatch: got {0} expected {1}", p.i32_, expectedI32);
			return 5;
		}
		if (p.s_ != expectedS)
		{
			Console.Error.WriteLine("string mismatch");
			return 6;
		}
		if (p.b_ == null || p.b_.Length != expectedB.Length)
		{
			Console.Error.WriteLine("bytes length mismatch");
			return 7;
		}
		for (int i = 0; i < expectedB.Length; i++)
		{
			if (p.b_[i] != expectedB[i])
			{
				Console.Error.WriteLine("bytes mismatch at {0}", i);
				return 8;
			}
		}
		return 0;
	}

	private static int WriteFile(string path, int i32, string strHex, string bytesHex)
	{
		var p = new CrossLangPayload
		{
			i32_ = i32,
			s_ = Encoding.UTF8.GetString(HexUtil.Parse(strHex)),
			b_ = HexUtil.Parse(bytesHex)
		};
		var w = new TestMemWriter();
		p.serialize(w);
		File.WriteAllBytes(path, w.ToArray());
		return 0;
	}

	public static int Main(string[] args)
	{
		try
		{
			if (args.Length < 5)
				return Usage();
			string cmd = args[0];
			string path = args[1];
			int i32 = int.Parse(args[2], CultureInfo.InvariantCulture);
			string strHex = args[3];
			string bytesHex = args[4];

			if (cmd == "verify-read")
				return VerifyRead(path, i32, strHex, bytesHex);
			if (cmd == "write")
				return WriteFile(path, i32, strHex, bytesHex);
			return Usage();
		}
		catch (Exception ex)
		{
			Console.Error.WriteLine(ex);
			return 3;
		}
	}
}
