package arpc

import (
	"encoding/binary"
	"errors"
	"math"
)

// MemWriter implements ProtocolWriter for in-memory buffers.
// It uses little-endian byte order for compatibility with C++.
type MemWriter struct {
	buf []byte
}

// NewMemWriter creates a new memory writer with the specified initial capacity.
func NewMemWriter() *MemWriter {
	return &MemWriter{
		buf: make([]byte, 0, 1024),
	}
}

// Write appends bytes to the buffer.
func (w *MemWriter) Write(data []byte) error {
	w.buf = append(w.buf, data...)
	return nil
}

// Bytes returns the written bytes.
func (w *MemWriter) Bytes() []byte {
	return w.buf
}

// Reset clears the buffer for reuse.
func (w *MemWriter) Reset() {
	w.buf = w.buf[:0]
}

// Len returns the current length of the buffer.
func (w *MemWriter) Len() int {
	return len(w.buf)
}

// WriteInt8 writes an 8-bit signed integer.
func (w *MemWriter) WriteInt8(v int8) error {
	return w.Write([]byte{byte(v)})
}

// WriteUint8 writes an 8-bit unsigned integer.
func (w *MemWriter) WriteUint8(v uint8) error {
	return w.Write([]byte{v})
}

// WriteInt16 writes a 16-bit signed integer in little-endian order.
func (w *MemWriter) WriteInt16(v int16) error {
	buf := make([]byte, 2)
	binary.LittleEndian.PutUint16(buf, uint16(v))
	return w.Write(buf)
}

// WriteUint16 writes a 16-bit unsigned integer in little-endian order.
func (w *MemWriter) WriteUint16(v uint16) error {
	buf := make([]byte, 2)
	binary.LittleEndian.PutUint16(buf, v)
	return w.Write(buf)
}

// WriteInt32 writes a 32-bit signed integer in little-endian order.
func (w *MemWriter) WriteInt32(v int32) error {
	buf := make([]byte, 4)
	binary.LittleEndian.PutUint32(buf, uint32(v))
	return w.Write(buf)
}

// WriteUint32 writes a 32-bit unsigned integer in little-endian order.
func (w *MemWriter) WriteUint32(v uint32) error {
	buf := make([]byte, 4)
	binary.LittleEndian.PutUint32(buf, v)
	return w.Write(buf)
}

// WriteInt64 writes a 64-bit signed integer in little-endian order.
func (w *MemWriter) WriteInt64(v int64) error {
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint64(buf, uint64(v))
	return w.Write(buf)
}

// WriteUint64 writes a 64-bit unsigned integer in little-endian order.
func (w *MemWriter) WriteUint64(v uint64) error {
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint64(buf, v)
	return w.Write(buf)
}

// WriteFloat32 writes a 32-bit floating point number in little-endian order.
func (w *MemWriter) WriteFloat32(v float32) error {
	buf := make([]byte, 4)
	binary.LittleEndian.PutUint32(buf, math.Float32bits(v))
	return w.Write(buf)
}

// WriteFloat64 writes a 64-bit floating point number in little-endian order.
func (w *MemWriter) WriteFloat64(v float64) error {
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint64(buf, math.Float64bits(v))
	return w.Write(buf)
}

// WriteBool writes a boolean value as a single byte.
func (w *MemWriter) WriteBool(v bool) error {
	if v {
		return w.Write([]byte{1})
	}
	return w.Write([]byte{0})
}

// WriteString writes a string with variable-length size prefix.
func (w *MemWriter) WriteString(v string) error {
	if err := w.WriteDynSize(uint32(len(v))); err != nil {
		return err
	}
	return w.Write([]byte(v))
}

// WriteDynSize writes a size value using variable-length encoding.
// This encoding must match the C++ implementation:
// - values <= 0x3F: 1 byte (0x00 - 0x3F)
// - values <= 0x3FFF: 2 bytes (0x40 - 0x3FFF)
// - values <= 0x3FFFFF: 3 bytes (0x4000 - 0x3FFFFF)
// - values <= 0x3FFFFFFF: 4 bytes (0x400000 - 0x3FFFFFFF)
// The high 2 bits of the first byte indicate the number of additional bytes.
func (w *MemWriter) WriteDynSize(size uint32) error {
	if size > 0x3FFFFFFF {
		return errors.New("size too large for variable-length encoding")
	}

	var n int
	if size <= 0x3F {
		n = 0
	} else if size <= 0x3FFF {
		n = 1
	} else if size <= 0x3FFFFF {
		n = 2
	} else {
		n = 3
	}

	buf := make([]byte, n+1)
	for i := n; i >= 0; i-- {
		b := byte(size >> (uint(i) * 8))
		if i == n {
			b |= byte(n << 6)
		}
		buf[n-i] = b
	}

	return w.Write(buf)
}
