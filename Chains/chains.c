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
 *  	File: chains.c
 *  	Authors:  He Yan, Dan Massey
 *  	Date: Oct 13, 2008 
 *	Implementation of chaining BGPMons together in order to achieve scalability. 
 */

/* externally visible structures and functions for clients */
#include "chains.h"
/* internal structures and functions for launching chains */
#include "chaininstance.h"

/* required for logging functions */
#include "../Util/log.h"
/* needed for reading and saving configuration */
#include "../Config/configdefaults.h"
#include "../Config/configfile.h"

/* required for TRUE/FALSE defines  */
#include "../Util/bgpmon_defaults.h"

/* needed for address management  */
#include "../Util/address.h"

/* needed for malloc and free */
#include <stdlib.h>
/* needed for strncpy */
#include <string.h>
/* needed for system error codes */
#include <errno.h>
/* needed for system types such as time_t */
#include <sys/types.h>
/* needed for time function */
#include <time.h>
/* needed for pthread related functions */
#include <pthread.h>

//#define DEBUG

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default chains configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int initChainsSettings()
{
	// initialize the list of active chains
	int i;
	for( i = 0; i < MAX_CHAIN_IDS; i++)
		Chains[i] = NULL;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Read the chains settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readChainsSettings()
{	
	int 	err = 0;   	// indicates whether read was successful
	int 	result;		// indicates whether XML read was successful
	char	*addr;		// used to store addresses from XML config
	int	    Uport;		// used to store the update port from XML config
	int     Rport;      // used to store the RIB port from XML config
	int	    enabled;	// used to store enabled status from XML config
	int	    retry;		// used to store retry interval from XML config
	int 	valid;		// is the current chain configuration valid

	// get the count of chains
	int chainsCount = getConfigListCount(XML_CHAINS_PATH);
#ifdef DEBUG
	debug(__FUNCTION__, "found %d configured chains", chainsCount);
#endif

	// check if the count of chains exceeds MAX_CHAIN_IDS. If so, trim it.
	if(chainsCount > MAX_CHAIN_IDS)
	{
		log_warning("Configured Chains (%d) exceeds maximum allowable number of chains (%d).", chainsCount, MAX_CHAIN_IDS);
		log_warning("using only first %d configured chains", MAX_CHAIN_IDS);
        chainsCount = MAX_CHAIN_IDS;
	}

	// read each chain's configuration and create the chain structure
	int i;
	for(i = 0; i < chainsCount; i++)
	{
		// initially, chain i configuration is valid
		valid = TRUE;
		
		// get chaining addr
		addr = NULL;
		result = getConfigValueFromListAsAddr(&addr, XML_CHAINS_PATH, XML_CHAIN_ADDR, i, ADDR_ACTIVE);
		if ( ( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) ) 
		{
		  	err = 1;
			log_warning("Invalid configuration of chain %d address.", i);
			valid = FALSE;
		}
#ifdef DEBUG
		debug(__FUNCTION__, "Chain %d Addr: %s", i, addr );
#endif	

		// get chaining port
		result = getConfigValueFromListAsInt(&Uport, XML_CHAINS_PATH, XML_U_CHAIN_PORT, i, 1, 65536);
		if (( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) )  
		{
		  	err = 1;
			log_warning("Invalid configuration of chain %d port.", i);
			valid = FALSE;
        	}
#ifdef DEBUG
		debug(__FUNCTION__, "Update Chain %d port set to %d", i, Uport);
#endif

		// get chaining port
		result = getConfigValueFromListAsInt(&Rport, XML_CHAINS_PATH, XML_R_CHAIN_PORT, i, 1, 65536);
		if (( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) )  
		{
		  	err = 1;
			log_warning("Invalid configuration of chain %d port.", i);
			valid = FALSE;
        	}
#ifdef DEBUG
		debug(__FUNCTION__, "RIB Chain %d port set to %d", i, Rport);
#endif		

		// get enabled flag
		result = getConfigValueFromListAsInt(&enabled, XML_CHAINS_PATH, XML_CHAIN_ENABLED, i, 0, 1);
		if ( result == CONFIG_INVALID_ENTRY )  
		{
		  	err = 1;
			log_warning("Invalid configuration of chain %d enabled state.", i);
			valid = FALSE;
        }
		else if( result == CONFIG_NO_ENTRY )
		{
			enabled = CHAINS_ENABLED;
		}
#ifdef DEBUG
		if (enabled == TRUE) 
			debug(__FUNCTION__, "Chain %d is enabled", i);
		else
			debug(__FUNCTION__, "Chain %d is disabled", i);
#endif

		
		// get retry interval CHAINS_RETRY_INTERVAL
		result = getConfigValueFromListAsInt(&retry, XML_CHAINS_PATH, XML_CHAIN_RETRY_INTERVAL, i, 0, 65536);
		if( result == CONFIG_INVALID_ENTRY )  
		{
		  	err = 1;
			log_warning("Invalid configuration of chain %d retry interval.", i);
			valid = FALSE;
        }
		else if( result == CONFIG_NO_ENTRY )
			retry = CHAINS_RETRY_INTERVAL;
#ifdef DEBUG
		debug(__FUNCTION__, "Chain connection %d retry interval set to %d", i, retry);
#endif

		// if we received all the parameters for the chain, create it
		if (valid == TRUE)
		{
			//create the chain structure
			int newChainID = createChainStruct( addr, Uport, Rport, enabled, retry );
			if (newChainID == -1) 
			{ 
				err = 1;
				log_warning("Unable to create structure for chain %d", i);
			}
			else
				log_msg("created new chain with id (%d)", newChainID);
		}
		// free adress if defined and get the next chain
		if (addr != NULL) 
			free(addr);
	}

	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Save the chains  settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int saveChainsSettings()
{	
	int err = 0;
	int i;

	// open the chains module
	if (openConfigElement(XML_CHAINS_LIST_TAG)) {
		err = 1;
		log_warning("Failed to save chains list tag to config file.");
	}

	for(i = 0; i < MAX_CHAIN_IDS; i++)
	{
		if( Chains[i] != NULL )
		{
			// open a chain 
			if (openConfigElement(XML_CHAIN_TAG)) {
				err = 1;
				log_warning("Failed to save chain tag to config file.");
			}

			// save addr
			if ( setConfigValueAsString(XML_CHAIN_ADDR, Chains[i]->addr) )
			{
				err = 1;
				log_warning("Failed to save chain(%d) address to config file.");
			}

			// save update and rib ports
			if ( setConfigValueAsInt(XML_U_CHAIN_PORT, Chains[i]->Uport) )
			{
				err = 1;
				log_warning("Failed to save Update chain(%d) port to config file.");
			}				
			if ( setConfigValueAsInt(XML_R_CHAIN_PORT, Chains[i]->Rport) )
			{
				err = 1;
				log_warning("Failed to save RIB chain(%d) port to config file.");
			}					

			// save  enabled flag
			if ( setConfigValueAsInt(XML_CHAIN_ENABLED, Chains[i]->enabled) )
			{
				err = 1;
				log_warning("Failed to save chain(%d) enaled flag to config file.");
			}

			// save retry interval
			if( setConfigValueAsInt(XML_CHAIN_RETRY_INTERVAL, Chains[i]->connectRetryInterval) )
			{
				err = 1;
				log_warning("Failed to save chain(%d) connection retry interval to config file.");
			}
			
			// close the chain
			if (closeConfigElement(XML_CHAIN_TAG)) {
				err = 1;
				log_warning("Failed to save chain tag to config file.");
			}
		}
	}
	
	// close the chains module
	if (closeConfigElement(XML_CHAINS_LIST_TAG)) {
		err = 1;
		log_warning("Failed to save chains list tag to config file.");
	}
	
	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: launch all the configured chains, called by main.c.
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void launchAllChainsThreads()
{
	int i;
	
	for(i = 0; i < MAX_CHAIN_IDS; i++)
	{
		if(Chains[i] != NULL) 
		{
			if (Chains[i]->enabled == TRUE) 
				enableChain(Chains[i]->chainID);
		}
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: dynamically create a chain and launch its thread, called by CLI.
 * Input:  address, update port, rib port, enabled flag and retry interval.
 * Output: id of the new chain or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createChain(char *addr, int Uport, int Rport, int enabled, int retryInterval) 
{
	int chainID = createChainStruct(addr, Uport, Rport, enabled, retryInterval);
	if (chainID == -1) 
	{
		log_err("Creation of chain structure failed for %s port %d, %d", addr, Uport, Rport);
		return -1;
	}
	if (enabled == TRUE && chainID >= 0) 
		enableChain(chainID);
	return chainID;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of IDs of all active chains
 * Input: a pointer to an array
 * Output: the length of the array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getActiveChainsIDs(int **chainIDs) 
{
	int i;
	// get a count of the current chains
	int chaincount = 0;
	for(i = 0; i < MAX_CHAIN_IDS; i++)
	{
		if( Chains[i] != NULL )
			chaincount++;
	}
	
	// allocate memory for the resulting ids
	int *IDs = malloc(sizeof(int)*chaincount);
	if (IDs == NULL) 
	{
		log_warning("getActiveChainsIDS: Unable to allocate memory");
		return 0; 
	}

	// add the ids to the chain
	// note is possible some chains were added since we counted above
	int actualchains = 0;
	for(i = 0; i < MAX_CHAIN_IDS; i++)
	{
		if( (Chains[i] != NULL ) && (actualchains < chaincount ) )
		{
			IDs[actualchains] = i; 
			actualchains++;
		}
	}
	*chainIDs = IDs; 
	return actualchains;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for chain connection
 * Input: ID of the Chain
 * Output: a timevalue indicating the last time the thread was active
 * Mikhail Strizhov @ Jun 24, 2009
 * -------------------------------------------------------------------------------------*/
time_t
getChainLastAction(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainLastAction: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainLastAction: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return Chains[chainID]->lastAction;
	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get ID of a chain given addr and port
 * Input: addr in string and port number
 * Output: ID of the chain
 * Note: If the chain with specified addr and port is not exsiting, return -1. 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getChainID(char *addr, int Uport, int Rport)
{
	int i;
	for(i = 0; i < MAX_CHAIN_IDS; i++)
	{
		if( Chains[i] != NULL && !strcmp(Chains[i]->addr, addr) && Chains[i]->Uport == Uport && Chains[i]->Rport == Rport)
			return Chains[i]->chainID;
	}
	return -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the chain's address
 * Input:	the chain's ID
 * Output: the address used to connect to the remote chain
 *          or NULL if no address for this chain
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * 
getChainAddress( int chainID )
{	
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainAddress: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return NULL;
	}
	if( Chains[chainID] == NULL )
	{	
		log_err("getChainAddress: couldn't find a chain with ID:%d", chainID);
		return NULL;
	}
	char *tmp = NULL;
	tmp = malloc(strlen(Chains[chainID]->addr)+1);	
	if( tmp == NULL )
	{	
		log_err("getChainAddress: couldn't allocate memory");
		return NULL;
	}
	strncpy(tmp , Chains[chainID]->addr, strlen(Chains[chainID]->addr) + 1);
	return tmp;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the chain's address
 * Input:	the chain's ID and the addr in string
 * Output:  ADDR_VALID means address is valid and set successfully.
 *	   ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *	   ADDR_ERR_INVALID_ACTIVE means this is not a valid active address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setChainAddress( int chainID, char *addr )
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("setChainAddress: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return ADDR_ERR_INVALID_FORMAT;
	}

	// check if the chain is defined
	if( Chains[chainID] == NULL )
	{
		log_err("setChainAddress: couldn't find a chain with ID:%d", chainID);
		return ADDR_ERR_INVALID_FORMAT;
	}

	// check if the supplied address is valid
	int result = checkAddress(addr, ADDR_ACTIVE);
	if(result != ADDR_VALID)
		return result;

	// check if the supplied address is different from current
	if( strcmp(Chains[chainID]->addr, addr) == 0 )
		return ADDR_VALID;

	// set the new address
	Chains[chainID]->reconnectFlag = TRUE;
	strncpy(Chains[chainID]->addr, addr, ADDR_MAX_CHARS);

	return ADDR_VALID;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the Update port of the chain
 * Input:	the chain's ID
 * Output: the chain's port
 *         or -1 if there is no chain with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getUChainPort(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainPort: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainPort: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return Chains[chainID]->Uport;
}
/*--------------------------------------------------------------------------------------
 * Purpose: Get the RIB port of the chain
 * Input:	the chain's ID
 * Output: the chain's port
 *         or -1 if there is no chain with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getRChainPort(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainPort: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainPort: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return Chains[chainID]->Rport;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the chain's port
 * Input:	the chain's ID and the port
 * Output: 
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void setChainPort( int chainID, int Uport, int Rport )
{	
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("setChainPort: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("setChainPort: couldn't find a chain with ID:%d", chainID);
		return;
	}
	//  check if the port is valid
	if ( (Uport < 0) || (Uport > 65536) ) 
	{
		log_err("setChainPort: invalid  port number of %d", Uport);
		return;
	}
	if ( (Rport < 0) || (Rport > 65536) ) 
	{
		log_err("setChainPort: invalid  port number of %d", Rport);
		return;
	}	
	// check if the port has changed configured
	if( Chains[chainID]->Uport == Uport )
		return;
	if( Chains[chainID]->Rport == Rport )
		return;
	// change the port
	Chains[chainID]->Uport = Uport;
	Chains[chainID]->Rport = Rport;	
	Chains[chainID]->reconnectFlag = TRUE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: return TRUE if the chain is enabled, otherwise return FALSE
 * Input:	the chain's ID
 * Output: TRUE or FALSE
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int isChainEnabled(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("isChainEnabled: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return FALSE;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("isChainEnabled: couldn't find a chain with ID:%d", chainID);
		return FALSE;
	}
	return Chains[chainID]->enabled;
}

/*--------------------------------------------------------------------------------------
 * Purpose: enable a Chain
 * Input:	the chain's ID
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void enableChain(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("enableChain: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("enable chain: couldn't find a chain with ID:%d", chainID);
		return;
	}
	// enable and launch the chain thread
	Chains[chainID]->enabled = TRUE;
	if( Chains[chainID]->runningFlag == FALSE )
	{
		int error;
#ifdef DEBUG
		debug(__FUNCTION__, "Creating chain thread %d...", chainID);
#endif
		pthread_t chainsThreadID;
		if ((error = pthread_create(&chainsThreadID, NULL, chainThread, Chains[chainID])) > 0 )
			log_err("Failed to create chain thread: %s\n", strerror(error));

		Chains[chainID]->chainThreadID = chainsThreadID;
#ifdef DEBUG
		debug(__FUNCTION__, "Created chain thread %d!", chainID);
#endif
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: disable a Chain
 * Input:	the chain's ID
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void disableChain(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("disableChain: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("disable chain: couldn't find a chain with ID:%d", chainID);
		return;
	}
	Chains[chainID]->enabled = FALSE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the chain's retry interval
 * Input:	the chain's ID
 * Output: the retry interval
 *         or -1 if the chain is not configured
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getChainConnectionRetryInterval(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainConnectionRetryInterval: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainConnectionRetryInterval: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return Chains[chainID]->connectRetryInterval;
}

/*--------------------------------------------------------------------------------------
 * Purpose: set the chain's retry interval
 * Input:	the chain's ID and the retry interval
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setChainConnectionRetryInterval(int chainID,int interval)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("setChainConnectionRetryInterval: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("setChainConnectionRetryInterval: couldn't find a chain with ID:%d", chainID);
		return;
	}
	if( interval < 0 )
	{
		log_err("setChainConnectionRetryInterval: invalid retry interval of %d", interval);
		return;
	}
	Chains[chainID]->connectRetryInterval = interval;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the Update chain's retry count
 * Input:	the chain's ID
 * Output: the retry count
 *         or -1 if the chain is not configured
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getUChainConnectionRetryCount(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainConnectionRetryCount: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainConnectionRetryCount: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return Chains[chainID]->UconnectRetryCounter;
}
/*--------------------------------------------------------------------------------------
 * Purpose: get the RIB chain's retry count
 * Input:	the chain's ID
 * Output: the retry count
 *         or -1 if the chain is not configured
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getRChainConnectionRetryCount(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainConnectionRetryCount: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainConnectionRetryCount: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return Chains[chainID]->RconnectRetryCounter;
}



/*--------------------------------------------------------------------------------------
 * Purpose: get the Update chain's connection state
 * Input:	the chain's ID
 * Output: the connection state: chainStateIdle, chainStateConnecting or chainStateConnected
 *         or chainStateError if the chain is not configured
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
getChainConnectionState( int chainID, int chain_stream )
{
	if (chain_stream == UPDATE_STREAM_CHAIN)
	{
		// check the chain ID is valid
		if (chainID >= MAX_CHAIN_IDS)
		{
			log_err("getChainConnectionState: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
			return chainStateError;
		}
		// check if the chain is configured
		if( Chains[chainID] == NULL ) 
		{
			log_err("getChainConnectionState: couldn't find a chain with ID:%d", chainID);
			return chainStateError;
		}
		return( Chains[chainID]->UconnectionState );
	}	
	if (chain_stream == RIB_STREAM_CHAIN)
	{
		// check the chain ID is valid
		if (chainID >= MAX_CHAIN_IDS)
		{
			log_err("getChainConnectionState: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
			return chainStateError;
		}
		// check if the chain is configured
		if( Chains[chainID] == NULL ) 
		{
			log_err("getChainConnectionState: couldn't find a chain with ID:%d", chainID);
			return chainStateError;
		}
		return( Chains[chainID]->RconnectionState );
	}	
	return chainStateError;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the Update chain's connected time
 * Input:	the chain's ID
 * Output: the connected time
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t 
getUChainConnectedTime(int chainID)
{
	time_t t = 0;
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainConnectedTime: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return t;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainConnectedTime: couldn't find a chain with ID:%d", chainID);
		return t;
	}
	if (Chains[chainID]->enabled == FALSE) 
		return t;

	if (Chains[chainID]->UconnectionState != chainStateConnected) 
		return t;
	
	// calculate the connection up from now - established time
	t = time(NULL);
	return( t - Chains[chainID]->UestablishedTime );
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the RIB chain's connected time
 * Input:	the chain's ID
 * Output: the connected time
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t 
getRChainConnectedTime(int chainID)
{
	time_t t = 0;
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainConnectedTime: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return t;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainConnectedTime: couldn't find a chain with ID:%d", chainID);
		return t;
	}
	if (Chains[chainID]->enabled == FALSE) 
		return t;

	if (Chains[chainID]->RconnectionState != chainStateConnected) 
		return t;
	
	// calculate the connection up from now - established time
	t = time(NULL);
	return( t - Chains[chainID]->RestablishedTime );
}


/*--------------------------------------------------------------------------------------
 * Purpose: get the Update chain's last down time
 * Input:	the chain's ID
 * Output: the last down time
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getULastConnectionDownTime(int chainID)
{
	time_t time = 0;
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainConnectionDownTime: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return time;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainConnectionDownTime: couldn't find a chain with ID:%d", chainID);
		return time;
	}
	return( Chains[chainID]->UlastDownTime );
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the RIB chain's last down time
 * Input:	the chain's ID
 * Output: the last down time
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getRLastConnectionDownTime(int chainID)
{
	time_t time = 0;
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainConnectionDownTime: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return time;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainConnectionDownTime: couldn't find a chain with ID:%d", chainID);
		return time;
	}
	return( Chains[chainID]->RlastDownTime );
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the Update chain's reset count
 * Input:	the chain's ID
 * Output: the reset count
 *         or -1 if chain not configured
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getUChainConnectionResetCount(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainConnectionResetCount: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainConnectionResetCount: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return( Chains[chainID]->UresetCounter );
}
/*--------------------------------------------------------------------------------------
 * Purpose: get the chain's reset count
 * Input:	the chain's ID
 * Output: the reset count
 *         or -1 if chain not configured
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getRChainConnectionResetCount(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainConnectionResetCount: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainConnectionResetCount: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return( Chains[chainID]->RresetCounter );
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of received messages of Update chain
 * Input:	the chain's ID
 * Output: the number of received messages
 *         or -1 if chain not configured
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getUChainReceivedMessages(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainReceivedMessages: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainReceivedMessages: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return( Chains[chainID]->UmessageRcvd );
}
/*--------------------------------------------------------------------------------------
 * Purpose: get the number of received messages of RIB chain
 * Input:	the chain's ID
 * Output: the number of received messages
 *         or -1 if chain not configured
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getRChainReceivedMessages(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("getChainReceivedMessages: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return -1;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("getChainReceivedMessages: couldn't find a chain with ID:%d", chainID);
		return -1;
	}
	return( Chains[chainID]->RmessageRcvd );
}


/*--------------------------------------------------------------------------------------
 * Purpose: delete a Chain, stopping the thread if enabled and destroying memory
 * Input:	the chain's ID
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
deleteChain(int chainID)
{
	// check the chain ID is valid
	if (chainID >= MAX_CHAIN_IDS)
	{
		log_err("deleteChain: chain ID %d exceeds max %d", chainID, MAX_CHAIN_IDS);
		return;
	}
	// check if the chain is configured
	if( Chains[chainID] == NULL ) 
	{
		log_err("deleteChain: couldn't find a chain with ID:%d", chainID);
		return;
	}
	// if the chain is running, let its own thread clear the memory
	if (Chains[chainID]->runningFlag == TRUE) 
	{
		Chains[chainID]->enabled = FALSE;
		Chains[chainID]->deleteChain = TRUE;
	}
	// else if no thread running, destroy the chain memory
	else
		destroyChain(Chains[chainID]);
}

/*--------------------------------------------------------------------------------------
 * Purpose: Intialize the shutdown process for the chaining module
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void
signalChainShutdown()
{
	int i;
	for(i = 0; i < MAX_CHAIN_IDS; i++)
	{
		if(Chains[i] != NULL) 
		{
			disableChain(Chains[i]->chainID);
		}
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: wait on all chain pieces to finish closing before returning
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void
waitForChainShutdown()
{
	int i;
	void * status = NULL;

	// wait for label control thread exit
	for(i = 0; i < MAX_CHAIN_IDS; i++)
	{
		if(Chains[i] != NULL ) 
		{
                  if(Chains[i]->enabled){
			pthread_join(Chains[i]->chainThreadID, status);
                  }
		}
	}
}
