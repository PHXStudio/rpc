package arpc

import (
	"bytes"
	"encoding/binary"
	"math"
	"testing"
)

// TestMemWriter_WriteInt8 tests writing int8 values
func TestMemWriter_WriteInt8(t *testing.T) {
	w := NewMemWriter()
	err := w.WriteInt8(-42)
	if err != nil {
		t.Fatalf("WriteInt8 failed: %v", err)
	}

	data := w.Bytes()
	if len(data) != 1 {
		t.Fatalf("expected 1 byte, got %d", len(data))
	}
	if data[0] != 0xD6 { // -42 as unsigned byte
		t.Errorf("expected 0xD6, got 0x%02X", data[0])
	}
}

// TestMemWriter_WriteInt16 tests little-endian int16 writing
func TestMemWriter_WriteInt16(t *testing.T) {
	w := NewMemWriter()
	err := w.WriteInt16(-1000)
	if err != nil {
		t.Fatalf("WriteInt16 failed: %v", err)
	}

	data := w.Bytes()
	if len(data) != 2 {
		t.Fatalf("expected 2 bytes, got %d", len(data))
	}
	// -1000 in little-endian is 0x18FC
	expected := []byte{0xFC, 0x18}
	if !bytes.Equal(data, expected) {
		t.Errorf("expected %v, got %v", expected, data)
	}
}

// TestMemWriter_WriteInt32 tests little-endian int32 writing
func TestMemWriter_WriteInt32(t *testing.T) {
	w := NewMemWriter()
	err := w.WriteInt32(0x12345678)
	if err != nil {
		t.Fatalf("WriteInt32 failed: %v", err)
	}

	data := w.Bytes()
	if len(data) != 4 {
		t.Fatalf("expected 4 bytes, got %d", len(data))
	}
	// little-endian: 78 56 34 12
	expected := []byte{0x78, 0x56, 0x34, 0x12}
	if !bytes.Equal(data, expected) {
		t.Errorf("expected %v, got %v", expected, data)
	}
}

// TestMemWriter_WriteFloat32 tests float32 writing
func TestMemWriter_WriteFloat32(t *testing.T) {
	w := NewMemWriter()
	val := float32(3.14159)
	err := w.WriteFloat32(val)
	if err != nil {
		t.Fatalf("WriteFloat32 failed: %v", err)
	}

	data := w.Bytes()
	if len(data) != 4 {
		t.Fatalf("expected 4 bytes, got %d", len(data))
	}

	// Decode and verify
	bits := binary.LittleEndian.Uint32(data)
	decoded := math.Float32frombits(bits)
	if math.Abs(float64(decoded-val)) > 0.0001 {
		t.Errorf("expected %f, got %f", val, decoded)
	}
}

// TestMemWriter_WriteFloat64 tests float64 writing
func TestMemWriter_WriteFloat64(t *testing.T) {
	w := NewMemWriter()
	val := 3.141592653589793
	err := w.WriteFloat64(val)
	if err != nil {
		t.Fatalf("WriteFloat64 failed: %v", err)
	}

	data := w.Bytes()
	if len(data) != 8 {
		t.Fatalf("expected 8 bytes, got %d", len(data))
	}

	// Decode and verify
	bits := binary.LittleEndian.Uint64(data)
	decoded := math.Float64frombits(bits)
	if math.Abs(decoded-val) > 1e-15 {
		t.Errorf("expected %f, got %f", val, decoded)
	}
}

// TestMemWriter_WriteString tests string writing with size prefix
func TestMemWriter_WriteString(t *testing.T) {
	w := NewMemWriter()
	str := "hello"
	err := w.WriteString(str)
	if err != nil {
		t.Fatalf("WriteString failed: %v", err)
	}

	data := w.Bytes()
	// Size prefix (1 byte for "hello") + string
	expectedLen := 1 + len(str)
	if len(data) != expectedLen {
		t.Fatalf("expected %d bytes, got %d", expectedLen, len(data))
	}

	if data[0] != byte(len(str)) {
		t.Errorf("expected size %d, got %d", len(str), data[0])
	}

	actualStr := string(data[1:])
	if actualStr != str {
		t.Errorf("expected %q, got %q", str, actualStr)
	}
}

// TestMemWriter_WriteDynSize tests variable-length size encoding
func TestMemWriter_WriteDynSize(t *testing.T) {
	tests := []struct {
		value    uint32
		expected []byte
	}{
		{0x3F, []byte{0x3F}},           // 1 byte
		{0x40, []byte{0x40, 0x00}},     // 2 bytes
		{0x3FFF, []byte{0xFF, 0x3F}},   // 2 bytes
		{0x4000, []byte{0x40, 0x40, 0x00}}, // 3 bytes
		{0x3FFFFF, []byte{0xFF, 0xFF, 0x3F}}, // 3 bytes
		{0x400000, []byte{0x40, 0x40, 0x40, 0x00}}, // 4 bytes
	}

	for _, tt := range tests {
		t.Run("", func(t *testing.T) {
			w := NewMemWriter()
			err := w.WriteDynSize(tt.value)
			if err != nil {
				t.Fatalf("WriteDynSize(%d) failed: %v", tt.value, err)
			}

			data := w.Bytes()
			if !bytes.Equal(data, tt.expected) {
				t.Errorf("value=0x%X: expected %v, got %v", tt.value, tt.expected, data)
			}
		})
	}
}

// TestMemWriter_WriteDynSizeTooLarge tests error on too-large values
func TestMemWriter_WriteDynSizeTooLarge(t *testing.T) {
	w := NewMemWriter()
	err := w.WriteDynSize(0x40000000) // exceeds max
	if err == nil {
		t.Error("expected error for size > 0x3FFFFFFF, got nil")
	}
}

// TestMemWriter_Reset tests buffer reset
func TestMemWriter_Reset(t *testing.T) {
	w := NewMemWriter()
	w.WriteInt8(42)
	if w.Len() != 1 {
		t.Errorf("expected len 1, got %d", w.Len())
	}

	w.Reset()
	if w.Len() != 0 {
		t.Errorf("expected len 0 after reset, got %d", w.Len())
	}

	w.WriteInt8(99)
	if w.Len() != 1 {
		t.Errorf("expected len 1 after reset and write, got %d", w.Len())
	}
	if w.Bytes()[0] != 99 {
		t.Errorf("expected 99, got %d", w.Bytes()[0])
	}
}

// TestMemWriter_BytesIsCopy tests that Bytes returns a copy
func TestMemWriter_BytesIsCopy(t *testing.T) {
	w := NewMemWriter()
	w.WriteInt8(42)

	data := w.Bytes()
	data[0] = 99

	// Original should be unchanged
	if w.Bytes()[0] != 42 {
		t.Error("modifying returned slice affected original")
	}
}
