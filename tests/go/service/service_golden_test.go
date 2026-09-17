package fulltest_test

// Service-path conformance for the Go backend: the stub's method payloads and
// the dispatcher that consumes them. The expected bytes are the same vectors
// tests/runtime/service_test.cpp pins for C++, so the two backends are held to
// one hand-derived contract rather than to their own output.
//
// This is deliberately an *external* test package, and two defects lived here
// precisely because nothing outside the generated package ever touched this
// code:
//
//   - The stub methods and the proxy interface were emitted under their IDL
//     names, which are lower-case, so Go left them unexported: no package other
//     than the generated one could call a stub method or implement a handler.
//     A test in `package fulltest` compiles fine against that and proves
//     nothing; this file sits in `fulltest_test` and fails to build instead.
//
//   - The dispatcher skipped a newer peer's extra field-mask bytes only after
//     type-asserting the reader to *rpc.MemReader, which silently dropped the
//     skip for every other reader -- so the version-compatibility path worked
//     in memory and corrupted arguments over a socket. streamReader below is
//     such a reader.

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"math"
	"strings"
	"testing"

	"fulltest"

	"github.com/rpc/runtime"
)

// ---------------------------------------------------------------- helpers ---

func hexOf(b []byte) string {
	parts := make([]string, len(b))
	for i, v := range b {
		parts[i] = fmt.Sprintf("%02x", v)
	}
	return strings.Join(parts, " ")
}

func expectHex(t *testing.T, got []byte, want string) {
	t.Helper()
	if h := hexOf(got); h != want {
		t.Fatalf("method payload mismatch\n got: %s\nwant: %s", h, want)
	}
}

// ----------------------------------------------------------- streamReader ---

// streamReader is a rpc.ProtocolReader backed by a byte stream rather than by
// MemReader. It reads exactly like MemReader does -- same little-endian widths,
// same dynSize encoding -- and is the closest thing to a socket-backed reader
// that runs without a socket.
type streamReader struct {
	src *bytes.Reader
}

func newStreamReader(b []byte) *streamReader {
	return &streamReader{src: bytes.NewReader(b)}
}

func (s *streamReader) take(n int) ([]byte, error) {
	buf := make([]byte, n)
	if _, err := io.ReadFull(s.src, buf); err != nil {
		return nil, err
	}
	return buf, nil
}

func (s *streamReader) Read(data []byte) (int, error) {
	return io.ReadFull(s.src, data)
}

func (s *streamReader) ReadInt8() (int8, error) {
	b, err := s.take(1)
	if err != nil {
		return 0, err
	}
	return int8(b[0]), nil
}

func (s *streamReader) ReadUint8() (uint8, error) {
	b, err := s.take(1)
	if err != nil {
		return 0, err
	}
	return b[0], nil
}

func (s *streamReader) ReadInt16() (int16, error) {
	b, err := s.take(2)
	if err != nil {
		return 0, err
	}
	return int16(binary.LittleEndian.Uint16(b)), nil
}

func (s *streamReader) ReadUint16() (uint16, error) {
	b, err := s.take(2)
	if err != nil {
		return 0, err
	}
	return binary.LittleEndian.Uint16(b), nil
}

func (s *streamReader) ReadInt32() (int32, error) {
	b, err := s.take(4)
	if err != nil {
		return 0, err
	}
	return int32(binary.LittleEndian.Uint32(b)), nil
}

func (s *streamReader) ReadUint32() (uint32, error) {
	b, err := s.take(4)
	if err != nil {
		return 0, err
	}
	return binary.LittleEndian.Uint32(b), nil
}

func (s *streamReader) ReadInt64() (int64, error) {
	b, err := s.take(8)
	if err != nil {
		return 0, err
	}
	return int64(binary.LittleEndian.Uint64(b)), nil
}

func (s *streamReader) ReadUint64() (uint64, error) {
	b, err := s.take(8)
	if err != nil {
		return 0, err
	}
	return binary.LittleEndian.Uint64(b), nil
}

func (s *streamReader) ReadFloat32() (float32, error) {
	b, err := s.take(4)
	if err != nil {
		return 0, err
	}
	return math.Float32frombits(binary.LittleEndian.Uint32(b)), nil
}

func (s *streamReader) ReadFloat64() (float64, error) {
	b, err := s.take(8)
	if err != nil {
		return 0, err
	}
	return math.Float64frombits(binary.LittleEndian.Uint64(b)), nil
}

func (s *streamReader) ReadBool() (bool, error) {
	b, err := s.ReadUint8()
	if err != nil {
		return false, err
	}
	return b != 0, nil
}

func (s *streamReader) ReadString(maxLen uint32) (string, error) {
	size, err := s.ReadDynSize()
	if err != nil {
		return "", err
	}
	if size > maxLen {
		return "", errors.New("string length exceeds maximum")
	}
	b, err := s.take(int(size))
	if err != nil {
		return "", err
	}
	return string(b), nil
}

func (s *streamReader) ReadDynSize() (uint32, error) {
	b, err := s.ReadUint8()
	if err != nil {
		return 0, err
	}
	n := int((b & 0xC0) >> 6)
	size := uint32(b & 0x3F)
	for i := 0; i < n; i++ {
		more, err := s.ReadUint8()
		if err != nil {
			return 0, err
		}
		size = (size << 8) | uint32(more)
	}
	return size, nil
}

func (s *streamReader) Skip(n uint32) error {
	if int(n) > s.src.Len() {
		return io.EOF
	}
	_, err := s.src.Seek(int64(n), io.SeekCurrent)
	return err
}

// ------------------------------------------------------- stub and handler ---

// captureStub records the bytes each call serializes to, mirroring the
// capturing stub in the C++ suite.
type captureStub struct {
	*fulltest.ServiceBaseStub
	writer *rpc.MemWriter
}

func newCaptureStub() *captureStub {
	s := &captureStub{}
	s.ServiceBaseStub = fulltest.NewServiceBaseStub(
		func() rpc.ProtocolWriter {
			s.writer = rpc.NewMemWriter()
			return s.writer
		},
		func() {},
	)
	return s
}

func (s *captureStub) payload() []byte {
	return s.writer.Bytes()
}

type captureDerivedStub struct {
	*fulltest.DerivedServiceStub
	writer *rpc.MemWriter
}

func newCaptureDerivedStub() *captureDerivedStub {
	s := &captureDerivedStub{}
	s.DerivedServiceStub = fulltest.NewDerivedServiceStub(
		func() rpc.ProtocolWriter {
			s.writer = rpc.NewMemWriter()
			return s.writer
		},
		func() {},
	)
	return s
}

func (s *captureDerivedStub) payload() []byte {
	return s.writer.Bytes()
}

// recordingHandler implements the whole ServiceBaseProxy surface and keeps
// what it was handed, so a shifted parameter shows up as a wrong value rather
// than as a missed call.
type recordingHandler struct {
	calls []string

	m1Username, m1Password string
	m2Value                fulltest.StructBase
	m3D                    float64
	m3F                    float32
	m3I64                  int64
	m3U64                  uint64
	m5I8                   int8
	m5U8                   uint8
	m5B                    bool
	m5E                    fulltest.EnumName
	m7Ints                 []int32
	m7Strs                 []string
	m8Structs              []fulltest.StructType
}

func (h *recordingHandler) Method1(Username string, Password string) error {
	h.calls = append(h.calls, "Method1")
	h.m1Username, h.m1Password = Username, Password
	return nil
}

func (h *recordingHandler) Method2(S fulltest.StructBase) error {
	h.calls = append(h.calls, "Method2")
	h.m2Value = S
	return nil
}

func (h *recordingHandler) Method3(D float64, F float32, I64 int64, U64 uint64) error {
	h.calls = append(h.calls, "Method3")
	h.m3D, h.m3F, h.m3I64, h.m3U64 = D, F, I64, U64
	return nil
}

func (h *recordingHandler) Method4(I32 int32, U32 uint32, I16 int16, U16 uint16) error {
	h.calls = append(h.calls, "Method4")
	return nil
}

func (h *recordingHandler) Method5(I8 int8, U8 uint8, B bool, E fulltest.EnumName) error {
	h.calls = append(h.calls, "Method5")
	h.m5I8, h.m5U8, h.m5B, h.m5E = I8, U8, B, E
	return nil
}

func (h *recordingHandler) Method6(FixedStr string, Data []uint8) error {
	h.calls = append(h.calls, "Method6")
	return nil
}

func (h *recordingHandler) Method7(Ints []int32, Strs []string) error {
	h.calls = append(h.calls, "Method7")
	h.m7Ints, h.m7Strs = Ints, Strs
	return nil
}

func (h *recordingHandler) Method8(Structs []fulltest.StructType) error {
	h.calls = append(h.calls, "Method8")
	h.m8Structs = Structs
	return nil
}

func dispatchTo(t *testing.T, r rpc.ProtocolReader) *recordingHandler {
	t.Helper()
	h := &recordingHandler{}
	d := &fulltest.ServiceBaseDispatcher{}
	if err := d.Dispatch(r, h); err != nil {
		t.Fatalf("Dispatch returned %v", err)
	}
	return h
}

// ------------------------------------------------------- payload golden ----

// These pin the exact bytes. The round-trip cases further down would pass
// against any self-consistent encoding, including one that disagrees with the
// other three backends.

// methodId(2) + fmLen(1) + mask 0xF0 + i8 + u8 + enum. The bool contributes no
// byte at all -- its value is the mask bit above it.
func TestMethodPayloadGolden_AllFieldsPresent(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method5(1, 2, true, fulltest.EN2); err != nil {
		t.Fatalf("Method5: %v", err)
	}
	expectHex(t, stub.payload(), "04 00 01 f0 01 02 01")
}

func TestMethodPayloadGolden_AllDefaultsIsMaskOnly(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method5(0, 0, false, fulltest.EN1); err != nil {
		t.Fatalf("Method5: %v", err)
	}
	expectHex(t, stub.payload(), "04 00 01 00")
}

// A method with no parameters carries no mask segment at all, matching
// C++/C#/Python.
func TestMethodPayloadGolden_ParameterlessMethodHasNoMask(t *testing.T) {
	stub := newCaptureDerivedStub()
	if err := stub.Method10(); err != nil {
		t.Fatalf("Method10: %v", err)
	}
	expectHex(t, stub.payload(), "09 00")
}

// Every numeric is written at its declared width, not widened to 8 bytes.
func TestMethodPayloadGolden_ScalarsAtDeclaredWidth(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method3(1.5, 2.5, 8, 9); err != nil {
		t.Fatalf("Method3: %v", err)
	}
	expectHex(t, stub.payload(),
		"02 00"+
			" 01"+
			" f0"+
			" 00 00 00 00 00 00 f8 3f"+
			" 00 00 20 40"+
			" 08 00 00 00 00 00 00 00"+
			" 09 00 00 00 00 00 00 00")
}

func TestMethodPayloadGolden_Strings(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method1("alice", "secret"); err != nil {
		t.Fatalf("Method1: %v", err)
	}
	expectHex(t, stub.payload(),
		"00 00"+
			" 01"+
			" c0"+
			" 05 61 6c 69 63 65"+
			" 06 73 65 63 72 65 74")
}

// A struct parameter always occupies its mask bit and is always written, even
// when every one of its members is at the default.
func TestMethodPayloadGolden_StructParameterIsAlwaysPresent(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method8([]fulltest.StructType{
		{Aaa: "first", Bbb: 10},
		{Aaa: "second", Bbb: 20},
	}); err != nil {
		t.Fatalf("Method8: %v", err)
	}
	expectHex(t, stub.payload(),
		"07 00"+
			" 01"+
			" 80"+
			" 02"+
			" 01 c0 05 66 69 72 73 74 0a 00 00 00"+
			" 01 c0 06 73 65 63 6f 6e 64 14 00 00 00")
}

func TestMethodPayloadGolden_EmptyStructArrayStillSetsItsBit(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method8(nil); err != nil {
		t.Fatalf("Method8: %v", err)
	}
	expectHex(t, stub.payload(), "07 00 01 00")
}

// ------------------------------------------------------------- roundtrip ---

// Stub writes, dispatcher reads, and the handler sees what went in.
func TestDispatchRoundTrip_Strings(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method1("user", "pass"); err != nil {
		t.Fatalf("Method1: %v", err)
	}

	h := dispatchTo(t, rpc.NewMemReader(stub.payload()))
	if h.m1Username != "user" || h.m1Password != "pass" {
		t.Fatalf("got %q/%q, want \"user\"/\"pass\"", h.m1Username, h.m1Password)
	}
}

// Unicode survives, which requires the runtime to treat the payload as bytes
// rather than as UTF-16 code units.
func TestDispatchRoundTrip_Utf8Strings(t *testing.T) {
	const utf8 = "Привет" // "Privet"

	stub := newCaptureStub()
	if err := stub.Method1(utf8, "pass"); err != nil {
		t.Fatalf("Method1: %v", err)
	}

	h := dispatchTo(t, rpc.NewMemReader(stub.payload()))
	if h.m1Username != utf8 || h.m1Password != "pass" {
		t.Fatalf("got %q/%q, want %q/\"pass\"", h.m1Username, h.m1Password, utf8)
	}
}

func TestDispatchRoundTrip_Scalars(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method3(3.14, -2.5, math.MinInt64, math.MaxUint64); err != nil {
		t.Fatalf("Method3: %v", err)
	}

	h := dispatchTo(t, rpc.NewMemReader(stub.payload()))
	if h.m3D != 3.14 || h.m3F != -2.5 {
		t.Fatalf("got %v/%v, want 3.14/-2.5", h.m3D, h.m3F)
	}
	if h.m3I64 != math.MinInt64 || h.m3U64 != math.MaxUint64 {
		t.Fatalf("got %d/%d, want %d/%d", h.m3I64, h.m3U64, int64(math.MinInt64), uint64(math.MaxUint64))
	}
}

func TestDispatchRoundTrip_Arrays(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method7([]int32{-1, 0, 42}, []string{"a", "bb", "ccc"}); err != nil {
		t.Fatalf("Method7: %v", err)
	}

	h := dispatchTo(t, rpc.NewMemReader(stub.payload()))
	if len(h.m7Ints) != 3 || h.m7Ints[2] != 42 {
		t.Fatalf("got %v, want [-1 0 42]", h.m7Ints)
	}
	if len(h.m7Strs) != 3 || h.m7Strs[1] != "bb" {
		t.Fatalf("got %v, want [a bb ccc]", h.m7Strs)
	}
}

func TestDispatchRoundTrip_StructArray(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method8([]fulltest.StructType{
		{Aaa: "first", Bbb: 10},
		{Aaa: "second", Bbb: 20},
	}); err != nil {
		t.Fatalf("Method8: %v", err)
	}

	h := dispatchTo(t, rpc.NewMemReader(stub.payload()))
	if len(h.m8Structs) != 2 {
		t.Fatalf("got %d structs, want 2", len(h.m8Structs))
	}
	if h.m8Structs[0].Aaa != "first" || h.m8Structs[0].Bbb != 10 {
		t.Fatalf("element 0 = %+v", h.m8Structs[0])
	}
	if h.m8Structs[1].Aaa != "second" || h.m8Structs[1].Bbb != 20 {
		t.Fatalf("element 1 = %+v", h.m8Structs[1])
	}
}

func TestDispatchRoundTrip_EmptyContainers(t *testing.T) {
	stub := newCaptureStub()
	if err := stub.Method7(nil, nil); err != nil {
		t.Fatalf("Method7: %v", err)
	}

	h := dispatchTo(t, rpc.NewMemReader(stub.payload()))
	if len(h.m7Ints) != 0 || len(h.m7Strs) != 0 {
		t.Fatalf("got %v/%v, want both empty", h.m7Ints, h.m7Strs)
	}
}

// ------------------------------------------------- version compatibility ---

// A peer speaking a newer schema writes a longer field mask than this build
// knows about: method1 gains parameters, so fmLen goes 1 -> 2 and one extra
// mask byte trails the ones we read. The dispatcher must step over it, or every
// parameter that follows decodes from the wrong offset.
//
// pid = 0, fmLen = 2, mask = c0 00, then "user" and "pass".
var newerPeerPayload = []byte{
	0x00, 0x00,
	0x02,
	0xc0, 0x00,
	0x04, 'u', 's', 'e', 'r',
	0x04, 'p', 'a', 's', 's',
}

// The memory reader is the case that always worked: the old code asserted on
// *rpc.MemReader, so this reader executed the skip. Kept as the control.
func TestDispatchNewerPeer_MemReader(t *testing.T) {
	h := dispatchTo(t, rpc.NewMemReader(newerPeerPayload))
	if h.m1Username != "user" || h.m1Password != "pass" {
		t.Fatalf("got %q/%q, want \"user\"/\"pass\"", h.m1Username, h.m1Password)
	}
}

// The regression guard. Before Skip moved onto the ProtocolReader interface the
// assertion above failed here, the extra mask byte was left in the stream, and
// this test read username="" and password="user" -- a shifted but perfectly
// well-formed decode, with no error reported anywhere.
func TestDispatchNewerPeer_StreamReader(t *testing.T) {
	h := dispatchTo(t, newStreamReader(newerPeerPayload))
	if h.m1Username != "user" || h.m1Password != "pass" {
		t.Fatalf("got %q/%q, want \"user\"/\"pass\" -- the peer's extra mask byte was not skipped",
			h.m1Username, h.m1Password)
	}
}

// Same reader, same-version peer: nothing to skip, and this passed even before
// the fix. It is here so a failure above cannot be blamed on the reader itself.
func TestDispatchSameVersion_StreamReader(t *testing.T) {
	payload := []byte{
		0x00, 0x00,
		0x01,
		0xc0,
		0x04, 'u', 's', 'e', 'r',
		0x04, 'p', 'a', 's', 's',
	}
	h := dispatchTo(t, newStreamReader(payload))
	if h.m1Username != "user" || h.m1Password != "pass" {
		t.Fatalf("got %q/%q, want \"user\"/\"pass\"", h.m1Username, h.m1Password)
	}
}

// The same skip happens inside a struct body, not only at method parameters --
// the two are separate code paths in the generator, and only one of them had a
// test before.
func TestNewerPeerStructBody_MemReaderAndStreamReader(t *testing.T) {
	// A peer's StructType grew a member, so its own mask is two bytes there
	// while this build reads one.
	payload := []byte{
		0x07, 0x00, // pid = 7 (method8)
		0x01,             // the method's own fmLen
		0x80,             // the array parameter is present
		0x01,             // one element
		0x02,             // StructType's own fmLen = 2
		0xc0, 0x00,       // mask: aaa and bbb set, then a byte we do not know
		0x02, 'h', 'i',   // aaa = "hi"
		0x07, 0x00, 0x00, 0x00, // bbb = 7
	}

	for _, tc := range []struct {
		name string
		r    rpc.ProtocolReader
	}{
		{"MemReader", rpc.NewMemReader(payload)},
		{"StreamReader", newStreamReader(payload)},
	} {
		t.Run(tc.name, func(t *testing.T) {
			h := dispatchTo(t, tc.r)
			if len(h.m8Structs) != 1 {
				t.Fatalf("got %d structs, want 1", len(h.m8Structs))
			}
			if h.m8Structs[0].Aaa != "hi" || h.m8Structs[0].Bbb != 7 {
				t.Fatalf("got %q/%d, want \"hi\"/7",
					h.m8Structs[0].Aaa, h.m8Structs[0].Bbb)
			}
		})
	}
}

// ------------------------------------------------------------ error path ---

func TestDispatchUnknownMethodIsRejected(t *testing.T) {
	payload := []byte{0xff, 0xff}
	d := &fulltest.ServiceBaseDispatcher{}
	err := d.Dispatch(rpc.NewMemReader(payload), &recordingHandler{})
	if !errors.Is(err, rpc.ErrUnknownMethod) {
		t.Fatalf("got %v, want ErrUnknownMethod", err)
	}
}

func TestDispatchTruncatedPayloadIsRejected(t *testing.T) {
	// pid only: the method's field mask never arrives.
	payload := []byte{0x00, 0x00}
	d := &fulltest.ServiceBaseDispatcher{}
	err := d.Dispatch(rpc.NewMemReader(payload), &recordingHandler{})
	if err == nil {
		t.Fatal("expected an error for a payload that ends before the field mask")
	}
}

func TestDispatchEmptyPayloadIsRejected(t *testing.T) {
	d := &fulltest.ServiceBaseDispatcher{}
	err := d.Dispatch(rpc.NewMemReader(nil), &recordingHandler{})
	if err == nil {
		t.Fatal("expected an error for an empty payload")
	}
}
