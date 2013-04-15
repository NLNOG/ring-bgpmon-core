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
 *  File: chains.h
 * 	Authors: He Yan, Dan Massey
 *  Date: July 24, 2008 
 */

#ifndef CHAINS_H_
#define CHAINS_H_

/* needed for system types such as time_t */
#include <sys/types.h>

// needed for MAX_CHAIN_IDS
#include "../site_defaults.h"

//needed for mutex
#include <pthread.h>

/* Constants for Update or RIB stream */
#define UPDATE_STREAM_CHAIN 1
#define RIB_STREAM_CHAIN 2

//BGPmon IDs and chain ownership information for duplicate detection
struct chainOwnerCache{
  pthread_mutex_t cache_mutex;//mutex for this entry
  u_int32_t id;               //BGPmon ID of an incoming chain
  int owner;                  //chain thread that will handle all mesgs for id
  u_int32_t seq;              //sequence number of the incoming message stream
  time_t timestamp;           //timestamp of reception of the last message
  struct chainOwnerCache* next;//next entry in the list
};
typedef struct chainOwnerCache* chainOwnerCachep;
chainOwnerCachep cacheHead;

/*-----------------------------------------------------------------------------
 * Purpose: Initialize the default chains configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * ---------------------------------------------------------------------------*/
int initChainsSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Read the chains settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readChainsSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Save the chains  settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int saveChainsSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: launch all the configured chains, called by main.c.
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void launchAllChainsThreads();

/*--------------------------------------------------------------------------------------
 * Purpose: dynamically create a chain and launch its thread, called by CLI.
 * Input:  address, port, enabled flag and retry interval.
 * Output: id of the new chain or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createChain(char *addr, int Uport, int Rport, int enabled,  int retryInterval);

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of IDs of all active chains
 * Input: a pointer to an array
 * Output: the length of the array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getActiveChainsIDs(int **IDs);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for chain connection
 * Input: ID of the Chain
 * Output: a timevalue indicating the last time the thread was active
 * Mikhail Strizhov @ Jun 24, 2009
 * -------------------------------------------------------------------------------------*/
time_t getChainLastAction(int chainIDs);

/*--------------------------------------------------------------------------------------
 * Purpose: Get ID of a chain given addr and port
 * Input: addr in string and port number
 * Output: ID of the chain
 * Note: If the chain with specified addr and port is not exsiting, return -1. 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getChainID(char *addr, int Uport, int Rport);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the chain's address
 * Input:	the chain's ID
 * Output: the address used to connect to the remote chain
 *          or NULL if no address for this chain 
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getChainAddress(int ID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the chain's address
 * Input:	the chain's ID and the addr in string
 * Output:  ADDR_VALID means address is valid and set successfully.
 *	    ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *          ADDR_ERR_INVALID_ACTIVE means this is not a valid active address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setChainAddress( int chainID, char *addr );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the port of the chain
 * Input:   the chain's ID
 * Output: the chain's port
 *         or -1 if there is no chain with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getUChainPort(int ID);
int getRChainPort(int ID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the chain's port
 * Input:	the chain's ID and the port
 * Output: 
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setChainPort( int chainID, int Uport, int Rport );

/*--------------------------------------------------------------------------------------
 * Purpose: return TRUE if the chain is enabled, otherwise return FALSE
 * Input:	the chain's ID
 * Output: TRUE or FALSE
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int isChainEnabled(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: enable a Chain
 * Input:	the chain's ID
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void enableChain(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: disable a Chain
 * Input:	the chain's ID
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void disableChain(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the chain's retry interval
 * Input:	the chain's ID
 * Output: the retry interval
 *         or -1 if the chain is not configured
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getChainConnectionRetryInterval(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: set the chain's retry interval
 * Input:	the chain's ID and the retry interval
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setChainConnectionRetryInterval(int chainID,int interval);

/*--------------------------------------------------------------------------------------
 * Purpose: get the chain's retry count
 * Input:	the chain's ID
 * Output: the retry count
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getUChainConnectionRetryCount(int chainID);
int getRChainConnectionRetryCount(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the chain's connection state
 * Input:	the chain's ID
 * Output: the connection state: chainStateIdle, chainStateConnecting or chainStateConnected
 *         or chainStateError if the chain is not configured.
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getChainConnectionState(int chainID, int chain_stream);
enum ChainConnectionStates 
{ 
	chainStateError=0,
	chainStateIdle,
	chainStateConnecting, 
	chainStateConnected 
};



/*--------------------------------------------------------------------------------------
 * Purpose: get the chain's connected time
 * Input:	the chain's ID
 * Output: the connected time
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getUChainConnectedTime(int chainID);
time_t getRChainConnectedTime(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the chain's last down time
 * Input:	the chain's ID
 * Output: the last down time
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getULastConnectionDownTime(int chainID);
time_t getRLastConnectionDownTime(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the chain's reset count
 * Input:	the chain's ID
 * Output: the reset count
 *         or -1 if the chain is not configured
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getUChainConnectionResetCount(int chainID);
int getRChainConnectionResetCount(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of received messages of this chain
 * Input:	the chain's ID
 * Output: the number of received messages
 *         or -1 if the chain is not configured
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getUChainReceivedMessages(int chainID);
int getRChainReceivedMessages(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: delete a Chain
 * Input:	the chain's ID
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void deleteChain(int chainID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the last action time of the CHAIN thread
 * Input:
 * Output: last action time
 * Mikhail Strizhov @ Jun 25, 2009
 * -------------------------------------------------------------------------------------*/
time_t getChainThreadLastAction();

/*--------------------------------------------------------------------------------------
 * Purpose: Intialize the shutdown process for the chaining module
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void
signalChainShutdown();

/*--------------------------------------------------------------------------------------
 * Purpose: wait on all chain pieces to finish closing before returning
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void
waitForChainShutdown();

#endif /*CHAINS_H_*/
