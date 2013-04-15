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
 *  File: clientscontrol.h
 * 	Authors: He Yan, Dan Massey
 *  Date: Oct 7, 2008 
 */

#ifndef CLIENTSCONTROL_H_
#define CLIENTSCONTROL_H_

// needed for Client structure
#include "clientinstance.h"

// needed for ADDR_MAX_CHARS
#include "../Util/bgpmon_defaults.h"

//needed for random sequence number generation
#include <stdlib.h>

//needed for UINT_MAX macro
#include <limits.h>

/* The client controls structure holds settings for listening to clients, 
 * and a linked list of active clients.
 */
struct ClientControls_struct_st
{
/* for UPDATA*/
	char listenUAddr[ADDR_MAX_CHARS];
	int listenUPort;
	int maxUClients; 		// the max number of clients
	int activeUClients; 		// the number of active clients
	long nextUClientID; 		// id for the next client to connect
	int rebindUFlag; 		// indicates whether to reopen socket
	ClientNode *firstUNode; 	// first node in list of active clients
	pthread_mutex_t clientULock; 	// lock client changes 

/* for RIB */
	char listenRAddr[ADDR_MAX_CHARS];
	int listenRPort;
	int maxRClients; 		// the max number of clients
	int activeRClients; 		// the number of active clients
	long nextRClientID; 		// id for the next client to connect
	int rebindRFlag; 		// indicates whether to reopen socket
	ClientNode *firstRNode; 	// first node in list of active clients
	pthread_mutex_t clientRLock; 	// lock client changes 

/* for both UPDATA and RIB */
	int enabled; 			// TRUE: enabled or FALSE: disabled
	int shutdown; 			// indicates whether to stop the thread
	time_t lastAction; 		// last time the thread was active
	u_int32_t seq_num;		// sequence number to detect message looping
	u_int32_t bgpmon_id;	// id number to detect message looping
	pthread_t clientsListenerThread;
};
typedef struct ClientControls_struct_st ClientsControls_struct;
ClientsControls_struct	ClientControls;

/*--------------------------------------------------------------------------------------
 * Purpose: the main function of clients control module
 * Input:  none 
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void *clientsControlThread( void *arg );

/*--------------------------------------------------------------------------------------
 * Purpose: Start to listen on the configured addr+port.
 * Input:  none
 * Output: socket ID of the listener or -1 if listener create fails
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int startListener(char *listenaddress, int listenport);

/*--------------------------------------------------------------------------------------
 * Purpose: Accept the new client and spawn a new thread for the client  
 *          if it passes the ACL check and the max client number is not reached.
 * Input:  the socket used for listening
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void startClient( int listenSocket, int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose: print all the active clients
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void printClients(int client_listener);

#endif /*CLIENTSCONTROL_H_*/
