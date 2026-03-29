#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "ProtocolBytesReader.h"
#include "ProtocolBytesWriter.h"
#include "ProtocolReader.h"
#include "ProtocolWriter.h"

namespace {

void roundtripDynSize(uint32_t v) {
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	w.writeDynSize(v);
	ProtocolBytesReader r(buf);
	uint32_t out = 0;
	ASSERT_TRUE(r.readDynSize(out));
	EXPECT_EQ(out, v);
	uint8_t junk = 0;
	EXPECT_FALSE(r.read(&junk, sizeof(junk))); // exhausted
}

void roundtripString(const std::string& s, uint32_t maxlen) {
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	w.writeType(s);
	ProtocolBytesReader r(buf);
	std::string out;
	ASSERT_TRUE(r.readType(out, maxlen));
	EXPECT_EQ(out, s);
}

template <typename T>
void roundtripScalar(const T& v) {
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	w.writeType(v);
	ProtocolBytesReader r(buf);
	T out{};
	ASSERT_TRUE(r.readType(out));
	EXPECT_EQ(out, v);
}

} // namespace

TEST(ProtocolMemory, DynSizeRoundtrip) {
	roundtripDynSize(0);
	roundtripDynSize(1);
	roundtripDynSize(0x3F);
	roundtripDynSize(0x40);
	roundtripDynSize(0x3FFF);
	roundtripDynSize(0x4000);
}

TEST(ProtocolMemory, StringRoundtrip) {
	roundtripString("", 65535);
	roundtripString("ascii", 65535);
	roundtripString(std::string(200, 'z'), 65535);
	// UTF-8 (protocol stores raw bytes after length)
	const std::string u8 = "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"; // "привет"
	roundtripString(u8, 65535);
}

TEST(ProtocolMemory, IntegerScalars) {
	roundtripScalar<int8_t>(-128);
	roundtripScalar<int8_t>(127);
	roundtripScalar<uint8_t>(0);
	roundtripScalar<uint8_t>(255);
	roundtripScalar<int16_t>(std::numeric_limits<int16_t>::min());
	roundtripScalar<int16_t>(std::numeric_limits<int16_t>::max());
	roundtripScalar<uint16_t>(std::numeric_limits<uint16_t>::max());
	roundtripScalar<int32_t>(std::numeric_limits<int32_t>::min());
	roundtripScalar<int32_t>(std::numeric_limits<int32_t>::max());
	roundtripScalar<uint32_t>(std::numeric_limits<uint32_t>::max());
	roundtripScalar<int64_t>(std::numeric_limits<int64_t>::min());
	roundtripScalar<int64_t>(std::numeric_limits<int64_t>::max());
	roundtripScalar<uint64_t>(std::numeric_limits<uint64_t>::max());
}

TEST(ProtocolMemory, FloatDouble) {
	roundtripScalar<float>(0.0f);
	roundtripScalar<float>(-1.5f);
	roundtripScalar<float>(std::numeric_limits<float>::max());
	roundtripScalar<double>(0.0);
	roundtripScalar<double>(3.141592653589793);
	roundtripScalar<double>(std::numeric_limits<double>::lowest());
}

TEST(ProtocolMemory, Bool) {
	roundtripScalar<bool>(false);
	roundtripScalar<bool>(true);
}

TEST(ProtocolMemory, CompositePayload) {
	// Mirrors typical struct field order: several scalars + string.
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	w.writeType(int32_t(-42));
	w.writeType(uint64_t(0xDEADBEEFCAFEBABEULL));
	w.writeType(std::string("rpc"));
	w.writeDynSize(3);
	w.writeType(uint8_t(1));
	w.writeType(uint8_t(2));
	w.writeType(uint8_t(3));

	ProtocolBytesReader r(buf);
	int32_t i32 = 0;
	uint64_t u64 = 0;
	std::string s;
	uint32_t alen = 0;
	uint8_t b0 = 0, b1 = 0, b2 = 0;
	ASSERT_TRUE(r.readType(i32));
	ASSERT_TRUE(r.readType(u64));
	ASSERT_TRUE(r.readType(s, 65535));
	ASSERT_TRUE(r.readDynSize(alen));
	ASSERT_EQ(alen, 3u);
	ASSERT_TRUE(r.readType(b0));
	ASSERT_TRUE(r.readType(b1));
	ASSERT_TRUE(r.readType(b2));
	EXPECT_EQ(i32, -42);
	EXPECT_EQ(u64, 0xDEADBEEFCAFEBABEULL);
	EXPECT_EQ(s, "rpc");
	EXPECT_EQ(b0, 1);
	EXPECT_EQ(b1, 2);
	EXPECT_EQ(b2, 3);
}
