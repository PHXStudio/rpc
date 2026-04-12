package rpc

import (
	"encoding/binary"
	"errors"
	"io"
	"math"
)

// MemReader implements ProtocolReader for in-memory buffers.
// It reads data in little-endian byte order for compatibility with C++.
type MemReader struct {
	buf []byte
	pos int
}

// NewMemReader creates a new memory reader from the given byte slice.
func NewMemReader(buf []byte) *MemReader {
	return &MemReader{
		buf: buf,
		pos: 0,
	}
}

// Read reads bytes into the given slice.
func (r *MemReader) Read(data []byte) (int, error) {
	if r.pos >= len(r.buf) {
		return 0, io.EOF
	}

	n := copy(data, r.buf[r.pos:])
	r.pos += n
	return n, nil
}

// Bytes returns the remaining unread bytes.
func (r *MemReader) Bytes() []byte {
	if r.pos >= len(r.buf) {
		return []byte{}
	}
	return r.buf[r.pos:]
}

// Len returns the number of remaining bytes.
func (r *MemReader) Len() int {
	if r.pos >= len(r.buf) {
		return 0
	}
	return len(r.buf) - r.pos
}

// Pos returns the current read position.
func (r *MemReader) Pos() int {
	return r.pos
}

// ReadInt8 reads an 8-bit signed integer.
func (r *MemReader) ReadInt8() (int8, error) {
	if r.pos+1 > len(r.buf) {
		return 0, io.EOF
	}
	v := int8(r.buf[r.pos])
	r.pos++
	return v, nil
}

// ReadUint8 reads an 8-bit unsigned integer.
func (r *MemReader) ReadUint8() (uint8, error) {
	if r.pos+1 > len(r.buf) {
		return 0, io.EOF
	}
	v := r.buf[r.pos]
	r.pos++
	return v, nil
}

// ReadInt16 reads a 16-bit signed integer in little-endian order.
func (r *MemReader) ReadInt16() (int16, error) {
	if r.pos+2 > len(r.buf) {
		return 0, io.EOF
	}
	v := int16(binary.LittleEndian.Uint16(r.buf[r.pos:]))
	r.pos += 2
	return v, nil
}

// ReadUint16 reads a 16-bit unsigned integer in little-endian order.
func (r *MemReader) ReadUint16() (uint16, error) {
	if r.pos+2 > len(r.buf) {
		return 0, io.EOF
	}
	v := binary.LittleEndian.Uint16(r.buf[r.pos:])
	r.pos += 2
	return v, nil
}

// ReadInt32 reads a 32-bit signed integer in little-endian order.
func (r *MemReader) ReadInt32() (int32, error) {
	if r.pos+4 > len(r.buf) {
		return 0, io.EOF
	}
	v := int32(binary.LittleEndian.Uint32(r.buf[r.pos:]))
	r.pos += 4
	return v, nil
}

// ReadUint32 reads a 32-bit unsigned integer in little-endian order.
func (r *MemReader) ReadUint32() (uint32, error) {
	if r.pos+4 > len(r.buf) {
		return 0, io.EOF
	}
	v := binary.LittleEndian.Uint32(r.buf[r.pos:])
	r.pos += 4
	return v, nil
}

// ReadInt64 reads a 64-bit signed integer in little-endian order.
func (r *MemReader) ReadInt64() (int64, error) {
	if r.pos+8 > len(r.buf) {
		return 0, io.EOF
	}
	v := int64(binary.LittleEndian.Uint64(r.buf[r.pos:]))
	r.pos += 8
	return v, nil
}

// ReadUint64 reads a 64-bit unsigned integer in little-endian order.
func (r *MemReader) ReadUint64() (uint64, error) {
	if r.pos+8 > len(r.buf) {
		return 0, io.EOF
	}
	v := binary.LittleEndian.Uint64(r.buf[r.pos:])
	r.pos += 8
	return v, nil
}

// ReadFloat32 reads a 32-bit floating point number in little-endian order.
func (r *MemReader) ReadFloat32() (float32, error) {
	if r.pos+4 > len(r.buf) {
		return 0, io.EOF
	}
	v := math.Float32frombits(binary.LittleEndian.Uint32(r.buf[r.pos:]))
	r.pos += 4
	return v, nil
}

// ReadFloat64 reads a 64-bit floating point number in little-endian order.
func (r *MemReader) ReadFloat64() (float64, error) {
	if r.pos+8 > len(r.buf) {
		return 0, io.EOF
	}
	v := math.Float64frombits(binary.LittleEndian.Uint64(r.buf[r.pos:]))
	r.pos += 8
	return v, nil
}

// ReadBool reads a boolean value from a single byte.
func (r *MemReader) ReadBool() (bool, error) {
	b, err := r.ReadUint8()
	if err != nil {
		return false, err
	}
	return b != 0, nil
}

// ReadString reads a string with variable-length size prefix.
// maxLen specifies the maximum allowed string length.
func (r *MemReader) ReadString(maxLen uint32) (string, error) {
	size, err := r.ReadDynSize()
	if err != nil {
		return "", err
	}
	if size > maxLen {
		return "", errors.New("string length exceeds maximum")
	}
	if r.pos+int(size) > len(r.buf) {
		return "", io.EOF
	}

	s := string(r.buf[r.pos : r.pos+int(size)])
	r.pos += int(size)
	return s, nil
}

// ReadDynSize reads a size value using variable-length encoding.
// This encoding must match the C++ implementation.
func (r *MemReader) ReadDynSize() (uint32, error) {
	if r.pos >= len(r.buf) {
		return 0, io.EOF
	}

	b := r.buf[r.pos]
	r.pos++

	n := int((b & 0xC0) >> 6)
	size := uint32(b & 0x3F)

	for i := 0; i < n; i++ {
		if r.pos >= len(r.buf) {
			return 0, io.EOF
		}
		b := r.buf[r.pos]
		r.pos++
		size = (size << 8) | uint32(b)
	}

	return size, nil
}

// Skip skips n bytes in the stream.
// Used for version compatibility to skip unknown fields.
func (r *MemReader) Skip(n uint32) error {
	if r.pos+int(n) > len(r.buf) {
		return io.EOF
	}
	r.pos += int(n)
	return nil
}
