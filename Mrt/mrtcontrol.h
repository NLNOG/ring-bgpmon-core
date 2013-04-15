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
 *  File: mrtcontrol.h
 * 	Authors: He Yan, Dan Massey
 *  Date: Oct 7, 2008 
 */

#ifndef MRTCONTROL_H_
#define MRTCONTROL_H_

// needed for thread reference
#include <pthread.h>

// needed for mrt structure
#include "mrtinstance.h"

// needed for ADDR_MAX_CHARS
#include "../Util/bgpmon_defaults.h"

/* The mrt control structure holds settings for listening to mrts, 
 * and a linked list of active mrts.
 */
struct MrtControls_struct_st
{
	char		listenAddr[ADDR_MAX_CHARS];
	int		listenPort;
	int		enabled;	// TRUE: enabled or FALSE: disabled
	int		maxMrts; 	// the max number of mrts
	int		labeAction; 	// the label action of mrts
	int		activeMrts; 	// the number of active mrts
	long		nextMrtID;	// id for the next mrt to connect
	int		rebindFlag;	// indicates whether to reopen socket
	int 		shutdown;	// indicates whether to stop the thread
	time_t		lastAction;	// last time the thread was active
	pthread_t	mrtListenerThread;	// reference to mrt thread
	MrtNode* 	firstNode; 	// first node in list of active mrts
	pthread_mutex_t	mrtLock;	// lock mrt changes 
};
typedef struct MrtControls_struct_st MrtControls_struct;
MrtControls_struct	MrtControls;

/*--------------------------------------------------------------------------------------
 * Purpose: the main function of mrt control module
 * Input:  none 
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void *mrtControlThread( void *arg );

/*--------------------------------------------------------------------------------------
 * Purpose: Start to listen on the configured addr+port.
 * Input:  none
 * Output: socket ID of the listener or -1 if listener create fails
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int startMrtListener();

/*--------------------------------------------------------------------------------------
 * Purpose: Accept the new client and spawn a new thread for the mrt  
 *          if it passes the ACL check and the max client number is not reached.
 * Input:  the socket used for listening
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void startMrt( int listenSocket );

/*--------------------------------------------------------------------------------------
 * Purpose: print all the active mrts
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void printMrts();

/*-------------------------------------------------------------------------------------- 
 * Purpose: Create a new MrtNode structure.
 * Input:  the mrt ID, address (as string), port, and socket
 * Output: a pointer to the new MrtNode structure 
 *         or NULL if an error occurred.
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
MrtNode * createMrtNode( long ID, char *addr, int port, int socket, int labelAction );


#endif /*MRTCONTROL_H_*/
