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
	bool readType(S64& v)
	{
		return read(&v, sizeof(S64));
	}
	bool readType(U64& v)
	{
		return read(&v, sizeof(U64));
	}
	bool readType(F64& v)
	{
		return read(&v, sizeof(F64));
	}
	bool readType(F32& v)
	{
		return read(&v, sizeof(F32));
	}
	bool readType(S32& v)
	{
		return read(&v, sizeof(S32));
	}
	bool readType(U32& v)
	{
		return read(&v, sizeof(U32));
	}
	bool readType(S16& v)
	{
		return read(&v, sizeof(S16));
	}
	bool readType(U16& v)
	{
		return read(&v, sizeof(U16));
	}
	bool readType(S8& v)
	{
		return read(&v, sizeof(S8));
	}
	bool readType(U8& v)
	{
		return read(&v, sizeof(U8));
	}
	bool readType(B8& v)
	{
		char vv;
		if(!read(&vv, sizeof(B8)))
			return false;
		v = vv?true:false;
		return true;
	}
	bool readType(STRING& v, U32 maxlen)
	{
		U32 len;
		if(!readDynSize(len) || len > maxlen)
			return false;
		v.resize(len);
		return read((void*)v.c_str(), len);
	}
	bool readDynSize(U32& s)
	{
		s = 0;
		U8 b;
		if(!readType(b))
			return false;
		S32 n = (b & 0XC0)>>6;
		s = (b & 0X3F);
		for(S32 i = 0; i < n; i++)
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
