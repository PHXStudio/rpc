/* Runtime boundary behaviour.

   The Skip assertions here were rescued from tests/version_compat_simple.cpp
   and tests/version_compatibility_test.cpp -- orphan files that were never
   referenced by any CMakeLists and therefore never built. They held the only
   *negative* Skip coverage in the repository (Skip must fail past the end of
   the buffer), so they were moved here before those files were deleted. */

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "ProtocolMemReader.h"
#include "ProtocolMemWriter.h"

using namespace std;

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
