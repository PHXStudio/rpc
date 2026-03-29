#ifndef __BINChannel_h__
#define __BINChannel_h__

#include "ProtocolMemReader.h"
#include "ProtocolWriter.h"

#include "Channel.h"

/** bin Channel.

	BINChannel  BIN_STUB BIN_PROXY  Channel 
	 Channel  BIN
	BINChannel  BIN_PROXY  BIN_PROXY 
	 BIN  BIN_STUB 
	 BINChannel  BIN_STUB  BIN 

	@see BINConnection
*/
template< class BIN_STUB, class BIN_PROXY >
class BINChannel :
	public Channel,
	public BIN_STUB,
	public ProtocolWriter /* protocol writer for BIN_STUB */
{
public:
	/** channel p . */
	BINChannel():
	proxy_(NULL)
	{
	}

	/** proxy. */
	void setProxy(BIN_PROXY* p)		{ proxy_ = p; }

	/** channel.
		BIN proxydispatch.
	*/
	virtual bool handleReceived(void* data, size_t size)
	{
		if(!proxy_)
			return true;

		//  BIN_PROXY::dispatch 
		ProtocolMemReader r(data, size);
		return proxy_->dispatch(&r);
	}

	/** Stub events. */
	virtual ProtocolWriter* methodBegin()
	{
		fillBegin();
		return this;
	}
	virtual void methodEnd()
	{
		fillEnd();
	}

	/** BIN_ProtocolWriter interface. */
	virtual void write(const void* data, size_t len)
	{
		fill((void*)data, len);
	}

protected:
	BIN_PROXY*		proxy_;
};

#endif//__BINChannel_h__
