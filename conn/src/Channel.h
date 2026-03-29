#ifndef __Channel_h__
#define __Channel_h__

#include <cstddef>

class ChannelConnection;

/** Logical duplex link over a shared ChannelConnection.

	Channel looks like an independent connection: send with the *SendingData helpers and
	override handleReceived for inbound data.

	All channels on the same transport share one ChannelConnection. Each channel has a guid
	used to multiplex messages on the wire.
*/
class Channel
{
public:
	friend class ChannelConnection;

	/** Default ctor: not bound to a ChannelConnection until wired by ChannelConnection. */
	Channel();

	/** Destructor calls close on the channel side. */
	virtual ~Channel();

	/** Current ChannelConnection, if any.
		@return NULL if not attached.
	*/
	ChannelConnection* getConn()	{ return conn_; }

	/** True when this channel is attached to a live ChannelConnection. */
	bool isValid();

	/** Start a send batch. */
	void fillBegin();
	/** Append payload bytes. */
	void fill(void* data, size_t size);
	/** Finish and send the batch. */
	void fillEnd();

	/** Inbound data from the peer.
		@param data Payload pointer.
		@param size Payload length.
		@return false if processing failed.
	*/
	virtual bool handleReceived( void* data, size_t size )	{ return true; }
	virtual bool handleConnect(){return true;}
	virtual bool handleClose();

protected:
	unsigned int			guid_;
	ChannelConnection*	conn_;
};

#endif//__Channel_h__
