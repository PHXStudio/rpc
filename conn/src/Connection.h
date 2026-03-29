#ifndef __SVC_EventHandler_h__
#define __SVC_EventHandler_h__

#include <vector>
#include "ace/Event_Handler.h"
#include "ace/SOCK_Stream.h"
#include "ace/Message_Block.h"


/** TCP stream: ACE_Event_Handler + ACE_SOCK_Stream + ACE_Message_Block buffers.

	@par Setup
	Works with ACE_Acceptor (passive) and ACE_Connector (active).

	@par State
	Starts Unestablished; becomes Established after accept/connect. Failures move to error states.
	Only Established is usable; use getStatus().

	@par Send
	Call fill() then flush(); flush may wait for writability if data remains.

	@par Recv
	Override handleReceived(); return bytes consumed or -1 on error. Connection owns recv buffering.
*/
class Connection : public ACE_Event_Handler
{
public:
	typedef ACE_SOCK_Stream	stream_type;
	typedef ACE_INET_Addr	addr_type;

	/** .
		@param rbSize buffer.
		@param sbSize buffer.
	*/
	Connection(size_t rbSize = 0XFFFF, size_t sbSize = 0XFFFF);

	/** . */
	virtual ~Connection();

	/** . */
	enum Status
	{
		/** connectaccept.
			.
		*/
		Established,
		/** connectaccept. */
		Unestablished,
		/** buffer. */
		RBOverflow,
		/** buffer. */
		SBOverflow,
		/** recv. errno. */
		RecvFailed,
		/** send. errno. */
		SendFailed,
		/** close. */
		LocalClosed,
		/** . */
		RemoteClosed,
		/** . */
		MsgProcFailed,
	};
	/** . */
	Status getStatus()						{ return status_; }
	/** . */
	const char* getStatusDesc();
	/** errno. */
	int getErrno()							{ return errno_; }
	/** socket stream. (ACE_Acceptor  ACE_Connector) */
	ACE_SOCK_Stream& peer()					{ return stream_; }
	/** . */
	const ACE_INET_Addr& getLocalAddr()		{ return localAddr_; }
	/** . */
	const ACE_INET_Addr& getRemoteAddr()	{ return remoteAddr_; }
	/** . */
	ACE_UINT64 getTotalReadBytes()			{ return totalRBytes_; }
	/** . */
	ACE_UINT64 getTotalWriteBytes()			{ return totalWBytes_; }

	/**  ACEConnector  ACE_Acceptor .
		, 
		 Established.
		@return 0-1(close)
	*/
	virtual int open (void * = 0);

	/** .
		LocalClosed.
	*/
	virtual int close (u_long flags = 0);

	/**	send buffer.
		 flushSendBuffer.
		@param data .
		@param size .
	*/
	virtual void fill(void* data, size_t size);

	/** sendbuffer. 
		.
	*/
	virtual void flush();

	/** .
		
		@param data .
		@param size .
		@return . -1.
	*/
	virtual int handleReceived(void* data, size_t size) { return size; }

public:
	/** ACE_Event_Handler Interface. */
	virtual int handle_input(ACE_HANDLE fd);
	/** ACE_Event_Handler Interface. */
	virtual int handle_output(ACE_HANDLE fd);
	/** ACE_Event_Handler Interface. */
	virtual int handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);
protected:
	/** . */
	void clear();

	Status					status_;		///< .
	int						errno_;			///< .
	ACE_SOCK_Stream			stream_;		///< tcp socket steam. 
	ACE_INET_Addr			localAddr_;		///< .
	ACE_INET_Addr			remoteAddr_;	///< .
	ACE_Message_Block		sendBuf_;		///< buffer.
	ACE_Message_Block		recvBuf_;		///< buffer.
	ACE_UINT64				totalRBytes_;	///< .
	ACE_UINT64				totalWBytes_;	///< .
};

/** . 
	
	ConnectionManager  CONN Connection
	 ConnectionManager  destroyInvalidConnections()
	 CONN  ConnectionManager 
	 CONN 
*/
template<class CONN>
class ConnectionManager
{
public:
	typedef std::vector<CONN*> ConnectionList;

	ConnectionManager() 
	{
	}

	virtual ~ConnectionManager() 
	{
		destroyAllConnections(); 
	}

	/** . */
	void destroyInvalidConnections()
	{
		for(typename ConnectionList::iterator iter = connections_.begin(); iter != connections_.end();)
		{
			if(!checkConnection(*iter))
			{
				delete *iter;
				iter = connections_.erase(iter);
			}
			else
				++iter;
		}
	}

	/** . */
	void destroyAllConnections()
	{
		for(typename ConnectionList::iterator iter = connections_.begin(); iter != connections_.end(); ++iter)
			delete *iter;
		connections_.clear();
	}

protected:
	/** . 
		
		.
		@return true .
	*/
	virtual bool checkConnection(CONN* c)
	{
		return (c->getStatus() == Connection::Established);
	}

	ConnectionList connections_;
};

#endif//__SVC_EventHandler_h__