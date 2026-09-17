/* Byte-level golden vectors for the wire format.
 *
 * The rest of the suite only asserts "serialize then deserialize gives the same
 * values back", which is satisfied by any self-consistent encoding -- including
 * a wrong one. These tests pin the actual bytes, so a change to the encoding
 * cannot pass unnoticed, and they are what proves the four backends agree.
 *
 * Every expected byte sequence below was derived by hand from the format
 * specification and then checked against the generator output.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "ProtocolBytesReader.h"
#include "ProtocolBytesWriter.h"

#include "CrossLangTest.h"
#include "FullCrossLang.h"
#include "InteropFull.h"
#include "Nested.h"

namespace {

std::string toHex(const std::vector<uint8_t>& v) {
	static const char* kDigits = "0123456789abcdef";
	std::string s;
	for (size_t i = 0; i < v.size(); i++) {
		if (i) s += ' ';
		s += kDigits[v[i] >> 4];
		s += kDigits[v[i] & 0x0F];
	}
	return s;
}

/** "01 60" -> {0x01, 0x60} */
std::vector<uint8_t> fromHex(const std::string& hex) {
	std::vector<uint8_t> out;
	int hi = -1;
	for (size_t i = 0; i < hex.size(); i++) {
		const char c = hex[i];
		int d;
		if (c >= '0' && c <= '9') d = c - '0';
		else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
		else continue;
		if (hi < 0) hi = d;
		else { out.push_back((uint8_t)((hi << 4) | d)); hi = -1; }
	}
	return out;
}

template <typename T>
void expectGolden(const T& value, const std::string& expectedHex) {
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	value.serialize(&w);

	const std::vector<uint8_t> expected = fromHex(expectedHex);
	EXPECT_EQ(toHex(buf), toHex(expected));
	EXPECT_EQ(buf.size(), expected.size());
}

template <typename T>
void expectRoundtrip(const T& value, const std::string& expectedHex) {
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	value.serialize(&w);
	ASSERT_EQ(toHex(buf), toHex(fromHex(expectedHex)));

	T back;
	ProtocolBytesReader r(buf);
	ASSERT_TRUE(back.deserialize(&r));

	std::vector<uint8_t> again;
	ProtocolBytesWriter w2(again);
	back.serialize(&w2);
	EXPECT_EQ(toHex(again), toHex(buf)) << "re-serializing the decoded value changed the bytes";
}

} // namespace

/* ---------------------------------------------------------------- masks ---- */

/* Three fields -> a one-byte mask; fmLen prefix is 1.
   i32_ is 0 (default) so bit 0 is clear, s_ and b_ are non-empty so bits 1 and
   2 are set: 0x60. The defaulted int32 contributes no payload at all. */
TEST(WireFormatGolden, CrossLangPayload_DefaultIntIsOmitted) {
	CrossLangPayload v;
	v.i32_ = 0;
	v.s_ = "hi";
	v.b_.push_back(1);
	v.b_.push_back(2);

	expectGolden(v, "01 60 02 68 69 02 01 02");
	expectRoundtrip(v, "01 60 02 68 69 02 01 02");
}

/* Same struct with every field non-default: all three mask bits set (0xE0),
   and the int32 now occupies its 4 little-endian bytes. */
TEST(WireFormatGolden, CrossLangPayload_AllFieldsPresent) {
	CrossLangPayload v;
	v.i32_ = 1;
	v.s_ = "hi";
	v.b_.push_back(1);
	v.b_.push_back(2);

	expectGolden(v, "01 e0 01 00 00 00 02 68 69 02 01 02");
	expectRoundtrip(v, "01 e0 01 00 00 00 02 68 69 02 01 02");
}

/* ------------------------------------------------------------- all types --- */

/* Seven fields, all at their defaults: just the mask length and an empty mask. */
TEST(WireFormatGolden, FullCrossLang_AllDefaults) {
	FullCrossLangPayload v;
	expectGolden(v, "01 00");
	expectRoundtrip(v, "01 00");
}

/* Field order is i32_, u32_, bool_, color_, s_, b_, i32Array_ -> mask 0xFE.
   Note what is NOT in the payload: the bool occupies no byte at all, its value
   lives in the mask bit above. The enum is a single byte, and each array
   element is a full 4-byte int32. */
TEST(WireFormatGolden, FullCrossLang_AllFieldsPresent) {
	FullCrossLangPayload v;
	v.i32_ = 1;
	v.u32_ = 2;
	v.bool_ = true;
	v.color_ = Green;
	v.s_ = "hi";
	v.b_.push_back(1);
	v.b_.push_back(2);
	v.i32Array_.push_back(3);
	v.i32Array_.push_back(4);

	expectGolden(v,
		"01 fe"
		" 01 00 00 00"          // i32_  = 1
		" 02 00 00 00"          // u32_  = 2
		                        // bool_ -> mask bit only, no bytes
		" 01"                   // color_ = Green
		" 02 68 69"             // s_   = "hi"
		" 02 01 02"             // b_   = {1,2}
		" 02 03 00 00 00 04 00 00 00"); // i32Array_ = {3,4}
	expectRoundtrip(v,
		"01 fe"
		" 01 00 00 00 02 00 00 00 01 02 68 69 02 01 02 02 03 00 00 00 04 00 00 00");
}

/* ---------------------------------------------------------------- bool ----- */

/* The bool is the one field type whose wire presence differs from every other:
   true sets bit 2 and writes nothing, false clears the bit and also writes
   nothing. Both encode in 2 bytes total. */
TEST(WireFormatGolden, FullCrossLang_BoolHasNoPayloadByte) {
	FullCrossLangPayload t;
	t.bool_ = true;
	expectGolden(t, "01 20");
	expectRoundtrip(t, "01 20");

	FullCrossLangPayload f;
	f.bool_ = false;
	expectGolden(f, "01 00");
	expectRoundtrip(f, "01 00");
}

/* A false bool must still decode as false, and must not consume a byte that was
   never written -- this is the regression guard for reading the mask bit
   directly instead of using it as a guard. */
TEST(WireFormatGolden, FullCrossLang_FalseBoolSurvivesRoundtrip) {
	FullCrossLangPayload v;
	v.bool_ = false;
	v.i32_ = 1;

	expectGolden(v, "01 80 01 00 00 00");

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	v.serialize(&w);

	FullCrossLangPayload back;
	ProtocolBytesReader r(buf);
	ASSERT_TRUE(back.deserialize(&r));
	EXPECT_EQ(back.i32_, 1);
	EXPECT_FALSE(back.bool_);
}

/* ------------------------------------------------------------ integer width */

/* Every integer is written at its declared width, not widened. A reader that
   consumed 8 bytes for the int32 would run off the end of this buffer. */
TEST(WireFormatGolden, IntegerWidthIsDeclaredWidthNotWidened) {
	CrossLangPayload v;
	v.i32_ = 5;

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	v.serialize(&w);

	// fmLen(1) + mask(1) + int32(4) = 6
	ASSERT_EQ(buf.size(), 6u);
	EXPECT_EQ(toHex(buf), std::string("01 80 05 00 00 00"));

	CrossLangPayload back;
	ProtocolBytesReader r(buf);
	ASSERT_TRUE(back.deserialize(&r));
	EXPECT_EQ(back.i32_, 5);
}

/* ------------------------------------------------------- nested struct ---- */

/* A nested struct occupies exactly ONE bit of its parent's mask, and that bit
   is always set -- the sub-struct is never "absent", it is always written in
   place. Emitting nothing for that bit shifts every following field down by
   one position, which is what the Python backend used to do. */

TEST(WireFormatGolden, NestedStructTakesOneMaskBit) {
	Outer v;
	v.in_.x_ = 7;
	v.in_.b_ = true;
	v.y_ = 3;

	// mask 0xC0: bit0 = in_ (always 1), bit1 = y_ != 0.
	// in_ carries its own block: 01 c0 (fmLen + mask x_!=0,b_=true) 07 00 00 00
	expectGolden(v, "01 c0 01 c0 07 00 00 00 03 00 00 00");
	expectRoundtrip(v, "01 c0 01 c0 07 00 00 00 03 00 00 00");
}

/* The nested struct's bit stays set even when the following field is at its
   default -- the sub-struct is still present. */
TEST(WireFormatGolden, NestedStructBitSetWhenSiblingDefaults) {
	Outer v;
	v.in_.x_ = 7;
	v.y_ = 0;

	expectGolden(v, "01 80 01 80 07 00 00 00");
	expectRoundtrip(v, "01 80 01 80 07 00 00 00");
}

/* All defaults: the parent mask still has bit0 set, because in_ is a struct
   and therefore always written. Only y_ is omitted. */
TEST(WireFormatGolden, NestedStructWrittenEvenWhenEmpty) {
	Outer v;
	expectGolden(v, "01 80 01 00");
	expectRoundtrip(v, "01 80 01 00");
}

/* Array elements are a different path: the element writer receives NO field
   mask (fm is None / omitted), because the array's own bit already carries
   presence. Touching a mask here would either crash or double-consume a bit. */
TEST(WireFormatGolden, NestedStructAsArrayElement) {
	Holder h;
	Inner a;
	a.x_ = 1;
	Inner c;
	c.x_ = 2;
	h.items_.push_back(a);
	h.items_.push_back(c);
	h.z_ = 5;

	expectGolden(h,
		"01 c0"                                  // fmLen, mask: items_ non-empty, z_ != 0
		" 02"                                    // dynSize = 2 elements
		" 01 80 01 00 00 00"                     // Inner { x_ = 1, b_ = false }
		" 01 80 02 00 00 00"                     // Inner { x_ = 2 }
		" 05 00 00 00");                         // z_ = 5
	expectRoundtrip(h,
		"01 c0 02 01 80 01 00 00 00 01 80 02 00 00 00 05 00 00 00");

	Holder empty;
	expectGolden(empty, "01 00");
}

/* ---------- InteropFull: the widths the older schemas miss ---------- */

/* One message carrying every scalar width, an enum, a string, bytes, a nested
   struct, an empty struct and two arrays.

   CrossLangTest and FullCrossLang between them never exercise int8/16/64,
   float/double, a nested struct or an array of strings, so a backend that
   widened an integer or dropped a float would still pass every cross-language
   test. This vector pins those widths.

   The identical bytes are asserted by tests/py/interop_test.py and the Go test
   under tests/go/, which is what makes this a shared contract rather than a
   C++-only expectation. */
TEST(WireFormatGolden, InteropPayloadAllFieldsPresent) {
	InteropPayload p;
	p.i8_ = -1;
	p.u8_ = 2;
	p.i16_ = -3;
	p.u16_ = 4;
	p.i32_ = -5;
	p.u32_ = 6;
	p.i64_ = -7;
	p.u64_ = 8;
	p.f32_ = 1.5f;
	p.f64_ = 2.5;
	p.b_ = true;
	p.kind_ = (InteropEnum)1;
	p.text_ = "hi";
	p.blob_.push_back(0xAA);
	p.blob_.push_back(0xBB);
	p.inner_.x_ = 9;
	p.inner_.flag_ = true;
	p.ints_.push_back(1);
	p.ints_.push_back(2);
	p.words_.push_back("a");
	p.words_.push_back("bc");

	expectGolden(p,
		"03 ff ff c0"                 // fmLen=3; 18 fields, all with a non-default value
		" ff"                         // i8_  = -1
		" 02"                         // u8_  = 2
		" fd ff"                      // i16_ = -3
		" 04 00"                      // u16_ = 4
		" fb ff ff ff"                // i32_ = -5
		" 06 00 00 00"                // u32_ = 6
		" f9 ff ff ff ff ff ff ff"    // i64_ = -7
		" 08 00 00 00 00 00 00 00"    // u64_ = 8
		" 00 00 c0 3f"                // f32_ = 1.5
		" 00 00 00 00 00 00 04 40"    // f64_ = 2.5
		// b_ = true has no payload byte: the mask bit carries the value
		" 01"                         // kind_ = IR1
		" 02 68 69"                   // text_ = "hi"
		" 02 aa bb"                   // blob_ = {0xAA, 0xBB}
		" 01 c0 09 00 00 00"          // inner_: own fmLen/mask, x_=9, flag_ is a mask bit
		// empty_: zero fields, so zero bytes -- not even a mask segment
		" 02 01 00 00 00 02 00 00 00" // ints_ = {1, 2}
		" 02 01 61 02 62 63");        // words_ = {"a", "bc"}

	expectRoundtrip(p,
		"03 ff ff c0 ff 02 fd ff 04 00 fb ff ff ff 06 00 00 00"
		" f9 ff ff ff ff ff ff ff 08 00 00 00 00 00 00 00"
		" 00 00 c0 3f 00 00 00 00 00 00 04 40 01 02 68 69 02 aa bb"
		" 01 c0 09 00 00 00 02 01 00 00 00 02 00 00 00 02 01 61 02 62 63");
}

/* A struct with no fields contributes nothing at all. Python's generator used
   to emit a one-byte mask segment here, which made it the only backend whose
   bytes disagreed. */
TEST(WireFormatGolden, InteropEmptyWritesNothing) {
	InteropEmpty e;
	expectGolden(e, "");

	/* A nested empty struct still sets its parent's mask bit -- being present
	   is what the bit means -- but adds no bytes of its own. */
	InteropPayload p;
	p.i32_ = 1;
	expectGolden(p,
		"03 08 03 00"        // fmLen=3; mask bits: i32_ (bit 4), inner_ and empty_ (bits 14,15)
		" 01 00 00 00"       // i32_ = 1
		" 01 00"             // inner_: own mask byte, all fields default
		);                   // empty_: nothing at all
}
