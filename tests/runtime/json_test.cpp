#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <sstream>

#include "FullTest.h"

namespace {

// ---------- helpers ----------

template <typename T>
void jsonRoundtrip(const T& obj) {
	std::stringstream ss;
	obj.toJson(ss);

	T decoded;
	std::string json = ss.str();
	ASSERT_TRUE(decoded.loadJson(json)) << "Failed to load JSON: " << json;

	std::stringstream ss2;
	decoded.toJson(ss2);
	EXPECT_EQ(ss.str(), ss2.str()) << "JSON roundtrip mismatch";
}

void fillStructType(StructType& s, const std::string& str, int32_t val) {
	s.aaa_ = str;
	s.bbb_ = val;
}

void expectStructType(const StructType& s, const std::string& str, int32_t val) {
	EXPECT_EQ(s.aaa_, str);
	EXPECT_EQ(s.bbb_, val);
}

} // namespace

// ============================================================
// StructType JSON tests
// ============================================================

TEST(JsonTest, StructType_SimpleRoundtrip) {
	StructType original;
	fillStructType(original, "test-aaa", -99);

	std::stringstream ss;
	original.toJson(ss);
	std::string json = ss.str();

	StructType decoded;
	ASSERT_TRUE(decoded.loadJson(json)) << "Failed to load JSON: " << json;
	expectStructType(decoded, "test-aaa", -99);
}

TEST(JsonTest, StructType_EmptyString) {
	StructType original;
	original.aaa_ = "";
	original.bbb_ = 0;

	std::stringstream ss;
	original.toJson(ss);

	StructType decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.aaa_, "");
	EXPECT_EQ(decoded.bbb_, 0);
}

TEST(JsonTest, StructType_Utf8String) {
	StructType s;
	const std::string utf8 = "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"; // "привет"
	s.aaa_ = utf8;
	s.bbb_ = 123;

	std::stringstream ss;
	s.toJson(ss);

	StructType decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.aaa_, utf8);
	EXPECT_EQ(decoded.bbb_, 123);
}

TEST(JsonTest, StructType_SpecialCharacters) {
	StructType s;
	s.aaa_ = "Hello \"World\"\n\t\\";
	s.bbb_ = 42;

	std::stringstream ss;
	s.toJson(ss);

	StructType decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.aaa_, "Hello \"World\"\n\t\\");
	EXPECT_EQ(decoded.bbb_, 42);
}

TEST(JsonTest, StructType_Roundtrip) {
	StructType original;
	fillStructType(original, "roundtrip-test", 456);

	jsonRoundtrip(original);
}

// ============================================================
// Scalar types JSON tests
// ============================================================

TEST(JsonTest, Scalar_Bool) {
	StructBase s;

	// Test true
	s.bool_ = true;
	std::stringstream ss1;
	s.toJson(ss1);

	StructBase decoded1;
	ASSERT_TRUE(decoded1.loadJson(ss1.str()));
	EXPECT_TRUE(decoded1.bool_);

	// Test false
	s.bool_ = false;
	std::stringstream ss2;
	s.toJson(ss2);

	StructBase decoded2;
	ASSERT_TRUE(decoded2.loadJson(ss2.str()));
	EXPECT_FALSE(decoded2.bool_);
}

TEST(JsonTest, Scalar_Int8) {
	StructBase s;
	s.int8_ = -128;

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.int8_, -128);
}

TEST(JsonTest, Scalar_Uint8) {
	StructBase s;
	s.uint8_ = 255;

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.uint8_, 255);
}

TEST(JsonTest, Scalar_Int16) {
	StructBase s;
	s.int16_ = std::numeric_limits<int16_t>::min();

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.int16_, std::numeric_limits<int16_t>::min());
}

TEST(JsonTest, Scalar_Uint16) {
	StructBase s;
	s.uint16_ = std::numeric_limits<uint16_t>::max();

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.uint16_, std::numeric_limits<uint16_t>::max());
}

TEST(JsonTest, Scalar_Int32) {
	StructBase s;
	s.int32_ = -19088743;

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.int32_, -19088743);
}

TEST(JsonTest, Scalar_Uint32) {
	StructBase s;
	s.uint32_ = 4000000000U;

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.uint32_, 4000000000U);
}

TEST(JsonTest, Scalar_Int64) {
	StructBase s;
	s.int64_ = std::numeric_limits<int64_t>::min();

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.int64_, std::numeric_limits<int64_t>::min());
}

TEST(JsonTest, Scalar_Uint64) {
	StructBase s;
	s.uint64_ = std::numeric_limits<uint64_t>::max();

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.uint64_, std::numeric_limits<uint64_t>::max());
}

TEST(JsonTest, Scalar_Float) {
	StructBase s;
	s.float_ = -1.5f;

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_FLOAT_EQ(decoded.float_, -1.5f);
}

TEST(JsonTest, Scalar_Double) {
	StructBase s;
	s.double_ = 3.141592653589793;

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_DOUBLE_EQ(decoded.double_, 3.141592653589793);
}

// ============================================================
// Enum JSON tests
// ============================================================

TEST(JsonTest, Enum_StringFormat) {
	StructBase s;
	s.enum_ = EN2;

	std::stringstream ss;
	s.toJson(ss);
	std::string json = ss.str();

	// Check that enum is serialized as string name
	EXPECT_NE(json.find("EN2"), std::string::npos);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(json));
	EXPECT_EQ(decoded.enum_, EN2);
}

TEST(JsonTest, Enum_AllValues) {
	StructBase s;

	for (int i = 0; i < 3; i++) {
		s.enum_ = (EnumName)i;
		std::stringstream ss;
		s.toJson(ss);

		StructBase decoded;
		ASSERT_TRUE(decoded.loadJson(ss.str()));
		EXPECT_EQ(decoded.enum_, (EnumName)i);
	}
}

TEST(JsonTest, Enum_NumericFormat) {
	// Test loading enum from numeric value
	StructBase s;
	std::string json = "{\"enum_\":1}";

	ASSERT_TRUE(s.loadJson(json));
	EXPECT_EQ(s.enum_, EN2);
}

// ============================================================
// Nested struct JSON tests
// ============================================================

TEST(JsonTest, NestedStruct) {
	StructBase s;
	fillStructType(s.struct_, "nested", 42);

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	expectStructType(decoded.struct_, "nested", 42);
}

// ============================================================
// String JSON tests
// ============================================================

TEST(JsonTest, String_Dynamic) {
	StructBase s;
	s.string_ = "hello/world";

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.string_, "hello/world");
}

TEST(JsonTest, String_FixedLength) {
	StructBase s;
	s.string1_ = "fixed32lenstringtest1234";

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.string1_, "fixed32lenstringtest1234");
}

TEST(JsonTest, String_Empty) {
	StructBase s;
	s.string_ = "";

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.string_, "");
}

// ============================================================
// Array JSON tests
// ============================================================

TEST(JsonTest, Array_Double) {
	StructBase s;
	s.doubleArray_ = {1.1, 2.2, 3.3};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.doubleArray_.size(), 3u);
	EXPECT_DOUBLE_EQ(decoded.doubleArray_[0], 1.1);
	EXPECT_DOUBLE_EQ(decoded.doubleArray_[1], 2.2);
	EXPECT_DOUBLE_EQ(decoded.doubleArray_[2], 3.3);
}

TEST(JsonTest, Array_Float) {
	StructBase s;
	s.floatArray_ = {0.5f, -0.5f};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.floatArray_.size(), 2u);
	EXPECT_FLOAT_EQ(decoded.floatArray_[0], 0.5f);
	EXPECT_FLOAT_EQ(decoded.floatArray_[1], -0.5f);
}

TEST(JsonTest, Array_Int64) {
	StructBase s;
	s.int64Array_ = {100, 200};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.int64Array_.size(), 2u);
	EXPECT_EQ(decoded.int64Array_[0], 100);
	EXPECT_EQ(decoded.int64Array_[1], 200);
}

TEST(JsonTest, Array_Uint64) {
	StructBase s;
	s.uint64Array_ = {1, 2, 3};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.uint64Array_.size(), 3u);
	EXPECT_EQ(decoded.uint64Array_[0], 1);
	EXPECT_EQ(decoded.uint64Array_[1], 2);
	EXPECT_EQ(decoded.uint64Array_[2], 3);
}

TEST(JsonTest, Array_Int32) {
	StructBase s;
	s.int32Array_ = {-1, 0, 1};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.int32Array_.size(), 3u);
	EXPECT_EQ(decoded.int32Array_[0], -1);
	EXPECT_EQ(decoded.int32Array_[1], 0);
	EXPECT_EQ(decoded.int32Array_[2], 1);
}

TEST(JsonTest, Array_Uint32) {
	StructBase s;
	s.uint32Array_ = {10, 20};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.uint32Array_.size(), 2u);
	EXPECT_EQ(decoded.uint32Array_[0], 10);
	EXPECT_EQ(decoded.uint32Array_[1], 20);
}

TEST(JsonTest, Array_Bool) {
	StructBase s;
	s.boolArray_ = {true, false, true};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.boolArray_.size(), 3u);
	EXPECT_EQ(decoded.boolArray_[0], true);
	EXPECT_EQ(decoded.boolArray_[1], false);
	EXPECT_EQ(decoded.boolArray_[2], true);
}

TEST(JsonTest, Array_String) {
	StructBase s;
	s.strArray_ = {"a", "bb", "ccc"};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.strArray_.size(), 3u);
	EXPECT_EQ(decoded.strArray_[0], "a");
	EXPECT_EQ(decoded.strArray_[1], "bb");
	EXPECT_EQ(decoded.strArray_[2], "ccc");
}

TEST(JsonTest, Array_Enum) {
	StructBase s;
	s.enumArray_ = {EN1, EN3, EN2};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.enumArray_.size(), 3u);
	EXPECT_EQ(decoded.enumArray_[0], EN1);
	EXPECT_EQ(decoded.enumArray_[1], EN3);
	EXPECT_EQ(decoded.enumArray_[2], EN2);
}

TEST(JsonTest, Array_Struct) {
	StructBase s;
	s.structArray_.resize(2);
	fillStructType(s.structArray_[0], "first", 1);
	fillStructType(s.structArray_[1], "second", 2);

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.structArray_.size(), 2u);
	expectStructType(decoded.structArray_[0], "first", 1);
	expectStructType(decoded.structArray_[1], "second", 2);
}

TEST(JsonTest, Array_Empty) {
	StructBase s;
	s.int32Array_ = {};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_TRUE(decoded.int32Array_.empty());
}

TEST(JsonTest, Array_SingleElement) {
	StructBase s;
	s.int32Array_ = {42};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.int32Array_.size(), 1u);
	EXPECT_EQ(decoded.int32Array_[0], 42);
}

// ============================================================
// Fixed-size array JSON tests
// ============================================================

TEST(JsonTest, FixedArray_String) {
	StructBase s;
	s.strarray1_ = {"a1", "b2", "c3", "d4", "e5", "f6", "g7", "h8"};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.strarray1_.size(), 8u);
	EXPECT_EQ(decoded.strarray1_[0], "a1");
	EXPECT_EQ(decoded.strarray1_[7], "h8");
}

// ============================================================
// Bytes JSON tests
// ============================================================

TEST(JsonTest, Bytes_Dynamic) {
	StructBase s;
	s.bytes_ = {0x01, 0x02, 0xDE, 0xAD, 0xBE, 0xEF};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.bytes_.size(), 6u);
	EXPECT_EQ(decoded.bytes_[0], 0x01);
	EXPECT_EQ(decoded.bytes_[5], 0xEF);
}

TEST(JsonTest, Bytes_FixedSize) {
	StructBase s;
	s.bytes11_ = {0xAA, 0xBB, 0xCC};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	ASSERT_EQ(decoded.bytes11_.size(), 3u);
	EXPECT_EQ(decoded.bytes11_[0], 0xAA);
	EXPECT_EQ(decoded.bytes11_[2], 0xCC);
}

TEST(JsonTest, Bytes_Empty) {
	StructBase s;
	s.bytes_ = {};

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_TRUE(decoded.bytes_.empty());
}

// ============================================================
// Complex struct JSON tests
// ============================================================

TEST(JsonTest, ComplexStruct_AllFields) {
	StructBase s;
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

	std::stringstream ss;
	s.toJson(ss);

	StructBase decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));

	EXPECT_DOUBLE_EQ(decoded.double_, 3.141592653589793);
	EXPECT_FLOAT_EQ(decoded.float_, -1.5f);
	EXPECT_EQ(decoded.int64_, std::numeric_limits<int64_t>::min());
	EXPECT_EQ(decoded.uint64_, std::numeric_limits<uint64_t>::max());
	EXPECT_EQ(decoded.int32_, -19088743);
	EXPECT_EQ(decoded.uint32_, 4000000000U);
	EXPECT_EQ(decoded.int16_, std::numeric_limits<int16_t>::min());
	EXPECT_EQ(decoded.uint16_, std::numeric_limits<uint16_t>::max());
	EXPECT_EQ(decoded.int8_, -128);
	EXPECT_EQ(decoded.uint8_, 255);
	EXPECT_EQ(decoded.bool_, true);
	EXPECT_EQ(decoded.enum_, EN2);
	expectStructType(decoded.struct_, "nested", 42);
	EXPECT_EQ(decoded.string_, "hello/world");
	EXPECT_EQ(decoded.string1_, "fixed32lenstringtest1234");
}

// ============================================================
// Inheritance JSON tests
// ============================================================

TEST(JsonTest, Inheritance_DerivedStruct) {
	DerivedStruct original;
	original.int32_ = 12345;
	original.aaa_ = 999;

	std::stringstream ss;
	original.toJson(ss);

	DerivedStruct decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.int32_, 12345);
	EXPECT_EQ(decoded.aaa_, 999);
}

TEST(JsonTest, Inheritance_OnlyDerivedField) {
	DerivedStruct original;
	original.aaa_ = -1;

	std::stringstream ss;
	original.toJson(ss);

	DerivedStruct decoded;
	ASSERT_TRUE(decoded.loadJson(ss.str()));
	EXPECT_EQ(decoded.aaa_, -1);
	EXPECT_EQ(decoded.int32_, 0); // base field at default
}

// ============================================================
// JSON format validation tests
// ============================================================

TEST(JsonTest, Format_ValidJsonStructure) {
	StructType s;
	s.aaa_ = "test";
	s.bbb_ = 123;

	std::stringstream ss;
	s.toJson(ss);
	std::string json = ss.str();

	// Check basic JSON structure
	EXPECT_EQ(json[0], '{');
	EXPECT_EQ(json[json.length() - 1], '}');

	// Check field presence
	EXPECT_NE(json.find("\"aaa_\""), std::string::npos);
	EXPECT_NE(json.find("\"bbb_\""), std::string::npos);
	EXPECT_NE(json.find("\"test\""), std::string::npos);
	EXPECT_NE(json.find("123"), std::string::npos);
}

// ============================================================
// Error handling tests
// ============================================================

TEST(JsonTest, Error_InvalidJson) {
	StructBase s;
	std::string invalidJson = "{invalid json}";

	EXPECT_FALSE(s.loadJson(invalidJson));
}

TEST(JsonTest, Error_EmptyString) {
	StructBase s;
	std::string emptyJson = "";

	EXPECT_FALSE(s.loadJson(emptyJson));
}

TEST(JsonTest, Error_TypeMismatch) {
	StructBase s;
	// Try to load a string into an int field
	std::string badJson = "{\"int32_\":\"not_a_number\"}";

	EXPECT_FALSE(s.loadJson(badJson));
}

TEST(JsonTest, Error_UnknownField_Ignored) {
	// Unknown fields should be ignored (lenient parsing)
	StructType s;
	std::string jsonWithUnknownField = "{\"aaa_\":\"test\",\"bbb_\":123,\"unknownField\":456}";

	EXPECT_TRUE(s.loadJson(jsonWithUnknownField));
	EXPECT_EQ(s.aaa_, "test");
	EXPECT_EQ(s.bbb_, 123);
}

// ============================================================
// LoadJson method variant tests
// ============================================================

TEST(JsonTest, LoadJson_Variants) {
	StructType s;
	s.aaa_ = "variant-test";
	s.bbb_ = 789;

	std::stringstream ss;
	s.toJson(ss);
	std::string jsonStr = ss.str();

	// Test loadJson(const char*, size_t)
	StructType decoded1;
	ASSERT_TRUE(decoded1.loadJson(jsonStr.c_str(), jsonStr.length()));
	EXPECT_EQ(decoded1.aaa_, "variant-test");
	EXPECT_EQ(decoded1.bbb_, 789);

	// Test loadJson(const std::string&)
	StructType decoded2;
	ASSERT_TRUE(decoded2.loadJson(jsonStr));
	EXPECT_EQ(decoded2.aaa_, "variant-test");
	EXPECT_EQ(decoded2.bbb_, 789);
}

// ============================================================
// toJson needBracket parameter tests
// ============================================================

TEST(JsonTest, ToJson_NeedBracket) {
	StructType s;
	s.aaa_ = "bracket-test";
	s.bbb_ = 111;

	std::stringstream ss1;
	s.toJson(ss1, true);
	std::string withBracket = ss1.str();
	EXPECT_EQ(withBracket[0], '{');
	EXPECT_EQ(withBracket[withBracket.length() - 1], '}');

	std::stringstream ss2;
	s.toJson(ss2, false);
	std::string withoutBracket = ss2.str();
	EXPECT_NE(withoutBracket[0], '{');
}

// ============================================================
// Roundtrip consistency tests
// ============================================================

TEST(JsonTest, Roundtrip_MultipleCycles) {
	StructBase original;
	original.double_ = 1.234;
	original.int32_ = 5678;
	original.string_ = "multi-cycle";
	original.enum_ = EN3;
	original.int32Array_ = {10, 20, 30};

	std::stringstream ss;
	original.toJson(ss);

	StructBase decoded1;
	ASSERT_TRUE(decoded1.loadJson(ss.str()));

	std::stringstream ss2;
	decoded1.toJson(ss2);

	StructBase decoded2;
	ASSERT_TRUE(decoded2.loadJson(ss2.str()));

	EXPECT_DOUBLE_EQ(decoded2.double_, 1.234);
	EXPECT_EQ(decoded2.int32_, 5678);
	EXPECT_EQ(decoded2.string_, "multi-cycle");
	EXPECT_EQ(decoded2.enum_, EN3);
	ASSERT_EQ(decoded2.int32Array_.size(), 3u);
	EXPECT_EQ(decoded2.int32Array_[0], 10);
}
