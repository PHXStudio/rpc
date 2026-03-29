#if defined (_MSC_VER)
	// Disable vc warnings.
	#define _CRT_SECURE_NO_WARNINGS
	#pragma warning(disable:4996)
	#pragma warning(disable:4267)
	#pragma warning(disable:4129)//unrecognized character escape sequence
	#pragma warning(disable:4819)//The file contains a character that cannot be represented in the current code page (936). Save the file in Unicode format to prevent data loss
#endif
#include "ChannelConnection.h"
#include "Channel.h"
#include "ace/OS_NS_string.h"

ChannelConnection::ChannelConnection(bool isC , size_t rbSize, size_t sbSize, size_t bktSize):
Connection(rbSize,sbSize),
isConnector_(isC),
guidGen_(0),
msgLen_(NULL),
nodes_(NULL),
bktSize_(bktSize),
nodeNum_(0)
{
	// allocate 16k memory buckets.
	nodes_ = (Node**)ACE_OS::malloc(getBucketSize()*sizeof(Node*));
	ACE_OS::memset(nodes_, 0, getBucketSize()*sizeof(Node*));
}

ChannelConnection::~ChannelConnection()
{
	// close all channels.
	std::vector<Channel*> channels;
	getAllChannels(channels);
	for(size_t i = 0; i < channels.size(); i++)
		disconnect(channels[i]);
	// free buckets.
	ACE_OS::free(nodes_);
}

void ChannelConnection::connect(Channel* c)
{
	ACE_ASSERT(isConnector_);
	// channel.
	
	if (NULL == c)
	{
		ACE_ASSERT(0);
	}
	
	c->conn_ = this;
	c->guid_ = guidGen_++;
	///@todo guid.
	addChannel(c->guid_, c);

	// Header.
	char type = (char)PT_ChanConnect;
	fill(&type, sizeof(char));
	fill(&(c->guid_), sizeof(unsigned int));
	// .
	msgLen_ = sendBuf_.wr_ptr();
	unsigned int len = 0;
	fill(&len, sizeof(unsigned int));
	flush();
}

void ChannelConnection::disconnect(Channel*c)
{
	if(NULL == c)
		return;
	if(c->conn_ != this)
		return;

	char type = (char)PT_ChanDisconnect;
	fill(&type,sizeof(char));
	fill(&(c->guid_), sizeof(unsigned int));
	msgLen_ = sendBuf_.wr_ptr();
	unsigned int len = 0;
	fill(&len, sizeof(unsigned int));
	flush();
	// channel.
	c->conn_ = NULL;
	removeChannel(c->guid_);
}

void ChannelConnection::fill(void* data, size_t size)
{
	Connection::fill(data, size);
}

void ChannelConnection::flush()
{
	*((unsigned int*)msgLen_) = (unsigned int)((sendBuf_.wr_ptr() - msgLen_) - sizeof(unsigned int));
	// Send whole message.
	Connection::flush();
}

void ChannelConnection::getAllChannels(std::vector<Channel*>& channels)
{
	for(size_t i = 0; i < getBucketSize(); i++)
	{
		Node* n = nodes_[i];
		while(n)
		{
			channels.push_back(n->chan_);
			n = n->next_;
		}
	}
}

void ChannelConnection::initChannelSendingData(Channel* c)
{
	// Header.
	char type = (char)PT_ChanData;
	unsigned int chId = c->guid_;
	fill(&type, sizeof(char));
	fill(&chId, sizeof(unsigned int));
	// .
	msgLen_ = sendBuf_.wr_ptr();
	unsigned int len = 0;
	fill(&len, sizeof(unsigned int));
}

void ChannelConnection::initGlobalSendingData()
{
	// Header.
	char type = (char)PT_GlobalData;
	fill(&type, sizeof(char));
	// .
	msgLen_ = sendBuf_.wr_ptr();
	unsigned int len = 0;
	fill(&len, sizeof(unsigned int));
}

void ChannelConnection::initBCSendingData(std::set<Channel*>& channels)
{
	// Header.
	char type = (char)PT_ChanBCData;
	fill(&type, sizeof(char));
	// channelchannel.
	unsigned int* pNumChan = (unsigned int*)sendBuf_.wr_ptr();
	unsigned int numChan = 0;
	fill(&numChan, sizeof(unsigned int));
	// Channels.
	for(std::set<Channel*>::iterator iter = channels.begin(); iter != channels.end(); ++iter)
	{
		Channel* ch = *iter;
		if(ch == NULL || ch->conn_ != this)
			continue;
		unsigned int guid = ch->guid_;
		fill(&guid, sizeof(unsigned int));
		// channel.
		*pNumChan = *pNumChan + 1;
	}
	// .
	msgLen_ = sendBuf_.wr_ptr();
	unsigned int len = 0;
	fill(&len, sizeof(unsigned int));
}

void ChannelConnection::fillSendingData(void* data, size_t size)
{
	fill(data, size);
}

void ChannelConnection::flushSendingData()
{
	*((unsigned int*)msgLen_) = (unsigned int)((sendBuf_.wr_ptr() - msgLen_) - sizeof(unsigned int));
	// Send whole message.
	flush();
}

bool ChannelConnection::handleGlobalData(void* data, size_t size)
{
	return true;
}

int ChannelConnection::handleReceived(void* data, size_t size)
{
	int handled = 0;
	while(1)
	{
		char* curData = (char*)data + handled;
		size_t curDataSize = size - handled;

		// 
		if(curDataSize < sizeof(char))
			return handled;
		char* type = (char*)curData;

		// .
		switch(*type)
		{
		case PT_ChanConnect:
			{
				if(isConnector_)
				{
					// .
					return -1;
				}

				// .
				size_t hdrSize = sizeof(char) + sizeof(unsigned int)*2;
				if(curDataSize < hdrSize)
					return handled;
				unsigned int* chId	= (unsigned int*)(curData+1);
				unsigned int* len	= chId+1;

				// .
				if(curDataSize < hdrSize + *len)
					return handled;

				// 
				if(findChannel(*chId))
				{
					// channel.
					return -1;
				}
				Channel* ch = accept();
				if(ch)
				{
					ch->conn_ = this;
					ch->guid_ = *chId;
					addChannel(*chId, ch);
				}
				handled += hdrSize + *len;
			}
			break;
		case PT_ChanDisconnect:
			{
				size_t hdrSize = sizeof(char) + sizeof(unsigned int)*2;
				if(curDataSize < hdrSize)
					return handled;
				unsigned int* chId	= (unsigned int*)(curData+1);
				unsigned int* len	= chId+1;

				// .
				if(curDataSize < hdrSize + *len)
					return handled;

				// 
				Channel* ch = findChannel(*chId);
				if(ch)
				{
					removeChannel(*chId);
					ch->handleClose();
				}
				else
					return -1;
				handled += hdrSize + *len;
			}
			break;
		case PT_ChanData:
			{
				// .
				size_t hdrSize = sizeof(char) + sizeof(unsigned int)*2;
				if(curDataSize < hdrSize)
					return handled;
				unsigned int* chId	= (unsigned int*)(curData+1);
				unsigned int* len	= chId+1;

				// .
				if(curDataSize < hdrSize + *len)
					return handled;

				// 
				Channel* ch = findChannel(*chId);
				if(ch)
				{
					if(!ch->handleReceived(curData+hdrSize, *len))
						disconnect(ch);
				}
				handled += hdrSize + *len;
			}
			break;
		case PT_ChanBCData:
			{
				// .
				size_t hdrSize = sizeof(char) + sizeof(unsigned int);
				if(size - handled < hdrSize)
					return handled;
				unsigned int* numChan	= (unsigned int*)(curData+1);
				unsigned int* chanIds	= (unsigned int*)(curData + hdrSize);
				hdrSize += (*numChan)*sizeof(unsigned int) + sizeof(unsigned int);
				if(curDataSize < hdrSize)
					return handled;
				unsigned int* len		= (unsigned int*)(curData + hdrSize - sizeof(unsigned int));
				if(curDataSize < hdrSize + *len)
					return handled;
				// .
				void* bcdata			= curData + hdrSize;
				for(unsigned int i = 0; i < *numChan; i++)
				{
					Channel* ch = findChannel(chanIds[i]);
					if(ch)
					{
						if(!ch->handleReceived(bcdata, *len))
							disconnect(ch);
					}
				}
				handled += hdrSize + *len;
			}
			break;
		case PT_GlobalData:
			{
				// .
				size_t hdrSize = sizeof(char) + sizeof(unsigned int);
				if(curDataSize < hdrSize)
					return handled;
				unsigned int* len = (unsigned int*)(curData+1);
				if(curDataSize < hdrSize + *len)
					return handled;
				if(!handleGlobalData(curData+sizeof(char)+sizeof(unsigned int), *len))
					return -1;
				handled += hdrSize + *len;
			}
			break;
		default:
			{
				// .
				return -1;
			}
		}
	}

	// Never goes here.
	ACE_ASSERT(0);
	return handled;
}

bool ChannelConnection::addChannel(unsigned int key, Channel* value)
{
	Node* n = new Node(key, value, NULL);
	unsigned int hid = getHash(key);
	Node** hash_n = &(nodes_[hid]);
	while(*hash_n)
	{
		if((*hash_n)->key_ == key)
		{
			delete n;
			return false;
		}
		hash_n = &((*hash_n)->next_);
	}
	*hash_n = n;
	nodeNum_++;
	return true;
}

bool ChannelConnection::removeChannel(unsigned int key)
{
	unsigned int hid = getHash(key);
	Node** hash_n = &(nodes_[hid]);
	while(*hash_n)
	{
		if((*hash_n)->key_ == key)
		{
			Node* n = (*hash_n);
			(*hash_n) = (*hash_n)->next_;
			delete n;
			nodeNum_--;
			return true;
		}
		hash_n = &((*hash_n)->next_);
	}
	return false;
}

Channel* ChannelConnection::findChannel(unsigned int key)
{
	unsigned int hid = getHash(key);
	Node* hash_n = nodes_[hid];
	while(hash_n)
	{
		if(hash_n->key_ == key)
			return hash_n->chan_;
		hash_n = hash_n->next_;
	}
	return NULL;
}