#ifndef __ProtocolBytesReader_h__
#define __ProtocolBytesReader_h__
#include "Common.h"
#include "ProtocolReader.h"

/** ProtocolReader over a byte vector. */
class ProtocolBytesReader : public ProtocolReader
{
public:
	ProtocolBytesReader(std::vector<uint8_t>& bytes):
	bytes_(bytes),
	rdPtr_(0)
	{}

	virtual bool read(void* data, size_t len)
	{
		if(rdPtr_ + len > bytes_.size())
			return false;
		::memcpy(data, &(bytes_[rdPtr_]), len);
		rdPtr_ += len;
		return true;
	}

private:
	std::vector<uint8_t>& bytes_;
	size_t					rdPtr_;
};


#endif//__ProtocolBytesReader_h__
