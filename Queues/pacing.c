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
 *  File: pacing.c
 * 	Authors: He Yan, Dan Massey
 *  Date: Oct 7, 2008
 */
/* need for malloc*/
#include <stdlib.h>

/* pacing function prototypes */
#include "pacing.h"

/* needed for queue data type */
#include "queue.h"

/* needed for QUEUE_MAX_ITEMS */
#include "../site_defaults.h"

/* needed for TRUE/FALSE macro */
#include "../Util/bgpmon_defaults.h"

/* needed for logging */
#include "../Util/log.h"

/* needed for system functions such as sleep */
#include <unistd.h>
/* needed for memory functions as memcpy and memset*/
#include <string.h>
/*  needed to lock structures */
#include <pthread.h>

//#define DEBUG

/******************************************************************************
 * Purpose: any initialization needed by the queueing policy, when the queue
 * is created
 * Input: The queue and the desired pacing policy
 * Output: 0 on success
 * Cathie Olschanowsky @ Feb. 2012
******************************************************************************/
int
pacing_init(Queue queue, int policy)
{
  // set new pacing enable flag
  queue->pacingPolicy = policy;
  queue->readCount = 0;

  if(policy == ideal_reader){
    queue->idealReaderPosition = queue->head;
  }

  return 0;
}

/******************************************************************************
 * Purpose: any work that needs to be done immediatly following the lock
 * Input: The queue 
 * Output: 0 on success
 * Cathie Olschanowsky @ Feb. 2012
******************************************************************************/
int
pacing_read_post_lock(Queue queue)
{
  return 0;
}

/******************************************************************************
 * Purpose: any work that needs to be done immediatly after the read
 * Input: The queue 
 * Output: 0 on success
 * Cathie Olschanowsky @ Feb. 2012
******************************************************************************/
int
pacing_read_post_read(Queue queue)
{
  // update writes limit and reset readcout and writecount if needed
  updateInterval(queue);
  queue->readCount++;

  //only if use the old pacing, otherwise skip this step 
  if(queue->pacingPolicy == ff_jump) {
  // check if need to stop pacing
    if( checkPacingStop(queue) ){
      log_warning("%s queue error stopping pacing rules", queue->name);
      return 1;
    }
  }

  return 0;
}


/******************************************************************************
 * Purpose: any work that needs to be done immediatly after lock
 * Input: The queue and the desired pacing policy
 * Output: 0 on success, 1 if writing to full queue, -1 on failure
 * Cathie Olschanowsky @ Feb. 2012
******************************************************************************/
int
pacing_write_post_lock(Queue queue)
{
  int i;
  int queue_full = 0;

  // in the backlog policy when the queue is full we need to switch
  // to backlog mode, if already in backlog mode, we need to skip
  // only one item in the queue
  if( queue->pacingPolicy == backlog ){
    // for this one we call full one slot early 
    if( (queue->tail-queue->head) >= (QUEUE_MAX_ITEMS-1) ){
      queue_full = 1;
    }    
  }


  // if the queue is full -- respond according to policy
  if (( queue->tail - queue->head) >= QUEUE_MAX_ITEMS ){

    queue_full = 1;

    #ifdef DEBUG
    log_warning("queue %s is full, head=%ld, tail=%ld; adjusting slowest readers", queue->name, queue->head, queue->tail);
    #endif

    // we have to figure out which queue reader is going the slowest
    for ( i = 0; i < MAX_QUEUE_READERS; i++ ){

      #ifdef DEBUG
      debug(__FUNCTION__, "queue %s's head is %ld", queue->name, queue->head);
      if (queue->nextItem[i] >= 0){
        debug(__FUNCTION__, "queue %s's reader %d's next item is %ld", queue->name, i, queue->nextItem[i]);
      }
      #endif

      // if this reader is the slowest one, adjust it
      if ( queue->head == queue->nextItem[i] ){
        if(queue->pacingPolicy == ff_jump){
          adjustSlowestQueueReader(queue, i);

        }else if(queue->pacingPolicy == ideal_reader || queue->pacingPolicy == backlog){
          // step the slowest reader ahead one step
          long j = queue->nextItem[i] %  QUEUE_MAX_ITEMS;
          queue->items[j].count--;
          if ( queue->items[j].count == 0 ){
            free( queue->items[j].messagBuf );
            queue->items[j].messagBuf = NULL;
            queue->head++;
          }
          queue->nextItem[i]++;
          #ifdef DEBUG
          log_warning("Move the slowest client 1 pos forward");
          #endif
        }
      }
    }
  } // end of if the queue is full

  // should not happen, fatal error if it happens
  if (( queue->tail - queue->head ) >= QUEUE_MAX_ITEMS ){
    log_warning("%s queue is still FULL after adjusting readers", queue->name);
    return -1;
  }

  // old pacing: update pacing limit and reset readcout and writecount if needed
  // new pacing: update write count moveing average
  updateInterval(queue);

  // only for the new pacing algorithm  
  if(queue->pacingPolicy == ideal_reader) {
    if ( ( (float)(queue->tail - queue->head)/(float)QUEUE_MAX_ITEMS ) >=  QueueConfig.pacingOnThresh){
#ifdef DEBUG            
      log_warning("queue %s is over pacing threshold, head=%ld, tail=%ld; adjusting slowest readers", queue->name, QueueConfig.pacingOnThresh, queue->head, queue->tail);
#endif
      for ( i = 0; i < MAX_QUEUE_READERS; i++ ){
#ifdef DEBUG
        debug(__FUNCTION__, "queue %s's head is %ld", queue->name, queue->head);
        if (queue->nextItem[i] >= 0){
          debug(__FUNCTION__, "queue %s's reader %d's next item is %ld", queue->name, i, queue->nextItem[i]);
        }
#endif
        // if this reader is the slowest one, adjust it
        if ( queue->head == queue->nextItem[i] ){
          adjustSlowestQueueReader(queue, i);
        }
      }
    }
  }

  // only for the old pacing algorithm, otherwise skip this step        
  if(queue->pacingPolicy == ff_jump) {
    // check if need to stop pacing
    checkPacingStop(queue);
  }

  return queue_full;

}

/******************************************************************************
 * Purpose: any work that needs to be done immediatly after read
 * Input: The queue and the desired pacing policy
 * Output: 0 on success
 * Cathie Olschanowsky @ Feb. 2012
******************************************************************************/
int
pacing_write_post_write(Queue queue,int writer)
{
  // only if use the old pacing, otherwise skip this step 
  if(queue->pacingPolicy == ff_jump) {
    // apply pacing rules to the current writer
    if ( applyPacing(queue, writer) ){
      log_fatal("%s queue error applying pacing rules", queue->name);
      // not reached
    }     
  }       
  return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: apply pacing rules to this queue
 * Input:   the queue and writer (identified by index) to apply pacing rules
 * Output:   0 on success,  1 on error
 * Dan Massey @ July 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
applyPacing(Queue q, int writerindex)
{

	//  turn on pacing if queue size exceeds threshhold
	if ( ( (float)(q->tail - q->head)/(float)QUEUE_MAX_ITEMS ) >=  QueueConfig.pacingOnThresh)
	{
		if ( q->pacingFlag == FALSE )
			q->logPacingCount++;
		q->pacingFlag = TRUE;
	}
	
	// if not pacing, nothing more to do 
	if (q->pacingFlag == FALSE) 
		return 0;

	if( q->writeCounts[writerindex] > q->writesLimit)
	{
		// this writer must pause until a new interval arrives before writing again                        
		if ( pthread_mutex_unlock( &q->queueLock ) )
                	log_fatal( "unlockQueue: failed");
#ifdef DEBUG
		debug(__FUNCTION__, "Queue %s writer %d is paced for %d seccond with write count of %d and limit of %d", q->name, writerindex, q->tick + QueueConfig.pacingInterval - time(NULL), q->writeCounts[writerindex], q->writesLimit);
#endif
		sleep( q->tick + QueueConfig.pacingInterval - time(NULL));
		if ( pthread_mutex_lock( &q->queueLock ) )
                	log_fatal( "lockQueue: failed");
	}

	return 0;

}

/*--------------------------------------------------------------------------------------
 * Purpose: Check if need to stop pacing for this queue
 * Input: queue to check 
 * Output: 0 on success, 1 on error
 * Dan Massey @ July 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
checkPacingStop(Queue q)
{
	if ( ( (float)(q->tail - q->head)/(float)QUEUE_MAX_ITEMS ) < QueueConfig.pacingOffThresh )
		q->pacingFlag = FALSE;
	return 0;
}


/*--------------------------------------------------------------------------------------
 * Purpose: calculate the number of allowed writes per interval for each writer to this queue
 * Input:  the queue that needs a new limit
 * Output:   returns 0 on success, 1 on error
 *           sets the queue's writesLimit parameter
 * Dan Massey @ July 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
calculateWritesLimit (Queue q)
{
	if( q->readercount == 0)
	{
		// if there are no readers, leave the writelimit unchanged.
                // with no readers, the queue will be drained and
                // pacing will turn off.   the writelimit will not be used.
		return 0;
	}

	if( q->writercount == 0)
	{
		// if there are no writers, leave the writelimit unchanged.
                // with no writers, the queue will be drained and
                // pacing will turn off.   the writelimit will not be used.
		return 0;
	}

	// objective is to write at a pace that matches the average reader

	// calculate the average speed of a reader
	int averageReads = q->readCount/q->readercount;

	// calculate how much each writer can write if shared equall
	// note this function is called by a writer thus writercount >= 1
	int instantWritesLimit = averageReads/q->writercount;

	// combine the current instant value with past history, using an
	// exponential weighted moving average
	q->writesLimit = 
		// a percentage of the previous limit; 1-alpha in an EWMA
		(1 - QueueConfig.alpha) * q->writesLimit + 
		// a percentage of the number of items read last interval ;  alpha in an EWMA
		QueueConfig.alpha * instantWritesLimit;

	
	// add an upbound on the amount a writer can write
	int availableQueueItems = QUEUE_MAX_ITEMS - (q->tail - q->head);
	int upbound = availableQueueItems / 2 ;
	//  allow each writer to consume up to half the remaining queue
	if( q->writesLimit > upbound )
		q->writesLimit = upbound;

	// ensure each writer can write at least some minimum number of entries
	if( q->writesLimit < QueueConfig.minWritesPerInterval)
		q->writesLimit = QueueConfig.minWritesPerInterval;	

	//log_msg( "Queue %s: new writes limit %d", q->name, q->writesLimit);

	return 0;
};

/*--------------------------------------------------------------------------------------
 * Purpose: calculate EWMA writes of all writers per interval 
 * Input:  the queue that needs a new EWMA writes
 * Output:   returns 0 on success, 1 on error
 *           sets the queue's EWMA writes
 * He Yan @ Sep 19, 2009
 * -------------------------------------------------------------------------------------*/ 
int 
calculateEWMAwrites (Queue q)
{	
	if(q->writesEWMA == 0)
		q->writesEWMA = q->writeCount;
	else
		q->writesEWMA = (1 - QueueConfig.alpha) * q->writesEWMA + QueueConfig.alpha * q->writeCount;
	
	return 0;
}
/*--------------------------------------------------------------------------------------
 * Purpose: Check if a new interval starts. If so, update writes limit and reset
 *          readcout and writecount.
 * Input:  the queue that needs to update interval
 * Output: 
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void updateInterval( Queue q )
{
	int now =  time( NULL );
	if( now < q->tick + QueueConfig.pacingInterval)
		return;

        while( now >= q->tick + QueueConfig.pacingInterval ){
		q->tick = q->tick + QueueConfig.pacingInterval;
		if(q->pacingPolicy == ff_jump){

			calculateWritesLimit( q );
			q->readCount = 0;
			memset(q->writeCounts, 0, sizeof(int)*MAX_QUEUE_WRITERS );
#ifdef DEBUG
			float util = (float)(q->tail - q->head)/(float)QUEUE_MAX_ITEMS;
			debug(__FUNCTION__, "Queue:%f, PacingOn: %f, reader(%d) current writes limit:%d readcout:%d, writecount:%d, %d, %d, pacingscount:%d",
				util, QueueConfig.pacingOnThresh, q->pacingFlag, q->writesLimit, q->readCount, q->writeCounts[0], q->writeCounts[1], q->writeCounts[2], q->logPacingCount);
#endif
		}else if(q->pacingPolicy == ideal_reader){
			// update the position of ideal reader
			q->idealReaderPosition += q->writesEWMA;
			if(q->idealReaderPosition > q->tail)
				q->idealReaderPosition = q->tail;  
			// update the moving average
			calculateEWMAwrites( q );
#ifdef DEBUF
			log_msg("Now ideal reader is at %ld, head is %ld, tail is %ld, %d readers", q->idealReaderPosition, q->head, q->tail, q->readercount) ;
#endif
			// reset the write count
			q->writeCount = 0;
		}
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: skip some messages for the slowest reader when queue is full
 * Input:  queue and reader to adjust
 * Output:
 * Note: Assumes that the queue lock is already in place
 *         Skip all the messages that only unread for the slowest reader
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void
adjustSlowestQueueReader( Queue q, int readerIndex )
{
        /* decrement reference counts for all affected items by this reader*/
        long i = 0;
        long tmpPos = q->nextItem[readerIndex];
        long destPos;
        if(q->pacingPolicy == ff_jump)
        {
                destPos = q->tail;
        }
        else
        {
                destPos = q->idealReaderPosition;
        }

        if (destPos <= q->nextItem[readerIndex] )
        {
#ifdef DEBUG
                log_msg("Slowest reader at %ld is still faster than the ideal reader at %ld", q->nextItem[readerIndex], q->idealReaderPosition);
#endif                
                return;
        }

        // clean up the queue contents
        for ( i = q->nextItem[readerIndex]; i < destPos; i++ )
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

        q->nextItem[readerIndex] = destPos;

        if(q->pacingPolicy == ff_jump){
          log_msg("%d messages are skipped for Reader %d in queue %s", q->nextItem[readerIndex]-tmpPos, readerIndex, q->name);
        }else{
          log_msg("%d messages are skipped for Reader %d in queue %s, ideal reader is at %ld", q->nextItem[readerIndex]-tmpPos, readerIndex, q->name, q->idealReaderPosition);
        }

        return;
}


