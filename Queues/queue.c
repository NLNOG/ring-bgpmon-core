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
 *  File: queue.c
 * 	Authors: He Yan, Dan Massey
 *  Date: Oct 7, 2008
 */

/* need for malloc*/ 
#include <stdlib.h>

/* structures and functions for this module */
#include "queue.h"
/* internal structures and functions for this module */
#include "queueinternal.h"
/* structures and functions for applying pacing */
#include "pacing.h"

/* required for logging functions */
#include "../Util/log.h"

/* needed for reading and saving configuration */
#include "../Config/configdefaults.h"
#include "../Config/configfile.h"

/* required for TRUE/FALSE defines  */
#include "../Util/bgpmon_defaults.h"

/* needed for copying BGPmon Internal Format messages */
#include "../Util/bgpmon_formats.h"

/* needed for function getXMLMessageLen */
#include "../XML/xml.h"

/*  needed to lock structures */
#include <pthread.h>

/* needed for malloc */
#include <stdlib.h>


//#define DEBUG

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default queue creation parameters.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * Dan Massey @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
initQueueSettings() 
{
	int err = 0;

	// pacing on threshold
	if ( (QUEUE_PACING_ON_THRESHOLD < 0) || (QUEUE_PACING_ON_THRESHOLD > 1) ) {
		err = 1;
		log_warning("Invalid site default for queue pacing on threshold.");
		QueueConfig.pacingOnThresh = 0.5;
	} 
	else 
		QueueConfig.pacingOnThresh = QUEUE_PACING_ON_THRESHOLD;

	// pacing off threshold
	if ( (QUEUE_PACING_OFF_THRESHOLD < 0) || (QUEUE_PACING_OFF_THRESHOLD > 1) ) {
		err = 1;
		log_warning("Invalid site default for queue pacing off threshold.");
		QueueConfig.pacingOffThresh = 0.5;
	} 
	else 
		QueueConfig.pacingOffThresh = QUEUE_PACING_OFF_THRESHOLD;

	// pacing alpha value
	if ( (QUEUE_ALPHA < 0) || (QUEUE_ALPHA > 1) ) {
		err = 1;
		log_warning("Invalid site default for queue alpha value.");
		QueueConfig.alpha = 0.5;
	} 
	else 
		QueueConfig.alpha = QUEUE_ALPHA;

	// pacing minimum writes per interval
	if (QUEUE_MIN_WRITES_LIMIT < 0) {
		err = 1;
		log_warning("Invalid site default for queue min writes per interval.");
		QueueConfig.minWritesPerInterval = 10;
	}
	else 
		QueueConfig.minWritesPerInterval = QUEUE_MIN_WRITES_LIMIT;

	// pacing interval in seconds
	if (QUEUE_PACING_INTERVAL < 0) {
		err = 1;
		log_warning("Invalid site default for queue pacing interval.");
		QueueConfig.pacingInterval = 30;
	}
	else
		QueueConfig.pacingInterval = QUEUE_PACING_INTERVAL;

	// log interval in seconds
	if (QUEUE_LOG_INTERVAL < 0) {
		err = 1;
		log_warning("Invalid site default for queue pacing interval.");
		QueueConfig.logInterval = 30;
	}
	else
		QueueConfig.logInterval = QUEUE_LOG_INTERVAL;

#ifdef DEBUG
	debug( __FUNCTION__, "Initialized default Queue Settings" );
#endif

	return err;
};

/*--------------------------------------------------------------------------------------
 * Purpose: Read the queue settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * Dan Massey @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
readQueueSettings() 
{
	int err = 0; 
	int result;
	float fnum;
	int num;

	// get the pacing on threshold
	result = getConfigValueAsFloat(&fnum, XML_QUEUE_PACING_ON_PATH, 0, 1);
	if (result == CONFIG_VALID_ENTRY) 
		QueueConfig.pacingOnThresh = fnum;	
	else if (result == CONFIG_INVALID_ENTRY) 
	{
		err = 1;
		log_warning("Invalid configuration of queue pacing on threshold.");
	}
	else 
		log_msg("No configuration of queue pacing on threshold, using default.");
#ifdef DEBUG
	debug( __FUNCTION__, "queue's pacingOnThresh %f.", QueueConfig.pacingOnThresh );
#endif

	// get pacing off threshold
	result = getConfigValueAsFloat(&fnum, XML_QUEUE_PACING_OFF_PATH, 0, 1);
	if (result == CONFIG_VALID_ENTRY) 
		QueueConfig.pacingOffThresh = fnum;	
	else if (result == CONFIG_INVALID_ENTRY) 
	{
		err = 1;
		log_warning("Invalid configuration of queue pacing off threshold.");
	}
	else 
		log_msg("No configuration of queue pacing off threshold, using default.");
#ifdef DEBUG
	debug( __FUNCTION__, "queue's pacingOffThresh %f.", QueueConfig.pacingOffThresh );
#endif

	// get alpha
	result = getConfigValueAsFloat(&fnum, XML_QUEUE_ALPHA_PATH, 0, 1);
	if (result == CONFIG_VALID_ENTRY) 
		QueueConfig.alpha = fnum;
	else if (result == CONFIG_INVALID_ENTRY) 
	{
		err = 1;
		log_warning("Invalid configuration of queue alpha value.");
	}
	else 
		log_msg("No configuration of queue alpha value, using default.");
#ifdef DEBUG
	debug( __FUNCTION__, "queue's alpha %f.", QueueConfig.alpha);
#endif		

	// get min writes limit
	result = getConfigValueAsInt(&num, XML_QUEUE_MIN_WRITES_LIMIT_PATH, 0, 65536);
	if (result == CONFIG_VALID_ENTRY) 
		QueueConfig.minWritesPerInterval= num;
	else{	
		if (result == CONFIG_INVALID_ENTRY) 
		{
			err = 1;
			log_warning("Invalid configuration of queue min writes per interval.");
		}
		else 
			log_msg("No configuration of queue min writes per interval, using default.");
	}
#ifdef DEBUG
	debug( __FUNCTION__, "queue's min writes limit per interval %d.", QueueConfig.minWritesPerInterval);
#endif
	
	// get pacing Interval
	result = getConfigValueAsInt(&num, XML_QUEUE_PACING_INTERVAL_PATH, 0, 65536);
	if (result == CONFIG_VALID_ENTRY) 
		QueueConfig.pacingInterval = num;	
	else{
	if (result == CONFIG_INVALID_ENTRY) 
		{
			err = 1;
			log_warning("Invalid configuration of queue pacing interval.");
		}
		else 
			log_msg("No configuration of queue pacing interval, using default.");
	}
#ifdef DEBUG
	debug( __FUNCTION__, "queue's pacing interval %d.", QueueConfig.pacingInterval);
#endif		

	// get log Interval
	result = getConfigValueAsInt(&num, XML_QUEUE_LOG_INTERVAL_PATH, 0, 65536);
	if (result == CONFIG_VALID_ENTRY) 
		QueueConfig.logInterval = num;	
	else{
		if (result == CONFIG_INVALID_ENTRY) 
		{
			err = 1;
			log_warning("Invalid configuration of queue log interval.");
		}
		else 
			log_msg("No configuration of queue log interval, using default.");
	}
#ifdef DEBUG
	debug( __FUNCTION__, "queue's log interval %d.", QueueConfig.logInterval);
#endif		

	return err;
};

/*--------------------------------------------------------------------------------------
 * Purpose: Save the queue settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure.
 * Dan Massey @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
saveQueueSettings() 
{
	int err = 0;

	// save queue tag
	if ( openConfigElement(XML_QUEUE_TAG) ) {
		err = 1;
		log_warning("Failed to save queue to config file.");
	}

	// save pacing on threshold
	if ( setConfigValueAsFloat(XML_QUEUE_PACING_ON, QueueConfig.pacingOnThresh) ) {
		err = 1;
		log_warning("Failed to save queue's PacingOnThresh to config file.");
	}

	// save pacing off threshold
	if ( setConfigValueAsFloat(XML_QUEUE_PACING_OFF, QueueConfig.pacingOffThresh) ) {
		err = 1;
		log_warning("Failed to save queue's PacingOffThresh to config file.");
	}

	// save alpha
	if ( setConfigValueAsFloat(XML_QUEUE_ALPHA, QueueConfig.alpha) ) {
		err = 1;
		log_warning("Failed to save queue's alpha value to config file.");
	}

	// save min writes limit
	if ( setConfigValueAsInt(XML_QUEUE_MIN_WRITES_LIMIT, QueueConfig.minWritesPerInterval) ) {
		err = 1;
		log_warning("Failed to save queue's min writes limit per interval to config file.");
	}
	// save pacing Interval
	if ( setConfigValueAsInt(XML_QUEUE_PACING_INTERVAL, QueueConfig.pacingInterval) ) {
		err = 1;
		log_warning("Failed to save queue's pacing interval to config file.");
	}

	// save log Interval
	if ( setConfigValueAsInt(XML_QUEUE_LOG_INTERVAL, QueueConfig.logInterval) ) {
		err = 1;
		log_warning("Failed to save queue's log interval to config file.");
	}
	
	// save queue tag
	if ( closeConfigElement(XML_QUEUE_TAG) ) {
		err = 1;
		log_warning("Failed to save queue to config file.");
	}

	return err;
};

/*--------------------------------------------------------------------------------------
 * Purpose: Create a queue instance
 * Input:  the copy function for queue elements, the queue name, and the name length
 * Output:  the resulting queue, exits on fatal error if creation fails
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
Queue 
createQueue( void (*copy)(void **copy, void *original), int (*sizeOf)(void *msg),
             char *name, int pacing,pthread_cond_t *groupCond, pthread_mutex_t *groupLock)
{

	//  allocate the queue memory 	
	Queue q = malloc( sizeof ( struct QueueStruct ) );
	if ( q == NULL) 
		log_fatal( "out of memory: malloc queue structure failed");
		// not reached
	memset( q, 0, sizeof( struct QueueStruct ) );

	// allocate and set the queue name
	q->name = malloc ( (strlen(name)+1) * sizeof(char) );
	if ( q->name == NULL) 
		log_fatal( "out of memory: malloc queue name string failed");
		// not reached
	strcpy(q->name, name);
	log_msg( "queue name:%s", q->name );

	// set queue head and tail to 0, clear all queue items
	q->head = 0;
	q->tail = 0;
	memset( q->items, 0, QUEUE_MAX_ITEMS*sizeof( QueueEntry ) );

	// set the reader copy function for this queue
	if ( copy == NULL )
		log_fatal( "no function provided for copying items to queue %s", q->name );
		// not reached
	q->copy = copy;
	q->sizeOf = sizeOf;
	
	// initialize the log variables
	q->lastLogTime = time( NULL );
	q->logMaxItems = 0;
	q->logMaxReaders = 0;
	q->logMaxWriters = 0;
	q->logPacingCount = 0;
	
	// mark each reader slot as available
	int i;
	q->readercount = 0;
	for ( i = 0; i < MAX_QUEUE_READERS; i++ )
	{
		q->nextItem[i] = READER_SLOT_AVAILABLE;
		q->itemsRead[i] = 0;
	}
	
	// mark each writer slot as unused 
	q->writercount = 0;
	for ( i = 0; i < MAX_QUEUE_WRITERS; i++ )
	{
		q->used[i] = 0;
		q->writeCounts[i] = 0;
	}
	
	// initialize current packing tick
	q->tick = time(NULL);
	// initialize pacing values
	q->pacingFlag = FALSE;  
	// How many items are written into queue by all producers per interval		
	q->writeCount = 0;
	// How many items can a writer put into queue during pacing per interval.
	q->writesLimit = QUEUE_MIN_WRITES_LIMIT; 
				
	// create a lock for this queue	
	if (pthread_mutex_init( &q->queueLock, NULL ) ) 
		log_fatal( "unable to init mutex lock for queue %s", q->name);
		// not reached

        // create the lock and condition variable for this queue group.
        // if a mutex and cond variable where sent in as parameters just use those and assume that 
        // they have already been initializes, if they are null, allocate and initialize.
        if(groupLock == NULL){
          q->queueGroupLock = malloc(sizeof(pthread_mutex_t));
          if(q->queueGroupLock == NULL){
            log_fatal("Unable to allocate group lock");
          }
          if (pthread_mutex_init( q->queueGroupLock, NULL ) )
                log_fatal( "unable to init mutex lock for queue %s", q->name);
        }else{
          q->queueGroupLock = groupLock;
        }
        if(groupCond == NULL){
          q->queueGroupCond = malloc(sizeof(pthread_cond_t));
          if(q->queueGroupCond == NULL){
            log_fatal("Unable to allocate group condition");
          }
          if (pthread_cond_init( q->queueGroupCond, NULL ) )
                log_fatal( "unable to init group condition for queue %s", q->name);
        }else{
          q->queueGroupCond = groupCond;
        }

        if(pacing_init(q,pacing)){
          log_fatal( "Unable to init pacing policy" );
        }	

#ifdef DEBUG
	debug( __FUNCTION__, "queue %s successfully created.", q->name );
#endif

	return( q ); 
}

/*--------------------------------------------------------------------------------------
 * Purpose: Create a writer for a queue
 * Input:  the queue associated with this writer
 * Output: the resulting writer or NULL if creation failed
 * Note: Maintains writer details in order to pacing the fast writers when needed.
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
QueueWriter
createQueueWriter( Queue q )
{
	// allocate the writer structure
	QueueWriter writer = malloc( sizeof (struct QueueWriterStruct ) );
	if ( writer == NULL )
		log_fatal("out of memory: malloc of queue writer structure failed");
		// not reached
	memset( writer, 0, sizeof( struct QueueWriterStruct ) );

	// lock the queue to be associated with this writer
	if ( pthread_mutex_lock( &q->queueLock ) )
		log_fatal( "lockQueue: failed");

	// check writer slots are available for this queue
	if ( q->writercount >= MAX_QUEUE_WRITERS ) {
		// no room for another writer, allow caller to decide what to do
		free (writer);
		log_warning("queue %s: no room for another writer", q->name);
		return NULL;
	}

	// assign the writer to this queue
	writer->queue = q;

	// find first available writer slot for this queue
	int avail;
	for( avail = 0; q->used[avail] > 0; avail++ )
	{}
	writer->index= avail;
	q->used[avail] = 1;
			
	// update number of writers
	q->writercount++;
	if ( q->writercount > q->logMaxWriters )
		q->logMaxWriters = q->writercount;

	// writer starts with the no items written
	q->writeCounts[avail] = 0;

	// unlock the queue
	if ( pthread_mutex_unlock( &q->queueLock ) )
		log_fatal( "unlockQueue: failed");

#ifdef DEBUG
	debug(__FUNCTION__, "queue writer %d added to %s, %d writers total", writer->index, q->name, q->writercount);
#endif

	return( writer );
}

/*--------------------------------------------------------------------------------------
 * Purpose: Create a reader for a queue specified by parameters
 * Input:  the queue associated with this reader
 * Output: the resulting reader or NULL if creation failed
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
QueueReader
createQueueReader( Queue *queues, int count )
{
	// allocate the reader structure
	QueueReader reader = malloc( sizeof (struct QueueReaderStruct ) ) ;
	if ( reader == NULL )
		log_fatal("out of memory: malloc of queue reader structure failed");
	memset( reader, 0, sizeof( struct QueueReaderStruct ) );
        reader->queues = malloc(sizeof(Queue*)*count);
        if(reader->queues == NULL)
          log_fatal("out of memory: malloc of queue reader queues failed");
        memset(reader->queues,0,sizeof(Queue*)*count);
        reader->indexes = malloc(sizeof(int*)*count);
        if(reader->indexes == NULL)
          log_fatal("out of memory: malloc of queue reader indexes failed");
        memset(reader->indexes,0,sizeof(int*)*count);
        reader->items = malloc(sizeof(void**)*count);
        if(reader->items == NULL)
          log_fatal("out of memory: malloc of queue reader items failed");
        memset(reader->items,0,sizeof(void**)*count);
        reader->count = count;

        int idx;
        for(idx = 0; idx < count; idx++){
          Queue q = queues[idx];
	  // lock the queue to be associate with this reader
	  if ( pthread_mutex_lock( &q->queueLock ) )
		log_fatal( "lockQueue: failed");

	  // check reader slots are available for this queue
	  if ( q->readercount >= MAX_QUEUE_READERS )
	  {
		// no room for another reader, allow caller to decide what to do
		free ( reader );
		log_warning("queue %s(%d): no room for another reader", q->name, MAX_QUEUE_READERS);
		if ( pthread_mutex_unlock( &q->queueLock ) )
		log_fatal( "unlockQueue: failed");		
		return NULL;
	  }

	  // assign the reader to this queue
	  reader->queues[idx]= q;

	  // find first available reader slot for this queue
	  int avail;
	  for( avail = 0; q->nextItem[avail] != READER_SLOT_AVAILABLE; avail++ )
	  {}
	  reader->indexes[idx] = avail;
		
	  // reader starts with the average position of queue	
	  int i = 0;
	  int count = 0;
	  long tmp = 0;
	  while(  count < q->readercount )
	  {
		if( q->nextItem[i] >= 0 )
		{
			tmp += q->nextItem[i];
			count++;
		}
		i++;	
	  }
	  if( q->readercount == 0 )
		q->nextItem[reader->indexes[idx]] = q->tail;
	  else
	  {
		q->nextItem[reader->indexes[idx]] = tmp/q->readercount;
		for( i=q->nextItem[reader->indexes[idx]]; i<q->tail ; i++)
		{
			int j = i % QUEUE_MAX_ITEMS;
			q->items[j].count++;
		}
	  }
	  q->itemsRead[reader->indexes[idx]] = 0;

	  // update number of readers
	  q->readercount++;
	  if ( q->readercount > q->logMaxReaders )
		q->logMaxReaders = q->readercount;
	
	
	  // unlock the queue
	  if ( pthread_mutex_unlock( &q->queueLock ) )
		log_fatal( "unlockQueue: failed");

#ifdef DEBUG
	  debug(__FUNCTION__, "queue reader %d of %d added to %s", reader->index, q->readercount, q->name);
#endif
        }
	return( reader );
}

/*--------------------------------------------------------------------------------------
 * Purpose: Check if the queue is empty 
 * Input:  Queue to echk
 * Output:  returns 1 if empty, 0 otherwise 
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int 
isQueueEmpty( Queue q )
{
	if ( pthread_mutex_lock( &q->queueLock ) )
		log_fatal( "lockQueue: failed");
	if( q->tail == q->head )
	{
		if ( pthread_mutex_unlock( &q->queueLock ) )
			log_fatal( "unlockQueue: failed");
		return TRUE;
	}
	else
	{
		if ( pthread_mutex_unlock( &q->queueLock ) )
			log_fatal( "unlockQueue: failed");
		return FALSE;	
	} 
}

/*--------------------------------------------------------------------------------------
 * Purpose: Write a new item into the queue
 * Input: queue to write item and the item itself
 * Output: returns 0 on success, 1 on queue_full, but success, -1 on failure
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int 
writeQueue( QueueWriter writer, void *item )
{
  Queue q = writer->queue;

  // lock the queue	
  if ( pthread_mutex_lock( &q->queueLock ) ){
    log_warning( "lockQueue: failed");
    return -1;
  }

  int q_full = pacing_write_post_lock(q);

  // write the data to the next spot in the queue
  q->writeCount++;
  if(  q->readercount > 0 ){
    q->writeCounts[writer->index]++;
    long l = q->tail % QUEUE_MAX_ITEMS;
    q->items[l].count = q->readercount;
    q->items[l].messagBuf = (void *)item;
    q->tail++;
    if ( (q->tail - q->head) > q->logMaxItems){
      q->logMaxItems = q->tail - q->head;
    }
  } else{
    free( item );
  }

  pacing_write_post_write(q,writer->index);

  // unlock the queue
  if ( pthread_mutex_unlock( &q->queueLock ) ){
    log_warning( "unlockQueue: failed");
    return -1;
  }

  // notify blocked readers new data has appeared
  if(  q->readercount > 0 ){
    pthread_cond_broadcast( q->queueGroupCond );
  }

#ifdef DEBUG
  debug(__FUNCTION__, "Writer %d (%d writes in last interval(%d)) wrote to queue %s;  tail/head: %ld/%ld ",
        writer->index, q->writeCounts[writer->index], QueueConfig.pacingInterval, q->name, q->tail, q->head);
#endif	

  /* log a message if we are past the log interval */
  int now = time( NULL );
  if ( now > q->lastLogTime + QueueConfig.logInterval) {
    q->lastLogTime = now;
    log_msg("Queue %s status: tail=%ld head=%ld", q->name, q->tail, q->head); 
    log_msg("Queue %s usage: currentItems=%ld, usedBytes=%ld, peakItems=%ld, allowedItems=%ld", 
            q->name, getItemsUsed(q->name), getBytesUsed(q->name), q->logMaxItems, QUEUE_MAX_ITEMS); 
    log_msg("Queue %s writers: current=%d, peak=%d, allowed=%d", 
            q->name, getWriterCount(q->name), q->logMaxWriters, MAX_QUEUE_WRITERS); 
    log_msg("Queue %s readers: current=%d, peak=%d, allowed=%d", 
            q->name, getReaderCount(q->name), q->logMaxReaders, MAX_QUEUE_READERS); 
  }

  return q_full; 
}


/*--------------------------------------------------------------------------------------
 * Purpose: Determine if all the queues attached to the reader are empty
 * Input: The QueueReader
 * Output: n -> the number of queues that are not empty
 * -------------------------------------------------------------------------------------*/
int
anyQueueReady( QueueReader reader ){
  int idx;
  int nonEmptyCount = 0;
  for(idx=0;idx<reader->count;idx++){
    Queue q = reader->queues[idx];
    int s = reader->indexes[idx];
    if( q->nextItem[s] < q->tail ){
      nonEmptyCount++;
    }
  }
  return nonEmptyCount;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Determine if all of the queues are deceased
 * Input: The QueueReader
 * Output: n -> the number of queues that are not deceased
 * -------------------------------------------------------------------------------------*/
int
anyQueueAlive( QueueReader reader ){
  int idx;
  int aliveCount = 0;
  for(idx=0;idx<reader->count;idx++){
    Queue q = reader->queues[idx];
    int s = reader->indexes[idx];
    if( q->nextItem[s] != READER_SLOT_AVAILABLE ){
      aliveCount++;
    }
  }
  return aliveCount;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Read a specified reader's next item from the queue
 * Input: queue reader and a pointer to the item read.  
 *        if other readers have yet to read this item, the pointer is copy of the item
 *        if this is the last reader to read this item, the pointer is the object itself
 *        in either case the reader should free this item after it has processed it
 * Output: numbers of unread items associated with this reader
 *         or returns READER_SLOT_AVAILABLE if reader has ceased
 * Note: When the reader doesn't have new items to reader, it will be blocked and wait until 
 *       a new item becomes available.  
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long 
readQueue( QueueReader reader )
{

  // just grab the first one for these, we are guaranteed that there is at least one
  // and that if there are more than one they are all the same
  pthread_mutex_t *groupLock = reader->queues[0]->queueGroupLock;
  pthread_cond_t *groupCond = reader->queues[0]->queueGroupCond;

  // if all of the queues are ceased return and error
  if(!anyQueueAlive(reader)){
    return READER_SLOT_AVAILABLE;
  }

  // lock the group
  if ( pthread_mutex_lock( groupLock ) )
    log_fatal( "lockQueue: failed");

  // unlock and wait for an item to become available
  // notice the predicate -- we only wait for a signal when all of the queues
  // are empty
  while(!anyQueueReady(reader)){
    if ( pthread_cond_wait( groupCond, groupLock ) ){
      log_fatal("Queue conditional wait for reader failed: ");
    }
  }
  // now we can unlock the group because we only use it for the signals
  pthread_mutex_unlock( groupLock );

  // again check that at least one of the queues is alive
  if(!anyQueueAlive(reader)){
    log_warning("Ceased reader trying to read from Queue ");
    return READER_SLOT_AVAILABLE;
  }

  // at this point we know that at least one of the queues has data,
  // but we don't know which one
  int idx;
  for(idx = 0; idx < reader->count; idx++){
    
	Queue q = reader->queues[idx];
	int s = reader->indexes[idx];

	// initialize the read item to NULL	
	reader->items[idx] = NULL;

        // if this queue is empty we don't need to do anything
        if( q->nextItem[s] >= q->tail ){
          continue;
        }
	//  if this reader has ceased, also don't do anything
	if( q->nextItem[s] == READER_SLOT_AVAILABLE )
	{
		log_warning("Ceased reader %d trying to read from Queue %s", s, q->name);
                continue;
	}

	// lock the queue 
        if ( pthread_mutex_lock( &q->queueLock ) )
                log_fatal( "lockQueue: failed");

	//  if this reader has ceased, return READER_SLOT_AVAILABLE
	if( q->nextItem[s] == READER_SLOT_AVAILABLE )
	{
		// unlock the queue
		if ( pthread_mutex_unlock( &q->queueLock ) )
			log_fatal( "unlockQueue: failed");
		log_warning("Ceased reader %d trying to read from Queue %s", s, q->name);
		continue;
	}
     
        // let the queuing policies do their work
        pacing_read_post_lock(q);

	// determine where the next item is in the buffer and decrement its reference count
	int i = q->nextItem[s] % QUEUE_MAX_ITEMS; 
	q->items[i].count--;
	if ( q->items[i].count == 0 )
	{
		// return the original if the last reference 
		reader->items[idx] = q->items[i].messagBuf;
		// move to the head of the queue foward to next position 
		q->head++;		
		// prevent accidental reuse
		q->items[i].messagBuf = NULL; 
	} 
	else 
	{
		// return a copy if not the only reference 
		reader->items[idx] = NULL;
		q->copy( &(reader->items[idx]), q->items[i].messagBuf); 
	}
	q->nextItem[s]++; 
	q->itemsRead[s]++;

        // let the queing policies do more work
        pacing_read_post_read(q);
	
	// unlock the queue
	if ( pthread_mutex_unlock( &q->queueLock ) )
		log_fatal( "unlockQueue: failed");

#ifdef DEBUG
	debug( __FUNCTION__, "Reader %d read from queue %s; tail/head: %ld/%ld ",
		s, q->name, q->tail, q->head);
#endif		

  }

	return reader->count;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Copy function for BMF message
 * Input:  pointer to hold copy and original message
 * Output: none
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void 
copyBMF( void **copy, void *original )
{
	BMF bmf = (BMF)original;
	BMF cpy = malloc( bmf->length + BMF_HEADER_LEN );
	if ( cpy == NULL) 
		log_fatal( "out of memory: malloc copy of queue item failed");
		// not reached
	memcpy( cpy, bmf, bmf->length + BMF_HEADER_LEN ); 
	*copy =  (void *) cpy; 
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get Size of a BMF message
 * Input:  pointer to a BMF message
 * Output: the size in bytes
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int 
sizeOfBMF( void *msg )
{
	BMF bmf = (BMF)msg;
	return ( bmf->length + BMF_HEADER_LEN );
}

/*--------------------------------------------------------------------------------------
 * Purpose: Copy function for XML message
 * Input: pointer to hold copy and original message
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void copyXML ( void **copy, void *original )
{
	//int len = strlen((const char *)original);
	int len = getXMLMessageLen((char *)original);
	//u_char *cpy = malloc( (len+1)*sizeof(u_char) );
	u_char *cpy = malloc( len*sizeof(u_char) );
	if ( cpy == NULL) 
		log_fatal( "out of memory: malloc copy of queue item failed");
		// not reached
	//memcpy( cpy, original, len+1 );
	memcpy( cpy, original, len );
	*copy = (void *)cpy;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get Size of a XML message
 * Input: pointer to a XML message
 * Output: the size in bytes
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int sizeOfXML ( void *msg )
{
	return strlen((const char *)msg);
}

/*--------------------------------------------------------------------------------------
 * Purpose: Free the queue
 * Input:  the queue to destroy
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void 
destroyQueue(Queue q)
{
	// free all publication data
        if ( pthread_mutex_lock( &q->queueLock ) )
		log_fatal( "lockQueue: failed");

	int i;
	for( i = 0; i < QUEUE_MAX_ITEMS; i++ )
	{
		if(q->items[i].messagBuf != NULL)
		{
			free( q->items[i].messagBuf );
			q->items[i].messagBuf = NULL;
		}
	}
	//free(q->items);

	// free the queue name string
	free(q->name);

	// clear the structure as a precaution
	memset(q, 0, sizeof( struct QueueStruct)); 

	if ( pthread_mutex_unlock( &q->queueLock ) )
		log_fatal( "unlockQueue: failed");

	free( q );

	return;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Destroy a writer for a queue
 * Input:  the queue writer to destroy
 * Output: 
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void	
destroyQueueWriter( QueueWriter writer)
{
	if ( writer == NULL )
		return;
			
	Queue q = writer->queue;

	if ( pthread_mutex_lock( &q->queueLock ) )
		log_fatal( "lockQueue: failed");

	// delete the writer from the queue structure
	q->writercount--;
	q->used[writer->index] = 0;
	q->writeCounts[writer->index] = 0;

	if ( pthread_mutex_unlock( &q->queueLock ) )
		log_fatal( "unlockQueue: failed");
			
	log_msg("Writer %d removed from Queue %s", writer->index, q->name);

	free( writer);

	return;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Destroy a reader, called by external functions
 * Input:  the queue reader to destroy
 * Output:
 * Note:  typically called after a reader has been canceled.   
 *       any future attempt to read using this reader will return READER_SLOT_AVAILABLE
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void 
destroyQueueReader( QueueReader reader)
{
	if (reader == NULL) 
		return;
	
        int idx;
        for(idx = 0; idx< reader->count; idx++){		
	  Queue q = reader->queues[idx];
	  if ( pthread_mutex_lock( &q->queueLock ) )
		log_fatal( "lockQueue: failed");

	  // delete the reader from the queue structure
	  if(q->nextItem[reader->indexes[idx]] >= 0 )
	  {
		/* decrement reference counts for all affected items by this reader*/
		long i = 0;
		for ( i = q->nextItem[reader->indexes[idx]]; i < q->tail; i++ )
		{
			long j = i %  QUEUE_MAX_ITEMS;
			q->items[j].count--;
			if ( q->items[j].count == 0 )
			{
				free( q->items[j].messagBuf );
				q->items[j].messagBuf = NULL;
				q->head++;
			}				
		}
	  }
	  q->readercount--;
	  q->nextItem[ reader->indexes[idx]] = READER_SLOT_AVAILABLE;
	  q->itemsRead[ reader->indexes[idx]] = 0;

	  if ( pthread_mutex_unlock( &q->queueLock ) )
		log_fatal( "unlockQueue: failed");

	  log_msg("Reader %d removed from Queue %s", reader->indexes[idx], q->name);
       
        } 
        free( reader->queues );
        free( reader->indexes );
        free( reader->items );
	free( reader );

	return;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return queue pacing on threshhold
 * Input:
 * Output: the pacing on threshold
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
float getPacingOnThresh( )
{
	return QueueConfig.pacingOnThresh;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set queue pacing on threshhold
 * Input: the pacing on threshold
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setPacingOnThresh(float pacingOnThresh)
{
	QueueConfig.pacingOnThresh = pacingOnThresh;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return queue pacing off threshhold
 * Input:
 * Output: the pacing off threshold
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
float getPacingOffThresh()
{
	return QueueConfig.pacingOffThresh;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set queue pacing off threshhold
 * Input: the pacing off threshold
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setPacingOffThresh(float pacingOffThresh)
{
	QueueConfig.pacingOffThresh = pacingOffThresh;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Return queue's alpha
 * Input:
 * Output: the queue's alpha
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
float getAlpha()
{
	return QueueConfig.alpha;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set queue's alpha
 * Input:  the alpha
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setAlpha(float alpha)
{
	QueueConfig.alpha= alpha;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the minimal writes limit of the queue
 * Input:  
 * Output: the minimal writes limit
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getMinimumWritesLimit()
{
	return QueueConfig.minWritesPerInterval;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the minimal writes limit of the queue
 * Input:  the minimal writes limit  
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setMinimumWritesLimit(int minWritesLimit)
{
	QueueConfig.minWritesPerInterval= minWritesLimit;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return queue pacing interval
 * Input:  
 * Output: the pacing interval in seconds
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getPacingInterval()
{
	return QueueConfig.pacingInterval;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set queue pacing interval
 * Input: the pacing interval in seconds
 * Output: 
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setPacingInterval(int pacingInterval)
{
	QueueConfig.pacingInterval = pacingInterval;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return queue by giving queue name in string
 * Input: the queue name as a string
 * Output: the queue or NULL if no such queue
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
Queue getQueueByName(char *name)
{
	if(strcmp(name, PEER_QUEUE_NAME) == 0)
		return peerQueue;
	if(strcmp(name, LABEL_QUEUE_NAME) == 0)
		return labeledQueue;
	if(strcmp(name, XML_U_QUEUE_NAME) == 0)
		return xmlUQueue;	
	if(strcmp(name, XML_R_QUEUE_NAME) == 0)
		return xmlRQueue;
	
	log_warning("Unable to find a queue with name %s", name);
	return NULL;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return how many bytes are used by the queue
 * Input: the queue name in string
 * Output: the number of used bytes
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long getBytesUsed(char *queueName)
{
        Queue q = getQueueByName(queueName);
        int size = 0;
        if(q == NULL)
        {
                return 0;
        }
        else
        {
                long i;
                //lock the queue
                if ( pthread_mutex_lock( &q->queueLock ) )
                        log_fatal( "lockQueue: failed");

                for ( i = q->head; i < q->tail; i++ )
                {
                        long j = i %  QUEUE_MAX_ITEMS;
                        size += sizeof(long);
                        size += q->sizeOf(q->items[j].messagBuf);
                }

                //unlock the queue
                if ( pthread_mutex_unlock( &q->queueLock ) )
                        log_fatal( "unlockQueue: failed");
        }
        return size;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Return how many items are used by the queue
 * Input: the queue name in string
 * Output: the number of used items in the queue
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long 
getItemsUsed(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	else
	{	
		return q->tail - q->head;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return how many items are allowed by the queue
 * Input: the queue name in string
 * Output: the number of allowed items in the queue
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getItemsTotal(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	return QUEUE_MAX_ITEMS;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return logged max items
 * Input: the queue name in string
 * Output: the number of logged max items
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getLoggedMaxItems(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	return q->logMaxItems;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return logged max readers
 * Input: the queue name in string
 * Output: the number of logged max readers
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getLoggedMaxReaders(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	return q->logMaxReaders;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return logged max writers
 * Input: the queue name in string
 * Output: the number of logged max writers
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getLoggedMaxWriters(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	return q->logMaxWriters;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return cumulative pacing count
 * Input: the queue name in string
 * Output: the cumulative pacing count
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getPacingCount(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	return q->logPacingCount;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return current pacing count
 * Input: the queue name in string
 * Output: the cumulative pacing count
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getCurrentPacingCount(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}

	int current = q->logPacingCount - q->lastSentLogPacingCount;
	q->lastSentLogPacingCount = q->logPacingCount;

	return current;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return pacing state of the queue
 * Input: the queue name in string
 * Output: the pacing state,  TRUE means on and FALSE means off
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getPacingState(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return FALSE;
	}
	else
	{	
		if(q->pacingFlag == FALSE)
			return FALSE;
		else
			return TRUE;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the current writes limit of the queue. If pacing is off, return 0.
 * Input: the queue name in string
 * Output: the current writes limit
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getCurrentWritesLimit(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	else
	{
		return q->writesLimit;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the queue name associated with this QueueWriter
 * Input: the Queue Writer
 * Output: the name of the queue associated with this writer*          or NULL if no queue for the writer
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 * He Yan @ June 15, 2008
 * ------------------------------------------------------------------------------------
*/
char * 
getQueueNameForWriter(QueueWriter writer)
{
	// get the writer's Queue structure
	Queue q = writer->queue;
	// get the length of the queue name
	int length = strlen(q->name);
	// allocate memory for the result
	char *ans = malloc(length);
	if (ans == NULL) {
		log_err("getQueueNameForWriter: couldn't allocate string memory");
		return NULL;
	}
	// copy the string and return result
	strncpy(ans, q->name, length);
        return ans;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the queue name associated with this QueueReader
 * Input: the Queue Reader
 * Output: the name of the queue associated with this writer
*          or NULL if no queue for the writer
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 *       3. this is only used by the clients module. Therefore, it is
 *          OK to return the first name, however, this should be fixed
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
char * 
getQueueNameForReader(QueueReader reader)
{
	// get the reader's Queue structure
	Queue q = reader->queues[0];
	// get the length of the queue name
	int length = strlen(q->name);
	// allocate memory for the result
	char *ans = malloc(length);
	if (ans == NULL) {
		log_err("getQueueNameForReader: couldn't allocate string memory");
		return NULL;
	}
	// copy the string and return result
	strncpy(ans, q->name, length);
        return ans;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the queue name associated with this QueueReader
 * Input: the Queue Reader
 * Output: the index of the Queue Reader or -1 if no index
 *       3. this is only used by the clients module. Therefore, it is
 *          OK to return the first name, however, this should be fixed
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getQueueIndexForReader(QueueReader reader)
{
        return reader->indexes[0];
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the number of unread items of a reader in the queue.
 * Input: the queue name in string and the reader's index
 * Output: the number of unread items for this reader
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long 
getReaderUnreadItems(char *queueName, int readerIndex)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	else
	{
		return q->tail - q->nextItem[readerIndex];
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the number of items read by this reader.
 * Input: the queue name in string and the reader's index
 * Output: the number of items read by this reader
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long 
getReaderReadItems(char *queueName, int readerIndex) 
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	else
	{
		return q->itemsRead[readerIndex];
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the position of a reader in the queue.
 * Input: the queue name in string and the reader's index
 * Output: the position of this reader
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long getReaderPosition(char *queueName, int readerIndex)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	else
	{
		return q->nextItem[readerIndex];
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the count of readers of the queue.
 * Input: the queue name in string
 * Output: the number of readers
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getReaderCount(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	else
	{
		return q->readercount;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Return the count of writers of the queue.
 * Input: the queue name in string
 * Output: the number of writers
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getWriterCount(char *queueName)
{
	Queue q = getQueueByName(queueName);
	if(q == NULL)
	{
		return 0;
	}
	else
	{
		return q->writercount;
	}
}

