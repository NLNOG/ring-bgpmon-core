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
 *  File: queue.h
 * 	Authors: He Yan, Dan Massey
 *  Date: Oct 7, 2008 
 */

#ifndef QUEUE_H_
#define QUEUE_H_

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

/* need the internal structures */
#include "queueinternal.h"

/* The QueueReader and QueueWriter structures are
 * used by other threads and modules to simplify 
 * reading and writing from the Queues. 
 */
typedef struct QueueReaderStruct *QueueReader;
typedef struct QueueWriterStruct *QueueWriter;

/* The Queue settings structure holds size limits and  
 * pacing parameters related to queue management.
 */
QueueConfiguration  QueueConfig;

/*Peer queue 
 *   Written by: Peering Module
 *   Read by: Labeling Module
 */
Queue peerQueue;

/*MRT queue
 *  Written by: MRT Module
 *  Read by: Labeling Module
 */
Queue mrtQueue;

/*Labeled Data queue,
 *  Written by: Labeling Module & Periodic Events module
 *  Read by: XML Module
 */
Queue labeledQueue;

/*XML queue,
 *  Written by: XML Module
 *  Read by: Clients Module
 */
Queue xmlUQueue;
Queue xmlRQueue;

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default queue parameters.
 * Input: none
 * Output: returns 0 on success, 1 on failure.
 * Dan Massey @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int initQueueSettings(); 

/*--------------------------------------------------------------------------------------
 * Purpose: Read the queue settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure.
 * Dan Massey @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readQueueSettings( ); 

/*--------------------------------------------------------------------------------------
 * Purpose: Save the queue settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure.
 * Dan Massey @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int saveQueueSettings( ); 

/*--------------------------------------------------------------------------------------
 * Purpose: Create a queue instance
 * Input:  the copy function for queue elements, the queue name, and the name length
 * Output:  the resulting queue, exits on fatal error if creation fails
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
Queue createQueue( void (*copy)(void **copy, void *original), int (*sizeOf)(void *msg),
                   char *name, int pacing,pthread_cond_t *groupCond,
                   pthread_mutex_t *groupLock);

/*--------------------------------------------------------------------------------------
 * Purpose: Create a writer for a queue
 * Input:  the queue associated with this writer
 * Output: the resulting writer or NULL if creation failed
 * Note: Maintains writer details in order to pacing the fast writers when needed.
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
QueueWriter createQueueWriter( Queue q );

/*--------------------------------------------------------------------------------------
 * Purpose: Create a reader for a queue specified by parameters
 * Input:  the queue associated with this reader
 * Output: the resulting reader or NULL if creation failed
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
QueueReader createQueueReader( Queue *q, int count );

/*--------------------------------------------------------------------------------------
 * Purpose: Check if the queue is empty
 * Input:  Queue to echk
 * Output:  returns TRUE if empty, FALSE otherwise
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int isQueueEmpty( Queue q );

/*--------------------------------------------------------------------------------------
 * Purpose: Write a new item into the queue
 * Input: queue to write item and the item itself
 * Output: returns 0, exits on fatal error if write fails
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int writeQueue( QueueWriter writer, void *item );

/*--------------------------------------------------------------------------------------
 * Purpose: Read a specified reader's next item from the queue
 * Input: queue reader and a pointer to the item read.
 *        if other readers have yet to read this item, the pointer is copy of the item
 *        if this is the last reader to read this item, the pointer is the object itself
 *        in either case the reader should free this item after it has processed it
 * Output: numbers of unread items associated with this reader
 *         or returns READER_SLOT_AVAILABLE if reader has ceased 
 * Note: When the reader doesn't have new items to reader, it will be blocked and wait 
 *       until a new item becomes available.
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long readQueue( QueueReader reader);
/* flags to indicate a reader's status  */
#define READER_SLOT_AVAILABLE -1

/*--------------------------------------------------------------------------------------
 * Purpose: Copy function for BMF message
 * Input:  pointer to hold copy and original message
 * Output: none
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void copyBMF( void **copy, void *original );

/*--------------------------------------------------------------------------------------
 * Purpose: Get Size of a BMF message
 * Input:  pointer to a BMF message
 * Output: the size in bytes
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int sizeOfBMF ( void *msg );

/*--------------------------------------------------------------------------------------
 * Purpose: Copy function for XML message
 * Input: pointer to hold copy and original message
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void copyXML ( void **copy, void *original );

/*--------------------------------------------------------------------------------------
 * Purpose: Get Size of a XML message
 * Input: pointer to a XML message
 * Output: the size in bytes
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int sizeOfXML ( void *msg );

/*--------------------------------------------------------------------------------------
 * Purpose: Free the queue
 * Input:  the queue to destroy
 * Output: none
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void destroyQueue(Queue q);

/*--------------------------------------------------------------------------------------
 * Purpose: Destroy a writer for a queue
 * Input:  the queue writer to destroy
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void destroyQueueWriter( QueueWriter writer);

/*--------------------------------------------------------------------------------------
 * Purpose: skip some messages for the slowest reader when queue is full
 * Input:  queue and reader to adjust
 * Output:
 * Note: Assumes that the queue lock is already in place
 *         Skip all the messages that only unread for the slowest reader
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void 
adjustSlowestQueueReader( Queue q, int readerIndex );

/*--------------------------------------------------------------------------------------
 * Purpose: Destroy a reader, called by external functions
 * Input:  the queue reader to destroy
 * Note:  typically called after a reader has been canceled.   
 *       any future attempt to read using this reader will return READER_SLOT_AVAILABLE
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void destroyQueueReader( QueueReader reader);

/*--------------------------------------------------------------------------------------
 * Purpose: Return queue pacing on threshhold
 * Input:
 * Output: the pacing on threshold
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
float getPacingOnThresh( );

/*--------------------------------------------------------------------------------------
 * Purpose: Set queue pacing on threshhold
 * Input: the pacing on threshold
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setPacingOnThresh(float);

/*--------------------------------------------------------------------------------------
 * Purpose: Return queue pacing off threshhold
 * Input:
 * Output: the pacing off threshold
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
float getPacingOffThresh();

/*--------------------------------------------------------------------------------------
 * Purpose: Set queue pacing off threshhold
 * Input: the pacing off threshold
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setPacingOffThresh(float);

/*--------------------------------------------------------------------------------------
 * Purpose: Return queue's alpha
 * Input:
 * Output: the queue's alpha
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
float getAlpha();

/*--------------------------------------------------------------------------------------
 * Purpose: Set queue's alpha
 * Input:  the alpha
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setAlpha(float);

/*--------------------------------------------------------------------------------------
 * Purpose: Return the minimal writes limit of the queue
 * Input:
 * Output: the minimal writes limit
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getMinimumWritesLimit();

/*--------------------------------------------------------------------------------------
 * Purpose: Set the minimal writes limit of the queue
 * Input:  the minimal writes limit
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setMinimumWritesLimit(int);

/*--------------------------------------------------------------------------------------
 * Purpose: Return queue pacing interval
 * Input:
 * Output: the pacing interval in seconds
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getPacingInterval();

/*--------------------------------------------------------------------------------------
 * Purpose: Set queue pacing interval
 * Input: the pacing interval in seconds
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void setPacingInterval(int);

/*--------------------------------------------------------------------------------------
 * Purpose: Return queue by giving queue name in string
 * Input: the queue name as a string
 * Output: the queue or NULL if no such queue
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
Queue getQueueByName(char *name);

/*--------------------------------------------------------------------------------------
 * Purpose: Return how many bytes are used by the queue
 * Input: the queue name in string
 * Output: the number of used bytes
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long getBytesUsed(char *queueName);

/*--------------------------------------------------------------------------------------
 * Purpose: Return how many items are used by the queue
 * Input: the queue name in string
 * Output: the number of used items in the queue
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long getItemsUsed(char *queueName);

/*--------------------------------------------------------------------------------------
 * Purpose: Return how many items are allowed by the queue
 * Input: the queue name in string
 * Output: the number of allowed items in the queue
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getItemsTotal(char *queueName);

/*--------------------------------------------------------------------------------------
 * Purpose: Return logged max items
 * Input: the queue name in string
 * Output: the number of logged max items
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getLoggedMaxItems(char *queueName);

/*--------------------------------------------------------------------------------------
 * Purpose: Return logged max readers
 * Input: the queue name in string
 * Output: the number of logged max readers
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getLoggedMaxReaders(char *queueName);

/*--------------------------------------------------------------------------------------
 * Purpose: Return logged max writers
 * Input: the queue name in string
 * Output: the number of logged max writers
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getLoggedMaxWriters(char *queueName);

/*--------------------------------------------------------------------------------------
 * Purpose: Return cumulative pacing count
 * Input: the queue name in string
 * Output: the cumulative pacing count
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getPacingCount(char *queueName);
int getCurrentPacingCount(char *queueName);

/*-------------------------------------------------------------------------------------
-
 * Purpose: Return pacing state of the queue
 * Input: the queue name in string
 * Output: the pacing state,  TRUE means on and FALSE means off
 * He Yan @ June 15, 2008
 * ------------------------------------------------------------------------------------*/
int getPacingState(char *queueName);

/*--------------------------------------------------------------------------------------
 * Purpose: Return the current writes limit of the queue. If pacing is off, return 0.
 * Input: the queue name in string
 * Output: the current writes limit
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getCurrentWritesLimit(char *queueName);

/*-------------------------------------------------------------------------------------
 * Purpose: Return the queue name associated with this QueueWriter
 * Input: the Queue Writer
 * Output: the name of the queue associated with this writer 
*          or NULL if no queue for the writer
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 * He Yan @ June 15, 2008
 * ------------------------------------------------------------------------------------
 */
char * getQueueNameForWriter(QueueWriter writer);

/*-------------------------------------------------------------------------------------
 * Purpose: Return the queue name associated with this QueueReader
 * Input: the Queue Reader
 * Output: the name of the queue associated with this writer 
*          or NULL if no queue for the writer
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 * He Yan @ June 15, 2008
 * ------------------------------------------------------------------------------------
 */
char * getQueueNameForReader(QueueReader reader);

/*-------------------------------------------------------------------------------------
 * Purpose: Return the queue name associated with this QueueReader
 * Input: the Queue Reader
 * Output: the index of the Queue Reader or -1 if no index
 * He Yan @ June 15, 2008
 * ------------------------------------------------------------------------------------
 */
int getQueueIndexForReader(QueueReader reader);

/*--------------------------------------------------------------------------------------
 * Purpose: Return the number of unread items of a reader in the queue.
 * Input: the queue name in string and the reader's index
 * Output: the number of unread items for this reader
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long getReaderUnreadItems(char *queueName, int readerIndex);

/*--------------------------------------------------------------------------------------
 * Purpose: Return the number of items read by this reader. 
 * Input: the queue name in string and the reader's index
 * Output: the number of items read by this reader
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long getReaderReadItems(char *queueName, int readerIndex);

/*--------------------------------------------------------------------------------------
 * Purpose: Return the position of a reader in the queue.
 * Input: the queue name in string and the reader's index
 * Output: the position of this reader
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
long getReaderPosition(char *queueName, int readerIndex);

/*--------------------------------------------------------------------------------------
 * Purpose: Return the count of readers of the queue.
 * Input: the queue name in string
 * Output: the number of readers
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getReaderCount(char *queueName);

/*--------------------------------------------------------------------------------------
 * Purpose: Return the count of writers of the queue.
 * Input: the queue name in string
 * Output: the number of writers
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getWriterCount(char *queueName);

#endif /*QUEUE_H_*/
