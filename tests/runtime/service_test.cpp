#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "FullTest.h"
#include "ProtocolBytesReader.h"
#include "ProtocolBytesWriter.h"

namespace {

// ---------- Concrete stub: captures serialized bytes ----------
class TestServiceStub : public ServiceBaseStub {
public:
	std::vector<uint8_t> lastBuf;
	uint16_t lastPid = 0;

protected:
	ProtocolWriter* methodBegin() override {
		lastBuf.clear();
		writer_ = new ProtocolBytesWriter(lastBuf);
		return writer_;
	}
	void methodEnd() override {
		delete writer_;
		writer_ = nullptr;
		ProtocolBytesReader r(lastBuf);
		r.readType(lastPid);
	}

private:
	ProtocolBytesWriter* writer_ = nullptr;
};

// ---------- Concrete proxy: records deserialized args ----------
struct RecordedCall {
	uint16_t pid = 0xFFFF;
	std::vector<std::string> args;
	// method1
	std::string m1_username, m1_password;
	// method2
	StructBase m2_s;
	// method3
	double m3_d = 0; float m3_f = 0; int64_t m3_i64 = 0; uint64_t m3_u64 = 0;
	// method4
	int32_t m4_i32 = 0; uint32_t m4_u32 = 0; int16_t m4_i16 = 0; uint16_t m4_u16 = 0;
	// method5
	int8_t m5_i8 = 0; uint8_t m5_u8 = 0; bool m5_b = false; EnumName m5_e = (EnumName)0;
	// method6
	std::string m6_fixedStr; std::vector<uint8_t> m6_data;
	// method7
	std::vector<int32_t> m7_ints; std::vector<std::string> m7_strs;
	// method8
	std::vector<StructType> m8_structs;
	// method9 (inherited)
	DerivedStruct m9_d;
};

class TestServiceProxy : public ServiceBaseProxy {
public:
	RecordedCall last;

	bool method1(std::string& username, std::string& password) override {
		last.pid = 0;
		last.m1_username = username;
		last.m1_password = password;
		last.args.push_back("method1");
		return true;
	}
	bool method2(StructBase& s) override {
		last.pid = 1;
		last.m2_s = s;
		last.args.push_back("method2");
		return true;
	}
	bool method3(double d, float f, int64_t i64, uint64_t u64) override {
		last.pid = 2;
		last.m3_d = d; last.m3_f = f; last.m3_i64 = i64; last.m3_u64 = u64;
		last.args.push_back("method3");
		return true;
	}
	bool method4(int32_t i32, uint32_t u32, int16_t i16, uint16_t u16) override {
		last.pid = 3;
		last.m4_i32 = i32; last.m4_u32 = u32; last.m4_i16 = i16; last.m4_u16 = u16;
		last.args.push_back("method4");
		return true;
	}
	bool method5(int8_t i8, uint8_t u8, bool b, EnumName e) override {
		last.pid = 4;
		last.m5_i8 = i8; last.m5_u8 = u8; last.m5_b = b; last.m5_e = e;
		last.args.push_back("method5");
		return true;
	}
	bool method6(std::string& fixedStr, std::vector<uint8_t>& data) override {
		last.pid = 5;
		last.m6_fixedStr = fixedStr;
		last.m6_data = data;
		last.args.push_back("method6");
		return true;
	}
	bool method7(std::vector<int32_t>& ints, std::vector<std::string>& strs) override {
		last.pid = 6;
		last.m7_ints = ints;
		last.m7_strs = strs;
		last.args.push_back("method7");
		return true;
	}
	bool method8(std::vector<StructType>& structs) override {
		last.pid = 7;
		last.m8_structs = structs;
		last.args.push_back("method8");
		return true;
	}
};

class TestDerivedServiceProxy : public DerivedServiceProxy {
public:
	RecordedCall last;
	bool method1(std::string& username, std::string& password) override {
		last.pid = 0; return true;
	}
	bool method2(StructBase& s) override {
		last.pid = 1; return true;
	}
	bool method3(double d, float f, int64_t i64, uint64_t u64) override {
		last.pid = 2; return true;
	}
	bool method4(int32_t i32, uint32_t u32, int16_t i16, uint16_t u16) override {
		last.pid = 3; return true;
	}
	bool method5(int8_t i8, uint8_t u8, bool b, EnumName e) override { return true; }
	bool method6(std::string& fixedStr, std::vector<uint8_t>& data) override { return true; }
	bool method7(std::vector<int32_t>& ints, std::vector<std::string>& strs) override { return true; }
	bool method8(std::vector<StructType>& structs) override { return true; }
	bool method9(DerivedStruct& d) override {
		last.m9_d = d;
		last.pid = 8; return true;
	}
	bool method10() override {
		last.pid = 9; return true;
	}
};

// ---------- Concrete stub for DerivedService ----------
class TestDerivedStub : public DerivedServiceStub {
public:
	std::vector<uint8_t> lastBuf;
	uint16_t lastPid = 0;

protected:
	ProtocolWriter* methodBegin() override {
		lastBuf.clear();
		writer_ = new ProtocolBytesWriter(lastBuf);
		return writer_;
	}
	void methodEnd() override {
		delete writer_;
		writer_ = nullptr;
		ProtocolBytesReader r(lastBuf);
		r.readType(lastPid);
	}

private:
	ProtocolBytesWriter* writer_ = nullptr;
};

void fillStructBase(StructBase& s) {
	s.double_  = 3.141592653589793;
	s.float_   = -1.5f;
	s.int64_   = std::numeric_limits<int64_t>::min();
	s.uint64_  = std::numeric_limits<uint64_t>::max();
	s.int32_   = -19088743;
	s.uint32_  = 4000000000U;
	s.int16_   = std::numeric_limits<int16_t>::min();
	s.uint16_  = std::numeric_limits<uint16_t>::max();
	s.int8_    = -128;
	s.uint8_   = 255;
	s.bool_    = true;
	s.enum_    = EN2;
	s.struct_.aaa_ = "nested";
	s.struct_.bbb_ = 42;
	s.string_  = "hello";
	s.string1_ = "fixed32";
	s.doubleArray_  = {1.1, 2.2};
	s.floatArray_   = {0.5f};
	s.int32Array_   = {-1, 0, 1};
	s.uint8Array_   = {200, 210};
	s.boolArray_    = {true, false};
	s.strArray_     = {"a", "bb"};
	s.enumArray_    = {EN1, EN3};
	s.structArray_.resize(1);
	s.structArray_[0].aaa_ = "x";
	s.structArray_[0].bbb_ = 7;
	s.bytes_        = {0x01, 0x02, 0xDE, 0xAD};
}

void expectStructBase(const StructBase& s) {
	EXPECT_DOUBLE_EQ(s.double_, 3.141592653589793);
	EXPECT_FLOAT_EQ(s.float_, -1.5f);
	EXPECT_EQ(s.int64_, std::numeric_limits<int64_t>::min());
	EXPECT_EQ(s.uint64_, std::numeric_limits<uint64_t>::max());
	EXPECT_EQ(s.int32_, -19088743);
	EXPECT_EQ(s.uint32_, 4000000000U);
	EXPECT_EQ(s.int16_, std::numeric_limits<int16_t>::min());
	EXPECT_EQ(s.uint16_, std::numeric_limits<uint16_t>::max());
	EXPECT_EQ(s.int8_, -128);
	EXPECT_EQ(s.uint8_, 255);
	EXPECT_EQ(s.bool_, true);
	EXPECT_EQ(s.enum_, EN2);
	EXPECT_EQ(s.struct_.aaa_, "nested");
	EXPECT_EQ(s.struct_.bbb_, 42);
	EXPECT_EQ(s.string_, "hello");
	EXPECT_EQ(s.string1_, "fixed32");
	ASSERT_EQ(s.int32Array_.size(), 3u);
	EXPECT_EQ(s.int32Array_[0], -1);
	EXPECT_EQ(s.bytes_.size(), 4u);
	EXPECT_EQ(s.bytes_[0], 0x01);
}

} // namespace

// ============================================================
// method1: strings
// ============================================================

TEST(ServiceStub, Method1_Strings) {
	TestServiceStub stub;
	stub.method1("alice", "secret");
	EXPECT_FALSE(stub.lastBuf.empty());
	EXPECT_EQ(stub.lastPid, 0);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m1_username, "alice");
	EXPECT_EQ(proxy.last.m1_password, "secret");
}

TEST(ServiceStub, Method1_EmptyStrings) {
	TestServiceStub stub;
	stub.method1("", "");

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m1_username, "");
	EXPECT_EQ(proxy.last.m1_password, "");
}

TEST(ServiceStub, Method1_Utf8Strings) {
	TestServiceStub stub;
	const std::string utf8 = "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
	stub.method1(utf8, "pass");

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m1_username, utf8);
	EXPECT_EQ(proxy.last.m1_password, "pass");
}

// ============================================================
// method2: struct
// ============================================================

TEST(ServiceStub, Method2_Struct) {
	TestServiceStub stub;
	StructBase s;
	fillStructBase(s);
	stub.method2(s);

	EXPECT_EQ(stub.lastPid, 1);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	expectStructBase(proxy.last.m2_s);
}

TEST(ServiceStub, Method2_EmptyStruct) {
	TestServiceStub stub;
	StructBase s;
	stub.method2(s);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m2_s.int32_, 0);
	EXPECT_TRUE(proxy.last.m2_s.doubleArray_.empty());
}

// ============================================================
// method3: double, float, int64, uint64
// ============================================================

TEST(ServiceStub, Method3_LargeNumerics) {
	TestServiceStub stub;
	stub.method3(3.14, -2.5f,
	            std::numeric_limits<int64_t>::min(),
	            std::numeric_limits<uint64_t>::max());

	EXPECT_EQ(stub.lastPid, 2);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_DOUBLE_EQ(proxy.last.m3_d, 3.14);
	EXPECT_FLOAT_EQ(proxy.last.m3_f, -2.5f);
	EXPECT_EQ(proxy.last.m3_i64, std::numeric_limits<int64_t>::min());
	EXPECT_EQ(proxy.last.m3_u64, std::numeric_limits<uint64_t>::max());
}

TEST(ServiceStub, Method3_Zeros) {
	TestServiceStub stub;
	stub.method3(0.0, 0.0f, 0, 0);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_DOUBLE_EQ(proxy.last.m3_d, 0.0);
	EXPECT_FLOAT_EQ(proxy.last.m3_f, 0.0f);
	EXPECT_EQ(proxy.last.m3_i64, 0);
	EXPECT_EQ(proxy.last.m3_u64, 0u);
}

// ============================================================
// method4: int32, uint32, int16, uint16
// ============================================================

TEST(ServiceStub, Method4_MediumNumerics) {
	TestServiceStub stub;
	stub.method4(-999999, 3000000000,
	            std::numeric_limits<int16_t>::min(),
	            std::numeric_limits<uint16_t>::max());

	EXPECT_EQ(stub.lastPid, 3);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m4_i32, -999999);
	EXPECT_EQ(proxy.last.m4_u32, 3000000000u);
	EXPECT_EQ(proxy.last.m4_i16, std::numeric_limits<int16_t>::min());
	EXPECT_EQ(proxy.last.m4_u16, std::numeric_limits<uint16_t>::max());
}

// ============================================================
// method5: int8, uint8, bool, enum
// ============================================================

TEST(ServiceStub, Method5_SmallTypesAndEnum) {
	TestServiceStub stub;
	stub.method5(-1, 200, true, EN3);

	EXPECT_EQ(stub.lastPid, 4);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m5_i8, -1);
	EXPECT_EQ(proxy.last.m5_u8, 200);
	EXPECT_EQ(proxy.last.m5_b, true);
	EXPECT_EQ(proxy.last.m5_e, EN3);
}

TEST(ServiceStub, Method5_Defaults) {
	TestServiceStub stub;
	stub.method5(0, 0, false, EN1);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m5_i8, 0);
	EXPECT_EQ(proxy.last.m5_u8, 0);
	EXPECT_EQ(proxy.last.m5_b, false);
	EXPECT_EQ(proxy.last.m5_e, EN1);
}

// ============================================================
// method6: fixed-length string + bytes
// ============================================================

TEST(ServiceStub, Method6_FixedStringAndBytes) {
	TestServiceStub stub;
	std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
	stub.method6("hello", data);

	EXPECT_EQ(stub.lastPid, 5);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m6_fixedStr, "hello");
	ASSERT_EQ(proxy.last.m6_data.size(), 4u);
	EXPECT_EQ(proxy.last.m6_data[0], 0xDE);
	EXPECT_EQ(proxy.last.m6_data[3], 0xEF);
}

TEST(ServiceStub, Method6_Empty) {
	TestServiceStub stub;
	stub.method6("", {});

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m6_fixedStr, "");
	EXPECT_TRUE(proxy.last.m6_data.empty());
}

// ============================================================
// method7: array<int32> + array<string>
// ============================================================

TEST(ServiceStub, Method7_Arrays) {
	TestServiceStub stub;
	stub.method7({-1, 0, 42, 100}, {"a", "bb", "ccc"});

	EXPECT_EQ(stub.lastPid, 6);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	ASSERT_EQ(proxy.last.m7_ints.size(), 4u);
	EXPECT_EQ(proxy.last.m7_ints[2], 42);
	ASSERT_EQ(proxy.last.m7_strs.size(), 3u);
	EXPECT_EQ(proxy.last.m7_strs[1], "bb");
}

TEST(ServiceStub, Method7_EmptyArrays) {
	TestServiceStub stub;
	stub.method7({}, {});

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_TRUE(proxy.last.m7_ints.empty());
	EXPECT_TRUE(proxy.last.m7_strs.empty());
}

// ============================================================
// method8: array<StructType>
// ============================================================

TEST(ServiceStub, Method8_StructArray) {
	TestServiceStub stub;
	std::vector<StructType> structs(2);
	structs[0].aaa_ = "first";
	structs[0].bbb_ = 10;
	structs[1].aaa_ = "second";
	structs[1].bbb_ = 20;
	stub.method8(structs);

	EXPECT_EQ(stub.lastPid, 7);

	ProtocolBytesReader r(stub.lastBuf);
	TestServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	ASSERT_EQ(proxy.last.m8_structs.size(), 2u);
	EXPECT_EQ(proxy.last.m8_structs[0].aaa_, "first");
	EXPECT_EQ(proxy.last.m8_structs[0].bbb_, 10);
	EXPECT_EQ(proxy.last.m8_structs[1].aaa_, "second");
	EXPECT_EQ(proxy.last.m8_structs[1].bbb_, 20);
}

// ============================================================
// DerivedService: method9 (DerivedStruct) + method10 (no args)
// ============================================================

TEST(DerivedServiceStub, Method9_DerivedStruct) {
	TestDerivedStub stub;
	const DerivedStruct d = []{
		DerivedStruct d;
		fillStructBase(d);
		d.aaa_ = 999;
		return d;
	}();
	stub.method9(d);

	EXPECT_EQ(stub.lastPid, 8); // pid=8 for method9

	ProtocolBytesReader r(stub.lastBuf);
	TestDerivedServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
	EXPECT_EQ(proxy.last.m9_d.int32_, -19088743);
	EXPECT_EQ(proxy.last.m9_d.aaa_, 999);
	EXPECT_EQ(proxy.last.m9_d.enum_, EN2);
}

TEST(DerivedServiceStub, Method10_NoArgs) {
	TestDerivedStub stub;
	stub.method10();

	EXPECT_EQ(stub.lastPid, 9); // pid=9 for method10

	ProtocolBytesReader r(stub.lastBuf);
	TestDerivedServiceProxy proxy;
	ASSERT_TRUE(proxy.dispatch(&r));
}

// ============================================================
// Dispatch: invalid pid
// ============================================================

TEST(ServiceProxy, Dispatch_InvalidPid) {
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	uint16_t badPid = 99;
	w.writeType(badPid);

	ProtocolBytesReader r(buf);
	TestServiceProxy proxy;
	EXPECT_FALSE(proxy.dispatch(&r));
}

// ============================================================
// Dispatch: empty buffer
// ============================================================

TEST(ServiceProxy, Dispatch_EmptyBuffer) {
	std::vector<uint8_t> buf;
	ProtocolBytesReader r(buf);
	TestServiceProxy proxy;
	EXPECT_FALSE(proxy.dispatch(&r));
}

// ============================================================
// Dispatch: truncated buffer (pid only, no field mask)
// ============================================================

TEST(ServiceProxy, Dispatch_TruncatedBuffer) {
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	w.writeType((uint16_t)0);

	ProtocolBytesReader r(buf);
	TestServiceProxy proxy;
	EXPECT_FALSE(proxy.dispatch(&r));
}
