#ifndef __BINConnection_h__
#define __BINConnection_h__


#include "ProtocolMemReader.h"
#include "ProtocolWriter.h"

#include "Connection.h"

/** BIN.
	BINConnection aprc.
	BINConnection   BIN stubBIN stub 
	.
	BINConnection  BIN proxy 
	 proxy  .

	BIN :
	- HdrType: .
	- BIN
*/
template< class BIN_STUB, class BIN_PROXY >
class BINConnection : 
	public BIN_ProtocolWriter,
	public Connection,
	public BIN_STUB
{
public:
	typedef unsigned short	HdrType;

	BINConnection(size_t rbSize = 0XFFFF, size_t sbSize = 0XFFFF):
	Connection(rbSize, sbSize),
	proxy_(NULL),
	hdr_(NULL)
	{
	}

	/** proxy. */
	void setProxy(BIN_PROXY* p)		{ proxy_ = p; }

	/** .
		handleReceived BIN proxydispatch.
	*/
	virtual int handleReceived(void* data, size_t size)
	{
		if(!proxy_)
			return size;

		// BIN dispatch 
		int processed = 0;

		// .
		unsigned char* d = (unsigned char*)data;
		size_t dLeft = size;
		while(1)
		{
			// .
			if(sizeof(HdrType) > dLeft)
				return processed;
			HdrType dLen = *((HdrType*)d);

			// .
			size_t dTLen = sizeof(HdrType) + dLen;
			if(dTLen > dLeft)
				return processed;

			//  BIN dispatcher.
			ProtocolMemReader r((d+sizeof(HdrType)), dLen);
			if(!proxy_->dispatch(&r))
				return -1;

			d			+= dTLen;
			dLeft		-= dTLen;
			processed	+= dTLen;
		}
		return processed;
	}

	/** Stub events. */
	virtual ProtocolWriter* methodBegin()
	{
		if(getStatus() != Connection::Established)
			return NULL;

		// .
		hdr_ = sendBuf_.wr_ptr();
		// .
		HdrType tempHdr = 0;
		fill(&tempHdr, sizeof(HdrType));
		return this;
	}
	virtual void methodEnd()
	{
		if(getStatus() != Connection::Established)
			return;
		// 
		*((HdrType*)hdr_) = (HdrType)((sendBuf_.wr_ptr() - hdr_) - sizeof(HdrType));
		// aprc.
		flush();
	}
	/** BIN_ProtocolWriter interface. */
	virtual void write(const void* data, size_t len)
	{
		if(getStatus() != Connection::Established)
			return;
		fill((void*)data, len);
	}

private:
	BIN_PROXY*			proxy_;
	char*				hdr_;
};

#endif//__BINConnection_h__
