/* 
 * 	Copyright (c) 2010 Colorado State University
 * 
 *	Permission is hereby granted, free of charge, to any person
 *      obtaining a copy of this software and associated documentation
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
 *  File: bgpmon_defaults.h
 * 	Authors:  Dan Massey
 *  Date: Oct 7, 2008
 */

#ifndef BGPMON_DEFAULTS_H_
#define BGPMON_DEFAULTS_H_

#define TRUE 1
#define FALSE 0 

#define ADDR_MAX_CHARS 256

#define PREFIX_TABLE_SIZE 40000

#define ATTRIBUTE_TABLE_SIZE 40000

#define MAX_HASH_COLLISION 400

#define PEER_GROUP_MAX_CHARS 256

long bgpmon_start_time;

/* MAX_CLI_IDS controls how many CLI connections can simultaneoously 
 * connect to a BGPmon instance.  
 */
#define MAX_CLI_IDS 100

/* Set the maximum number of characters that can appear in any password
 * This includes the access and enable passwords.
 */
#define PASSWORD_MAX_CHARS 256

/* Set the maximum number of characters that can appear in any file
 * read or written by BGPmon.   This includes the BGPmon configuration
 * file and the full path to the BGPmon executable program.  Names 
 * with more characters than this are truncated.
 */
#define FILENAME_MAX_CHARS 2000

/* Set the maximum number of characters that can appear 
 * in any directory path.   
 */
#define PATH_MAX_CHARS 2000
/* QUEUE RELATED DEFAULTS  */

/* QUEUE_MAX_ITEMS determines how many messages can be stored in
 * a queue.   The space is pre-allocated so setting a large number
 * can consume a large amount of memory.   Setting a small number
 * reduces memory, but large update bursts from peers and/or slow
 * clients reading data could trigger pacing and the loss of 
 * peers and/or clients if the queues become full
 * This value is specified as the maximum number of items in the queue.
 */
#define QUEUE_MAX_ITEMS 5000

/* QUEUE_PACING_ON_THRESHOLD is percentage (0 to 1) that determines
 * when pacing is turned on for a queue.   When the queue utilization 
 * exceeds this percentage, pacing rules are applied to slow queue
 * writers.
 * This value is specified as a percentage between 0 and 1.
 */
#define QUEUE_PACING_ON_THRESHOLD 0.50

/* QUEUE_PACING_OFF_THRESHOLD is percentage (0 to 1) that determines
 * when pacing is turned off for a queue.   When the queue utilization 
 * drops below this percentage, pacing rules are no applied to writers
 * associated with the queue.
 * This value is specified as a percentage between 0 and 1.
 */
#define QUEUE_PACING_OFF_THRESHOLD 0.25

/* QUEUE_ALPHA is a percentage of the pervious interval
 * limit that will be carried over to the new interval limit. 
 * The writes limit is recalculated each interval.  QUEUE_ALPHA
 * controls how much weight is given to limit values. 
 * For example, if the QUEUE_ALPHA is 100%, then 100% of the past
 * interval limit is used in the new interval.   In other words,
 * a QUEUE_ALPHA value of 100% means the limit never changes.
 * If the QUEUE_ALPHA is 0%, then 0% of the past interval limit is
 * used in the new interval.   In other words, a QUEUE_ALPHA value of
 * 0% means the new limit is calculated from scratch each interval
 * and any past history of the limit is ignored.
 * This value is specified as a percentage between 0 and 1.
 */
#define QUEUE_ALPHA 0.25

/* QUEUE_MIN_WRITES_LIMIT limits how much pacing can be applied
 * to a single queue writer.   If pacing is in effect for a queue,
 * writers are limited in how many items can be written to the queue
 * during the pacing interval.     The number of items are a writer
 * can write during this interval vary depending on the pacing 
 * algorithm.   The writer will always be allowed to write at least this
 * many items during an interval. 
 * This value is the minimum number of items a writer can write to the 
*  queue.
 */
#define QUEUE_MIN_WRITES_LIMIT 1

/* QUEUE_PACING_INTERVAL is the interval (in seconds) over which 
 * pacing is applied.   If pacing has been triggered, writers
 * are only allowed to write a limited number of items to the queue
 * during an interval.
 * This value is the number of seconds in a pacing interval 
 */
#define QUEUE_PACING_INTERVAL 1

/* QUEUE_LOG_INTERVAL determines how often (in seconds) queue status 
 * messages are  are written to the log.   Queue status messages are 
 * logged at log level 6 (INFORMATIONAL) and will be displayed to the 
 * log only if the log level has been set to 6 or higher.  
 * This value is specified as number of seconds.
 */
#define QUEUE_LOG_INTERVAL 1800

#define PEER_QUEUE_NAME "PeerQueue"
#define MRT_QUEUE_NAME "MrtQueue"
#define LABEL_QUEUE_NAME "LabelQueue"
#define XML_U_QUEUE_NAME "XMLUQueue"
#define XML_R_QUEUE_NAME "XMLRQueue"

/* PEERING RELATED DEFAULTS  */
/* MAX_PEER_IDS controls how many peers can be supported over the lifetime
 * of BGPmon execution instance.    As a peer is added, it is assigned
 * an ID.  If fundamental characteristics, such as the address changesi,
 * a new ID is assigned to the peer.   IDs are NOT RELEASED when a peer
 * is deleted.   The total number of IDs used during a BGPmon execution
 * cannot exceed MAX_PEER_IDS.   This value can be set large, but a small 
 * amount of memory, typically * the size of a few pointers, is associated 
 * with each ID even when the ID has yet to be assigned.  
 */
#define MAX_PEER_IDS 1000
#define MAX_PEER_GROUP_IDS 1000
#define MAX_SESSION_IDS 20000
/* MAX_CHAIN_IDS controls how many other BGPmon instances can provide 
 * data to this BGPmon via a chain.   As a chain is added, it is assigned
 * an ID.  If fundamental characteristics, such as the address changesi,
 * a new ID is assigned to the chain.   IDs are NOT RELEASED when a peer
 * is deleted.   The total number of IDs used during a BGPmon execution
 * cannot exceed MAX_CHAIN_IDS.   This value can be set large, but a small 
 * amount of memory, typically * the size of a few pointers, is associated 
 * with each ID even when the ID has yet to be assigned.  
 */
#define MAX_CHAIN_IDS 1000

/* MAX_DOWNSTREAM_IDS controls how many BGPmon IDs can be chained through
 * a single BGPmon instance.  This number is used in the Chain module to
 * keep track of the BGPmon instances that a single chain is responsible for.
 */
#define MAX_DOWNSTREAM_IDS 3000

/* Chains default retry interval in seconds */
#define CHAINS_RETRY_INTERVAL 60

/* MAX_CLIENTS_IDS controls how many clients can simultaneoously 
 * receive data via TCP connections to this BGPmon instance.  As a client
 * is added, the total number of active clients is increased.   As a
 * client connection is closed, the total number of active clients is
 * decreased.   The total number of active clients at any point in time
 * during a BGPmon execution cannot exceed MAX_CLIENTS_IDS
 */
#define MAX_CLIENT_IDS 10000

/* MAX_MRTS_IDS controls how many mrts can simultaneoously 
 * connect to this BGPmon instance.  As a mrt is connected, 
 * the total number of active mrts  is increased.   As a
 * mrt connection is closed, the total number of active mrts is
 * decreased.   The total number of active mrts at any point in time
 * during a BGPmon execution cannot exceed MAX_MRTS_IDS
 */
#define MAX_MRTS_IDS 500
/* GMT_TIME_STAMP decides if GMT timestamp will be generated under the "time" tag or not */
#define GMT_TIME_STAMP TRUE

/* Labeling RELATED DEFAULTS  */

/* LABEL_UPDATES_ENABLED decides if updates need to be labeled*/
#define LABEL_UPDATES_ENABLED TRUE

/* STORE_RIB_ENABLED decides if store the rib table*/
#define STORE_RIB_ENABLED TRUE
// CACHE_EXPIRATION_INTERVAL defines how often the entries in the chain/ownership database get checked
#define CACHE_EXPIRATION_INTERVAL 1200
// CACHE_ENTRY_LIFETIME defines how long a chain/ownership entry lasts before getting cleared
#define CACHE_ENTRY_LIFETIME 7200

/* BGPmon SYSTEM RELATED DEFAULTS  */

// the amount of time a thread should sleep while waiting
#define THREAD_CHECK_INTERVAL 60

// the amount of time a thread can idle before being declared dead
#define THREAD_DEAD_INTERVAL 2 * THREAD_CHECK_INTERVAL

#endif /*BGPMON_DEFAULTS_H_*/
