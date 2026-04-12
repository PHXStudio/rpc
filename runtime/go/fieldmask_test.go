package rpc

import (
	"testing"
)

// TestFieldMask_WriteReadBit tests basic bit writing and reading
func TestFieldMask_WriteReadBit(t *testing.T) {
	fm := NewFieldMask(16) // 16 fields = 2 bytes

	// Write some bits
	fm.WriteBit(true)
	fm.WriteBit(false)
	fm.WriteBit(true)
	fm.WriteBit(true)

	// Reset and read back
	fm.Reset()

	if !fm.ReadBit() {
		t.Error("expected first bit to be true")
	}
	if fm.ReadBit() {
		t.Error("expected second bit to be false")
	}
	if !fm.ReadBit() {
		t.Error("expected third bit to be true")
	}
	if !fm.ReadBit() {
		t.Error("expected fourth bit to be true")
	}
}

// TestFieldMask_BitOrder tests bit order (MSB first within each byte)
func TestFieldMask_BitOrder(t *testing.T) {
	fm := NewFieldMask(8)

	// Write pattern: 10101010
	for i := 0; i < 8; i++ {
		fm.WriteBit(i%2 == 0)
	}

	bytes := fm.Bytes()
	// Should be 0xAA (10101010 in binary, MSB first)
	expected := byte(0xAA)
	if bytes[0] != expected {
		t.Errorf("expected 0xAA, got 0x%02X", bytes[0])
	}
}

// TestFieldMask_AllTrue tests all bits set to true
func TestFieldMask_AllTrue(t *testing.T) {
	fm := NewFieldMask(16)

	for i := 0; i < 16; i++ {
		fm.WriteBit(true)
	}

	fm.Reset()

	for i := 0; i < 16; i++ {
		if !fm.ReadBit() {
			t.Errorf("bit %d should be true", i)
		}
	}
}

// TestFieldMask_AllFalse tests all bits set to false
func TestFieldMask_AllFalse(t *testing.T) {
	fm := NewFieldMask(16)

	for i := 0; i < 16; i++ {
		fm.WriteBit(false)
	}

	fm.Reset()

	for i := 0; i < 16; i++ {
		if fm.ReadBit() {
			t.Errorf("bit %d should be false", i)
		}
	}
}

// TestFieldMask_SetBytes tests setting bytes from external data
func TestFieldMask_SetBytes(t *testing.T) {
	fm := NewFieldMask(16)

	// Set pattern: 0xFF 0xAA (11111111 10101010)
	data := []byte{0xFF, 0xAA}
	fm.SetBytes(data)

	// First byte: all true
	for i := 0; i < 8; i++ {
		if !fm.ReadBit() {
			t.Errorf("first byte bit %d should be true", i)
		}
	}

	// Second byte: 10101010
	expected := []bool{true, false, true, false, true, false, true, false}
	for i, exp := range expected {
		if fm.ReadBit() != exp {
			t.Errorf("second byte bit %d should be %v", i, exp)
		}
	}
}

// TestFieldMask_BytesReturnsCopy tests that Bytes returns the actual data
func TestFieldMask_BytesReturnsCopy(t *testing.T) {
	fm := NewFieldMask(8)
	fm.WriteBit(true)

	bytes := fm.Bytes()
	if len(bytes) != 1 {
		t.Fatalf("expected 1 byte, got %d", len(bytes))
	}
	if bytes[0] != 0x80 {
		t.Errorf("expected 0x80, got 0x%02X", bytes[0])
	}
}

// TestFieldMask_Reset tests reset functionality
func TestFieldMask_Reset(t *testing.T) {
	fm := NewFieldMask(8)

	fm.WriteBit(true)
	fm.WriteBit(false)
	fm.WriteBit(true)

	if fm.Pos() != 3 {
		t.Errorf("expected pos 3, got %d", fm.Pos())
	}

	fm.Reset()

	if fm.Pos() != 0 {
		t.Errorf("expected pos 0 after reset, got %d", fm.Pos())
	}

	// Data should be preserved
	if !fm.ReadBit() {
		t.Error("first bit should still be true")
	}
}

// TestFieldMask_Clear tests clearing all bits
func TestFieldMask_Clear(t *testing.T) {
	fm := NewFieldMask(16)

	for i := 0; i < 16; i++ {
		fm.WriteBit(true)
	}

	fm.Clear()
	fm.Reset()

	for i := 0; i < 16; i++ {
		if fm.ReadBit() {
			t.Errorf("bit %d should be false after clear", i)
		}
	}
}

// TestFieldMask_Len tests Len method
func TestFieldMask_Len(t *testing.T) {
	tests := []struct {
		numFields int
		expected  int
	}{
		{1, 1},
		{8, 1},
		{9, 2},
		{16, 2},
		{17, 3},
	}

	for _, tt := range tests {
		t.Run("", func(t *testing.T) {
			fm := NewFieldMask(tt.numFields)
			if fm.Len() != tt.expected {
				t.Errorf("numFields=%d: expected len %d, got %d",
					tt.numFields, tt.expected, fm.Len())
			}
		})
	}
}

// TestFieldMask_ZeroFields tests zero field count
func TestFieldMask_ZeroFields(t *testing.T) {
	fm := NewFieldMask(0)

	if fm.Len() != 0 {
		t.Errorf("expected len 0, got %d", fm.Len())
	}

	if len(fm.Bytes()) != 0 {
		t.Errorf("expected empty bytes, got %v", fm.Bytes())
	}
}

// TestFieldMask_Boundary tests byte boundary behavior
func TestFieldMask_Boundary(t *testing.T) {
	fm := NewFieldMask(16)

	// Write first byte all true
	for i := 0; i < 8; i++ {
		fm.WriteBit(true)
	}

	// Write one bit of second byte as true
	fm.WriteBit(true)

	// Rest false
	for i := 0; i < 7; i++ {
		fm.WriteBit(false)
	}

	fm.Reset()

	// First byte
	for i := 0; i < 8; i++ {
		if !fm.ReadBit() {
			t.Errorf("first byte bit %d should be true", i)
		}
	}

	// Second byte: only first bit true
	if !fm.ReadBit() {
		t.Error("second byte first bit should be true")
	}
	for i := 0; i < 7; i++ {
		if fm.ReadBit() {
			t.Errorf("second byte bit %d should be false", i+1)
		}
	}
}
