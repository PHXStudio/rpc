// Package rpc provides the core protocol interfaces for RPC serialization
// This package contains the reader and writer interfaces used by generated code.
package rpc

// ProtocolWriter writes binary RPC payloads.
// Implementations should write data in little-endian byte order for compatibility.
type ProtocolWriter interface {
	// Write writes raw bytes to the output
	Write(data []byte) error

	// WriteInt8 writes an 8-bit signed integer
	WriteInt8(v int8) error

	// WriteUint8 writes an 8-bit unsigned integer
	WriteUint8(v uint8) error

	// WriteInt16 writes a 16-bit signed integer
	WriteInt16(v int16) error

	// WriteUint16 writes a 16-bit unsigned integer
	WriteUint16(v uint16) error

	// WriteInt32 writes a 32-bit signed integer
	WriteInt32(v int32) error

	// WriteUint32 writes a 32-bit unsigned integer
	WriteUint32(v uint32) error

	// WriteInt64 writes a 64-bit signed integer
	WriteInt64(v int64) error

	// WriteUint64 writes a 64-bit unsigned integer
	WriteUint64(v uint64) error

	// WriteFloat32 writes a 32-bit floating point number
	WriteFloat32(v float32) error

	// WriteFloat64 writes a 64-bit floating point number
	WriteFloat64(v float64) error

	// WriteBool writes a boolean value
	WriteBool(v bool) error

	// WriteString writes a string with variable-length encoding
	WriteString(v string) error

	// WriteDynSize writes a size value using variable-length encoding
	// This encoding is compatible with the C++ implementation:
	// - values <= 0x3F: 1 byte
	// - values <= 0x3FFF: 2 bytes
	// - values <= 0x3FFFFF: 3 bytes
	// - values <= 0x3FFFFFFF: 4 bytes
	WriteDynSize(size uint32) error
}

// ProtocolReader reads binary RPC payloads.
// Implementations should read data in little-endian byte order for compatibility.
type ProtocolReader interface {
	// Read reads raw bytes from the input
	Read(data []byte) (int, error)

	// ReadInt8 reads an 8-bit signed integer
	ReadInt8() (int8, error)

	// ReadUint8 reads an 8-bit unsigned integer
	ReadUint8() (uint8, error)

	// ReadInt16 reads a 16-bit signed integer
	ReadInt16() (int16, error)

	// ReadUint16 reads a 16-bit unsigned integer
	ReadUint16() (uint16, error)

	// ReadInt32 reads a 32-bit signed integer
	ReadInt32() (int32, error)

	// ReadUint32 reads a 32-bit unsigned integer
	ReadUint32() (uint32, error)

	// ReadInt64 reads a 64-bit signed integer
	ReadInt64() (int64, error)

	// ReadUint64 reads a 64-bit unsigned integer
	ReadUint64() (uint64, error)

	// ReadFloat32 reads a 32-bit floating point number
	ReadFloat32() (float32, error)

	// ReadFloat64 reads a 64-bit floating point number
	ReadFloat64() (float64, error)

	// ReadBool reads a boolean value
	ReadBool() (bool, error)

	// ReadString reads a string with variable-length encoding
	// maxLen specifies the maximum allowed string length
	ReadString(maxLen uint32) (string, error)

	// ReadDynSize reads a size value using variable-length encoding
	ReadDynSize() (uint32, error)
}
