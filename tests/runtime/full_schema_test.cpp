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

// ---------- helpers ----------

template <typename T>
void roundtripStruct(const T& obj) {
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	obj.serialize(&w);

	ProtocolBytesReader r(buf);
	T decoded;
	ASSERT_TRUE(decoded.deserialize(&r)) << "deserialize failed";
}

void fillStructType(StructType& s, const std::string& str, int32_t val) {
	s.aaa_ = str;
	s.bbb_ = val;
}

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
	fillStructType(s.struct_, "nested", 42);
	s.string_  = "hello/world";
	s.string1_ = "fixed32lenstringtest1234";
	s.doubleArray_  = {1.1, 2.2, 3.3};
	s.floatArray_   = {0.5f, -0.5f};
	s.int64Array_   = {100, 200};
	s.uint64Array_  = {1, 2, 3};
	s.int32Array_   = {-1, 0, 1};
	s.uint32Array_  = {10, 20};
	s.int16Array_   = {300, 400};
	s.uint16Array_  = {60000};
	s.int8Array_    = {-1, -2, -3};
	s.uint8Array_   = {200, 210, 220};
	s.boolArray_    = {true, false, true};
	s.strArray_     = {"a", "bb", "ccc"};
	s.enumArray_    = {EN1, EN3, EN2};
	s.structArray_.resize(2);
	fillStructType(s.structArray_[0], "first", 1);
	fillStructType(s.structArray_[1], "second", 2);
	s.strarray1_    = {"a", "bb", "ccc", "dddd", "eeeee", "ffffff", "ggggggg", "hhhhhhhh"};
	s.bytes_        = {0x01, 0x02, 0xDE, 0xAD, 0xBE, 0xEF};
	s.bytes11_      = {0xAA, 0xBB, 0xCC};
}

void expectStructType(const StructType& s, const std::string& str, int32_t val) {
	EXPECT_EQ(s.aaa_, str);
	EXPECT_EQ(s.bbb_, val);
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
	expectStructType(s.struct_, "nested", 42);
	EXPECT_EQ(s.string_, "hello/world");
	EXPECT_EQ(s.string1_, "fixed32lenstringtest1234");

	ASSERT_EQ(s.doubleArray_.size(), 3u);
	EXPECT_DOUBLE_EQ(s.doubleArray_[0], 1.1);
	EXPECT_DOUBLE_EQ(s.doubleArray_[1], 2.2);
	EXPECT_DOUBLE_EQ(s.doubleArray_[2], 3.3);

	ASSERT_EQ(s.floatArray_.size(), 2u);
	EXPECT_FLOAT_EQ(s.floatArray_[0], 0.5f);
	EXPECT_FLOAT_EQ(s.floatArray_[1], -0.5f);

	ASSERT_EQ(s.int64Array_.size(), 2u);
	EXPECT_EQ(s.int64Array_[0], 100);
	EXPECT_EQ(s.int64Array_[1], 200);

	ASSERT_EQ(s.uint64Array_.size(), 3u);
	EXPECT_EQ(s.uint64Array_[0], 1);
	EXPECT_EQ(s.uint64Array_[1], 2);
	EXPECT_EQ(s.uint64Array_[2], 3);

	ASSERT_EQ(s.int32Array_.size(), 3u);
	EXPECT_EQ(s.int32Array_[0], -1);
	EXPECT_EQ(s.int32Array_[1], 0);
	EXPECT_EQ(s.int32Array_[2], 1);

	ASSERT_EQ(s.uint32Array_.size(), 2u);
	EXPECT_EQ(s.uint32Array_[0], 10);
	EXPECT_EQ(s.uint32Array_[1], 20);

	ASSERT_EQ(s.int16Array_.size(), 2u);
	EXPECT_EQ(s.int16Array_[0], 300);
	EXPECT_EQ(s.int16Array_[1], 400);

	ASSERT_EQ(s.uint16Array_.size(), 1u);
	EXPECT_EQ(s.uint16Array_[0], 60000);

	ASSERT_EQ(s.int8Array_.size(), 3u);
	EXPECT_EQ(s.int8Array_[0], -1);
	EXPECT_EQ(s.int8Array_[1], -2);
	EXPECT_EQ(s.int8Array_[2], -3);

	ASSERT_EQ(s.uint8Array_.size(), 3u);
	EXPECT_EQ(s.uint8Array_[0], 200);
	EXPECT_EQ(s.uint8Array_[1], 210);
	EXPECT_EQ(s.uint8Array_[2], 220);

	ASSERT_EQ(s.boolArray_.size(), 3u);
	EXPECT_EQ(s.boolArray_[0], true);
	EXPECT_EQ(s.boolArray_[1], false);
	EXPECT_EQ(s.boolArray_[2], true);

	ASSERT_EQ(s.strArray_.size(), 3u);
	EXPECT_EQ(s.strArray_[0], "a");
	EXPECT_EQ(s.strArray_[1], "bb");
	EXPECT_EQ(s.strArray_[2], "ccc");

	ASSERT_EQ(s.enumArray_.size(), 3u);
	EXPECT_EQ(s.enumArray_[0], EN1);
	EXPECT_EQ(s.enumArray_[1], EN3);
	EXPECT_EQ(s.enumArray_[2], EN2);

	ASSERT_EQ(s.structArray_.size(), 2u);
	expectStructType(s.structArray_[0], "first", 1);
	expectStructType(s.structArray_[1], "second", 2);

	ASSERT_EQ(s.strarray1_.size(), 8u);
	EXPECT_EQ(s.strarray1_[0], "a");
	EXPECT_EQ(s.strarray1_[7], "hhhhhhhh");

	ASSERT_EQ(s.bytes_.size(), 6u);
	EXPECT_EQ(s.bytes_[0], 0x01);
	EXPECT_EQ(s.bytes_[5], 0xEF);

	ASSERT_EQ(s.bytes11_.size(), 3u);
	EXPECT_EQ(s.bytes11_[0], 0xAA);
	EXPECT_EQ(s.bytes11_[2], 0xCC);
}

} // namespace

// ============================================================
// StructType (skipcomp) tests
// ============================================================

TEST(StructType, Roundtrip) {
	StructType original;
	fillStructType(original, "test-aaa", -99);

	roundtripStruct(original);

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	original.serialize(&w);

	ProtocolBytesReader r(buf);
	StructType decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	expectStructType(decoded, "test-aaa", -99);
}

TEST(StructType, EmptyStrings) {
	StructType original;
	original.aaa_ = "";
	original.bbb_ = 0;

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	original.serialize(&w);

	ProtocolBytesReader r(buf);
	StructType decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	EXPECT_EQ(decoded.aaa_, "");
	EXPECT_EQ(decoded.bbb_, 0);
}

// ============================================================
// StructBase (all field types) tests
// ============================================================

TEST(StructBase, FullRoundtrip) {
	StructBase original;
	fillStructBase(original);

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	original.serialize(&w);

	ProtocolBytesReader r(buf);
	StructBase decoded;
	ASSERT_TRUE(decoded.deserialize(&r)) << "deserialize failed";
	expectStructBase(decoded);
}

TEST(StructBase, DefaultsRoundtrip) {
	// All fields at default values — field mask should be all-zero, minimal bytes.
	StructBase original;

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	original.serialize(&w);

	ProtocolBytesReader r(buf);
	StructBase decoded;
	ASSERT_TRUE(decoded.deserialize(&r));

	EXPECT_EQ(decoded.double_, 0.0);
	EXPECT_EQ(decoded.float_, 0.0f);
	EXPECT_EQ(decoded.int32_, 0);
	EXPECT_EQ(decoded.bool_, false);
	EXPECT_EQ(decoded.enum_, (EnumName)0);
	EXPECT_EQ(decoded.string_, "");
	EXPECT_TRUE(decoded.doubleArray_.empty());
	EXPECT_TRUE(decoded.int32Array_.empty());
	EXPECT_TRUE(decoded.bytes_.empty());
}

TEST(StructBase, EmptyArraysRoundtrip) {
	StructBase original;
	original.int32_ = 1; // set one field so buf is not completely empty
	// all arrays stay empty

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	original.serialize(&w);

	ProtocolBytesReader r(buf);
	StructBase decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	EXPECT_EQ(decoded.int32_, 1);
	EXPECT_TRUE(decoded.doubleArray_.empty());
	EXPECT_TRUE(decoded.strArray_.empty());
	EXPECT_TRUE(decoded.enumArray_.empty());
	EXPECT_TRUE(decoded.structArray_.empty());
	EXPECT_TRUE(decoded.bytes_.empty());
}

TEST(StructBase, SingleElementArrays) {
	StructBase original;
	original.int32Array_ = {1};
	original.strArray_ = {"only"};
	original.enumArray_ = {EN3};
	original.structArray_.resize(1);
	fillStructType(original.structArray_[0], "solo", 7);

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	original.serialize(&w);

	ProtocolBytesReader r(buf);
	StructBase decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	ASSERT_EQ(decoded.int32Array_.size(), 1u);
	EXPECT_EQ(decoded.int32Array_[0], 1);
	ASSERT_EQ(decoded.strArray_.size(), 1u);
	EXPECT_EQ(decoded.strArray_[0], "only");
	ASSERT_EQ(decoded.enumArray_.size(), 1u);
	EXPECT_EQ(decoded.enumArray_[0], EN3);
	ASSERT_EQ(decoded.structArray_.size(), 1u);
	expectStructType(decoded.structArray_[0], "solo", 7);
}

// ============================================================
// DerivedStruct (inheritance) tests
// ============================================================

TEST(DerivedStruct, InheritanceRoundtrip) {
	DerivedStruct original;
	fillStructBase(original);
	original.aaa_ = 12345; // derived field

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	original.serialize(&w);

	ProtocolBytesReader r(buf);
	DerivedStruct decoded;
	ASSERT_TRUE(decoded.deserialize(&r)) << "deserialize failed";
	expectStructBase(decoded);
	EXPECT_EQ(decoded.aaa_, 12345);
}

TEST(DerivedStruct, OnlyDerivedField) {
	DerivedStruct original;
	original.aaa_ = -1;

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	original.serialize(&w);

	ProtocolBytesReader r(buf);
	DerivedStruct decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	EXPECT_EQ(decoded.aaa_, -1);
	// base fields stay at default
	EXPECT_EQ(decoded.int32_, 0);
	EXPECT_TRUE(decoded.doubleArray_.empty());
}

// ============================================================
// Enum roundtrip tests
// ============================================================

TEST(EnumField, AllValues) {
	StructBase s;

	for (int i = 0; i < 3; i++) {
		s.enum_ = (EnumName)i;
		std::vector<uint8_t> buf;
		ProtocolBytesWriter w(buf);
		s.serialize(&w);

		ProtocolBytesReader r(buf);
		StructBase decoded;
		ASSERT_TRUE(decoded.deserialize(&r));
		EXPECT_EQ(decoded.enum_, (EnumName)i);
	}
}

// ============================================================
// Fixed-size string tests
// ============================================================

TEST(FixedString, MaxLength) {
	StructType s;
	s.aaa_ = std::string(65535, 'X');
	s.bbb_ = 1;

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	s.serialize(&w);

	ProtocolBytesReader r(buf);
	StructType decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	EXPECT_EQ(decoded.aaa_.length(), 65535u);
	EXPECT_EQ(decoded.bbb_, 1);
}

// ============================================================
// Bytes tests
// ============================================================

TEST(Bytes, LargeBytes) {
	StructBase s;
	s.bytes_.resize(1000, 0xAB);

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	s.serialize(&w);

	ProtocolBytesReader r(buf);
	StructBase decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	ASSERT_EQ(decoded.bytes_.size(), 1000u);
	for (size_t i = 0; i < decoded.bytes_.size(); i++)
		EXPECT_EQ(decoded.bytes_[i], 0xAB);
}

TEST(Bytes, FixedSizeBytes) {
	StructBase s;
	s.bytes11_ = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	s.serialize(&w);

	ProtocolBytesReader r(buf);
	StructBase decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	ASSERT_EQ(decoded.bytes11_.size(), 8u);
	for (int i = 0; i < 8; i++)
		EXPECT_EQ(decoded.bytes11_[i], (uint8_t)i);
}

// ============================================================
// Fixed-size array tests
// ============================================================

TEST(FixedArray, StrArray) {
	StructBase s;
	s.strarray1_ = {"a1", "b2", "c3", "d4", "e5", "f6", "g7", "h8"};

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	s.serialize(&w);

	ProtocolBytesReader r(buf);
	StructBase decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	ASSERT_EQ(decoded.strarray1_.size(), 8u);
	EXPECT_EQ(decoded.strarray1_[0], "a1");
	EXPECT_EQ(decoded.strarray1_[7], "h8");
}

// ============================================================
// UTF-8 string in struct
// ============================================================

TEST(StructBase, Utf8String) {
	StructBase s;
	const std::string utf8 = "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"; // "привет"
	s.string_ = utf8;

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	s.serialize(&w);

	ProtocolBytesReader r(buf);
	StructBase decoded;
	ASSERT_TRUE(decoded.deserialize(&r));
	EXPECT_EQ(decoded.string_, utf8);
}
