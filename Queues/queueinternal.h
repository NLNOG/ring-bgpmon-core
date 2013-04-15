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
 *  File: queueinternal.h
 * 	Authors: He Yan, Dan Massey
 *  Date: Oct 7, 2008 
 */

#ifndef QUEUEINTERNAL_H_
#define QUEUEINTERNAL_H_

/* Implements a publish/subscribe mechanism where an item
 * can be published once, yet be read by several different
 * routines.  In effect, a one to many queue rather than
 * several separate queues.
 * 
 * New items are published to the publication at any time,
 * each subscription is able to read all of the items in the 
 * publication from the time they join.  An item is removed
 * from publication when all subscriptions have read that
 * item.  
 * 
 * Note that each subscriber really receives a copy of the
 * item except the last.
 */

/* need MAX_QUEUE_ITEMS and other max values to set MAX_QUEUE_READERS/WRITERS */
#include "../site_defaults.h"
#include "../Util/bgpmon_defaults.h"

#include <sys/types.h>

/*  maximum writers could be:
 *      each peer writes to queue (peer queue max)
 *      the labeling and periodic module writed to queue (labeled queue max)
 *      chains and the XML threads write to queue (xml queue max)
 */
#define MAX_QUEUE_WRITERS ( MAX_PEER_IDS > MAX_CHAIN_IDS ? MAX_PEER_IDS : (MAX_CHAIN_IDS + 1) )

/*  maximum readers could be:
 *      one reader, the labeling thread  (peer queue max)
 *      one reader, the xml thread (labeled queue max)
 *      num of clients clients (xml queue max)
 */
#define MAX_QUEUE_READERS ( MAX_CLIENT_IDS > 1 ? MAX_CLIENT_IDS : 1 )

/*----------------------------------------------------------------------------------------
 * Configuration Parameters Used For Controling Queues 
 * -------------------------------------------------------------------------------------*/
typedef struct QueueConfigurationStruct
{
	float		pacingOnThresh;
	float		pacingOffThresh;
	float		alpha;
	int		minWritesPerInterval;
	int		pacingInterval;
	int		logInterval;
} QueueConfiguration;


/*----------------------------------------------------------------------------------------
 * Entries that are stored in the queue 
 * -------------------------------------------------------------------------------------*/
typedef struct QueueEntryStruct
{
	// a reference count	
	int 		count; 
	// a pointer to the actual message buffer
	void		*messagBuf; 
} QueueEntry; 

/*----------------------------------------------------------------------------------------
 * Queue Structure Definition
 * -------------------------------------------------------------------------------------*/
struct QueueStruct
{
	// name of the queue
	char 			*name;
	// locking for multithreaded environment
	pthread_mutex_t 	queueLock;

        pthread_mutex_t         *queueGroupLock;
        pthread_cond_t          *queueGroupCond;

	// logging information
	time_t			lastLogTime;
	long			logMaxItems;
	int 			logMaxReaders;
	int			logMaxWriters;
	int 			logPacingCount; 
	int 			lastSentLogPacingCount; 

	//   the queue data
	// the index of oldest item
	long			head; 
	// the index of next available slot
	long			tail; 
	// an array of messages that are stored in the queue
	QueueEntry		items[QUEUE_MAX_ITEMS]; 
	// the data copy function for items in this queue
	void			(*copy)(void **copy, void *original);
	// the sizeof function for items in this queue
	int			(*sizeOf)(void *msg);
	
	
	// Readers information
	// current number of readers	
	int			readercount; 
 	// index number of next item for each reader
	long			nextItem[MAX_QUEUE_READERS]; 
	// total items read by each reader
	long			itemsRead[MAX_QUEUE_READERS];

	// Writer information
	int 			writercount;
	int 			used[MAX_QUEUE_WRITERS];
	
	// the pacing policy
        int                     pacingPolicy;
        // data needed by pacing policies	
	//old pacing related
	// is pacing currently on
	int 			pacingFlag;  		
	// start time of the current pacing interval
	time_t			tick;
	// How many items were read by all readers this interval. 
	int 			readCount;	
	// How many items were written by a given writer this interval
	int 			writeCounts[MAX_QUEUE_WRITERS];
	// How many items can be written by a writer during this interval
	int			writesLimit;

	//ideal reader
	// exponential weighted moving average for write counts		
	int			writesEWMA;	
	// How many items were written by all writers this interval.
	int                     writeCount;
	// the current position of the ideal reader used to adjust the slower readers
	long			idealReaderPosition;
};
typedef struct QueueStruct      *Queue;

/*----------------------------------------------------------------------------------------
 * QueueReader Structure Definition
 * -------------------------------------------------------------------------------------*/
struct QueueReaderStruct
{
	Queue 		*queues;
	int		*indexes; 
        int              count;
        void            **items;
};

/*----------------------------------------------------------------------------------------
 * QueueWriter Structure Definition
 * -------------------------------------------------------------------------------------*/
struct QueueWriterStruct
{
	Queue 		queue;
	int		index; 
};

#endif /*QUEUEINTERNAL_H_*/
