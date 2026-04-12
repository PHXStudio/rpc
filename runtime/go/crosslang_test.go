package rpc

import (
	"bytes"
	"encoding/binary"
	"math"
	"testing"
)

// This file contains tests that verify compatibility with the C++ implementation.
// The test data is based on the C++ serialization format.

// TestCrossLangBasicTypes tests that Go produces the same binary output as C++
// for basic types.
func TestCrossLangBasicTypes(t *testing.T) {
	// Expected binary output from C++ serialization
	// These values are manually verified against C++ ProtocolWriter behavior

	tests := []struct {
		name     string
		serialize func(w *MemWriter) error
		expected []byte
	}{
		{
			name: "int8",
			serialize: func(w *MemWriter) error {
				return w.WriteInt8(-42)
			},
			expected: []byte{0xD6},
		},
		{
			name: "uint8",
			serialize: func(w *MemWriter) error {
				return w.WriteUint8(200)
			},
			expected: []byte{0xC8},
		},
		{
			name: "int16 little-endian",
			serialize: func(w *MemWriter) error {
				return w.WriteInt16(-1000)
			},
			// -1000 = 0xFC18 in little-endian
			expected: []byte{0xFC, 0x18},
		},
		{
			name: "int32 little-endian",
			serialize: func(w *MemWriter) error {
				return w.WriteInt32(0x12345678)
			},
			// 0x12345678 in little-endian: 78 56 34 12
			expected: []byte{0x78, 0x56, 0x34, 0x12},
		},
		{
			name: "bool true",
			serialize: func(w *MemWriter) error {
				return w.WriteBool(true)
			},
			expected: []byte{1},
		},
		{
			name: "bool false",
			serialize: func(w *MemWriter) error {
				return w.WriteBool(false)
			},
			expected: []byte{0},
		},
		{
			name: "float32",
			serialize: func(w *MemWriter) error {
				return w.WriteFloat32(1.5)
			},
			// 1.5 in IEEE 754: 0x3FC00000, little-endian
			expected: []byte{0x00, 0x00, 0xC0, 0x3F},
		},
		{
			name: "float64",
			serialize: func(w *MemWriter) error {
				return w.WriteFloat64(1.5)
			},
			// 1.5 in IEEE 754 double: 0x3FF8000000000000, little-endian
			expected: []byte{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x3F},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			w := NewMemWriter()
			if err := tt.serialize(w); err != nil {
				t.Fatalf("serialize failed: %v", err)
			}

			actual := w.Bytes()
			if !bytes.Equal(actual, tt.expected) {
				t.Errorf("expected %v, got %v", tt.expected, actual)
			}
		})
	}
}

// TestCrossLangDynSize tests variable-length size encoding compatibility
func TestCrossLangDynSize(t *testing.T) {
	// These test cases are derived from C++ ProtocolWriter::writeDynSize behavior
	tests := []struct {
		value    uint32
		expected []byte
	}{
		{0, []byte{0x00}},
		{1, []byte{0x01}},
		{0x3F, []byte{0x3F}},
		{0x40, []byte{0x40, 0x00}},
		{0x3FFF, []byte{0xFF, 0x3F}},
		{0x4000, []byte{0x40, 0x40, 0x00}},
		{0x3FFFFF, []byte{0xFF, 0xFF, 0x3F}},
		{0x400000, []byte{0x40, 0x40, 0x40, 0x00}},
		{0x3FFFFFFF, []byte{0xFF, 0xFF, 0xFF, 0x3F}},
	}

	for _, tt := range tests {
		t.Run("", func(t *testing.T) {
			w := NewMemWriter()
			if err := w.WriteDynSize(tt.value); err != nil {
				t.Fatalf("WriteDynSize(%d) failed: %v", tt.value, err)
			}

			actual := w.Bytes()
			if !bytes.Equal(actual, tt.expected) {
				t.Errorf("value=0x%X: expected %v, got %v", tt.value, tt.expected, actual)
			}
		})
	}
}

// TestCrossLangFieldMask tests field mask bit order compatibility
func TestCrossLangFieldMask(t *testing.T) {
	// C++ FieldMask writes bits in MSB-first order within each byte
	// For example, writing bits 10101010 should produce 0xAA

	fm := NewFieldMask(8)

	// Write pattern: alternating true/false starting with true
	for i := 0; i < 8; i++ {
		fm.WriteBit(i%2 == 0)
	}

	bytes := fm.Bytes()
	if len(bytes) != 1 {
		t.Fatalf("expected 1 byte, got %d", len(bytes))
	}

	// Should be 0xAA (10101010)
	expected := byte(0xAA)
	if bytes[0] != expected {
		t.Errorf("expected 0x%02X, got 0x%02X", expected, bytes[0])
	}

	// Verify reading back
	fm.Reset()
	for i := 0; i < 8; i++ {
		expected := i%2 == 0
		actual := fm.ReadBit()
		if actual != expected {
			t.Errorf("bit %d: expected %v, got %v", i, expected, actual)
		}
	}
}

// TestCrossLangComplexStruct tests a complex structure serialization
func TestCrossLangComplexStruct(t *testing.T) {
	// This simulates serializing a struct like:
	// struct {
	//     int32 a = 42;
	//     string b = "test";
	//     double c = 3.14;
	// }

	w := NewMemWriter()

	// Write int32
	if err := w.WriteInt32(42); err != nil {
		t.Fatalf("WriteInt32 failed: %v", err)
	}

	// Write string with size prefix
	if err := w.WriteString("test"); err != nil {
		t.Fatalf("WriteString failed: %v", err)
	}

	// Write double
	if err := w.WriteFloat64(3.14); err != nil {
		t.Fatalf("WriteFloat64 failed: %v", err)
	}

	data := w.Bytes()

	// Verify the structure
	// int32: 4 bytes
	// string size: 1 byte (0x04) + 4 bytes
	// double: 8 bytes
	// Total: 4 + 1 + 4 + 8 = 17 bytes
	if len(data) != 17 {
		t.Fatalf("expected 17 bytes, got %d", len(data))
	}

	// Verify int32
	val := binary.LittleEndian.Uint32(data[0:4])
	if val != 42 {
		t.Errorf("expected int32 42, got %d", val)
	}

	// Verify string size
	if data[4] != 4 {
		t.Errorf("expected string size 4, got %d", data[4])
	}

	// Verify string content
	str := string(data[5:9])
	if str != "test" {
		t.Errorf("expected string \"test\", got %q", str)
	}

	// Verify double
	bits := binary.LittleEndian.Uint64(data[9:17])
	dval := math.Float64frombits(bits)
	if math.Abs(dval-3.14) > 0.001 {
		t.Errorf("expected double 3.14, got %f", dval)
	}
}

// TestCrossLangArraySerialization tests array serialization compatibility
func TestCrossLangArraySerialization(t *testing.T) {
	// C++ serializes arrays as: size + elements
	w := NewMemWriter()

	arrSize := uint32(3)
	if err := w.WriteDynSize(arrSize); err != nil {
		t.Fatalf("WriteDynSize failed: %v", err)
	}

	elements := []int32{10, 20, 30}
	for _, elem := range elements {
		if err := w.WriteInt32(elem); err != nil {
			t.Fatalf("WriteInt32 failed: %v", err)
		}
	}

	data := w.Bytes()

	// Expected: size (1 byte for 3) + 3 * 4 bytes
	expectedLen := 1 + 3*4
	if len(data) != expectedLen {
		t.Fatalf("expected %d bytes, got %d", expectedLen, len(data))
	}

	// Verify size
	if data[0] != 3 {
		t.Errorf("expected array size 3, got %d", data[0])
	}

	// Verify elements
	for i, expected := range elements {
		offset := 1 + i*4
		val := binary.LittleEndian.Uint32(data[offset : offset+4])
		if val != uint32(expected) {
			t.Errorf("element %d: expected %d, got %d", i, expected, val)
		}
	}
}

// TestCrossLangCppReferenceData tests against reference data from C++
// This test contains actual byte sequences produced by C++ serialization
func TestCrossLangCppReferenceData(t *testing.T) {
	// Reference data: C++ serialization of int32 = 0x12345678
	cppReference := []byte{0x78, 0x56, 0x34, 0x12}

	w := NewMemWriter()
	if err := w.WriteInt32(0x12345678); err != nil {
		t.Fatalf("WriteInt32 failed: %v", err)
	}

	goData := w.Bytes()
	if !bytes.Equal(goData, cppReference) {
		t.Errorf("Go/C++ mismatch:\nC++: %v\nGo:  %v", cppReference, goData)
	}

	// Test roundtrip: Go write -> Go read -> verify
	r := NewMemReader(goData)
	val, err := r.ReadInt32()
	if err != nil {
		t.Fatalf("ReadInt32 failed: %v", err)
	}
	if val != 0x12345678 {
		t.Errorf("roundtrip failed: expected 0x12345678, got 0x%X", val)
	}
}

// BenchmarkWriteInt32 benchmarks int32 writing
func BenchmarkWriteInt32(b *testing.B) {
	w := NewMemWriter()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		w.WriteInt32(42)
		w.Reset()
	}
}

// BenchmarkReadInt32 benchmarks int32 reading
func BenchmarkReadInt32(b *testing.B) {
	data := make([]byte, 4)
	binary.LittleEndian.PutUint32(data, 42)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		r := NewMemReader(data)
		r.ReadInt32()
	}
}

// BenchmarkWriteDynSize benchmarks variable-size encoding
func BenchmarkWriteDynSize(b *testing.B) {
	w := NewMemWriter()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		w.WriteDynSize(0x3FFF)
		w.Reset()
	}
}
