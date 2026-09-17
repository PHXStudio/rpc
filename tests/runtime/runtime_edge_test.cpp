/* Runtime boundary behaviour.

   The Skip assertions here were rescued from tests/version_compat_simple.cpp
   and tests/version_compatibility_test.cpp -- orphan files that were never
   referenced by any CMakeLists and therefore never built. They held the only
   *negative* Skip coverage in the repository (Skip must fail past the end of
   the buffer), so they were moved here before those files were deleted. */

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "ProtocolBytesWriter.h"
#include "ProtocolMemReader.h"
#include "ProtocolMemWriter.h"

using namespace std;

namespace {

string toHex(const vector<uint8_t>& v) {
	static const char* kDigits = "0123456789abcdef";
	string s;
	for (size_t i = 0; i < v.size(); i++) {
		if (i) s += ' ';
		s += kDigits[v[i] >> 4];
		s += kDigits[v[i] & 0x0F];
	}
	return s;
}

} // namespace

/* ---------- Skip limits ---------- */

TEST(RuntimeEdge, SkipAdvancesPosition) {
	vector<uint8_t> buffer(64);
	for (size_t i = 0; i < buffer.size(); i++)
		buffer[i] = (uint8_t)(i & 0xFF);

	ProtocolMemReader reader(buffer.data(), buffer.size());
	ASSERT_TRUE(reader.skip(10));

	uint8_t b = 0;
	ASSERT_TRUE(reader.readType(b));
	EXPECT_EQ(b, 10) << "read should resume at the byte after the skipped run";
}

TEST(RuntimeEdge, SkipExactlyToEndSucceeds) {
	vector<uint8_t> buffer(16);

	ProtocolMemReader reader(buffer.data(), buffer.size());
	EXPECT_TRUE(reader.skip(buffer.size())) << "skipping the remaining bytes is legal";

	uint8_t b = 0;
	EXPECT_FALSE(reader.readType(b)) << "nothing left to read";
}

TEST(RuntimeEdge, SkipBeyondEndFails) {
	vector<uint8_t> buffer(16);

	ProtocolMemReader reader(buffer.data(), buffer.size());
	EXPECT_FALSE(reader.skip(buffer.size() + 1));
}

TEST(RuntimeEdge, SkipBeyondEndAfterPartialReadFails) {
	vector<uint8_t> buffer(16);

	ProtocolMemReader reader(buffer.data(), buffer.size());
	ASSERT_TRUE(reader.skip(4));
	// Only 12 bytes remain, so skipping the full size must fail even though it
	// would have succeeded on a fresh reader.
	EXPECT_FALSE(reader.skip(buffer.size()));
}

/* ---------- version compatibility read path ---------- */

/* An older reader knows fewer fields than the writer wrote: it reads the mask
   bytes it understands, skips the rest, and must still land on the field that
   follows the mask. This is the whole point of the length prefix. */
TEST(RuntimeEdge, ReaderSkipsMaskBytesItDoesNotUnderstand) {
	vector<uint8_t> buffer(64);
	memset(buffer.data(), 0, buffer.size());

	const uint8_t writtenFmLen = 10;   // newer version wrote a 10-byte mask
	const uint8_t knownFmLen = 5;      // this reader only understands 5
	buffer[0] = writtenFmLen;
	memset(buffer.data() + 1, 0xFF, writtenFmLen);
	buffer[1 + writtenFmLen] = 42;     // the field that follows the mask

	ProtocolMemReader reader(buffer.data(), buffer.size());

	uint8_t actualFmLen = 0;
	ASSERT_TRUE(reader.readType(actualFmLen));
	ASSERT_EQ(actualFmLen, writtenFmLen);

	uint8_t fmData[10];
	ASSERT_TRUE(reader.read(fmData, knownFmLen));

	ASSERT_TRUE(reader.skip(actualFmLen - knownFmLen));

	uint8_t nextByte = 0;
	ASSERT_TRUE(reader.readType(nextByte));
	EXPECT_EQ(nextByte, 42) << "skip must land exactly on the trailing field";
}

/* ---------- MemWriter overflow ---------- */

/* ProtocolMemWriter writes into a caller-supplied fixed buffer. Writing past
   its end is silently dropped rather than reported: write() returns void, so
   a caller cannot tell a truncated message from a complete one. Pinning the
   current behaviour documents the hazard described in docs/knowledge-base.md. */
TEST(RuntimeEdge, MemWriterOverflowIsSilent) {
	uint8_t buffer[4];
	ProtocolMemWriter writer(buffer, sizeof(buffer));

	const uint8_t four[4] = { 1, 2, 3, 4 };
	writer.write(four, sizeof(four));
	// This one does not fit; the writer neither throws nor reports.
	const uint8_t overflow[4] = { 9, 9, 9, 9 };
	writer.write(overflow, sizeof(overflow));

	EXPECT_EQ(buffer[0], 1);
	EXPECT_EQ(buffer[1], 2);
	EXPECT_EQ(buffer[2], 3);
	EXPECT_EQ(buffer[3], 4) << "overflowing write must not corrupt earlier bytes";
}

/* ---------- byte-level golden vectors ---------- */

/* dynSize is the only variable-length encoding: the high 2 bits of the first
   byte record how many bytes follow, so the value is NOT a plain big-endian
   rendering.

   Every case here was previously covered by round-trip only, and a round-trip
   is satisfied by both `40 40` and `40 00` -- which is precisely how a wrong
   expectation survived unnoticed in the Go suite (see docs/knowledge-base.md).
   Pinning the bytes is what makes the boundary testable. */
TEST(RuntimeEdge, DynSizeBoundaryBytes) {
	struct Case { uint32_t value; const char* hex; };
	const Case cases[] = {
		{ 0x00000000u, "00" },           // 1 byte:  0nnnnnnn
		{ 0x0000003Fu, "3f" },           // largest value needing 1 byte
		{ 0x00000040u, "40 40" },        // 2 bytes: 01nnnnnn nnnnnnnn
		{ 0x00003FFFu, "7f ff" },        // largest value needing 2 bytes
		{ 0x00004000u, "80 40 00" },     // 3 bytes: 10nnnnnn ...
		{ 0x003FFFFFu, "bf ff ff" },     // largest value needing 3 bytes
		{ 0x00400000u, "c0 40 00 00" },  // 4 bytes: 11nnnnnn ...
		{ 0x3FFFFFFFu, "ff ff ff ff" },  // largest representable value
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		vector<uint8_t> buf;
		ProtocolBytesWriter w(buf);
		w.writeDynSize(cases[i].value);

		EXPECT_EQ(toHex(buf), cases[i].hex)
			<< "value = 0x" << std::hex << cases[i].value;
	}
}

/* Fixed width, little-endian, two's complement -- there is no varint anywhere
   in this protocol. int16/uint16 had no byte assertion before this test, so a
   byte-order slip in either would only have shown up as a cross-language
   mismatch that no schema happened to exercise. */
TEST(RuntimeEdge, IntegerWidthBytes) {
	vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);

	w.writeType((int16_t)-1000);   // 0xFC18 -> low byte first
	w.writeType((uint16_t)0xBEEF);
	w.writeType((int8_t)-1);
	w.writeType((uint8_t)0xFF);

	EXPECT_EQ(toHex(buf), "18 fc ef be ff ff");
}

/* IEEE-754, little-endian. */
TEST(RuntimeEdge, FloatWidthBytes) {
	vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);

	w.writeType((float)1.5f);   // 0x3FC00000
	w.writeType((double)2.5);   // 0x4004000000000000

	EXPECT_EQ(toHex(buf), "00 00 c0 3f 00 00 00 00 00 00 04 40");
}

/* A struct field equal to its default is omitted entirely: the mask bit alone
   says it is absent, so no payload byte appears. bool is never written to the
   payload at all -- its value IS the mask bit. */
TEST(RuntimeEdge, DynSizeReaderRejectsTruncatedInput) {
	// First byte claims two bytes follow, but the buffer ends there.
	const uint8_t truncated[] = { 0x40 };
	ProtocolMemReader r(truncated, sizeof(truncated));

	uint32_t value = 0;
	EXPECT_FALSE(r.readDynSize(value));
}
