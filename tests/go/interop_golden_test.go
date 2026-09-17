package interopfull

// Cross-language golden check for the InteropFull schema, run against the
// generated Go code.
//
// This file is copied into a build-time module alongside the generated
// InteropFull.go (see tests/CMakeLists.txt), so it compiles and runs the real
// Go output rather than grepping it. The expected bytes are *the same vector*
// asserted by tests/runtime/wire_format_golden_test.cpp and
// tests/py/interop_test.py -- three independently written backends held to one
// hand-derived byte string.

import (
	"encoding/hex"
	"testing"

	"github.com/rpc/runtime"
)

// Every scalar width, an enum, a string, bytes, a nested struct, an empty
// struct and two arrays, all with non-default values. 74 bytes.
const interopGoldenHex = "03ffffc0ff02fdff0400fbffffff06000000f9ffffffffffffff080000000000" +
	"00000000c03f00000000000004400102686902aabb01c0090000000201000000" +
	"02000000020161026263"

// Only i32_ set: the nested and empty structs hold their mask bits but
// contribute no data; the empty one contributes no mask segment either.
const interopSparseHex = "03080300010000000100"

func fillPayload() *InteropPayload {
	p := &InteropPayload{}
	p.I8, p.U8, p.I16, p.U16 = -1, 2, -3, 4
	p.I32, p.U32, p.I64, p.U64 = -5, 6, -7, 8
	p.F32, p.F64, p.B = 1.5, 2.5, true
	p.Kind, p.Text, p.Blob = InteropEnum(1), "hi", []uint8{0xAA, 0xBB}
	p.Inner = InteropInner{X: 9, Flag: true}
	p.Ints, p.Words = []int32{1, 2}, []string{"a", "bc"}
	return p
}

func serialize(t *testing.T, p *InteropPayload) []byte {
	t.Helper()
	w := rpc.NewMemWriter()
	if err := p.Serialize(w); err != nil {
		t.Fatalf("serialize: %v", err)
	}
	return w.Bytes()
}

func TestInteropPayloadGolden(t *testing.T) {
	got := hex.EncodeToString(serialize(t, fillPayload()))
	if got != interopGoldenHex {
		t.Errorf("wire bytes differ\n got: %s\nwant: %s", got, interopGoldenHex)
	}
}

func TestInteropSparseGolden(t *testing.T) {
	p := &InteropPayload{}
	p.I32 = 1
	got := hex.EncodeToString(serialize(t, p))
	if got != interopSparseHex {
		t.Errorf("wire bytes differ\n got: %s\nwant: %s", got, interopSparseHex)
	}
}

// A struct with no fields writes nothing at all.
func TestInteropEmptyWritesNothing(t *testing.T) {
	w := rpc.NewMemWriter()
	e := &InteropEmpty{}
	if err := e.Serialize(w); err != nil {
		t.Fatalf("serialize: %v", err)
	}
	if len(w.Bytes()) != 0 {
		t.Errorf("empty struct must write 0 bytes, wrote % x", w.Bytes())
	}
}

// Decoding the golden bytes and re-encoding must reproduce them exactly.
func TestInteropRoundTrip(t *testing.T) {
	raw, err := hex.DecodeString(interopGoldenHex)
	if err != nil {
		t.Fatalf("bad golden hex: %v", err)
	}

	back := &InteropPayload{}
	r := rpc.NewMemReader(raw)
	if err := back.Deserialize(r); err != nil {
		t.Fatalf("deserialize: %v", err)
	}

	// Field-level check so a silently-dropped field cannot hide behind a
	// matching re-encode.
	if back.I8 != -1 || back.U64 != 8 || back.F64 != 2.5 || !back.B {
		t.Errorf("scalars did not survive: %+v", back)
	}
	if back.Text != "hi" || back.Inner.X != 9 || !back.Inner.Flag {
		t.Errorf("string/nested did not survive: text=%q inner=%+v", back.Text, back.Inner)
	}
	if len(back.Ints) != 2 || back.Ints[0] != 1 || back.Ints[1] != 2 {
		t.Errorf("int array did not survive: %v", back.Ints)
	}
	if len(back.Words) != 2 || back.Words[0] != "a" || back.Words[1] != "bc" {
		t.Errorf("string array did not survive: %v", back.Words)
	}

	again := hex.EncodeToString(serialize(t, back))
	if again != interopGoldenHex {
		t.Errorf("re-encoding changed the bytes\n got: %s\nwant: %s", again, interopGoldenHex)
	}
}

// Non-ASCII text must survive, which requires the string writer and reader to
// agree on encoding rather than copying bytes blindly.
func TestInteropUTF8RoundTrip(t *testing.T) {
	const text = "中文 привет"
	p := &InteropPayload{}
	p.Text = text

	raw := serialize(t, p)
	back := &InteropPayload{}
	if err := back.Deserialize(rpc.NewMemReader(raw)); err != nil {
		t.Fatalf("deserialize: %v", err)
	}
	if back.Text != text {
		t.Errorf("text = %q, want %q", back.Text, text)
	}
}
