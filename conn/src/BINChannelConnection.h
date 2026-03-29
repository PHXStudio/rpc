#ifndef __BINChannelConnection_h__
#define __BINChannelConnection_h__

#include "ProtocolMemReader.h"
#include "ProtocolWriter.h"

#include "ChannelConnection.h"

/** BIN ChannelConnection. 
*/
template<
	class CHANNEL,
	class BIN_CHAN_INIT_DATA,
	class BIN_STUB, 
	class BIN_PROXY 
>
class BINChannelConnection :
	public ChannelConnection,
	public BIN_STUB,
	public ProtocolWriter /* protocol writer for BIN_STUB */
{
public:
	BINChannelConnection(bool isConnector, size_t rbSize = 0XFFFF, size_t sbSize = 0XFFFF):
	ChannelConnection(isConnector, rbSize, sbSize),
	proxy_(NULL)
	{}

	/** proxy. */
	void setProxy(BIN_PROXY* p)		{ proxy_ = p; }

	/**  Channel . */
	void getAllChannels(std::vector<CHANNEL*>& channels)
	{
		ChannelConnection::getAllChannels((std::vector<Channel*>&)channels);
	}

	/** channel. 
		BIN struct.
		@param c channel.
		@param initData .
	*/
	void connectChannel(CHANNEL* c)
	{
		connect(c);
	}

	/**  CHANNEL .
		channel.
		@param initData channel.
	*/
	virtual Channel* accept() { return new CHANNEL();}

	virtual bool handleGlobalData(void* data, size_t size)
	{
		if(!proxy_)
			return size;

		//  BIN_PROXY::dispatch 
		ProtocolMemReader r(data, size);
		return proxy_->dispatch(&r);
	}

	/** Stub events. */
	virtual ProtocolWriter* methodBegin()
	{
		initGlobalSendingData();
		return this;
	}
	virtual void methodEnd()
	{
		flushSendingData();
	}
	/** BIN_ProtocolWriter interface. */
	virtual void write(const void* data, size_t len)
	{
		fillSendingData((void*)data, len);
	}

private:
	BIN_PROXY*		proxy_;
};

#endif//__BINChannelConnection_h__
