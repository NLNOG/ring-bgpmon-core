/* 
 * 	Copyright (c) 2010 Colorado State University
 * 
 *	Permission is hereby granted, free of charge, to any person
 *  	obtaining a copy of this software and associated documentation
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
 *  	File: periodic.c
 *  	Authors:  He Yan
 *  	Date: Jun 24, 2008 
 *	Implementation of periodic module
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
//#ifdef TCPMD5
//#include <linux/tcp.h>
//#endif
#include <assert.h>

#include "../Util/log.h"
#include "../Queues/queue.h"
#include "periodic.h"
#include "internalperiodic.h"
#include "../Peering/peersession.h"
#include "../Labeling/label.h"
#include "../site_defaults.h"

/* needed for reading and saving configuration */
#include "../Config/configdefaults.h"
#include "../Config/configfile.h"

/* required for TRUE/FALSE defines  */
#include "../Util/bgpmon_defaults.h"

//needed to access chain/ownership cache
#include "../Chains/chains.h"

//#define DEBUG

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the periodic events settings.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
initPeriodicSettings() 
{
	int err = 0;

	// default interval of sending status messages
	if ( SESSION_STATUS_INTERVAL < 0 ) {
		err = 1;
		log_warning("Invalid site default for the interval of sending status messages.");
		PeriodicEvents.StatusMessageInterval = 3600;
	} 
	else 
		PeriodicEvents.StatusMessageInterval = SESSION_STATUS_INTERVAL;

	// default interval of route refresh
	if ( ROUTE_REFRESH_INTERVAL < 0 ) {
		err = 1;
		log_warning("Invalid site default for the interval of route refresh.");
		PeriodicEvents.RouteRefreshInterval = 7200;
	} 
	else 
		PeriodicEvents.RouteRefreshInterval= ROUTE_REFRESH_INTERVAL;

	if ( CACHE_EXPIRATION_INTERVAL < 0 ) {
		err = 1;
		log_warning("Invalid site default for the cache-clearing interval.");
		PeriodicEvents.CacheExpirationInterval = 1200;
	}
	else
		PeriodicEvents.CacheExpirationInterval = CACHE_EXPIRATION_INTERVAL;

	if ( CACHE_ENTRY_LIFETIME < 0 ) {
		err = 1;
		log_warning("Invalid site default for the cache entry lifetime.");
		PeriodicEvents.CacheExpirationInterval = 7200;
	}
	else
		PeriodicEvents.CacheEntryLifetime = CACHE_ENTRY_LIFETIME;

	// default transfer type
	PeriodicEvents.isRouteRefreshEnabled = FALSE;

	PeriodicEvents.shutdown = FALSE;

#ifdef DEBUG
	debug( __FUNCTION__, "Initialized default Periodic Settings" );
#endif

	return err;
};

/*--------------------------------------------------------------------------------------
 * Purpose: Read the periodic events settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
readPeriodicSettings() 
{
	int err = 0; 
	int result;
	int num;

	// get the interval of sending status messages
	result = getConfigValueAsInt(&num, XML_PERIODIC_STATUS_INTERVAL_PATH, 0, 65536);
	if (result == CONFIG_VALID_ENTRY) 
		PeriodicEvents.StatusMessageInterval = num;	
	else if (result == CONFIG_INVALID_ENTRY) 
	{
		err = 1;
		log_warning("Invalid configuration of the interval of sending status messages.");
	}
	else 
		log_msg("No configuration of the interval of sending status messages, using default.");
#ifdef DEBUG
	debug( __FUNCTION__, "Interval of sending status messages %d.", PeriodicEvents.StatusMessageInterval );
#endif

	// get the interval of route refresh
	result = getConfigValueAsInt(&num, XML_PERIODIC_RR_INTERVAL_PATH, 0, 65536);
	if (result == CONFIG_VALID_ENTRY) 
		PeriodicEvents.RouteRefreshInterval = num; 
	else if (result == CONFIG_INVALID_ENTRY) 
	{
		err = 1;
		log_warning("Invalid configuration of the interval of route refresh.");
	}
	else 
		log_msg("No configuration of the interval of route refresh, using default.");
#ifdef DEBUG
	debug( __FUNCTION__, "Interval of route refresh %d.", PeriodicEvents.RouteRefreshInterval );
#endif

	// get the flag indicating whether route refresh is enabled
	result = getConfigValueAsInt(&num, XML_PERIODIC_SEND_ROUTE_REFRESH_PATH, 0, 1);
	if (result == CONFIG_VALID_ENTRY) 
		PeriodicEvents.isRouteRefreshEnabled = num; 
	else if (result == CONFIG_INVALID_ENTRY) 
	{
		err = 1;
		log_warning("Invalid configuration of the SEND_ROUTE_REFRESH.");
	}
	else 
		log_msg("No configuration of the SEND_ROUTE_REFRESH, using default.");

#ifdef DEBUG
	debug( __FUNCTION__, "Route refresh is %d.", PeriodicEvents.isRouteRefreshEnabled );
#endif


	return err;
};

/*--------------------------------------------------------------------------------------
 * Purpose: Save the periodic events settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure.
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
savePeriodicSettings() 
{
	int err = 0;

	// save queue tag
	if ( openConfigElement(XML_PERIODIC_TAG) ) {
		err = 1;
		log_warning("Failed to save periodic settings to config file.");
	}

	// save status message interval
	if ( setConfigValueAsInt(XML_STATUS_MSG_INTERVAL, PeriodicEvents.StatusMessageInterval) ) {
		err = 1;
		log_warning("Failed to save status message interval to config file.");
	}

	// save route refresh interval
	if ( setConfigValueAsInt(XML_ROUTE_REFRESH_INTERVAL, PeriodicEvents.RouteRefreshInterval) ) {
		err = 1;
		log_warning("Failed to save route refresh interval to config file.");
	}

	// save transfer type
	if ( setConfigValueAsInt(XML_SEND_ROUTE_REFRESH, PeriodicEvents.isRouteRefreshEnabled) ) {
		err = 1;
		log_warning("Failed to save send_route_refresh to config file.");
	}

	// save queue tag
	if ( closeConfigElement(XML_PERIODIC_TAG) ) {
		err = 1;
		log_warning("Failed to save periodic settings to config file.");
	}

	return err;
};


/*--------------------------------------------------------------------------------------
 * Purpose: launch periodic route refresh and status message threads, called by main.c
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void LaunchPeriodicThreads()
{
	int error;
	
	pthread_t rrThreadID;
	if ((error = pthread_create(&rrThreadID, NULL, periodicRouteRefreshThread, NULL)) > 0 )
		log_fatal("Failed to create periodic route refresh thread: %s\n", strerror(error));
	PeriodicEvents.periodicRouteRefreshThread = rrThreadID;
	debug(__FUNCTION__, "Created periodic route refresh thread!");

	pthread_t statusThreadID;
	if ((error = pthread_create(&statusThreadID, NULL, periodicStatusMessageThread, NULL)) > 0 )
		log_fatal("Failed to create periodic status message thread: %s\n", strerror(error));
	PeriodicEvents.periodicStatusThread = statusThreadID;
	debug(__FUNCTION__, "Created periodic status message thread!");	

	pthread_t cacheThreadID;
	if ((error = pthread_create(&cacheThreadID, NULL, periodicCacheExpirationThread, NULL)) > 0 )
		log_warning("Failed to create periodic cache expiration thread: %s\n", strerror(error));
	PeriodicEvents.periodicCacheExpirationThread = cacheThreadID;
	debug(__FUNCTION__, "Created periodic cache expiration thread!");
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the interval of route refresh
 * Input:	the interval
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
setPeriodicRouteRefreshInterval( int interval )
{
	if( interval >= 0 )
		PeriodicEvents.RouteRefreshInterval = interval;
}


/*--------------------------------------------------------------------------------------
 * Purpose: get the interval of route refresh
 * Input:	
 * Output: the interva
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
getPeriodicRouteRefreshInterval()
{
	return PeriodicEvents.RouteRefreshInterval;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Set the interval of sending status message
 * Input:	the interval
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
setPeriodicStatusMessageInterval( int interval )
{
	if( interval >= 0 )
		PeriodicEvents.StatusMessageInterval= interval;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Get the interval of sending status message
 * Input:	
 * Output: the interval
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
getPeriodicStatusMessageInterval()
{
	return PeriodicEvents.StatusMessageInterval;
}

/*----------------------------------------------------------------------------------------
 * Purpose: send a status message for a speficified session
 * Input: sessionID - ID of the session 
 *		labeledQueueWriter - the writer of label queue
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void sendSessionStatusMessage( int sessionID, QueueWriter labeledQueueWriter)
{
	BMF bmf = createBMF( sessionID, BMF_TYPE_SESSION_STATUS );
	bgpmonMessageAppend( bmf, &sessionID, sizeof(int) );
	writeQueue( labeledQueueWriter, bmf );
}

/*----------------------------------------------------------------------------------------
 * Purpose: send a status message for all queues
 * Input: labeledQueueWriter - the writer of label queue
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void sendQueuesStatusMessage( QueueWriter labeledQueueWriter)
{
	BMF bmf = createBMF( 0, BMF_TYPE_QUEUES_STATUS );
	writeQueue( labeledQueueWriter, bmf );
}



/*----------------------------------------------------------------------------------------
 * Purpose: send a status message for all chains
 * Input: sessionID - ID of the session 
 *		labeledQueueWriter - the writer of label queue
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void sendChainsStatusMessage( QueueWriter labeledQueueWriter)
{
	BMF bmf = createBMF( 0, BMF_TYPE_CHAINS_STATUS );
	writeQueue( labeledQueueWriter, bmf );
}

/*----------------------------------------------------------------------------------------
 * Purpose: send a status message for all connected MRT's
 * Input: labeledQueueWriter - the writer of label queue
 * Output: none
 * Jason Bartlett @ 14 Feb 2010
 * -------------------------------------------------------------------------------------*/
void sendMRTStatusMessage( QueueWriter labeledQueueWriter)
{
	BMF bmf = createBMF( 0, BMF_TYPE_MRT_STATUS );
	writeQueue( labeledQueueWriter, bmf );
}

/*----------------------------------------------------------------------------------------
 * Purpose: do a route refresh for a speficified session
 * Input: 	sessionID - ID of the session 
 *		labeledQueueWriter - the writer of label queue 
 *		time to transfer to each session
 * Output:
 * He Yan @ Jun 22, 2008
 * Mikhail Strizhov @ July 23, 2010
 * -------------------------------------------------------------------------------------*/
void doRouteRefresh( int sessionID, QueueWriter labeledQueueWriter, int transfer_time ) 
{
	// always send the periodic table refresh
	sendRibTable(sessionID, labeledQueueWriter, transfer_time);

	if (getSessionUPTime(sessionID) > PeriodicEvents.RouteRefreshInterval )
	{
		int RRType = getSessionRouteRefreshType(sessionID);

		// if peer supports transfer and flag set, then request route refresh from peer
		if( RRType == TRUE && PeriodicEvents.isRouteRefreshEnabled == TRUE )
		{
			setSessionRouteRefreshFlag(sessionID);
		}
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: the thread of periodic sending route refresh for all sessions.
 * Input:
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void *
periodicRouteRefreshThread( void *arg )
{
	log_msg( "Periodic route refresh thread started" );
	PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
	
	int i, transferInterval, extraSleepTime, sendsPerInt, extraSends;
	int actualsends = 0;
	int establishedSessions[MAX_SESSION_IDS];
	int establishedSessionCount;
	QueueWriter labeledQueueWriter = createQueueWriter( labeledQueue );
	while( PeriodicEvents.shutdown == FALSE )
	{
		// reset counter
		actualsends = 0;

		// after RR update thread time
		PeriodicEvents.routeRefreshThreadLastAction = time(NULL);

		// reset the established session count and list
		establishedSessionCount = 0;
		memset(establishedSessions, 0, sizeof(int)*MAX_SESSION_IDS);

		// get the established session count and list
		for( i=0; i<MAX_SESSION_IDS; i++ )
		{
			// check Sessions and check Sessions trigger = established or mrtestablished
			if( Sessions[i] != NULL && isSessionEstablished(Sessions[i]->sessionID) == TRUE )
			{
				establishedSessions[establishedSessionCount] = Sessions[i]->sessionID;
				establishedSessionCount++;		
			}
		}
		log_msg( "There are %d established sessions",  establishedSessionCount);
		
		// check if there is any established sessions and if the route refresh interval is zero
		if( establishedSessionCount == 0 || PeriodicEvents.RouteRefreshInterval == 0 )
		{
			sleep(THREAD_CHECK_INTERVAL);
			continue;
		}
		
		// distribute the route refresh evenly over time in order to prevent the queues being overwhelmed
		if( PeriodicEvents.RouteRefreshInterval >= establishedSessionCount )
		{
			// each RR should take transferInterval seconds
			transferInterval = PeriodicEvents.RouteRefreshInterval/establishedSessionCount;
			sendsPerInt = 1;
			extraSleepTime = PeriodicEvents.RouteRefreshInterval%establishedSessionCount;
			extraSends = 0;
		}
		else
		{
			// send multiple RRs every 1s. If x = establishedSessionCount%PeriodicEvents.RouteRefreshInterval and y = establishedSessionCount%PeriodicEvents.RouteRefreshInterval then:
			// In the first x seconds, y + 1 RRs are sent
			// In the rest of RouteRefreshInterval, y RRs are sent	
			transferInterval = 1;
			sendsPerInt = establishedSessionCount/PeriodicEvents.RouteRefreshInterval;
			extraSleepTime = 0;
			extraSends = establishedSessionCount%PeriodicEvents.RouteRefreshInterval;
		}
	
		// start to do route refresh
		int nextSession = 0;
		int sends = 0;
#ifdef DEBUG		
		log_msg("transferIntervval is %d", transferInterval);
		log_msg("extraSleepTime is %d", extraSleepTime);
		log_msg("Number of established sessions %d", establishedSessionCount);
#endif		
		while( nextSession < establishedSessionCount )
		{
			if( extraSends >0 )
			{
				sends = sendsPerInt + 1;
				extraSends--;
			}
			else
				sends = sendsPerInt;
			
			for( i=0; i<sends; i++ )
			{
				int sessionID = establishedSessions[nextSession];
				if( getSessionRouteRefreshAction(sessionID) == TRUE )
				{	
					log_msg( "Session %d route refresh scheduled!", sessionID);
					doRouteRefresh(sessionID, labeledQueueWriter, transferInterval);
					actualsends++;
					log_msg("Done with %d Session", sessionID);
				}
				nextSession++;
				if ( PeriodicEvents.shutdown != FALSE )
				{
					destroyQueueWriter(labeledQueueWriter);
					log_warning( "Periodic route refresh thread exiting" );
					return NULL;
				}
				PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
			}
		} // end of while loop

		// check if doRouteRefresh was triggered, otherwise sleep
		if ( actualsends == 0 )
		{
			PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
			sleep(THREAD_CHECK_INTERVAL);
			continue;
		}

		// sleep(extraSleepTime);
		int num_of_sleeps = extraSleepTime / THREAD_CHECK_INTERVAL;
		for (i=0; i < num_of_sleeps; i++) 
		{
			sleep(THREAD_CHECK_INTERVAL);
			// after sleep update thread time
			PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
			// check if BGPmon is closing
			if ( PeriodicEvents.shutdown != FALSE )
			{
				destroyQueueWriter(labeledQueueWriter);
				log_warning( "Periodic route refresh thread exiting" );
				return NULL;
			}
		}
		sleep(extraSleepTime % THREAD_CHECK_INTERVAL);
		// after sleep update thread time
		PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
	}	
	destroyQueueWriter(labeledQueueWriter);
	log_warning( "periodic route refresh thread exiting" );
	return NULL;
}

/*----------------------------------------------------------------------------------------
 * Purpose: the thread of periodic sending status messages for all sessions.
 * Input:
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void *
periodicStatusMessageThread( void *arg )
{
	log_msg( "Periodic status message thread started" );
	PeriodicEvents.statusMessageThreadLastAction = time(NULL);
	int totalTime = 0;
	int sendStatusMessage = FALSE;

	QueueWriter labeledQueueWriter = createQueueWriter( labeledQueue );
	while( PeriodicEvents.shutdown == FALSE )
	{
		// update thread time 
		PeriodicEvents.statusMessageThreadLastAction = time(NULL);
	
		log_msg("Sending periodic status messages");

		if(totalTime*THREAD_CHECK_INTERVAL>=PeriodicEvents.StatusMessageInterval) {
			// if the total amount of time spent sleeping is greater than the StatusMessageInterval
			// then send the status message
			sendStatusMessage = TRUE;
		}
		else {
			if(PeriodicEvents.StatusMessageInterval<THREAD_CHECK_INTERVAL) {
				// if the StatusMessageInterval is smaller than the default thread sleep time then sleep 
				// the StatusMessageInterval then send the status message
				if( PeriodicEvents.StatusMessageInterval==0 ) {
					sleep(THREAD_CHECK_INTERVAL);
				}
				else {
					sleep(PeriodicEvents.StatusMessageInterval);
				}

				sendStatusMessage = TRUE;
			}
			else {
				// if StatusMessageInterval is larger than default thread sleep time then increment 
				// total time counter
				sleep(THREAD_CHECK_INTERVAL);
				totalTime++;
			}
		}

		if(sendStatusMessage==TRUE) {
			// send sessions' status
			/*
			for(i=0; i<MAX_SESSION_IDS; i++)
			{
				if( Sessions[i] != NULL )
					sendSessionStatusMessage(Sessions[i]->sessionID, labeledQueueWriter);
			}
			*/
			//these calls send stub BMF_TYPE_SESSION/QUEUES/CHAINS/MRT_STATUS message to the XML module,
			//and the actual messages get constructed in the XML module
			sendSessionStatusMessage(0, labeledQueueWriter);

			// send queues'' status
			sendQueuesStatusMessage(labeledQueueWriter);

			// send chains'' status
			sendChainsStatusMessage(labeledQueueWriter);

			//send MRT-status message stub
			sendMRTStatusMessage(labeledQueueWriter);

			// reset timer and flags
			sendStatusMessage = FALSE;
			totalTime = 0;
		}
		//printf("totalTime: %d, %d\n", totalTime, totalTime*THREAD_CHECK_INTERVAL);
	}

	destroyQueueWriter(labeledQueueWriter);
	log_warning( "Periodic status message thread exiting" );
	return NULL;
}

/*--------------------------------------------------------------------------------------
 * Purpose: periodically check and clear the chain-ownership cache
 * Jason Bartlett @ 4 Jan 2011
 *-------------------------------------------------------------------------------------*/
void* periodicCacheExpirationThread(void* arg){
	log_msg("periodic cache expiration thread started.");
	PeriodicEvents.cacheExpirationThreadLastAction = time(NULL);

	while(PeriodicEvents.shutdown == FALSE){
		//update timer
		PeriodicEvents.cacheExpirationThreadLastAction = time(NULL);

		//if it's been a little while since the cache was checked, go ahead
		if(difftime(time(NULL),PeriodicEvents.cacheExpirationThreadLastAction) >= PeriodicEvents.CacheExpirationInterval){

			chainOwnerCachep curr = cacheHead;
			if (curr == NULL) continue;
			else{
				//check the first element of the list and move pointers appropriately
				while(difftime(time(NULL),curr->timestamp) > PeriodicEvents.CacheEntryLifetime){
					cacheHead = cacheHead->next;
					free(curr);
					curr = cacheHead;
				}
				if(curr == NULL) break;
				//then check the rest of the list (note that this loop looks ahead one for faster deletes)
				while(curr->next != NULL){
					if(difftime(time(NULL),curr->next->timestamp) > PeriodicEvents.CacheEntryLifetime){
						chainOwnerCachep del = curr->next;
						curr->next = del->next;
						free(del);
					}
					curr = curr->next;
				}
			}
		}
		//if the cache has been recently checked, wait a bit before checking again
		else sleep(THREAD_CHECK_INTERVAL);
	}
	log_msg("periodic cache expiration thread exiting!");
	return NULL;
}

/*--------------------------------------------------------------------------------------
 * Purpose: enable route refresh from peers
 * Input: 
 * Output: 
 * Kevin Burnett @ June 23, 2009
 * -------------------------------------------------------------------------------------*/ 
void 
enablePeriodicRouteRefresh() {
	PeriodicEvents.isRouteRefreshEnabled = TRUE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: disables route refresh from peers
 * Input: 
 * Output: 
 * Kevin Burnett @ June 23, 2009
 * -------------------------------------------------------------------------------------*/ 
void 
disablePeriodicRouteRefresh() {
	PeriodicEvents.isRouteRefreshEnabled = FALSE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the transfer type
 * Input:	
 * Output: the type
 * Kevin Burnett @ June 18, 2009
 * -------------------------------------------------------------------------------------*/ 
int
getPeriodicRouteRefreshEnableStatus() {
	return PeriodicEvents.isRouteRefreshEnabled;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the last action time of the Periodic thread
 * Input:
 * Output: last action time
 * Mikhail Strizhov @ Jun 25, 2009
 * -------------------------------------------------------------------------------------*/
time_t getPeriodicRouteRefreshThreadLastActionTime()
{
	return PeriodicEvents.routeRefreshThreadLastAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the last action time of the Periodic thread
 * Input:
 * Output: last action time
 * Mikhail Strizhov @ Jun 25, 2009
 * -------------------------------------------------------------------------------------*/
time_t getPeriodicStatusMessageThreadLastActionTime()
{
	return PeriodicEvents.statusMessageThreadLastAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Intialize the shutdown process for the periodic module
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void signalPeriodicShutdown()
{
	PeriodicEvents.shutdown = TRUE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: wait on all periodic pieces to finish closing before returning
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void waitForPeriodicShutdown() 
{
	void * status = NULL;

	// wait for control thread exit
	pthread_join(PeriodicEvents.periodicRouteRefreshThread, status);

	// wait for control thread exit
	pthread_join(PeriodicEvents.periodicStatusThread, status);

	pthread_join(PeriodicEvents.periodicCacheExpirationThread, status);
}

