#ifndef __ProtocolWriter_h__
#define __ProtocolWriter_h__

#include "Common.h"

/** ARPC protocol writer interface.
	Used by generated stubs to write marshaled bytes; implement write() to send to memory or a socket.
*/
class ProtocolWriter
{
public:
	/** Write marshaled RPC payload bytes.
		Called by the stub while sending a call.
		@param data Source buffer.
		@param len Number of bytes.
	*/
	virtual void write(const void* data, size_t len) = 0;

	/** @name write basic types. */
	//@{
	void writeType(int64_t v)
	{
		write(&v, sizeof(int64_t));
	}
	void writeType(uint64_t v)
	{
		write(&v, sizeof(uint64_t));
	}
	void writeType(double v)
	{
		write(&v, sizeof(double));
	}
	void writeType(float v)
	{
		write(&v, sizeof(float));
	}
	void writeType(int32_t v)
	{
		write(&v, sizeof(int32_t));
	}
	void writeType(uint32_t v)
	{
		write(&v, sizeof(uint32_t));
	}
	void writeType(int16_t v)
	{
		write(&v, sizeof(int16_t));
	}
	void writeType(uint16_t v)
	{
		write(&v, sizeof(uint16_t));
	}
	void writeType(int8_t v)
	{
		write(&v, sizeof(int8_t));
	}
	void writeType(uint8_t v)
	{
		write(&v, sizeof(uint8_t));
	}
	void writeType(bool v)
	{
		char vv = v?1:0;
		write(&vv, sizeof(bool));
	}
	void writeType(const std::string& v)
	{
		uint32_t len = (uint32_t)v.length();
		writeDynSize(len);
		write(v.c_str(), v.length());
	}
	void writeDynSize(uint32_t s)
	{
		uint8_t* p = (uint8_t*)(&s);
		uint8_t n = 0;
		if(s <= 0X3F)
			n = 0;
		else if(s <= 0X3FFF)
			n = 1;
		else if(s <= 0X3FFFFF)
			n = 2;
		else if(s <= 0X3FFFFFFF)
			n = 3;
		p[n] |= (n<<6);
		for(int32_t i = (int32_t)n; i >= 0; i--)
			writeType(p[i]);
	}
	//@}
};



#endif//__ProtocolWriter_h__
