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
 *  File: clientinstance.h
 * 	Authors: He Yan, Dan Massey
 *  Date: Oct 7, 2008 
 */

#ifndef CLIENTINSTANCE_H_
#define CLIENTINSTANCE_H_

// needed for ADDR_MAX_CHARS
#include "../Util/bgpmon_defaults.h"

// needed for QueueReader
#include "../Queues/queue.h"

/* structure holding client information  */
struct ClientStruct
{
	int		id;			// client ID number
	char		addr[ADDR_MAX_CHARS];	// client's address
	int		port;			// client's port
	int		socket;			// client's socket for writing
	time_t		connectedTime;		// client's connected time
	time_t		lastAction;		// client's last action time
	QueueReader 	qReader;		// client's XML queue reader 
	int		deleteClient;		// flag to indicate delete
	pthread_t	clientThread;
	struct ClientStruct *	next;		// pointer to next client node
};
typedef struct ClientStruct ClientNode;

/*-------------------------------------------------------------------------------------- 
 * Purpose: Create a new UPDATE ClientNode structure.
 * Input:  the client ID, address (as string), port, and socket
 * Output: a pointer to the new ClientNode structure 
 *         or NULL if an error occurred.
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
ClientNode * createClientUNode( long ID, char *addr, int port, int socket );

/*-------------------------------------------------------------------------------------- 
 * Purpose: Create a new RIB ClientNode structure.
 * Input:  the client ID, address (as string), port, and socket
 * Output: a pointer to the new ClientNode structure 
 *         or NULL if an error occurred.
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
ClientNode * createClientRNode( long ID, char *addr, int port, int socket );


/*--------------------------------------------------------------------------------------
 * Purpose: The main function of a thread handling one client
 * Input:  the client node structure for this client
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void * clientUThread( void *arg );
void * clientRThread( void *arg );

#endif /*CLIENTINSTANCE_H_*/
