package rpc

import "errors"

// Standard error definitions for RPC operations.
// These errors can be used for error checking with errors.Is().
var (
	// ErrBufferOverflow is returned when a read operation exceeds the buffer size.
	ErrBufferOverflow = errors.New("buffer overflow")

	// ErrInvalidLength is returned when a length value is invalid.
	ErrInvalidLength = errors.New("invalid length")

	// ErrUnknownMethod is returned when an unknown method ID is encountered during dispatch.
	ErrUnknownMethod = errors.New("unknown method")

	// ErrInvalidEnumValue is returned when an enum value is out of range.
	ErrInvalidEnumValue = errors.New("invalid enum value")

	// ErrStringTooLong is returned when a string exceeds its maximum length.
	ErrStringTooLong = errors.New("string exceeds maximum length")

	// ErrArrayTooLong is returned when an array exceeds its maximum length.
	ErrArrayTooLong = errors.New("array exceeds maximum length")

	// ErrInvalidFormat is returned when data format is invalid.
	ErrInvalidFormat = errors.New("invalid format")
)
