#ifndef __ProtocolReader_h__
#define __ProtocolReader_h__

#include "Common.h"

/** ARPC protocol reader interface.
	Provides demarshaling reads for service proxies: implement read() to supply bytes from memory or a socket.
*/
class ProtocolReader
{
public:
	/** Read marshaled RPC payload bytes.
		Called by the proxy while handling a call.
		@param data Destination buffer.
		@param len Number of bytes.
	*/
	virtual bool read(void* data, size_t len) = 0;

	/** @name read basic types. */
	//@{
	bool readType(int64_t& v)
	{
		return read(&v, sizeof(int64_t));
	}
	bool readType(uint64_t& v)
	{
		return read(&v, sizeof(uint64_t));
	}
	bool readType(double& v)
	{
		return read(&v, sizeof(double));
	}
	bool readType(float& v)
	{
		return read(&v, sizeof(float));
	}
	bool readType(int32_t& v)
	{
		return read(&v, sizeof(int32_t));
	}
	bool readType(uint32_t& v)
	{
		return read(&v, sizeof(uint32_t));
	}
	bool readType(int16_t& v)
	{
		return read(&v, sizeof(int16_t));
	}
	bool readType(uint16_t& v)
	{
		return read(&v, sizeof(uint16_t));
	}
	bool readType(int8_t& v)
	{
		return read(&v, sizeof(int8_t));
	}
	bool readType(uint8_t& v)
	{
		return read(&v, sizeof(uint8_t));
	}
	bool readType(bool& v)
	{
		char vv;
		if(!read(&vv, sizeof(bool)))
			return false;
		v = vv?true:false;
		return true;
	}
	bool readType(std::string& v, uint32_t maxlen)
	{
		uint32_t len;
		if(!readDynSize(len) || len > maxlen)
			return false;
		v.resize(len);
		return read((void*)v.c_str(), len);
	}
	bool readDynSize(uint32_t& s)
	{
		s = 0;
		uint8_t b;
		if(!readType(b))
			return false;
		int32_t n = (b & 0XC0)>>6;
		s = (b & 0X3F);
		for(int32_t i = 0; i < n; i++)
		{
			if(!readType(b))
				return false;
			s = (s<<8)|b;
		}
		return true;
	}
	//@}
};


#endif//__ProtocolReader_h__
