/* 
 * 	Copyright (c) 2010 Colorado State University
 * 
 *	Permission is hereby granted, free of charge, to any person
 *	obtaining a copy of this software and associated documentation
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
 *  File: chaininstance.h
 * 	Authors:  He Yan, Dan Massey
 *  Date: Jun 24, 2008 
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#ifndef CHAININSTANCE_H_
#define CHAININSTANCE_H_

/* needed for ADDR_MAX_CHARS (maximum address size) */
#include "../Util/bgpmon_defaults.h"

/* needed so chain can write to XML queue */
#include "../Queues/queue.h"

/* needed for system types such as time_t */
#include <sys/types.h>

#define XML_MSG_HEADER_SIZE 122	//make it big enough to handle type ROUTE_REFRESH

struct ChainStruct
{
	int		chainID;					

	// configuration settings
	char		addr[ADDR_MAX_CHARS];
	int		Uport;
	int		Rport;
	int		enabled;
	int		connectRetryInterval;

	// thread control
	int		deleteChain;
	int		reconnectFlag;
	int		runningFlag;
	time_t		lastAction;
	pthread_t	chainThreadID;
	
	// socket related fields 
	int		Usocket;
	int		Userrno;
	int 		UconnectRetryCounter;
	int		UconnectionState;
	char		UmsgHeaderBuf[XML_MSG_HEADER_SIZE];

	int		Rsocket;
	int		Rserrno;
	int 		RconnectRetryCounter;
	int		RconnectionState;
	char		RmsgHeaderBuf[XML_MSG_HEADER_SIZE];


	// up time, peer reset counter, the number of received message
	time_t		UestablishedTime;
	time_t		UlastDownTime;
	int 		UresetCounter;
	int 		UmessageRcvd;

	time_t		RestablishedTime;
	time_t		RlastDownTime;
	int 		RresetCounter;
	int 		RmessageRcvd;

	// periodic check for configuration changes
	int 		periodicCheckInt;

	// write data to the xml queue
	QueueWriter	UxmlQueueWriter;
	QueueWriter	RxmlQueueWriter;

};
typedef struct ChainStruct * Chain_structp;

/* array of BGP chaining structures for receiving dat from other
 * BGPmon instances.  This BGPmon instance acts as a client
 * and connects to other BGPmon instances to form chains.
 */
Chain_structp Chains[MAX_CHAIN_IDS];

/*--------------------------------------------------------------------------------------
 * Purpose: Create a chain data structure, it is a internal function
 * Input:  address, port, enabled flag and retry interval.
 * Output: id of the new chain or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int
createChainStruct( char *addr, int Uport, int Rport, int enabled, int connectRetryInterval );

/*--------------------------------------------------------------------------------------
 * Purpose: entry function of a chain thread
 * Input:  pointer to a chain structure
 * Output: 
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void *
chainThread( void *arg );

/*--------------------------------------------------------------------------------------
 * Purpose: establish a chain connection.   if the connection succeeeds,
 *          the function returns true.    otherwise the fucntion sleeps
 *          for the retry timer and then returns FALSE.
 * Input:  the chain structure
 * Output: TRUE on successfully building a connection
 *         or sleep for retry time and return FALSE if the connection failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int establishConnection ( Chain_structp chain );

/*--------------------------------------------------------------------------------------
 * Purpose: manage a chain connection.  continually check for new messages
 *          and periodically check to see if the connection should be closed
 * Input:  the chain structure
 * Output: returns 0 if the connection was closed by the user
 *         returns -1 if the connection was closed due to an error
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int manageConnection ( Chain_structp chain );

/*-------------------------------------------------------------------------------------- 
 * Purpose: read a message from the remote chain
 * Input:  the chain structure, Update or RIB const from manageConnection function
 * Output: returns 0 if the message was read
 *         returns -1 on error
 * He Yan @ July 22, 2008 
 * -------------------------------------------------------------------------------------*/
int readMessage ( Chain_structp chain, int chain_stream );

/*-------------------------------------------------------------------------------------
 * Purpose:	Get the location in the cache of the msgCache for a given ID
 * Input:	The ID of the message in question
 * Output:	A pointer to the appropriate entry in the list, or NULL if none is found
 * Jason Bartlett @ 28 Feb 2011
 *------------------------------------------------------------------------------------*/
chainOwnerCachep getCacheEntry(u_int32_t new_id);

/*-------------------------------------------------------------------------------------
 * Purpose:	Add a new entry to the chain-ownership cache
 * Input:	The ID and sequence number of the message in question and the chain ID
 * Output:	none
 * Jason Bartlett @ 28 Feb 2011
 *------------------------------------------------------------------------------------*/
int addCacheEntry(u_int32_t msgID,int ownerChain,u_int32_t msgSeq);

/*--------------------------------------------------------------------------------------
 * Purpose: close the socket for a connected chain.
 * Input:  the chain structure
 * Output:
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void closeChainSocket ( Chain_structp chain );

/*--------------------------------------------------------------------------------------
 * Purpose: destroy the memory associated with a chain
 * Input:       the chain structure to destroy
 * Output:
 * * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void destroyChain( Chain_structp chain);

/*--------------------------------------------------------------------------------------
 * Purpose: Cleanup a chain data structure so the thread can be closed
 *           sets the state to IDLE, clears counters and timers
 * Input:  the chain structure to cleanup
 * Output: 0 on success or -1 if cleanup failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int
cleanupChainStruct( Chain_structp chain );

#endif /*CHAININSTANCE_H_*/
