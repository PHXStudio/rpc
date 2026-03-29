#ifndef __ProtocolBytesWriter_h__
#define __ProtocolBytesWriter_h__

#include "Common.h"
#include "ProtocolWriter.h"


/** ProtocolWriter that appends to a byte vector. */
class ProtocolBytesWriter : public ProtocolWriter
{
public:
	ProtocolBytesWriter(std::vector<uint8_t>& b):
	bytes_(b)
	{}

	virtual void write(const void* data, size_t len)
	{
		size_t s = bytes_.size();
		bytes_.resize(s + len);
		::memcpy(&(bytes_[s]), data, len);
	}
	std::vector<uint8_t>& getBytes()	{ return bytes_; }

private:
	std::vector<uint8_t>& bytes_;
};


#endif//__ProtocolBytesWriter_h__
