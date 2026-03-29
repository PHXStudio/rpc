#ifndef __ChannelBroadcaster_h__
#define __ChannelBroadcaster_h__

#include "ChannelConnection.h"
#include <set>
#include <map>

/**  CHANNEL .
	
	ChannelBroadcaster  CHANNEL  ChannelBroadcaster 
	 CHANNEL 

	ChannelBroadcaster  ChannelConnection 
*/
template<class CHANNEL>
class ChannelBroadcaster 
{
public:
	ChannelBroadcaster()
	{
	}

	virtual ~ChannelBroadcaster()
	{
	}

	/** channel.
		@warning channelChannelBroadcaster
				 .
	*/
	void addChannel(CHANNEL* ch)
	{
		ACE_ASSERT(ch->isValid());
		ChannelConnection* conn = ch->getConn();
		ACE_ASSERT(conn);
		// channelconnection.
		std::pair<typename ConnToChanMap::iterator, bool> r = 
			connToChans_.insert(std::pair<ChannelConnection*, ChannelSet>(conn, ChannelSet()));
		r.first->second.insert(ch);
	}

	/** channel. */
	void removeChannel(CHANNEL* ch)
	{
		ACE_ASSERT(ch->isValid());
		ChannelConnection* conn = ch->getConn();
		ACE_ASSERT(conn);
		typename ConnToChanMap::iterator iter = connToChans_.find(conn);
		ACE_ASSERT(iter != connToChans_.end());
		int r = iter->second.erase(ch);
		ACE_ASSERT(r == 1);
		if(iter->second.size() == 0)
			connToChans_.erase(iter);
	}

	/** channel. */
	void clearChannels()
	{
		connToChans_.clear();
	}

	/** . 
		connectionchannel fillSendingData 
		.
	*/
	void initSendingData()
	{
		for(typename ConnToChanMap::iterator iter = connToChans_.begin(); iter != connToChans_.end(); ++iter)
		{
			ChannelConnection* conn = iter->first;
			conn->initBCSendingData((std::set<Channel*>&)iter->second);
		}
	}

	/** . 
		connection flushSendingData
		.
	*/
	void fillSendingData(void* data, size_t size)
	{
		for(typename ConnToChanMap::iterator iter = connToChans_.begin(); iter != connToChans_.end(); ++iter)
		{
			ChannelConnection* conn = iter->first;
			conn->fillSendingData(data, size);
		}
	}

	/** . 
		connection.
	*/
	void flushSendingData()
	{
		for(typename ConnToChanMap::iterator iter = connToChans_.begin(); iter != connToChans_.end(); ++iter)
		{
			ChannelConnection* conn = iter->first;
			conn->flushSendingData();
		}
	}

	typedef std::set<CHANNEL*> ChannelSet;
	typedef std::map<ChannelConnection*, ChannelSet>	ConnToChanMap;
	ConnToChanMap	connToChans_;
};

#endif//__ChannelBroadcaster_h__
