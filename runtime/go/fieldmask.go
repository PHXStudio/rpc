package arpc

// FieldMask provides bit-level field presence tracking.
// It packs boolean values into bytes for efficient serialization.
// This implementation is compatible with the C++ FieldMask template.
type FieldMask struct {
	masks []byte
	pos   int
}

// NewFieldMask creates a new FieldMask with the specified number of fields.
// The masks slice is sized to accommodate all field bits.
func NewFieldMask(numFields int) *FieldMask {
	if numFields <= 0 {
		return &FieldMask{
			masks: []byte{},
			pos:   0,
		}
	}
	byteNum := (numFields - 1) / 8 + 1
	return &FieldMask{
		masks: make([]byte, byteNum),
		pos:   0,
	}
}

// WriteBit writes a single bit to the mask.
// If b is true, the corresponding bit is set to 1.
// The position advances after each write.
func (fm *FieldMask) WriteBit(b bool) {
	if b {
		byteIdx := fm.pos / 8
		bitIdx := uint(7 - (fm.pos % 8))
		fm.masks[byteIdx] |= (1 << bitIdx)
	}
	fm.pos++
}

// ReadBit reads a single bit from the mask.
// Returns true if the bit is set, false otherwise.
// The position advances after each read.
func (fm *FieldMask) ReadBit() bool {
	byteIdx := fm.pos / 8
	bitIdx := uint(7 - (fm.pos % 8))
	b := (fm.masks[byteIdx] & (1 << bitIdx)) != 0
	fm.pos++
	return b
}

// Bytes returns the underlying mask bytes.
func (fm *FieldMask) Bytes() []byte {
	return fm.masks
}

// SetBytes sets the mask bytes from external data.
// This is useful when deserializing field masks.
func (fm *FieldMask) SetBytes(data []byte) {
	fm.masks = data
	fm.pos = 0
}

// Reset resets the position to 0 for reading/writing from the beginning.
// The mask data itself is preserved.
func (fm *FieldMask) Reset() {
	fm.pos = 0
}

// Len returns the number of bytes in the mask.
func (fm *FieldMask) Len() int {
	return len(fm.masks)
}

// Clear clears all bits in the mask.
func (fm *FieldMask) Clear() {
	for i := range fm.masks {
		fm.masks[i] = 0
	}
	fm.pos = 0
}
