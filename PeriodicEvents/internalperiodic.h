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
 *  File: internalperiodic.h
 * 	Authors:  He Yan
 *  Date: Jun 24, 2008 
 */
#include "../Queues/queue.h"
// needed for the transfer type enums 
#include "periodic.h"

#ifndef INTERNALPERIODIC_H_
#define INTERNALPERIODIC_H_

/* Data Structure of Periodic Event Handling Module */
struct Periodic_struct_st {
	int StatusMessageInterval;
	int RouteRefreshInterval;
	int isRouteRefreshEnabled;
	int CacheExpirationInterval;
	int CacheEntryLifetime;
	QueueWriter	lableQueueWriter;

	time_t		routeRefreshThreadLastAction;
	time_t		statusMessageThreadLastAction;
	time_t		cacheExpirationThreadLastAction;
	pthread_t 	periodicRouteRefreshThread;
	pthread_t 	periodicStatusThread;
	pthread_t	periodicCacheExpirationThread;
	int shutdown;
};

typedef struct Periodic_struct_st Periodic_struct;

/* The periodic events structure holds timers and settings
 * related to generating periodic events such as session status
 * and periodic RIB announcements throughhroute refresh messages
 * or simulated route refreshes by sending out local rib table.
 */
Periodic_struct  PeriodicEvents;

/*----------------------------------------------------------------------------------------
 * Purpose: send a status message for a speficified session
 * Input: sessionID - ID of the session 
 *		labeledQueueWriter - the writer of label queue
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void sendSessionStatusMessage( int sessionID, QueueWriter labeledQueueWriter);

/*----------------------------------------------------------------------------------------
 * Purpose: do a route refresh for a speficified session
 * Input: 	sessionID - ID of the session 
 *		labeledQueueWriter - the writer of label queue 
 *		time to transfer to each session
 * Output:
 * He Yan @ Jun 22, 2008
 * Mikhail Strizhov @ July 23, 2010
 * -------------------------------------------------------------------------------------*/
void doRouteRefresh( int sessionID, QueueWriter labeledQueueWriter, int transfer_time ); 

/*----------------------------------------------------------------------------------------
 * Purpose: the thread of periodic sending route refresh for all sessions.
 * Input:
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void *
periodicRouteRefreshThread( void *arg );

/*----------------------------------------------------------------------------------------
 * Purpose: the thread of periodic sending status messages for all sessions.
 * Input:
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void *
periodicStatusMessageThread( void *arg );

/*----------------------------------------------------------------------------------------
 * Purpose: thread that periodically checks the chain/ownership cache and cleans up old entries
 * Jason Bartlett @ 4 Jan 2011
 *--------------------------------------------------------------------------------------*/
void* periodicCacheExpirationThread(void* arg);

#endif /*INTERNALPERIODIC_H_*/
