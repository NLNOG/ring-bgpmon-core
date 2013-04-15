/* 
 * 	Copyright (c) 2010 Colorado State University
 * 
 *	Permission is hereby granted, free of charge, to any person
 *      obtaining a copy of this software and associated documentation
 *	files (the "Software"), to deal in the Software without
 *	restriction, including without limitation the rights to use,
 *	copy, modify, merge, publish, distribute, sublicense, and/or
 *	sell copies of the Software, and to permit persons to whom
 *	the Software is furnished to do so, subject to the following
 *	conditions:
 *
 *	The above copyright notice and this permission notice shall be
 *	included in all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *	OTHER DEALINGS IN THE SOFTWARE.
 * 
 * 
 *  	File: chaininstance.c
 *  	Authors:  He Yan, Dan Massey
 *  	Date: Oct 13, 2008 
 *	Implementation of chaining BGPMons together in order to achieve scalability. 
 */

/* externally visible structures and functions for clients */
#include "chains.h"
/* internal structures and functions for launching chains */
#include "chaininstance.h"

/* required for logging functions */
#include "../Util/log.h"

/* needed for ADDR_MAX_CHARS */
#include "../site_defaults.h"

/* required for TRUE/FALSE and chainXMLBufferLength  */
#include "../Util/bgpmon_defaults.h"

/* needed for address management  */
#include "../Util/address.h"

/* required for writing to the XML queue */
#include "../Queues/queue.h"

/* needed for writern and readn socket operations */
#include "../Util/unp.h"

/* needed for function getXMLMessageLen */
#include "../XML/xml.h"

/* needed for malloc and free */
#include <stdlib.h>
/* needed for strncpy */
#include <string.h>
/* needed for system error codes */
#include <errno.h>
/* needed for system types such as time_t */
#include <sys/types.h>
/* needed for time function */
#include <time.h>
/* needed for pthread related functions */
#include <pthread.h>
//needed for UINT_MAX
#include <limits.h>

//#define DEBUG

#ifndef MAX
#define MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

//pthread_mutex_t chainDictMutex = PTHREAD_MUTEX_INITIALIZER;

/*--------------------------------------------------------------------------------------
 * Purpose: Create a chain data structure
 * Input:  address, port, enabled flag and retry interval.
 * Output: id of the new chain or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int
createChainStruct( char *addr, int Uport, int Rport, int enabled, int connectRetryInterval )
{

	// find a empty slot for this new chain
	int i;
	for(i = 0; i < MAX_CHAIN_IDS; i++)
	{
		if(Chains[i] == NULL)
			break;
	}

	// if the number of chains exceeds MAX_CHAIN_IDS, return -1
	if( i == MAX_CHAIN_IDS )
	{
	 	log_err("Unable to create new chain for %s:  max chains exceeded", addr);
		return -1;
	}

	// Returns a new chain or NULL if not enough memory.
 	Chain_structp chain = malloc( sizeof( struct ChainStruct ) );
	if( chain == NULL )
	{
		log_err("createChain: malloc failed");
		return -1;
	}

	// set the structure values
	memset(chain, 0, sizeof( struct ChainStruct) );
	chain->chainID = i;

	// configuration settings
	strncpy(chain->addr, addr, ADDR_MAX_CHARS);
	chain->Uport = Uport;
	chain->Rport = Rport;	
	chain->enabled = enabled;
	chain->connectRetryInterval = connectRetryInterval;	

	// thread control
	chain->deleteChain = FALSE;
	chain->reconnectFlag = FALSE;
	chain->runningFlag = FALSE;
	chain->lastAction = 0;

	//socket related fields
	chain->Usocket = -1;
	chain->Userrno = 0;
	chain->UconnectRetryCounter = 0;
	chain->UconnectionState = chainStateIdle;

	chain->Rsocket = -1;
	chain->Rserrno = 0;
	chain->RconnectRetryCounter = 0;
	chain->RconnectionState = chainStateIdle;
	
	// up time, peer reset counter, number of messages
	chain->UestablishedTime = 0;
	chain->UlastDownTime = 0;
	chain->UresetCounter = 0;
	chain->UmessageRcvd = 0;

	chain->RestablishedTime = 0;
	chain->RlastDownTime = 0;
	chain->RresetCounter = 0;
	chain->RmessageRcvd = 0;
	
	// periodic check for configuration changes
	chain->periodicCheckInt = THREAD_CHECK_INTERVAL;

	// xml queue writer
	chain->UxmlQueueWriter = NULL;
	chain->RxmlQueueWriter = NULL;
	
	Chains[i] = chain;

#ifdef DEBUG
	debug(__FUNCTION__, "Created structure %d and assigned id %d",addr,i);
#endif
	
	return i;
}

/*--------------------------------------------------------------------------------------
 * Purpose: entry function of a chain thread
 * Input:  pointer to a chain structure
 * Output: 
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void *
chainThread( void *arg )
{
	Chain_structp chain = arg;
	log_msg("thread started for chain %d to %s Update port %d, RIB port %d", chain->chainID, chain->addr, chain->Uport, chain->Rport);
	
	// create the xml queue writer
	chain->UxmlQueueWriter = createQueueWriter(xmlUQueue);
	if (chain->UxmlQueueWriter == NULL)
	{
		log_err("chain thead %d failed to create Update Queue Writer. chain thread exiting.", chain->chainID);
		chain->runningFlag = FALSE;
		pthread_exit( NULL );
	}
	
	chain->RxmlQueueWriter = createQueueWriter(xmlRQueue);
	if (chain->RxmlQueueWriter == NULL)
	{
		log_err("chain thead %d failed to create RIB Queue Writer. chain thread exiting.", chain->chainID);
		chain->runningFlag = FALSE;
		pthread_exit( NULL );
	}	
	

	// set the running flag
	chain->runningFlag = TRUE;
	chain->lastAction = time(NULL);

	// main loop starts
	while(chain->enabled == TRUE )
	{  	  	
		// establish the connection
		if (establishConnection(chain) == TRUE )
			// read from the socket
			if( manageConnection(chain) < 0 )
				sleep(chain->connectRetryInterval);
	}

	log_msg("closing thread for chain %d to %s port %d, %d", chain->chainID, chain->addr, chain->Uport, chain->Rport);

	//chain has been disabled so close the socket
	if (chain->Usocket != -1) 
		close(chain->Usocket); 
	if (chain->Rsocket != -1) 
		close(chain->Usocket);
	
	// if the chain is to be destroyed, wipe out everything
	if (chain->deleteChain == TRUE) 
		destroyChain(chain);
	// otherwise just cleanup the chain structure
	else
		cleanupChainStruct(chain);

	pthread_exit( NULL );
}

/*-------------------------------------------------------------------------------------- 
 * Purpose: establish a chain connection.   if the connection succeeeds,
 *          the function returns true.    otherwise the fucntion sleeps
 *          for the retry timer and then returns FALSE.
 * Input:  the chain structure 
 * Output: TRUE on successfully building a connection 
 *         or sleep for retry time and return FALSE if the connection failed
 * He Yan @ July 22, 2008 
 * -------------------------------------------------------------------------------------*/
int
establishConnection ( Chain_structp chain ) 
{

#ifdef DEBUG
	//debug(__FUNCTION__, "Establishing connection for chain %d to %s port %d, %d",chain->ChainID, chain->addr, chain->Uport, chain->Rport);
#endif
	chain->reconnectFlag = FALSE;
	chain->lastAction = time(NULL);

	// Update SOCKET
	chain->UconnectionState = chainStateConnecting;
	chain->UconnectRetryCounter++;
	// create addrinfo struct
	struct addrinfo *Ures = createAddrInfo(chain->addr, chain->Uport);
	if( Ures == NULL )
	{
		// could not create an address??
		log_err("Unable to create address for Update chain %d to %s port %d", chain->chainID, chain->addr, chain->Uport);
		chain->UconnectionState = chainStateIdle;
		sleep(chain->connectRetryInterval);
		return FALSE;
	}

	// create the socket
	chain->Usocket = socket(Ures->ai_family, Ures->ai_socktype, Ures->ai_protocol);
	if ( chain->Usocket == -1 )
	{
		// could not create a socket??
		chain->Userrno = errno;
		log_err("Unable to create socket for Update chain %d to %s port %d", chain->chainID, chain->addr, chain->Uport);
		chain->UconnectionState = chainStateIdle;
		sleep(chain->connectRetryInterval);
		return FALSE;
	}


	// setup a receive timeout on the socket
	struct timeval tv;
	tv.tv_sec = chain->periodicCheckInt;
	tv.tv_usec = 0;	
	setsockopt(chain->Usocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	
	// connect to the remote chain	
	int Uconnection = connect(chain->Usocket, Ures->ai_addr, Ures->ai_addrlen);
	freeaddrinfo(Ures);
        Ures=NULL;
	if (Uconnection == -1)
	{
		// connection failed
		log_warning("Update Chain %d to %s port %d tcp connection error:%s", chain->chainID, chain->addr, chain->Uport, strerror(errno));
		close(chain->Usocket);
		chain->UconnectionState = chainStateIdle;
		sleep(chain->connectRetryInterval);
		return FALSE;
	}

	log_msg("Established connection for Update chain %d to %s port %d",chain->chainID, chain->addr, chain->Uport);

	chain->UconnectionState = chainStateConnected;
	chain->UestablishedTime = time(NULL);
	chain->UmessageRcvd = 0;
	

	// RIB SOCKET
	chain->RconnectionState = chainStateConnecting;
	chain->RconnectRetryCounter++;
	
	// create addrinfo struct
	struct addrinfo *Rres = createAddrInfo(chain->addr, chain->Rport);
	if( Rres == NULL )
	{
		// could not create an address??
		log_err("Unable to create address for RIB chain %d to %s port %d", chain->chainID, chain->addr, chain->Rport);
		chain->RconnectionState = chainStateIdle;
		sleep(chain->connectRetryInterval);
		return FALSE;
	}

	// create the socket
	chain->Rsocket = socket(Rres->ai_family, Rres->ai_socktype, Rres->ai_protocol);
	if ( chain->Rsocket == -1 )
	{
		// could not create a socket??
		chain->Rserrno = errno;
		log_err("Unable to create socket for RIB chain %d to %s port %d", chain->chainID, chain->addr, chain->Rport);
		chain->RconnectionState = chainStateIdle;
		sleep(chain->connectRetryInterval);
		return FALSE;
	}	
	
	//struct timeval tv;
	tv.tv_sec = chain->periodicCheckInt;
	tv.tv_usec = 0;
	setsockopt(chain->Rsocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));	

	
	// connect to the remote chain	
	int Rconnection = connect(chain->Rsocket, Rres->ai_addr, Rres->ai_addrlen);
	freeaddrinfo(Rres);
        Rres = NULL;
	if (Rconnection == -1)
	{
		// connection failed
		log_warning("RIB Chain %d to %s port %d tcp connection error:%s", chain->chainID, chain->addr, chain->Rport, strerror(errno));
		close(chain->Rsocket);
		chain->RconnectionState = chainStateIdle;
		sleep(chain->connectRetryInterval);
		return FALSE;
	}

	log_msg("Established connection for RIB chain %d to %s port %d",chain->chainID, chain->addr, chain->Rport);

	chain->RconnectionState = chainStateConnected;
	chain->RestablishedTime = time(NULL);
	chain->RmessageRcvd = 0;	
	
	
	return TRUE;
				
}

/*-------------------------------------------------------------------------------------- 
 * Purpose: manage a chain connection.  continually check for new messages 
 *          and periodically check to see if the connection should be closed
 * Input:  the chain structure 
 * Output: returns 0 if the connection was closed by the user
 *         returns -1 if the connection was closed due to an error
 * He Yan @ July 22, 2008 
 * -------------------------------------------------------------------------------------*/
int
manageConnection ( Chain_structp chain ) 
{

#ifdef DEBUG
	debug(__FUNCTION__,"Managing connection for chain %d to %s Update port %d, Rib port",chain->chainID, chain->addr, chain->Uport, chain->Rport);
#endif

	int UfirstRead = TRUE;
	int RfirstRead = TRUE;
	char buf[6] = {0};
	// make sure we have a live socket
	if (  chain->Usocket  < 0 )
	{
		log_warning("manageConnection for Update chain %d called without a valid socket connection", chain->chainID);
		closeChainSocket(chain);
		return -1;
	}
	
	// make sure we have a live socket
	if (  chain->Rsocket  < 0 )
	{
		log_warning("manageConnection for RIB chain %d called without a valid socket connection", chain->chainID);
		closeChainSocket(chain);
		return -1;
	}
	
	// setup the select function and timeout
	fd_set fds;
	int fdMax = 0;
	int select_status = 0;	
	FD_ZERO(&fds); 
	FD_SET( chain->Usocket, &fds);
	FD_SET( chain->Rsocket, &fds);
	fdMax = MAX(chain->Usocket,chain->Rsocket) + 1;

	// timeout periodically to check enabled and reconnect flags
	struct timeval timeout; 
	timeout.tv_sec =  chain->periodicCheckInt;
	timeout.tv_usec =  0;


	// loop until user exits or error
	while( (chain->enabled == TRUE) && (chain->reconnectFlag == FALSE) ) 
	{ 
		timeout.tv_sec =  chain->periodicCheckInt;
		FD_ZERO(&fds); 
		FD_SET( chain->Usocket, &fds);
		FD_SET( chain->Rsocket, &fds);
		fdMax = MAX(chain->Usocket,chain->Rsocket) + 1;
	
		chain->lastAction = time(NULL);
		// select will return 1 if data to read or 0 for timeout
		select_status =  select(fdMax, &fds, NULL, NULL, &timeout);
		if (select_status < 0)
		{
			log_err("Select failed !\n");
			closeChainSocket(chain);
			return -1;
		}
		if (select_status > 0)
		{
			if ( FD_ISSET (chain->Rsocket, &fds) )
			{
					// if it is the first read, discard the first 5 bytes for open tag <xml>
					if ( RfirstRead == TRUE )
					{
						recv(chain->Rsocket, buf, 5, MSG_PEEK);
						// check if the first 5 bytes are <xml>, for back compatability
						if ( strcmp(buf, "<xml>") == 0 )
							readn( chain->Rsocket, buf, 5 );
						RfirstRead =FALSE;
					}
					if ( readMessage(chain, RIB_STREAM_CHAIN) )	
					{
						//some sort of read error, close connection
						log_err("Socket read from RIB chain %d at %s port %d failed", chain->chainID, chain->addr, chain->Rport);
						closeChainSocket(chain);
						return -1;
					}
	
			}
			if (FD_ISSET(chain->Usocket, &fds))
			{
					// if it is the first read, discard the first 5 bytes for open tag <xml>
					if ( UfirstRead == TRUE )
					{
						recv(chain->Usocket, buf, 5, MSG_PEEK);
						// check if the first 5 bytes are <xml>, for back compatability
						if ( strcmp(buf, "<xml>") == 0 )
							readn( chain->Usocket, buf, 5 );
						UfirstRead =FALSE;
					}
					if ( readMessage(chain, UPDATE_STREAM_CHAIN) )	
					{
						//some sort of read error, close connection
						log_err("Socket read from Update chain %d at %s port %d failed", chain->chainID, chain->addr, chain->Uport);
						closeChainSocket(chain);
						return -1;
					}
			}
		}
	}

	// user has requested to disable or reconnect
	log_msg("Closing connection for Update chain %d to %s port %d",chain->chainID, chain->addr, chain->Uport);
	log_msg("Closing connection for RIB chain %d to %s port %d",chain->chainID, chain->addr, chain->Rport);
	closeChainSocket(chain);
	return 0;
}

/*-------------------------------------------------------------------------------------- 
 * Purpose: read a message from the remote chain
 * Input:  the chain structure, Update or RIB const from manageConnection function
 * Output: returns 0 if the message was read
 *         returns -1 on error
 * He Yan @ July 22, 2008 
 * -------------------------------------------------------------------------------------*/
int
readMessage ( Chain_structp chain, int chain_stream ) 
{

	if (chain_stream == UPDATE_STREAM_CHAIN)
	{
	
		if (chain == NULL) 
		{
			log_err("Read message called with NULL chain.");
			return -1;
		}

		// read first XML_MESSAGE_HEADER_SIZE bytes from socket. 
		// The length of the message must be included within these bytes.
		int len = recv(chain->Usocket, chain->UmsgHeaderBuf, XML_MSG_HEADER_SIZE, MSG_PEEK);
		// check something was read
		if (len <= 0 )
		{
			return -1;
		}
		else if (len < XML_MSG_HEADER_SIZE )
		{
			//log_msg("Read Update chain %d at %s port %d read only %d bytes, waiting for %d bytes", chain->chainID, chain->addr, chain->port, len, XML_MSG_HEADER_SIZE);
			return 0;
		}

		// get the total length of the message
		int msgLen = getXMLMessageLen(chain->UmsgHeaderBuf);
		char *msg = malloc(msgLen);
		if (msg == NULL)
		{
			log_err("Read Update chain %d at %s port %d realloc failed", chain->chainID, chain->addr, chain->Uport);
			return -1;
		}	

		// read the the message from socket.
		len = readn(chain->Usocket, msg, msgLen); 
		// check something was read
		if (len !=  msgLen )
		{
			log_err("Read Update chain %d at %s port %d expected %d bytes but read %d", chain->chainID, chain->addr, chain->Uport, msgLen, len);
			return -1;
		}

		//get BGPmon ID and sequence number of new message
		u_int32_t msgID = -1;
		u_int32_t msgSeq = -1;
		if(getMsgIdSeq(msg,msgLen,&msgID,&msgSeq)){
			if(msgLen > 0) writeQueue(chain->UxmlQueueWriter,msg);
			else log_err("Chain %d attempted to parse an invalid/old message.", chain->chainID);
			return 0;
		}

		chainOwnerCachep entry = getCacheEntry(msgID);
		if(entry == NULL){	//if we found no entry, create one and forward the message
			addCacheEntry(msgID,chain->chainID,msgSeq);
			writeQueue(chain->UxmlQueueWriter,msg);
		}
		else{
			//if this chain owns the ID, update the entry and forward the message
			if(entry->owner == chain->chainID){
				pthread_mutex_lock(&entry->cache_mutex);
				entry->timestamp = time(NULL);
				entry->seq = msgSeq;
				pthread_mutex_unlock(&entry->cache_mutex);
				writeQueue(chain->UxmlQueueWriter,msg);
			}
			//otherwise drop the message
			else free(msg);
		}

#ifdef DEBUG
		debug(__FUNCTION__, "Update Message from %d: %s\n", chain->chainID, msg );	
#endif 

		return 0;
	}
	
	if (chain_stream == RIB_STREAM_CHAIN)
	{
	
		if (chain == NULL) 
		{
			log_err("Read message called with NULL chain.");
			return -1;
		}

		// read first XML_MESSAGE_HEADER_SIZE bytes from socket. 
		// The length of the message must be included within these bytes.
		int len = recv(chain->Rsocket, chain->RmsgHeaderBuf, XML_MSG_HEADER_SIZE, MSG_PEEK);
		// check something was read
		if (len <= 0 )
		{
			return -1;
		}
		else if (len < XML_MSG_HEADER_SIZE )
		{
			//log_msg("Read chain %d at %s port %d read only %d bytes, waiting for %d bytes", chain->chainID, chain->addr, chain->port, len, XML_MSG_HEADER_SIZE);
			return 0;
		}

		// get the total length of the message
		int msgLen = getXMLMessageLen(chain->RmsgHeaderBuf);
		char *msg = malloc(msgLen);
		if (msg == NULL)
		{
			log_err("Read RIB chain %d at %s port %d realloc failed", chain->chainID, chain->addr, chain->Rport);
			return -1;
		}	

		// read the the message from socket.
		len = readn(chain->Rsocket, msg, msgLen); 
		// check something was read
		if (len !=  msgLen )
		{
			log_err("Read RIB chain %d at %s port %d expected %d bytes but read %d", chain->chainID, chain->addr, chain->Uport, msgLen, len);
			return -1;
		}

		//get BGPmon ID and sequence number of new message
		u_int32_t msgID = -1;
		u_int32_t msgSeq = -1;
		if(getMsgIdSeq(msg,msgLen,&msgID,&msgSeq)){
			if(msgLen > 0) writeQueue(chain->RxmlQueueWriter,msg);
			else log_err("Chain %d attempted to parse an invalid/old message.", chain->chainID);
			return 0;
		}

		chainOwnerCachep entry = getCacheEntry(msgID);
		if(entry == NULL){	//if we found no entry, create one and forward the message
			addCacheEntry(msgID,chain->chainID,msgSeq);
			writeQueue(chain->RxmlQueueWriter,msg);
		}
		else{
			//if this chain owns the ID, update the entry and forward the message
			if(entry->owner == chain->chainID){
				pthread_mutex_lock(&entry->cache_mutex);
				entry->timestamp = time(NULL);
				entry->seq = msgSeq;
				pthread_mutex_unlock(&entry->cache_mutex);
				writeQueue(chain->RxmlQueueWriter,msg);
			}
			//otherwise drop the message
			else free(msg);
		}
		return 0;
	}	
	
	return -1;
	
}

/*-------------------------------------------------------------------------------------
 * Purpose:	Get the location in the cache of the msgCache for a given ID
 * Input:	The ID of the message in question
 * Output:	A pointer to the appropriate entry in the list, or NULL if none is found
 * Jason Bartlett @ 28 Feb 2011
 *------------------------------------------------------------------------------------*/
chainOwnerCachep getCacheEntry(u_int32_t new_id){
	chainOwnerCachep curr;
	for(curr = cacheHead; curr != NULL; curr = curr->next){
		if(curr->id == new_id) return curr;
	}
	return curr;	//will return NULL if no entry is found
}

/*-------------------------------------------------------------------------------------
 * Purpose:	Add a new entry to the chain-ownership cache
 * Input:	The ID and sequence number of the message in question and the chain ID
 * Output:	none
 * Jason Bartlett @ 28 Feb 2011
 *------------------------------------------------------------------------------------*/
int addCacheEntry(u_int32_t msgID,int ownerChain,u_int32_t msgSeq){
	//create a new entry for the cache
	chainOwnerCachep new_entry = malloc(sizeof(struct chainOwnerCache));
	pthread_mutex_init(&new_entry->cache_mutex,NULL);
	new_entry->id = msgID;
	new_entry->seq = msgSeq;
	new_entry->owner = ownerChain;
	new_entry->next = NULL;

	//now add it to the linked list
	chainOwnerCachep curr = cacheHead;
	if(cacheHead == NULL) cacheHead = new_entry;
	else{
		while(curr->next != NULL) curr = curr->next;
		curr->next = new_entry;
	}

	return 0;
}

/*-------------------------------------------------------------------------------------- 
 * Purpose: close the socket for a connected chain.
 * Input:  the chain structure 
 * Output: 
 * He Yan @ July 22, 2008 
 * -------------------------------------------------------------------------------------*/
void
closeChainSocket ( Chain_structp chain ) 
{
	if (chain == NULL)
		return;
	chain->lastAction = time(NULL);
	close(chain->Usocket);
	close(chain->Rsocket);	
	chain->Usocket = -1;
	chain->Rsocket = -1;	
	chain->UconnectionState = chainStateIdle;
	chain->RconnectionState = chainStateIdle;
	chain->UlastDownTime = time(NULL);
	chain->RlastDownTime = time(NULL);
	chain->UresetCounter++;
	chain->RresetCounter++;	
	chain->UmessageRcvd = 0;
	chain->RmessageRcvd = 0;	
	return;
}

/*--------------------------------------------------------------------------------------
 * Purpose: destroy the memory associated with a chain
 * Input:       the chain structure to destroy
 * Output:
 * * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void
destroyChain( Chain_structp chain)
{
	cleanupChainStruct(chain);
	Chains[chain->chainID] = NULL;
	free(chain);
}

/*-------------------------------------------------------------------------------------- 
 * Purpose: Cleanup a chain data structure so the thread can be closed
 *           sets the state to IDLE, clears counters and timers
 * Input:  the chain structure to cleanup
 * Output: 0 on success or -1 if cleanup failed
 * He Yan @ July 22, 2008 
 * -------------------------------------------------------------------------------------*/
int
cleanupChainStruct( Chain_structp chain ) 
{
	if (chain == NULL) 
		return -1;

	// clear the thread control settings
	chain->reconnectFlag = FALSE;
	chain->runningFlag = FALSE;
	chain->lastAction = time(NULL);

	// clear the socket related fields
	if (chain->Usocket != -1 )
		close(chain->Usocket); 
	chain->Usocket = -1;
	chain->Userrno = 0;
	chain->UconnectionState = chainStateIdle;
	
	if (chain->Rsocket != -1 )
		close(chain->Rsocket); 
	chain->Rsocket = -1;
	chain->Rserrno = 0;
	chain->RconnectionState = chainStateIdle;
	
	// clear the up time, peer reset, etc
	chain->UestablishedTime = 0;
	chain->UlastDownTime = time(NULL);
	chain->UresetCounter++;
	chain->UmessageRcvd = 0;;

	chain->RestablishedTime = 0;
	chain->RlastDownTime = time(NULL);
	chain->RresetCounter++;
	chain->RmessageRcvd = 0;;
	
	// xml queue writer cleanup
	destroyQueueWriter(chain->UxmlQueueWriter);
	chain->UxmlQueueWriter = NULL;

	destroyQueueWriter(chain->RxmlQueueWriter);
	chain->RxmlQueueWriter = NULL;

	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: send a data request to V5 BGPmon.  obsolete in V6 BGPmon
 * Input:  pointer to a chain structure
 * Output: returns 0 on success or 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
/*int chainSendRequest( Chain_structp chain )
{
	int size=2048;
	char *xml = malloc(size+1);
	if (xml == NULL)
	{
    		log_err("chainSendRequest: malloc failed");	
    		return 1;
	}	
	xml[0] = '\0';
	xml[size] = '\0'; // null terminator after the buffer, just in case ;-)
	int lxml = 0;
	char *xmlver = ""; // "<?xml version=\"1.0\"?>\n";
	char *xmlname = "test client";	
	lxml = snprintf( xml, size, "%s<request type=\"%s\" name=\"%s\"/>\n\n", xmlver, "message", xmlname );	
	if ( send(chain->socket, xml, lxml, 0) != lxml )
	{
		free(xml);	
    		log_err("chainSendRequest: Request sent a different number of bytes than expected");	
    		return 1;
	}
	free(xml);
	return 0;
}
*/
