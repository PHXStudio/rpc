package rpc

import (
	"bytes"
	"math"
	"testing"
)

// TestMemReader_ReadInt8 tests reading int8 values
func TestMemReader_ReadInt8(t *testing.T) {
	data := []byte{0xD6} // -42
	r := NewMemReader(data)

	val, err := r.ReadInt8()
	if err != nil {
		t.Fatalf("ReadInt8 failed: %v", err)
	}
	if val != -42 {
		t.Errorf("expected -42, got %d", val)
	}
}

// TestMemReader_ReadInt16 tests little-endian int16 reading
func TestMemReader_ReadInt16(t *testing.T) {
	data := []byte{0xFC, 0x18} // -1000 in little-endian
	r := NewMemReader(data)

	val, err := r.ReadInt16()
	if err != nil {
		t.Fatalf("ReadInt16 failed: %v", err)
	}
	if val != -1000 {
		t.Errorf("expected -1000, got %d", val)
	}
}

// TestMemReader_ReadInt32 tests little-endian int32 reading
func TestMemReader_ReadInt32(t *testing.T) {
	data := []byte{0x78, 0x56, 0x34, 0x12} // 0x12345678 in little-endian
	r := NewMemReader(data)

	val, err := r.ReadInt32()
	if err != nil {
		t.Fatalf("ReadInt32 failed: %v", err)
	}
	if val != 0x12345678 {
		t.Errorf("expected 0x12345678, got 0x%X", val)
	}
}

// TestMemReader_ReadFloat32 tests float32 reading
func TestMemReader_ReadFloat32(t *testing.T) {
	val := float32(3.14159)
	bits := math.Float32bits(val)

	data := make([]byte, 4)
	// little-endian encoding
	data[0] = byte(bits)
	data[1] = byte(bits >> 8)
	data[2] = byte(bits >> 16)
	data[3] = byte(bits >> 24)

	r := NewMemReader(data)
	decoded, err := r.ReadFloat32()
	if err != nil {
		t.Fatalf("ReadFloat32 failed: %v", err)
	}

	if math.Abs(float64(decoded-val)) > 0.0001 {
		t.Errorf("expected %f, got %f", val, decoded)
	}
}

// TestMemReader_ReadFloat64 tests float64 reading
func TestMemReader_ReadFloat64(t *testing.T) {
	val := 3.141592653589793
	bits := math.Float64bits(val)

	data := make([]byte, 8)
	// little-endian encoding
	for i := 0; i < 8; i++ {
		data[i] = byte(bits >> (uint(i) * 8))
	}

	r := NewMemReader(data)
	decoded, err := r.ReadFloat64()
	if err != nil {
		t.Fatalf("ReadFloat64 failed: %v", err)
	}

	if math.Abs(decoded-val) > 1e-15 {
		t.Errorf("expected %f, got %f", val, decoded)
	}
}

// TestMemReader_ReadString tests string reading with size prefix
func TestMemReader_ReadString(t *testing.T) {
	str := "hello"
	data := make([]byte, 1+len(str))
	data[0] = byte(len(str))
	copy(data[1:], str)

	r := NewMemReader(data)
	result, err := r.ReadString(65535)
	if err != nil {
		t.Fatalf("ReadString failed: %v", err)
	}

	if result != str {
		t.Errorf("expected %q, got %q", str, result)
	}
}

// TestMemReader_ReadStringTooLong tests error on string exceeding max length
func TestMemReader_ReadStringTooLong(t *testing.T) {
	str := "hello"
	data := make([]byte, 1+len(str))
	data[0] = byte(len(str))
	copy(data[1:], str)

	r := NewMemReader(data)
	_, err := r.ReadString(4) // max len 4, but string is 5
	if err == nil {
		t.Error("expected error for string exceeding max length")
	}
}

// TestMemReader_ReadDynSize tests variable-length size decoding
func TestMemReader_ReadDynSize(t *testing.T) {
	tests := []struct {
		data     []byte
		expected uint32
	}{
		{[]byte{0x3F}, 0x3F},
		{[]byte{0x40, 0x00}, 0x40},
		{[]byte{0xFF, 0x3F}, 0x3FFF},
		{[]byte{0x40, 0x40, 0x00}, 0x4000},
		{[]byte{0xFF, 0xFF, 0x3F}, 0x3FFFFF},
		{[]byte{0x40, 0x40, 0x40, 0x00}, 0x400000},
	}

	for _, tt := range tests {
		t.Run("", func(t *testing.T) {
			r := NewMemReader(tt.data)
			val, err := r.ReadDynSize()
			if err != nil {
				t.Fatalf("ReadDynSize failed: %v", err)
			}
			if val != tt.expected {
				t.Errorf("expected %d, got %d", tt.expected, val)
			}
		})
	}
}

// TestMemReader_ReadBool tests boolean reading
func TestMemReader_ReadBool(t *testing.T) {
	tests := []struct {
		data     []byte
		expected bool
	}{
		{[]byte{0}, false},
		{[]byte{1}, true},
		{[]byte{0xFF}, true},
	}

	for _, tt := range tests {
		t.Run("", func(t *testing.T) {
			r := NewMemReader(tt.data)
			val, err := r.ReadBool()
			if err != nil {
				t.Fatalf("ReadBool failed: %v", err)
			}
			if val != tt.expected {
				t.Errorf("expected %v, got %v", tt.expected, val)
			}
		})
	}
}

// TestMemReader_PosAndLen tests position tracking
func TestMemReader_PosAndLen(t *testing.T) {
	data := []byte{1, 2, 3, 4, 5}
	r := NewMemReader(data)

	if r.Len() != 5 {
		t.Errorf("expected Len 5, got %d", r.Len())
	}
	if r.Pos() != 0 {
		t.Errorf("expected Pos 0, got %d", r.Pos())
	}

	r.ReadInt8()
	if r.Pos() != 1 {
		t.Errorf("expected Pos 1, got %d", r.Pos())
	}
	if r.Len() != 4 {
		t.Errorf("expected Len 4, got %d", r.Len())
	}

	// Read remaining
	r.ReadInt32()
	if r.Pos() != 5 {
		t.Errorf("expected Pos 5, got %d", r.Pos())
	}
	if r.Len() != 0 {
		t.Errorf("expected Len 0, got %d", r.Len())
	}
}

// TestMemReader_EOF tests EOF handling
func TestMemReader_EOF(t *testing.T) {
	data := []byte{1, 2}
	r := NewMemReader(data)

	// Read all data
	r.ReadInt16()

	// Next read should return EOF
	_, err := r.ReadInt8()
	if err == nil {
		t.Error("expected EOF error, got nil")
	}
}

// TestMemReader_Read tests bulk reading
func TestMemReader_Read(t *testing.T) {
	data := []byte{1, 2, 3, 4, 5}
	r := NewMemReader(data)

	buf := make([]byte, 3)
	n, err := r.Read(buf)
	if err != nil {
		t.Fatalf("Read failed: %v", err)
	}
	if n != 3 {
		t.Errorf("expected to read 3 bytes, got %d", n)
	}
	if !bytes.Equal(buf, []byte{1, 2, 3}) {
		t.Errorf("expected [1,2,3], got %v", buf)
	}

	// Read remaining
	n, err = r.Read(buf)
	if err != nil {
		t.Fatalf("Read failed: %v", err)
	}
	if n != 2 {
		t.Errorf("expected to read 2 bytes, got %d", n)
	}
	if !bytes.Equal(buf[:2], []byte{4, 5}) {
		t.Errorf("expected [4,5], got %v", buf[:2])
	}

	// Next read should return EOF
	n, err = r.Read(buf)
	if err == nil {
		t.Error("expected EOF, got nil")
	}
	if n != 0 {
		t.Errorf("expected 0 bytes at EOF, got %d", n)
	}
}

// TestMemReader_Bytes tests Bytes method
func TestMemReader_Bytes(t *testing.T) {
	data := []byte{1, 2, 3, 4, 5}
	r := NewMemReader(data)

	r.ReadInt8() // consume 1 byte

	remaining := r.Bytes()
	expected := []byte{2, 3, 4, 5}
	if !bytes.Equal(remaining, expected) {
		t.Errorf("expected %v, got %v", expected, remaining)
	}
}
