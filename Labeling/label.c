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
 *  File: Label.c
 *  Authors: He Yan
 *  Date: Jun 22, 2008 
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <assert.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#include "../Util/log.h"
#include "../site_defaults.h"
#include "../Peering/bgppacket.h"
#include "../Peering/bgpmessagetypes.h"
#include "label.h"
#include "labelinternal.h"
#include "rtable.h"

// needed to parse/save XML
#include "../Config/configdefaults.h"
#include "../Config/configfile.h"

// needed for TRUE/FALSE definitions
#include "../Util/bgpmon_defaults.h"

// needed for xml queue writer/reader
#include "../Queues/queue.h"

// needed to generateSessionString
#include "../XML/xml.h"

// needed to access Peers array
#include "../Peering/peers.h"
#include "../Peering/bgpstates.h"

#include "../PeriodicEvents/internalperiodic.h"

//#define DEBUG

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default Labeling configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int initLabelingSettings()
{
	LabelControls.shutdown = FALSE;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Read the labeling settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readLabelingSettings()
{	
	return 0;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Save the labeling settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int saveLabelingSettings()
{	
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: launch labeling thread, called by main.c
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void launchLabelingThread()
{
	int error;
	
	pthread_t labelingThreadID;

	if ((error = pthread_create(&labelingThreadID, NULL, labelingThread, NULL)) > 0 )
		log_fatal("Failed to create labeling thread: %s\n", strerror(error));

	LabelControls.labelThread = labelingThreadID;

	debug(__FUNCTION__, "Created labeling thread!");
}

/*--------------------------------------------------------------------------------------
 * Purpose: Clear the content of Rib table of a session
 * Input:  sessionID - ID of the session needs to clear Rib
 * Output: 0 means success, -1 means failure.
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int cleanRibTable(int sessionID)
{
	if( destroyPrefixTable(Sessions[sessionID]->prefixTable, Sessions[sessionID]) ) 
	{
		log_err ("Failed to clean the prefix table for session %d!", sessionID);
		return -1;
	}

	if( destroyAttrTable(Sessions[sessionID]->attributeTable, Sessions[sessionID]) ) 
	{
		log_err ("Failed to clean the attr table for session %d!", sessionID);
		return -1;
	}	
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose:delete the entrie Rib table of a session
 * Input:  sessionID - ID of the session needs to delete Rib
 * Output: 0 means success, -1 means failure.
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int deleteRibTable(int sessionID)
{
	int i;
	int error;
	// delete the prefix hash table
	if( Sessions[sessionID]->prefixTable !=NULL )
	{
		if( destroyPrefixTable(Sessions[sessionID]->prefixTable, Sessions[sessionID]) ) 
		{
			log_err("Failed to destroy the prefix table for session %d!", sessionID);
			return -1;
		}    
		free(Sessions[sessionID]->prefixTable->prefixEntries);
		free(Sessions[sessionID]->prefixTable);
		Sessions[sessionID]->prefixTable = NULL;
#ifdef DEBUG
		debug (__FUNCTION__,  "Successfully destroy the prefix table for session %d!", sessionID);
#endif 
	}
	else
		return -1;

	// delete the attribute hash table
	if( Sessions[sessionID]->attributeTable !=NULL )
	{

		if (destroyAttrTable(Sessions[sessionID]->attributeTable, Sessions[sessionID])) 
		{
			log_err("Failed to destroy the attribute table for session %d!", sessionID);
			return -1;
		}
		for (i=0; i<Sessions[sessionID]->attributeTable->tableSize; i++) 
		{
			if ((error = pthread_rwlock_destroy(&(Sessions[sessionID]->attributeTable->attrEntries[i].lock))) > 0)  
			{
				log_err("Failed to destroy rwlock: %s\n", strerror(error));  
				return -1;
			}
		}  		
		free(Sessions[sessionID]->attributeTable->attrEntries);
		free(Sessions[sessionID]->attributeTable);
		Sessions[sessionID]->attributeTable = NULL;
#ifdef DEBUG
		debug (__FUNCTION__,  "Successfully destroy the attribute table for session %d!", sessionID);
#endif
	}
	else
		return -1;
	
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Process one BMF message 
 * Input: BMF message
 * Output:  0 for success or -1 for failure
 * Note: 1. Apply this BGP update message to the correspondong rib table based on the sessionID.
 	 2. Label this BGP update message based on the correspondong rib.  
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int processBMF (BMF bmf)
{

	time_t			time = bmf->timestamp;
	u_int16_t		type = bmf->type;
	PBgpHeader		hdr = NULL;
	u_int16_t		bgpType = 0;
	u_int32_t		bgpMsgLen = 0;
	ParsedBGPUpdate	parsedUpdateMsg;
	
	//  labeling only applies to messages from the peer 
	if ((type != BMF_TYPE_MSG_FROM_PEER) && (type != BMF_TYPE_TABLE_TRANSFER)) 
	{
		#ifdef DEBUG
		debug (__FUNCTION__, "Got a BMF message (%d) from session %d.", bmf->type, bmf->sessionID);
		#endif                   
		return 0;
	}
		 


	hdr = (PBgpHeader)(bmf->message); 
	bgpType = hdr->type;
	bgpMsgLen = getBGPHeaderLength(hdr);

	if( bgpType != typeUpdate ) 
	{
#ifdef DEBUG
		debug (__FUNCTION__, "processBMF, Got a BGP non-update message(%d) from session %d.", bgpType, bmf->sessionID);
		hexdump (LOG_DEBUG, bmf->message, bgpMsgLen);
#endif
		return -1;
	}

#ifdef DEBUG
	debug (__FUNCTION__, "processBMF, Got a BGP update message from session %d.", bmf->sessionID);
	hexdump (LOG_DEBUG, bmf->message, bgpMsgLen);
#endif
	
	if( parseBGPUpdate(bmf->message, bgpMsgLen, &parsedUpdateMsg) )
	{
		log_err ("processBMF, Failed to process a BGP update message!");
#ifdef DEBUG	
		hexdump (LOG_ERR, bmf->message, bgpMsgLen);
#endif	
		return -1;
	}

#ifdef DEBUG
	if( parsedUpdateMsg.numOfMpNlri != 0 )
	{
		debug (__FUNCTION__, "processBMF, Successfully parsed a BGP update message!");
		debug (__FUNCTION__, "processBMF, IPv4 unreach nlri[%d]", parsedUpdateMsg.unreachNlri.nlriLen);
		if( parsedUpdateMsg.unreachNlri.nlriLen )       
			hexdump(LOG_DEBUG, parsedUpdateMsg.unreachNlri.nlri, parsedUpdateMsg.unreachNlri.nlriLen);

		debug (__FUNCTION__, "processBMF, IPv4 reach nlri[%d]", parsedUpdateMsg.reachNlri.nlriLen);
		if( parsedUpdateMsg.reachNlri.nlriLen )       
			hexdump(LOG_DEBUG, parsedUpdateMsg.reachNlri.nlri, parsedUpdateMsg.reachNlri.nlriLen);

		debug (__FUNCTION__, "processBMF, Basic attribute[%d], Mp attribute[%d]", parsedUpdateMsg.attr.basicAttrlen, parsedUpdateMsg.attr.totalLen-parsedUpdateMsg.attr.basicAttrlen);
		if( parsedUpdateMsg.attr.totalLen )
		{
			hexdump (LOG_DEBUG, parsedUpdateMsg.attr.data,  parsedUpdateMsg.attr.totalLen);
	 	}
		int i;
		for (i=0; i<parsedUpdateMsg.numOfMpNlri; i++)
		{
			if( parsedUpdateMsg.mpNlri[i].flag == 0)
			{
				debug (__FUNCTION__, "processBMF, MPreach[afi:%d, safi:%d] nlri[%d]:", parsedUpdateMsg.mpNlri[i].afi, parsedUpdateMsg.mpNlri[i].safi,
					parsedUpdateMsg.mpNlri[i].nlriLen);
			}
			else
				debug (__FUNCTION__, "processBMF, MPunreach[afi:%d, safi:%d] nlri[%d]:", parsedUpdateMsg.mpNlri[i].afi, parsedUpdateMsg.mpNlri[i].safi,
					parsedUpdateMsg.mpNlri[i].nlriLen);
			if (parsedUpdateMsg.mpNlri[i].nlriLen)       
				hexdump (LOG_DEBUG, parsedUpdateMsg.mpNlri[i].nlri, parsedUpdateMsg.mpNlri[i].nlriLen);
		}
	}
#endif
	
	// Need to label the update
	if( (type != BMF_TYPE_TABLE_TRANSFER) && (Sessions[bmf->sessionID]->configInUse.labelAction == Label ))
	{
		// Convert BMF message from type BMF_TYPE_MSG_FROM_PEER to type BMF_TYPE_MSG_LABELED
		bmf->type = BMF_TYPE_MSG_LABELED;
		if( applyBGPUpdate(time, &parsedUpdateMsg, Sessions[bmf->sessionID], bmf) )
		{
			log_err( "processBMF, Failed to apply BGP update message to rib table!");
			return -1;
		}
	}
	else
	{
		if( applyBGPUpdate(time, &parsedUpdateMsg, Sessions[bmf->sessionID], NULL) )
		{
			log_err( "processBMF, Failed to apply BGP update message to rib table!");
			return -1;
		}		
	}

	
	#ifdef DEBUG
		debug (__FUNCTION__, "processBMF, Done!");
	#endif
	return 0;


} 



/*----------------------------------------------------------------------------------------
 * Purpose: Entry function of rib/label thread
 * Input:
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void *
labelingThread( void *arg ) 
{
	log_msg( "Labeling Thread Started" );
        Queue queues[2] = {peerQueue, mrtQueue};
	QueueReader peerQueueReader =  createQueueReader( queues, 2 );
	QueueWriter labeledQueueWriter = createQueueWriter( labeledQueue );

	while( LabelControls.shutdown == FALSE )
	{
                // update the last active time for this thread
		LabelControls.lastAction = time(NULL);
		
		#ifdef DEBUG
		debug (__FUNCTION__, "Labeling thread waiting to read from peer queue");
		#endif
		BMF bmf = NULL;	
                int idx = 0;
		readQueue( peerQueueReader );
                for(idx=0;idx<peerQueueReader->count;idx++){
                  bmf = (BMF)peerQueueReader->items[idx];
                  // there is no guarantee that we will have an item from each queue
                  if(bmf == NULL){
                    continue;
                  }
		  #ifdef DEBUG
		  debug (__FUNCTION__, "Labeling thread read from queue, processing BMF");
		  #endif
		  incrementSessionMsgCount(bmf->sessionID);
		
		  int action  = getSessionLabelAction(bmf->sessionID);
		  if( action == Label || action == StoreRibOnly ){	
		    if(processBMF( bmf )){
                      free(bmf);
                      continue;
                    }
		  }


		  // delete the rib table of closed session
		  if( bmf->type == BMF_TYPE_FSM_STATE_CHANGE )
		  {
			if( checkStateChangeMessage(bmf) )
			{				
				if( deleteRibTable(bmf->sessionID) )
					log_msg( "no rib table for session %d", bmf->sessionID);
				else
					log_msg( "Successfully destroy the rib table for session %d!", bmf->sessionID);
			}

		  }		
		
		  #ifdef DEBUG
		  debug (__FUNCTION__, "Labeling thread processed BMF, writing to labeled queue");
		  #endif

		  if( bmf->type != BMF_TYPE_TABLE_TRANSFER ) {
			  writeQueue( labeledQueueWriter, bmf);
		  }else{
			  free(bmf);
		  }
			
		  #ifdef DEBUG
		  debug (__FUNCTION__, "wrote to queue");
		  #endif
        	}

	}

	destroyQueueReader(peerQueueReader);
	destroyQueueWriter(labeledQueueWriter);
	log_warning( "Labeling thread exiting" );

	return NULL;
}

/*----------------------------------------------------------------------------------------
 * Purpose: send out the rib table of a session
 * Input:	ID of a session
 * 		Queue rwiter
 * 		time for each session to tranfer data
 * Output: 0 means success, -1 means failure
 * He Yan @ Jun 22, 2008
 * Mikhail Strizhov @ July 23, 2010
 * -------------------------------------------------------------------------------------*/
int sendRibTable(int sessionID, QueueWriter labeledQueueWriter, int transfer_time)
{
	int i,j, error;
	int xml_message_counter = 0;
	u_int32_t num_of_sleeps = 0;
	int indexes_per_second=0, index_counter=0, timethrloop=0;
	// get time when we enter this function
	time_t function_start_stamp, current_time_stamp, function_end_stamp, desired_time;

	// for printing time
	char desired_time_extended[64];
	memset(desired_time_extended, 0, 64);
#ifdef DEBUG
	struct tm * desired_tm = NULL;
#endif

	// get time
	function_start_stamp = time(NULL);

 	Session_structp session;
	if( sessionID == -1 )
	{
		log_err ("Failed to send a rib table of session %d", sessionID);
		num_of_sleeps = transfer_time / THREAD_CHECK_INTERVAL;
		for (i=0; i < num_of_sleeps; i++) 
		{
			sleep(THREAD_CHECK_INTERVAL);
			// after sleep update thread time
			PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
			// check if BGPmon is closing
			if ( PeriodicEvents.shutdown != FALSE )
			{
				return -1;
			}
		}
		sleep(transfer_time % THREAD_CHECK_INTERVAL);
		// after sleep update thread time
		PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
		return -1;
	}
	else
		 session = Sessions[sessionID];

	// send TABLE_START message with sessionID
	BMF bmf_start = createBMF( sessionID, BMF_TYPE_TABLE_START );
	writeQueue( labeledQueueWriter, bmf_start );

	if( session == NULL || session->attributeTable == NULL )
	{
		log_err ("Failed to send a rib table of session %d", sessionID);
		num_of_sleeps = transfer_time / THREAD_CHECK_INTERVAL;
		for (i=0; i < num_of_sleeps; i++) 
		{
			sleep(THREAD_CHECK_INTERVAL);
			// after sleep update thread time
			PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
			// check if BGPmon is closing
			if ( PeriodicEvents.shutdown != FALSE )
			{
				return -1;
			}
		}
		sleep(transfer_time % THREAD_CHECK_INTERVAL);
		// after sleep update thread time
		PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
		return -1;
	}
	
	// calculate how many messages we need to send per second
	indexes_per_second = session->attributeTable->tableSize / transfer_time;
	if (indexes_per_second < 1)
	{
		indexes_per_second = 1;
		log_warning("Session %d, Indexes per seconds in table transfer is too low, set it to 1", sessionID);
	}
#ifdef DEBUG	  
	debug(__FUNCTION__, "Session %d, indexes per second is %d, transfer_time is %d", sessionID, indexes_per_second, transfer_time);
#endif 
	// to through table size
	// remember - tablesize is an index table and has different number of attribute entries inside
	for (i=0; i<session->attributeTable->tableSize; i++) 
	{
		// check i index is eq to send rate
		if ( index_counter == indexes_per_second)
		{
#ifdef DEBUG		
			debug(__FUNCTION__, "index is %d", i);
#endif			
			// update refresh time
			PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
			// close BGPmon if shutdown is enabled
			if ( PeriodicEvents.shutdown != FALSE )
			{
				return -1;
			}

			// reset index to 0
			index_counter = 0;

			// assume that each send block (messages_per_second ) will take 1 second
			timethrloop++;

			current_time_stamp = time(NULL);
			desired_time = function_start_stamp + timethrloop;
#ifdef DEBUG
			debug(__FUNCTION__, "Desired time - current time stamp is %f",difftime(desired_time, current_time_stamp));
#endif 			
			if ( (int)(difftime(desired_time, current_time_stamp)) > 2)
			{
#ifdef DEBUG			
				debug(__FUNCTION__, "going to sleep for %f seconds", difftime(desired_time, current_time_stamp));
#endif				
				int difference = (int)(difftime(desired_time, current_time_stamp));
				num_of_sleeps = difference / THREAD_CHECK_INTERVAL;
				for (j=0; j < num_of_sleeps; j++) 
				{
					sleep(THREAD_CHECK_INTERVAL);
					// after sleep update thread time
					PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
					// check if BGPmon is closing
					if ( PeriodicEvents.shutdown != FALSE )
					{
						return -1;
					}
				}
				sleep(difference % THREAD_CHECK_INTERVAL);
				// after sleep update thread time
				PeriodicEvents.routeRefreshThreadLastAction = time(NULL);
			}
			else 
			if ( (int)(difftime(desired_time,current_time_stamp)) < 2)
			{
#ifdef DEBUG
				desired_tm = localtime(&desired_time);
				strftime(desired_time_extended, sizeof(desired_time_extended), "%Y-%m-%dT%H:%M:%SZ", desired_tm);
				log_warning("Session %d, Table transfer took %d seconds to proceed %d/%d indexes, desired time is %s", sessionID, (int)(difftime(desired_time,current_time_stamp)), i, session->attributeTable->tableSize, desired_time_extended );
#endif
			}	
		} // end of if check

		//check to see if session has been shut down by another thread
		if(!Sessions[sessionID]){
			log_msg("Session %d closed while sending its RIB!",sessionID);
			// send TABLE_STOP message with sessionID
			BMF bmf_stop = createBMF( sessionID, BMF_TYPE_TABLE_STOP);
			u_int32_t super_counter = htonl(xml_message_counter);
			bgpmonMessageAppend( bmf_stop, &super_counter, sizeof(u_int32_t) );   // include number of xml messages in bmf_stop
			writeQueue( labeledQueueWriter, bmf_stop);
			return 0;	//if the session gets torn down somewhere along the line, break out of the loop because there will be no more stuff coming			
		}
	
		if ((error = pthread_rwlock_rdlock (&(session->attributeTable->attrEntries[i].lock))) > 0) 
		{
			log_err ("Failed to rdlock an entry in the rib table: %s", strerror(error));
		 	continue;
		}
		AttrNode *node;
		node = session->attributeTable->attrEntries[i].node;
		while (node != NULL)
		{
			// send messages
			if  (sendBMFFromAttrNode(node, session->sessionID, labeledQueueWriter) == -1)
			{
				log_err ("Failed to send BMF message for Session %d, Attribute index is %d", session->sessionID, i);
			}
			// increment number of sent XML messages
			xml_message_counter++;
			node = node->next;
		}	
		pthread_rwlock_unlock(&(session->attributeTable->attrEntries[i].lock));
		
		// count how many indexes were send
		index_counter++;
				
	} // end of tablesize for-loop

	// send TABLE_STOP message with sessionID
	BMF bmf_stop = createBMF( sessionID, BMF_TYPE_TABLE_STOP);
	u_int32_t super_counter = htonl(xml_message_counter);
	bgpmonMessageAppend( bmf_stop, &super_counter, sizeof(u_int32_t) );   // include number of xml messages in bmf_stop
	writeQueue( labeledQueueWriter, bmf_stop);
	
	// check time difference with transfer time
	function_end_stamp = time(NULL);
#ifdef DEBUG	
	debug(__FUNCTION__, "Diff btw finish and start functions is %d, transfer_time is %d", function_end_stamp - function_start_stamp, transfer_time);
#endif	
	if ( (function_end_stamp - function_start_stamp) > transfer_time)
	{
		log_warning("Session %d, Table transfer: Sending messages too slow!", sessionID);
	}
	else
	if ( (function_end_stamp - function_start_stamp) < transfer_time)
	{
		log_warning("Session %d, Table transfer: Sending messages too fast!", sessionID);
	}

	log_msg( "Successfully sent RIB table of session %d ! ", session->sessionID);
	return 0;
}

/*--------------------------------------------------------------------------------------
 *  * Purpose: get the last action time of the lable thread
 *   * Input:
 *    * Output: last action time
 *     * He Yan @ Sep 22, 2008
 *      * -------------------------------------------------------------------------------------*/
time_t getLabelThreadLastActionTime()
{
	return LabelControls.lastAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Intialize the shutdown process for the label module
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 9, 2009
 * -------------------------------------------------------------------------------------*/
void signalLabelShutdown() 
{
	LabelControls.shutdown = TRUE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: wait on all label pieces to finish closing before returning
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 9, 2009
 * -------------------------------------------------------------------------------------*/
void waitForLabelShutdown() 
{
	void * status = NULL;

	// wait for label control thread exit
	pthread_join(LabelControls.labelThread, status);
}

