#ifndef __ChannelConnection_h__
#define __ChannelConnection_h__

#include "Connection.h"
#include <map>
#include <vector>
#include <set>

class Channel;


/** Multiplex channels over one Connection: per-channel frames, globals, and broadcast.

	@par Modes
	- Connector: outbound channel setup (init/fill/flush connect data).
	- Acceptor: inbound setup (override makeChannel / acceptChannel).

	@par Connect extras
	Optional init bytes may ride with the handshake for peer-side setup.

	@par Shutdown
	closeChannel() is local-only; agree on teardown at the app protocol.

	@par Channel payloads
	initChannelSendingData + fillSendingData + flushSendingData; inbound via Channel::handleReceived.

	@par Global payloads
	initGlobalSendingData + fillSendingData + flushSendingData; inbound via handleGlobalData.

	@par Broadcast
	initBCSendingData + fillSendingData + flushSendingData to fan out to many channels.
*/
class ChannelConnection : public Connection
{
public:
	/** . */
	enum ProtocolType
	{
		/** Channel. */
		PT_ChanConnect,
		/** Channel*/
		PT_ChanDisconnect,
		/** Channel. */
		PT_ChanData,
		/** Channel. */
		PT_ChanBCData,
		/** Channel. */
		PT_GlobalData,
	};


	/** .
		@param isConnector  ChannelConnection  connector  acceptor.
		@param rbSize buffer
		@param sbSize buffer
		@param bktSize hash bucket size, power of 2(default = 4096).
	*/
	ChannelConnection(
		bool isConnector, 
		size_t rbSize = 0XFFFF, 
		size_t sbSize = 0XFFFF,
		size_t bktSize = 12); 
	/** .
		connectionchannel
		.
	*/
	virtual ~ChannelConnection();

	/**  Channel  fillChannelConnData 
		. 
		@note connector.
		@param c  Channel .channel
				 connection
	*/
	void connect(Channel* c);
	
	/**  Channel .
		 Channel  ChannelConnection .
		@note  Channel .
		@note connector  acceptor .
		@param c  Channel . Channel 
				 .
	*/
	void disconnect(Channel *c);
	/** . */
	void fill(void* data, size_t size);
	/** . */
	void flush();

	/**  Channel .
		 acceptor  Channel 
		 Channel  ChannelConnection
		channel 
		channel.
	*/
	virtual Channel* accept(){ ACE_ASSERT(0); return NULL;}
	
	/**  Channel . */
	void getAllChannels(std::vector<Channel*>& channels);

	/** channel. 
		channel startChannelData 
		fillSendBuffer flushData .
	*/
	void initChannelSendingData(Channel* c);
	/** . 
		 startGlobalData 
		fillSendBuffer flushData .
	*/
	void initGlobalSendingData();
	/** . 
		 startBCData 
		fillSendBuffer flushData .
	*/
	void initBCSendingData(std::set<Channel*>& channels);
	/** . */
	void fillSendingData(void* data, size_t size);
	/** . */
	void flushSendingData();

	/** channel.
		@param data .
		@param size .
		@return .
	*/
	virtual bool handleGlobalData(void* data, size_t size);

protected:
	/** .
		channel, Channel .
	*/
	virtual int handleReceived(void* data, size_t size);

	/** channel. */
	bool addChannel(unsigned int key, Channel* value);
	/** channel. */
	bool removeChannel(unsigned int key);
	/** guid Channel . */
	Channel* findChannel( unsigned int key );
	unsigned int getBucketSize()			{ return 1<<bktSize_; }
	unsigned int getHash(unsigned int key)	{ return key & ( 0XFFFFFFFF>>(32-bktSize_) ); }

	bool			isConnector_;
	unsigned int	guidGen_;
	char*			msgLen_;

	struct Node
	{
		Node(unsigned int k, Channel* c, Node* n): key_(k), chan_(c), next_(n) {}
		unsigned int	key_;
		Channel*	chan_;
		Node*			next_;
	};
	Node**			nodes_;
	unsigned int	bktSize_;
	unsigned int	nodeNum_;
};

#endif//__ChannelConnection_h__
