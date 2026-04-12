package rpc

import (
	"testing"
)

// TestSerializationRoundtrip tests serialization and deserialization roundtrip
func TestSerializationRoundtrip(t *testing.T) {
	// Create a writer and serialize some data
	writer := NewMemWriter()

	// Write various types
	if err := writer.WriteInt8(-42); err != nil {
		t.Fatalf("WriteInt8 failed: %v", err)
	}
	if err := writer.WriteUint8(200); err != nil {
		t.Fatalf("WriteUint8 failed: %v", err)
	}
	if err := writer.WriteInt16(-1000); err != nil {
		t.Fatalf("WriteInt16 failed: %v", err)
	}
	if err := writer.WriteUint16(50000); err != nil {
		t.Fatalf("WriteUint16 failed: %v", err)
	}
	if err := writer.WriteInt32(-1234567890); err != nil {
		t.Fatalf("WriteInt32 failed: %v", err)
	}
	if err := writer.WriteUint32(3234567890); err != nil {
		t.Fatalf("WriteUint32 failed: %v", err)
	}
	if err := writer.WriteInt64(-9876543210); err != nil {
		t.Fatalf("WriteInt64 failed: %v", err)
	}
	if err := writer.WriteUint64(18446744073709551615); err != nil {
		t.Fatalf("WriteUint64 failed: %v", err)
	}
	if err := writer.WriteFloat32(3.14159); err != nil {
		t.Fatalf("WriteFloat32 failed: %v", err)
	}
	if err := writer.WriteFloat64(2.718281828459045); err != nil {
		t.Fatalf("WriteFloat64 failed: %v", err)
	}
	if err := writer.WriteBool(true); err != nil {
		t.Fatalf("WriteBool failed: %v", err)
	}
	if err := writer.WriteString("hello world"); err != nil {
		t.Fatalf("WriteString failed: %v", err)
	}

	// Test variable-length size encoding
	if err := writer.WriteDynSize(0x3FFF); err != nil {
		t.Fatalf("WriteDynSize failed: %v", err)
	}

	data := writer.Bytes()

	// Now read it back
	reader := NewMemReader(data)

	// Read and verify each type
	val, err := reader.ReadInt8()
	if err != nil {
		t.Fatalf("ReadInt8 failed: %v", err)
	}
	if val != -42 {
		t.Errorf("expected -42, got %d", val)
	}

	val2, err := reader.ReadUint8()
	if err != nil {
		t.Fatalf("ReadUint8 failed: %v", err)
	}
	if val2 != 200 {
		t.Errorf("expected 200, got %d", val2)
	}

	val3, err := reader.ReadInt16()
	if err != nil {
		t.Fatalf("ReadInt16 failed: %v", err)
	}
	if val3 != -1000 {
		t.Errorf("expected -1000, got %d", val3)
	}

	val4, err := reader.ReadUint16()
	if err != nil {
		t.Fatalf("ReadUint16 failed: %v", err)
	}
	if val4 != 50000 {
		t.Errorf("expected 50000, got %d", val4)
	}

	val5, err := reader.ReadInt32()
	if err != nil {
		t.Fatalf("ReadInt32 failed: %v", err)
	}
	if val5 != -1234567890 {
		t.Errorf("expected -1234567890, got %d", val5)
	}

	val6, err := reader.ReadUint32()
	if err != nil {
		t.Fatalf("ReadUint32 failed: %v", err)
	}
	if val6 != 3234567890 {
		t.Errorf("expected 3234567890, got %d", val6)
	}

	val7, err := reader.ReadInt64()
	if err != nil {
		t.Fatalf("ReadInt64 failed: %v", err)
	}
	if val7 != -9876543210 {
		t.Errorf("expected -9876543210, got %d", val7)
	}

	val8, err := reader.ReadUint64()
	if err != nil {
		t.Fatalf("ReadUint64 failed: %v", err)
	}
	if val8 != 18446744073709551615 {
		t.Errorf("expected 18446744073709551615, got %d", val8)
	}

	val9, err := reader.ReadFloat32()
	if err != nil {
		t.Fatalf("ReadFloat32 failed: %v", err)
	}
	if val9 != 3.14159 {
		t.Errorf("expected 3.14159, got %f", val9)
	}

	val10, err := reader.ReadFloat64()
	if err != nil {
		t.Fatalf("ReadFloat64 failed: %v", err)
	}
	if val10 != 2.718281828459045 {
		t.Errorf("expected 2.718281828459045, got %f", val10)
	}

	val11, err := reader.ReadBool()
	if err != nil {
		t.Fatalf("ReadBool failed: %v", err)
	}
	if !val11 {
		t.Errorf("expected true, got false")
	}

	val12, err := reader.ReadString(65535)
	if err != nil {
		t.Fatalf("ReadString failed: %v", err)
	}
	if val12 != "hello world" {
		t.Errorf("expected \"hello world\", got %q", val12)
	}

	val13, err := reader.ReadDynSize()
	if err != nil {
		t.Fatalf("ReadDynSize failed: %v", err)
	}
	if val13 != 0x3FFF {
		t.Errorf("expected 0x3FFF, got 0x%X", val13)
	}

	// Verify we consumed all data
	if reader.Len() != 0 {
		t.Errorf("expected to consume all data, %d bytes remaining", reader.Len())
	}
}

// TestArraySerialization tests array serialization roundtrip
func TestArraySerialization(t *testing.T) {
	writer := NewMemWriter()

	// Write array size and elements
	arrSize := uint32(5)
	if err := writer.WriteDynSize(arrSize); err != nil {
		t.Fatalf("WriteDynSize failed: %v", err)
	}

	elements := []int32{10, 20, 30, 40, 50}
	for _, elem := range elements {
		if err := writer.WriteInt32(elem); err != nil {
			t.Fatalf("WriteInt32 failed: %v", err)
		}
	}

	data := writer.Bytes()
	reader := NewMemReader(data)

	// Read array size
	size, err := reader.ReadDynSize()
	if err != nil {
		t.Fatalf("ReadDynSize failed: %v", err)
	}
	if size != arrSize {
		t.Fatalf("expected size %d, got %d", arrSize, size)
	}

	// Read elements
	for _, expected := range elements {
		val, err := reader.ReadInt32()
		if err != nil {
			t.Fatalf("ReadInt32 failed: %v", err)
		}
		if val != expected {
			t.Errorf("expected %d, got %d", expected, val)
		}
	}
}

// TestStringArraySerialization tests string array serialization
func TestStringArraySerialization(t *testing.T) {
	writer := NewMemWriter()

	strings := []string{"foo", "bar", "baz"}

	// Write array size
	if err := writer.WriteDynSize(uint32(len(strings))); err != nil {
		t.Fatalf("WriteDynSize failed: %v", err)
	}

	// Write strings
	for _, s := range strings {
		if err := writer.WriteString(s); err != nil {
			t.Fatalf("WriteString failed: %v", err)
		}
	}

	data := writer.Bytes()
	reader := NewMemReader(data)

	// Read array size
	size, err := reader.ReadDynSize()
	if err != nil {
		t.Fatalf("ReadDynSize failed: %v", err)
	}
	if size != uint32(len(strings)) {
		t.Fatalf("expected size %d, got %d", len(strings), size)
	}

	// Read strings
	for _, expected := range strings {
		s, err := reader.ReadString(65535)
		if err != nil {
			t.Fatalf("ReadString failed: %v", err)
		}
		if s != expected {
			t.Errorf("expected %q, got %q", expected, s)
		}
	}
}

// TestFieldMaskRoundtrip tests field mask serialization roundtrip
func TestFieldMaskRoundtrip(t *testing.T) {
	fmWrite := NewFieldMask(16)

	// Set some bits
	fmWrite.WriteBit(true)
	fmWrite.WriteBit(false)
	fmWrite.WriteBit(true)
	fmWrite.WriteBit(true)
	fmWrite.WriteBit(false)
	fmWrite.WriteBit(true)
	fmWrite.WriteBit(false)
	fmWrite.WriteBit(false)

	// Get bytes
	maskBytes := fmWrite.Bytes()

	// Create new field mask and set bytes
	fmRead := NewFieldMask(16)
	fmRead.SetBytes(maskBytes)

	// Read back and verify
	if !fmRead.ReadBit() {
		t.Error("bit 0 should be true")
	}
	if fmRead.ReadBit() {
		t.Error("bit 1 should be false")
	}
	if !fmRead.ReadBit() {
		t.Error("bit 2 should be true")
	}
	if !fmRead.ReadBit() {
		t.Error("bit 3 should be true")
	}
	if fmRead.ReadBit() {
		t.Error("bit 4 should be false")
	}
	if !fmRead.ReadBit() {
		t.Error("bit 5 should be true")
	}
	if fmRead.ReadBit() {
		t.Error("bit 6 should be false")
	}
	if fmRead.ReadBit() {
		t.Error("bit 7 should be false")
	}
}
