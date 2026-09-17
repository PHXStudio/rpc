/* The wire format defines hard upper bounds; these check the reader actually
   enforces them instead of silently accepting out-of-range data.

   docs/knowledge-base.md §5 states each limit, but until now nothing asserted
   any of them: a reader that ignored maxlen entirely would have passed every
   other test, because the round-trip cases only ever produce in-range values.

   Every limit gets both a boundary case (must be accepted) and an out-of-range
   case (must be rejected). Without the boundary half, a reader that rejected
   everything would satisfy the negative half alone. */

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "ProtocolBytesReader.h"

#include "FullCrossLang.h"
#include "FullTest.h"

using namespace std;

namespace {

template <typename T>
bool accepts(vector<uint8_t> bytes) {
	ProtocolBytesReader r(bytes);
	T value;
	return value.deserialize(&r);
}

/* Append `n` filler bytes: a within-bounds case has to carry the payload the
   reader expects to consume after the length prefix, otherwise it is rejected
   for being truncated rather than for being too long. */
vector<uint8_t> withPayload(vector<uint8_t> head, size_t n, uint8_t fill) {
	head.insert(head.end(), n, fill);
	return head;
}

} // namespace

/* ---------- enum ---------- */

/* An enum travels as one uint8. A value past the last enumerator must fail the
   whole parse rather than be accepted as a garbage label. */
TEST(ProtocolLimit, EnumPastLastEnumeratorIsRejected) {
	// FullCrossLangPayload: i32_ u32_ bool_ color_ s_ b_ i32Array_
	// mask bit 3 = color_, reachable as byte0's 0x10.
	// Color has three members, so 2 is the last valid value.
	EXPECT_TRUE(accepts<FullCrossLangPayload>({ 0x01, 0x10, 0x02 }));

	// One past the end.
	EXPECT_FALSE(accepts<FullCrossLangPayload>({ 0x01, 0x10, 0x03 }));
}

/* ---------- string[N] ---------- */

/* The [N] on a string does not change the encoding -- it is a read-side bound
   only, and exceeding it must be rejected. */
TEST(ProtocolLimit, FixedStringLengthIsEnforced) {
	// StructBase.string1_ is string[32]; mask bit 14 -> byte1's 0x02.
	const vector<uint8_t> atLimit =
		withPayload({ 0x04, 0x00, 0x02, 0x00, 0x00, 0x20 }, 32, 'x');
	EXPECT_TRUE(accepts<StructBase>(atLimit));

	const vector<uint8_t> overLimit = { 0x04, 0x00, 0x02, 0x00, 0x00, 0x21 }; // 33
	EXPECT_FALSE(accepts<StructBase>(overLimit));
}

/* ---------- bytes[N] ---------- */

/* 64 needs a two-byte dynSize (0x40 0x40) because the high 2 bits of the first
   byte carry the length marker. Forgetting that is easy -- it is exactly how
   the first draft of this test failed, reporting "rejected" for a value that
   was actually within bounds. */
TEST(ProtocolLimit, FixedBytesLengthIsEnforced) {
	// StructBase.bytes11_ is bytes[64]; mask bit 31 -> byte3's 0x01.
	const vector<uint8_t> atLimit =
		withPayload({ 0x04, 0x00, 0x00, 0x00, 0x01, 0x40, 0x40 }, 64, 0xAB);
	EXPECT_TRUE(accepts<StructBase>(atLimit));

	const vector<uint8_t> overLimit =
		{ 0x04, 0x00, 0x00, 0x00, 0x01, 0x40, 0x41 }; // 65
	EXPECT_FALSE(accepts<StructBase>(overLimit));
}

/* ---------- array[N] ---------- */

TEST(ProtocolLimit, FixedArrayElementCountIsEnforced) {
	// StructBase.strarray1_ is array[8]<string[16]>; mask bit 29 -> byte3's 0x04.
	// Eight empty elements are eight dynSize(0) bytes.
	const vector<uint8_t> atLimit =
		withPayload({ 0x04, 0x00, 0x00, 0x00, 0x04, 0x08 }, 8, 0x00);
	EXPECT_TRUE(accepts<StructBase>(atLimit));

	const vector<uint8_t> overLimit = { 0x04, 0x00, 0x00, 0x00, 0x04, 0x09 }; // 9
	EXPECT_FALSE(accepts<StructBase>(overLimit));
}

/* The element's own bound is separate from the array's: array[8] of string[16]
   must reject a single 17-byte element even though the element count is fine. */
TEST(ProtocolLimit, ArrayElementStringLengthIsEnforced) {
	const vector<uint8_t> atLimit =
		withPayload({ 0x04, 0x00, 0x00, 0x00, 0x04, 0x01, 0x10 }, 16, 'y');
	EXPECT_TRUE(accepts<StructBase>(atLimit));

	const vector<uint8_t> overLimit =
		withPayload({ 0x04, 0x00, 0x00, 0x00, 0x04, 0x01, 0x11 }, 17, 'y');
	EXPECT_FALSE(accepts<StructBase>(overLimit));
}
