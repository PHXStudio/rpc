/* Decodes the InteropFull golden vector with the generated C# code, checks
   every field, and re-encodes it.

   The C++ and Python and Go tests all assert the same byte string; this one
   makes C# a fourth participant in that contract. It also compiles the
   InteropFull.cs output as a side effect, which is the part that matters:
   FullTest.cs was never compiled by anything, and that is how a CS0108 warning
   survived in it unnoticed.

   Usage: InteropVerifier verify-read <file>
   Exit:  0 ok, 2 usage, 3 read, 4 deserialize, 5 exception, 6.. field, 20 re-encode */

using System;
using System.IO;
using rpc;

public static class InteropVerifier
{
	public static int Main(string[] args)
	{
		if (args.Length != 2 || args[0] != "verify-read")
		{
			Console.Error.WriteLine("usage: InteropVerifier verify-read <file>");
			return 2;
		}

		byte[] data;
		try
		{
			data = File.ReadAllBytes(args[1]);
		}
		catch (Exception ex)
		{
			Console.Error.WriteLine("read failed: " + ex.Message);
			return 3;
		}

		InteropPayload p = new InteropPayload();
		try
		{
			TestMemReader r = new TestMemReader(data);
			if (!p.deserialize(r))
			{
				Console.Error.WriteLine("deserialize returned false");
				return 4;
			}
		}
		catch (Exception ex)
		{
			Console.Error.WriteLine("deserialize threw: " + ex.Message);
			return 5;
		}

		int rc = CheckFields(p);
		if (rc != 0)
			return rc;

		// Re-encoding the decoded value must reproduce the input exactly.
		TestMemWriter w = new TestMemWriter();
		p.serialize(w);
		byte[] again = w.ToArray();

		if (again.Length != data.Length)
		{
			Console.Error.WriteLine("re-encode length " + again.Length + ", want " + data.Length);
			return 20;
		}
		for (int i = 0; i < data.Length; i++)
		{
			if (again[i] != data[i])
			{
				Console.Error.WriteLine("re-encode differs at byte " + i
					+ ": " + again[i].ToString("x2") + " vs " + data[i].ToString("x2"));
				return 21;
			}
		}

		return 0;
	}

	static int CheckFields(InteropPayload p)
	{
		int rc = 6;
		if (p.i8_ != -1) return Bad("i8_", p.i8_, -1, rc++); rc++;
		if (p.u8_ != 2) return Bad("u8_", p.u8_, 2, rc++); rc++;
		if (p.i16_ != -3) return Bad("i16_", p.i16_, -3, rc++); rc++;
		if (p.u16_ != 4) return Bad("u16_", p.u16_, 4, rc++); rc++;
		if (p.i32_ != -5) return Bad("i32_", p.i32_, -5, rc++); rc++;
		if (p.u32_ != 6) return Bad("u32_", p.u32_, 6, rc++); rc++;
		if (p.i64_ != -7) return Bad("i64_", p.i64_, -7, rc++); rc++;
		if (p.u64_ != 8) return Bad("u64_", p.u64_, 8, rc++); rc++;
		if (p.f32_ != 1.5f) return Bad("f32_", p.f32_, 1.5f, rc++); rc++;
		if (p.f64_ != 2.5) return Bad("f64_", p.f64_, 2.5, rc++); rc++;
		if (!p.b_) return Bad("b_", p.b_, true, rc++); rc++;
		if ((int)p.kind_ != 1) return Bad("kind_", (int)p.kind_, 1, rc++); rc++;
		if (p.text_ != "hi") return Bad("text_", p.text_, "hi", rc++); rc++;

		if (p.blob_ == null || p.blob_.Length != 2 || p.blob_[0] != 0xAA || p.blob_[1] != 0xBB)
			return Bad("blob_", "len=" + (p.blob_ == null ? -1 : p.blob_.Length), "2 bytes AA BB", rc);

		if (p.inner_ == null || p.inner_.x_ != 9 || !p.inner_.flag_)
			return Bad("inner_", "x=" + (p.inner_ == null ? -1 : p.inner_.x_), "x=9 flag=true", rc);

		if (p.ints_ == null || p.ints_.Length != 2 || p.ints_[0] != 1 || p.ints_[1] != 2)
			return Bad("ints_", "len=" + (p.ints_ == null ? -1 : p.ints_.Length), "len=2 {1,2}", rc);

		if (p.words_ == null || p.words_.Length != 2 || p.words_[0] != "a" || p.words_[1] != "bc")
			return Bad("words_", "len=" + (p.words_ == null ? -1 : p.words_.Length), "len=2 {a,bc}", rc);

		return 0;
	}

	static int Bad(string field, object got, object want, int rc)
	{
		Console.Error.WriteLine("field " + field + " = " + got + ", want " + want);
		return rc;
	}
}
