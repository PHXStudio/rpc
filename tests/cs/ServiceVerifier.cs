/* Drives the generated C# service code -- stub payloads and the dispatcher that
   consumes them -- against the same byte vectors tests/runtime/service_test.cpp
   pins for C++ and tests/go/service asserts for Go, so all three are held to
   one hand-derived contract.

   Nothing compiled or ran this path before. FullTest.cs had been compiled since
   the InteropVerifier landed, but only its struct serialization; the stub
   methods, the proxy interface and the dispatcher were never executed, which is
   how the Go backend kept a dispatcher that dropped every array parameter.

   Usage: ServiceVerifier
   Exit:  0 ok, 1 one or more checks failed
*/

using System;
using System.Collections.Generic;
using rpc;

public static class ServiceVerifier
{
	private static int failures_ = 0;

	private static void Expect(bool ok, string what)
	{
		if (!ok)
		{
			Console.Error.WriteLine("FAIL: " + what);
			failures_++;
		}
	}

	private static string HexOf(byte[] b)
	{
		return BitConverter.ToString(b).Replace("-", " ").ToLowerInvariant();
	}

	private static void ExpectHex(byte[] got, string want, string what)
	{
		string g = HexOf(got);
		if (g != want)
		{
			Console.Error.WriteLine("FAIL: " + what);
			Console.Error.WriteLine("  got: " + g);
			Console.Error.WriteLine(" want: " + want);
			failures_++;
		}
	}

	// ------------------------------------------------------------ fixtures ---

	/** Captures what each stub call serializes, like the C++ and Go stubs. */
	private sealed class CapturingStub : ServiceBaseStub
	{
		private TestMemWriter w_;

		public byte[] Payload()
		{
			return w_ == null ? new byte[0] : w_.ToArray();
		}

		protected override rpc.IWriter methodBegin()
		{
			w_ = new TestMemWriter();
			return w_;
		}

		protected override void methodEnd() { }
	}

	private sealed class CapturingDerivedStub : DerivedServiceStub
	{
		private TestMemWriter w_;

		public byte[] Payload()
		{
			return w_ == null ? new byte[0] : w_.ToArray();
		}

		protected override rpc.IWriter methodBegin()
		{
			w_ = new TestMemWriter();
			return w_;
		}

		protected override void methodEnd() { }
	}

	/** Implements the whole derived proxy surface and keeps what it was handed,
	    so a shifted parameter shows up as a wrong value. */
	private sealed class RecordingHandler : DerivedServiceProxy
	{
		public readonly List<string> calls = new List<string>();

		public string m1Username = "", m1Password = "";
		public double m3D; public float m3F; public long m3I64; public ulong m3U64;
		public sbyte m5I8; public byte m5U8; public bool m5B; public EnumName m5E;
		public int[] m7Ints; public string[] m7Strs;
		public StructType[] m8Structs;
		public int m9Count;

		public bool method1(string username, string password)
		{
			calls.Add("method1");
			m1Username = username; m1Password = password;
			return true;
		}

		public bool method2(StructBase s) { calls.Add("method2"); return true; }

		public bool method3(double d, float f, long i64, ulong u64)
		{
			calls.Add("method3");
			m3D = d; m3F = f; m3I64 = i64; m3U64 = u64;
			return true;
		}

		public bool method4(int i32, uint u32, short i16, ushort u16)
		{
			calls.Add("method4");
			return true;
		}

		public bool method5(sbyte i8, byte u8, bool b, EnumName e)
		{
			calls.Add("method5");
			m5I8 = i8; m5U8 = u8; m5B = b; m5E = e;
			return true;
		}

		public bool method6(string fixedStr, byte[] data) { calls.Add("method6"); return true; }

		public bool method7(int[] ints, string[] strs)
		{
			calls.Add("method7");
			m7Ints = ints; m7Strs = strs;
			return true;
		}

		public bool method8(StructType[] structs)
		{
			calls.Add("method8");
			m8Structs = structs;
			return true;
		}

		public bool method9(DerivedStruct d) { calls.Add("method9"); m9Count++; return true; }
		public bool method10() { calls.Add("method10"); return true; }
	}

	/** Wraps any IReader and counts Skip calls. The C# dispatcher reaches Skip
	    through rpc.IReader, so a peer's extra mask bytes are stepped over for
	    every reader -- unlike Go, where the skip used to be gated on a concrete
	    reader type and silently vanished for anything else. */
	private sealed class SkipCountingReader : rpc.IReader
	{
		private readonly rpc.IReader inner_;
		public int skipCalls;
		public uint skippedBytes;

		public SkipCountingReader(rpc.IReader inner) { inner_ = inner; }

		public bool read(uint size, out byte[] data, out int startId)
		{
			return inner_.read(size, out data, out startId);
		}

		public bool Skip(uint len)
		{
			skipCalls++;
			skippedBytes += len;
			return inner_.Skip(len);
		}
	}

	private static RecordingHandler Dispatch(byte[] payload)
	{
		RecordingHandler h = new RecordingHandler();
		if (!ServiceBaseDispatcher.dispatch(new TestMemReader(payload), h))
			throw new Exception("dispatch returned false");
		return h;
	}

	// ------------------------------------------------------ payload golden ---

	private static void PayloadGoldens()
	{
		CapturingStub stub = new CapturingStub();

		stub.method5(1, 2, true, EnumName.EN2);
		// pid(2) + fmLen(1) + mask 0xF0 + i8 + u8 + enum. The bool is the mask
		// bit; it occupies no payload byte.
		ExpectHex(stub.Payload(), "04 00 01 f0 01 02 01", "method5 all fields present");

		stub.method5(0, 0, false, EnumName.EN1);
		ExpectHex(stub.Payload(), "04 00 01 00", "method5 all defaults");

		stub.method3(1.5, 2.5f, 8, 9);
		ExpectHex(stub.Payload(),
			"02 00 01 f0" +
			" 00 00 00 00 00 00 f8 3f" +
			" 00 00 20 40" +
			" 08 00 00 00 00 00 00 00" +
			" 09 00 00 00 00 00 00 00",
			"method3 at declared widths");

		stub.method1("alice", "secret");
		ExpectHex(stub.Payload(),
			"00 00 01 c0" +
			" 05 61 6c 69 63 65" +
			" 06 73 65 63 72 65 74",
			"method1 strings");

		StructType[] two = new StructType[2];
		two[0] = new StructType(); two[0].aaa_ = "first"; two[0].bbb_ = 10;
		two[1] = new StructType(); two[1].aaa_ = "second"; two[1].bbb_ = 20;
		stub.method8(two);
		ExpectHex(stub.Payload(),
			"07 00 01 80 02" +
			" 01 c0 05 66 69 72 73 74 0a 00 00 00" +
			" 01 c0 06 73 65 63 6f 6e 64 14 00 00 00",
			"method8 struct array");

		// A method with no parameters carries no mask segment at all.
		CapturingDerivedStub dstub = new CapturingDerivedStub();
		dstub.method10();
		ExpectHex(dstub.Payload(), "09 00", "method10 has no mask segment");
	}

	// ---------------------------------------------------------- roundtrip ---

	private static void RoundTrips()
	{
		CapturingStub stub = new CapturingStub();

		stub.method1("user", "pass");
		RecordingHandler h = Dispatch(stub.Payload());
		Expect(h.m1Username == "user" && h.m1Password == "pass",
			"method1 roundtrip, got \"" + h.m1Username + "\"/\"" + h.m1Password + "\"");

		// Unicode must survive, which requires the reader to treat the payload
		// as bytes rather than as UTF-16 code units.
		const string utf8 = "Привет";
		stub.method1(utf8, "pass");
		h = Dispatch(stub.Payload());
		Expect(h.m1Username == utf8, "method1 utf8 roundtrip, got \"" + h.m1Username + "\"");

		stub.method3(3.14, -2.5f, long.MinValue, ulong.MaxValue);
		h = Dispatch(stub.Payload());
		Expect(h.m3D == 3.14 && h.m3F == -2.5f,
			"method3 floats, got " + h.m3D + "/" + h.m3F);
		Expect(h.m3I64 == long.MinValue && h.m3U64 == ulong.MaxValue,
			"method3 wide integers, got " + h.m3I64 + "/" + h.m3U64);

		stub.method5(-1, 200, true, EnumName.EN3);
		h = Dispatch(stub.Payload());
		Expect(h.m5I8 == -1 && h.m5U8 == 200 && h.m5B && h.m5E == EnumName.EN3,
			"method5 roundtrip");

		// The array parameters are the ones the Go dispatcher dropped entirely;
		// the C# path is asserted here so a regression there is not silent.
		stub.method7(new int[] { -1, 0, 42 }, new string[] { "a", "bb", "ccc" });
		h = Dispatch(stub.Payload());
		Expect(h.m7Ints != null && h.m7Ints.Length == 3 && h.m7Ints[2] == 42,
			"method7 int array roundtrip");
		Expect(h.m7Strs != null && h.m7Strs.Length == 3 && h.m7Strs[1] == "bb",
			"method7 string array roundtrip");

		StructType[] two = new StructType[2];
		two[0] = new StructType(); two[0].aaa_ = "first"; two[0].bbb_ = 10;
		two[1] = new StructType(); two[1].aaa_ = "second"; two[1].bbb_ = 20;
		stub.method8(two);
		h = Dispatch(stub.Payload());
		Expect(h.m8Structs != null && h.m8Structs.Length == 2,
			"method8 struct array roundtrip");
		if (h.m8Structs != null && h.m8Structs.Length == 2)
		{
			Expect(h.m8Structs[0].aaa_ == "first" && h.m8Structs[0].bbb_ == 10,
				"method8 element 0");
			Expect(h.m8Structs[1].aaa_ == "second" && h.m8Structs[1].bbb_ == 20,
				"method8 element 1");
		}

		stub.method7(new int[0], new string[0]);
		h = Dispatch(stub.Payload());
		Expect(h.m7Ints != null && h.m7Ints.Length == 0,
			"method7 empty arrays");
	}

	// ------------------------------------------------- version compatibility ---

	private static void VersionCompatibility()
	{
		// A peer speaking a newer schema: method1 gained parameters, so its
		// field mask is two bytes where this build reads one.
		byte[] newerPeer = new byte[]
		{
			0x00, 0x00,             // pid = 0
			0x02,                   // fmLen = 2
			0xc0, 0x00,             // mask, one byte longer than we know
			0x04, (byte)'u', (byte)'s', (byte)'e', (byte)'r',
			0x04, (byte)'p', (byte)'a', (byte)'s', (byte)'s',
		};

		RecordingHandler h = Dispatch(newerPeer);
		Expect(h.m1Username == "user" && h.m1Password == "pass",
			"newer peer roundtrip, got \"" + h.m1Username + "\"/\"" + h.m1Password + "\"");

		// And the skip must actually reach the reader rather than being
		// optimised away or gated on a concrete reader type.
		SkipCountingReader counting = new SkipCountingReader(new TestMemReader(newerPeer));
		RecordingHandler h2 = new RecordingHandler();
		Expect(ServiceBaseDispatcher.dispatch(counting, h2), "dispatch via counting reader");
		Expect(counting.skipCalls == 1 && counting.skippedBytes == 1,
			"expected one skipped mask byte, got " + counting.skipCalls +
			" call(s)/" + counting.skippedBytes + " byte(s)");
		Expect(h2.m1Username == "user" && h2.m1Password == "pass",
			"counting reader roundtrip");

		// Same-version traffic skips nothing at all.
		byte[] sameVersion = new byte[]
		{
			0x00, 0x00,
			0x01,
			0xc0,
			0x04, (byte)'u', (byte)'s', (byte)'e', (byte)'r',
			0x04, (byte)'p', (byte)'a', (byte)'s', (byte)'s',
		};
		SkipCountingReader none = new SkipCountingReader(new TestMemReader(sameVersion));
		Expect(ServiceBaseDispatcher.dispatch(none, new RecordingHandler()),
			"same-version dispatch");
		Expect(none.skipCalls == 0, "same-version traffic should skip nothing, got " +
			none.skipCalls + " call(s)");

		// The same skip inside a struct body is a separate generator path.
		byte[] newerPeerStruct = new byte[]
		{
			0x07, 0x00,             // pid = 7 (method8)
			0x01, 0x80,             // method mask: the array parameter is present
			0x01,                   // one element
			0x02, 0xc0, 0x00,       // StructType's mask is one byte longer
			0x02, (byte)'h', (byte)'i',
			0x07, 0x00, 0x00, 0x00,
		};
		h = Dispatch(newerPeerStruct);
		Expect(h.m8Structs != null && h.m8Structs.Length == 1,
			"newer peer struct body: element count");
		if (h.m8Structs != null && h.m8Structs.Length == 1)
		{
			Expect(h.m8Structs[0].aaa_ == "hi" && h.m8Structs[0].bbb_ == 7,
				"newer peer struct body, got \"" + h.m8Structs[0].aaa_ + "\"/" +
				h.m8Structs[0].bbb_);
		}
	}

	// ------------------------------------------------------------ errors ---

	private static void ErrorPaths()
	{
		RecordingHandler h = new RecordingHandler();

		// Unknown method id.
		Expect(!ServiceBaseDispatcher.dispatch(new TestMemReader(new byte[] { 0xff, 0xff }), h),
			"an unknown method id must be rejected");

		// Truncated: the pid arrives but the field mask never does.
		Expect(!ServiceBaseDispatcher.dispatch(new TestMemReader(new byte[] { 0x00, 0x00 }), h),
			"a payload ending before the field mask must be rejected");

		// Empty.
		Expect(!ServiceBaseDispatcher.dispatch(new TestMemReader(new byte[0]), h),
			"an empty payload must be rejected");
	}

	public static int Main(string[] args)
	{
		PayloadGoldens();
		RoundTrips();
		VersionCompatibility();
		ErrorPaths();

		if (failures_ > 0)
		{
			Console.Error.WriteLine(failures_ + " check(s) failed");
			return 1;
		}

		Console.WriteLine("ServiceVerifier: all checks passed");
		return 0;
	}
}
