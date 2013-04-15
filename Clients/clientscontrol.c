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
 *	OTHER DEALINGS IN THE SOFTWARE.\
 * 
 * 
 *  File: clientscontrol.c
 * 	Authors: He Yan, Dan Massey
 *
 *  Date: Oct 7, 2008 
 */
 
/* 
 * Control and manage the BGPmon clients
 */

/* externally visible structures and functions for clients */
#include "clients.h"
/* internal structures and functions for this module */
#include "clientscontrol.h"
/* internal structures and functions for launching clients */
#include "clientinstance.h"

/* required for logging functions */
#include "../Util/log.h"
/* needed for reading and saving configuration */
#include "../Config/configdefaults.h"
#include "../Config/configfile.h"

/* required for TRUE/FALSE defines  */
#include "../Util/bgpmon_defaults.h"

/* needed for address management  */
#include "../Util/address.h"

/* needed for checkACL */
#include "../Util/acl.h"

/* needed for malloc and free */
#include <stdlib.h>
/* needed for strncpy */
#include <string.h>
/* needed for system error codes */
#include <errno.h>
/* needed for addrinfo struct */
#include <netdb.h>
/* needed for system types such as time_t */
#include <sys/types.h>
/* needed for time function */
#include <time.h>
/* needed for socket operations */
#include <sys/socket.h>
/* needed for pthread related functions */
#include <pthread.h>
/* needed for sleep and close */
#include <unistd.h>

/* needed for checkACL */
#include "../Peering/peers.h"

//#define DEBUG

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default clients control configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
initClientsControlSettings()
{
	int err = 0;

	// UPDATE SETTINGS
		// address used to listen for client connections
		int result = checkAddress(CLIENTS_UPDATES_LISTEN_ADDR, ADDR_PASSIVE);
		if(result != ADDR_VALID)
		{
			err = 1;
			strncpy(ClientControls.listenUAddr, IPv4_LOOPBACK, ADDR_MAX_CHARS);
		}
		else
			strncpy(ClientControls.listenUAddr, CLIENTS_UPDATES_LISTEN_ADDR, ADDR_MAX_CHARS);

		// port used to listen for client connections
		if ( (CLIENTS_UPDATES_LISTEN_PORT < 1) || (CLIENTS_UPDATES_LISTEN_PORT > 65536) ) {
			err = 1;
			log_warning("Invalid site default for client listen port.");
			ClientControls.listenUPort = 50001;
		}
		else
			ClientControls.listenUPort = CLIENTS_UPDATES_LISTEN_PORT;

		// Maximum number of clients allowed
		if (MAX_CLIENT_IDS < 0)  {
			err = 1;
			log_warning("Invalid site default for max allowed clients.");
			ClientControls.maxUClients = 1;
		}
		else
			ClientControls.maxUClients = MAX_CLIENT_IDS;

	// RIB SETTINGS
		// address used to listen for RIB client connections
		result = checkAddress(CLIENTS_RIB_LISTEN_ADDR, ADDR_PASSIVE);
		if(result != ADDR_VALID)
		{
			err = 1;
			strncpy(ClientControls.listenRAddr, IPv4_LOOPBACK, ADDR_MAX_CHARS);
		}
		else
			strncpy(ClientControls.listenRAddr, CLIENTS_RIB_LISTEN_ADDR, ADDR_MAX_CHARS);

		// port used to listen for RIB client connections
		if ( (CLIENTS_RIB_LISTEN_PORT < 1) || (CLIENTS_RIB_LISTEN_PORT > 65536) ) {
			err = 1;
			log_warning("Invalid site default for client listen port.");
			ClientControls.listenRPort = 50002;
		}
		else
			ClientControls.listenRPort = CLIENTS_RIB_LISTEN_PORT;

		// Maximum number of RIB clients allowed
		if (MAX_CLIENT_IDS < 0)  {
			err = 1;
			log_warning("Invalid site default for max allowed clients.");
			ClientControls.maxRClients = 1;
		}
		else
			ClientControls.maxRClients = MAX_CLIENT_IDS;

	// client connections enabled
	if ( (CLIENTS_LISTEN_ENABLED != TRUE) && (CLIENTS_LISTEN_ENABLED != FALSE) ) {
                err = 1;
                log_warning("Invalid site default for clients enabled.");
		ClientControls.enabled= FALSE;
        }
        else
		ClientControls.enabled= CLIENTS_LISTEN_ENABLED;

	// initial bookkeeping figures
	ClientControls.activeUClients = 0;
	ClientControls.nextUClientID = 1;
	ClientControls.rebindUFlag = FALSE;
	ClientControls.activeRClients = 0;
	ClientControls.nextRClientID = 1;
	ClientControls.rebindRFlag = FALSE;	
	
	ClientControls.shutdown = FALSE;
	ClientControls.lastAction = time(NULL);
	ClientControls.firstUNode = NULL;
	ClientControls.firstRNode = NULL;

	//randomize RNG and set initial sequence number
	srand(time(NULL));
	ClientControls.seq_num = rand();

        // initialize locks for the client list
        if (pthread_mutex_init( &(ClientControls.clientULock), NULL ) )
                log_fatal( "unable to init mutex lock for clients updates");
        if (pthread_mutex_init( &(ClientControls.clientRLock), NULL ) )
                log_fatal( "unable to init mutex lock for clients rib");

	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Read the clients control settings from the config file.
 * Input: none
 * Output:  returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
readClientsControlSettings()
{	
	int 	err = 0;
	int 	result;
	int 	num;
	char	*addr;

	// UPDATE LISTENER
		// get listen addr
		result = getConfigValueAsAddr(&addr, XML_CLIENTS_CTR_UPDATES_LISTEN_ADDR_PATH, ADDR_PASSIVE);
		if (result == CONFIG_VALID_ENTRY) 
		{
			result = checkAddress(addr, ADDR_PASSIVE);
			if(result != ADDR_VALID)
			{
				err = 1;
				log_warning("Invalid configuration of client update listener address.");
			}
			else 
			{
				strncpy(ClientControls.listenUAddr,addr,ADDR_MAX_CHARS);
				free(addr);
			}
		}
		else if ( result == CONFIG_INVALID_ENTRY ) 
		{
			err = 1;
			log_warning("Invalid configuration of client update listener address.");
		}
		else
			log_msg("No configuration of client update listener address, using default.");
#ifdef DEBUG
		debug(__FUNCTION__, "Client Update Listener Addr:%s", ClientControls.listenUAddr);
#endif

		// get listen port
		result = getConfigValueAsInt(&num, XML_CLIENTS_CTR_UPDATES_LISTEN_PORT_PATH,1,65536);
		if (result == CONFIG_VALID_ENTRY) 
			ClientControls.listenUPort = num;
		else if( result == CONFIG_INVALID_ENTRY ) 
		{
			err = 1;
			log_warning("Invalid configuration of client update listener port.");
		}
		else
			log_msg("No configuration of client update listener port, using default.");
#ifdef DEBUG
		debug(__FUNCTION__, "Clients Update Listener Port:%d", ClientControls.listenUPort);
#endif

		// get the max number of clients
		result = getConfigValueAsInt(&num, XML_CLIENTS_CTR_UPDATES_MAX_CLIENTS_PATH, 0, 65536);
		if (result == CONFIG_VALID_ENTRY) 
			ClientControls.maxUClients = num;	
		else if ( result == CONFIG_INVALID_ENTRY ) 
		{
			err = 1;
			log_warning("Invalid configuration of max update clients.");
		}
		else
			log_msg("No configuration of max update clients, using default.");
#ifdef DEBUG
		debug(__FUNCTION__, "Maximum update clients allowed is %d", ClientControls.maxUClients);
#endif

	// RIB LISTENER
		// get listen addr
		result = getConfigValueAsAddr(&addr, XML_CLIENTS_CTR_RIB_LISTEN_ADDR_PATH, ADDR_PASSIVE);
		if (result == CONFIG_VALID_ENTRY) 
		{
			result = checkAddress(addr, ADDR_PASSIVE);
			if(result != ADDR_VALID)
			{
				err = 1;
				log_warning("Invalid configuration of client rib listener address.");
			}
			else 
			{
				strncpy(ClientControls.listenRAddr,addr,ADDR_MAX_CHARS);
				free(addr);
			}
		}
		else if ( result == CONFIG_INVALID_ENTRY ) 
		{
			err = 1;
			log_warning("Invalid configuration of client rib listener address.");
		}
		else
			log_msg("No configuration of client rib listener address, using default.");
#ifdef DEBUG
		debug(__FUNCTION__, "Client RIB Listener Addr: %s", ClientControls.listenRAddr);
#endif

		// get listen port
		result = getConfigValueAsInt(&num, XML_CLIENTS_CTR_RIB_LISTEN_PORT_PATH,1,65536);
		if (result == CONFIG_VALID_ENTRY) 
			ClientControls.listenRPort = num;
		else if( result == CONFIG_INVALID_ENTRY ) 
		{
			err = 1;
			log_warning("Invalid configuration of client rib listener port.");
		}
		else
			log_msg("No configuration of client rib listener port, using default.");
#ifdef DEBUG
		debug(__FUNCTION__, "Clients RIB Listener Port: %d", ClientControls.listenRPort);
#endif

		// get the max number of clients
		result = getConfigValueAsInt(&num, XML_CLIENTS_CTR_RIB_MAX_CLIENTS_PATH, 0, 65536);
		if (result == CONFIG_VALID_ENTRY) 
			ClientControls.maxRClients = num;
		else if ( result == CONFIG_INVALID_ENTRY ) 
		{
			err = 1;
			log_warning("Invalid configuration of max rib clients.");
		}
		else
			log_msg("No configuration of max rib clients, using default.");
#ifdef DEBUG
		debug(__FUNCTION__, "Maximum RIB clients allowed is %d", ClientControls.maxRClients);
#endif

	// get enabled status of clients control module
	result = getConfigValueAsInt(&num, XML_CLIENTS_CTR_ENABLED_PATH, 0, 1);
	if (result == CONFIG_VALID_ENTRY) 
		ClientControls.enabled = num;
	else if ( result == CONFIG_INVALID_ENTRY ) 
	{
	  	err = 1;
		log_warning("Invalid configuration of client listen enabled.");
        }
        else
                log_msg("No configuration of client listen enabled, using default.");

	//get BGPmon ID (if present) or generate a new one
	result = getConfigValueAsInt(&num, XML_CLIENTS_CTR_BGPMON_ID_PATH, 0, INT_MAX);
#ifdef DEBUG
debug(__FUNCTION__,"Result of fetching BGPmon ID: %i. BGPmonID: %u",result,(u_int32_t)num);
#endif
	if(result == CONFIG_VALID_ENTRY){
		ClientControls.bgpmon_id = (u_int32_t)num;
	}
	else{
		if(result == CONFIG_NO_ENTRY || num == CONFIG_NO_ENTRY){
			ClientControls.bgpmon_id = rand();
			log_msg("No configuration for BGPmon ID found. New BGPmon ID Generated.");
		}
		else{
			err = 1;
			log_warning("Invalid configuration for BGPmon ID.");
		}
	}

#ifdef DEBUG
	if (num == TRUE) 
		debug(__FUNCTION__, "Clients listen enabled");
	else
		debug(__FUNCTION__, "Clients listen disabled");
#endif

	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Save the  clients control  settings to the config file.
 * Input:  none
 * Output:  retuns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
saveClientsControlSettings()
{
	int err = 0;
	
	// save client tag
	if ( openConfigElement(XML_CLIENTS_CTR_TAG) )
	{
		err = 1;
		log_warning("Failed to save client tag to config file.");
	}

	// UPDATE LISTENER
		// save update listener addr
		if ( setConfigValueAsString(XML_CLIENTS_CTR_UPDATES_LISTEN_ADDR, ClientControls.listenUAddr) ) 
		{
			err = 1;
			log_warning("Failed to save client updates listener address to config file.");
		}

		// save update listener port
		if ( setConfigValueAsInt(XML_CLIENTS_CTR_UPDATES_LISTEN_PORT, ClientControls.listenUPort) ) 
		{
			err = 1;
			log_warning("Failed to save client updates listener port to config file.");
		}

		// save the max number of update clients
		if (setConfigValueAsInt(XML_CLIENTS_CTR_UPDATES_MAX_CLIENTS, ClientControls.maxUClients) ) 
		{
			err = 1;
			log_warning("Failed to save max update clients to config file.");
		}

	// RIB LISTENER
		// save rib listener addr
		if ( setConfigValueAsString(XML_CLIENTS_CTR_RIB_LISTEN_ADDR, ClientControls.listenRAddr) ) 
		{
			err = 1;
			log_warning("Failed to save client rib listener address to config file.");
		}

		// save rib listener port
		if ( setConfigValueAsInt(XML_CLIENTS_CTR_RIB_LISTEN_PORT, ClientControls.listenRPort) ) 
		{
			err = 1;
			log_warning("Failed to save client rib listener port to config file.");
		}

		// save the max number of rib clients
		if (setConfigValueAsInt(XML_CLIENTS_CTR_RIB_MAX_CLIENTS, ClientControls.maxRClients) ) 
		{
			err = 1;
			log_warning("Failed to save max rib clients to config file.");
		}
	
	// save the status of clients control module
	if (setConfigValueAsInt(XML_CLIENTS_CTR_ENABLED, ClientControls.enabled) ) 
	{
		err = 1;
		log_warning("Failed to save client listen enabled status to config file.");
	}
	//save BGPmon ID
	if (setConfigValueAsInt(XML_CLIENTS_CTR_BGPMON_ID, ClientControls.bgpmon_id) ) 
	{
		err = 1;
		log_warning("Failed to save BGPmon ID to config file.");
	}

	// save client tag
	if ( closeConfigElement() )
	{
		err = 1;
		log_warning("Failed to save client tag to config file.");
	}

	return err;
}




/*--------------------------------------------------------------------------------------
 * Purpose: launch clients control thread, called by main.c
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
launchClientsControlThread()
{
	int error;
	
	pthread_t clientsThreadID;

	// create clients listener thread reference
	if ((error = pthread_create(&clientsThreadID, NULL, clientsControlThread, NULL)) > 0 )
		log_fatal("Failed to create Clients thread: %s\n", strerror(error));

	// keep clients listener thread reference
	ClientControls.clientsListenerThread = clientsThreadID;

#ifdef DEBUG
	debug(__FUNCTION__, "Created Clients thread!");
#endif
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the state of clients control
 * Input:
 * Output: returns TRUE or FALSE
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
isClientsControlEnabled()
{
	return ClientControls.enabled;
}

/*--------------------------------------------------------------------------------------
 * Purpose: enable the clients control
 * Input:	
 * Output: 
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
enableClientsControl()
{
	ClientControls.enabled = TRUE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: disable the clients control
 * Input:	
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
disableClientsControl()
{
	ClientControls.enabled = FALSE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the listening port of clients control
 * Input: 	UPDATA or RIB client trigger
 * Output: the listening port of clients control module 
 * He Yan @ Sep 22, 2008
 * Mikhail Strizhov @ June 2, 2009
 * -------------------------------------------------------------------------------------*/ 
int 
getClientsControlListenPort(int client_listener)
{
	int err = 0;
	if (client_listener == CLIENT_LISTENER_UPDATA)
	{
		return ClientControls.listenUPort;
	}	
	if (client_listener == CLIENT_LISTENER_RIB)
	{
		return ClientControls.listenRPort;
	}	
	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the listening port of clients control
 * Input:	the port and UPDATA or RIB client trigger
 * Output: 
 * He Yan @ Sep 22, 2008
 * Mikhail Strizhov @ June 2, 2009
 * -------------------------------------------------------------------------------------*/ 
void 
setClientsControlListenPort( int port, int client_listener)
{
	if (client_listener == CLIENT_LISTENER_UPDATA)
	{
		if( ClientControls.listenUPort != port && port > 0)
		{
			ClientControls.rebindUFlag = TRUE;
			ClientControls.listenUPort = port;
		}
	}
	if (client_listener == CLIENT_LISTENER_RIB)
	{
		if( ClientControls.listenRPort != port && port > 0)
		{
			ClientControls.rebindRFlag = TRUE;
			ClientControls.listenRPort = port;
		}
	}	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the listening address of clients control
 * Input:	UPDATA or RIB client trigger
 * Output: the listening address for client connections
 *          or NULL if no address for client connections
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 * He Yan @ Sep 22, 2008
 * Mikhail Strizhov @ January 29, 2010
 * -------------------------------------------------------------------------------------*/ 
char * 
getClientsControlListenAddr(int client_listener)
{

	if (client_listener == CLIENT_LISTENER_UPDATA)
	{
		// allocate memory for the result
		char *Uans = malloc(sizeof(ClientControls.listenUAddr));
		if (Uans == NULL)
		{
			log_err("getClientsControlListenAddr: couldn't allocate string memory");
			return NULL;
        }
        // copy the string and return result
	memcpy(Uans, ClientControls.listenUAddr, sizeof( ClientControls.listenUAddr));
	return Uans;
	}
	if (client_listener == CLIENT_LISTENER_RIB)
	{
		// allocate memory for the result
		char *Rans = malloc(sizeof(ClientControls.listenRAddr));
		if (Rans == NULL)
		{
			log_err("getClientsControlListenAddr: couldn't allocate string memory");
			return NULL;
        }
        // copy the string and return result
        //strncpy(Rans, ClientControls.listenRAddr, sizeof(ClientControls.listenRAddr));
	memcpy(Rans, ClientControls.listenRAddr, sizeof(ClientControls.listenRAddr));
        return Rans;
	}	
	return NULL;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the listening address of clients control
 * Input:	the addr in string format   and UPDATA or RIB client trigger
 * Output: ADDR_VALID means address is valid and set successfully.
 *	   ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *	   ADDR_ERR_INVALID_PASSIVE means this is not a valid passive address
 * He Yan @ Sep 22, 2008
 * Mikhail Strizhov @ June 2, 2009
 * -------------------------------------------------------------------------------------*/ 
int 
setClientsControlListenAddr( char *addr, int client_listener )
{
	int err = 0;	
	if (client_listener == CLIENT_LISTENER_UPDATA)
	{
		
		int result = checkAddress(addr, ADDR_PASSIVE);
		if(result == ADDR_VALID)
		{
			if( strcmp(ClientControls.listenUAddr, addr) != 0 )
			{
				ClientControls.rebindUFlag = TRUE;
				strncpy(ClientControls.listenUAddr, addr, ADDR_MAX_CHARS);
			}
		}
		return result;
	}	
	
	if (client_listener == CLIENT_LISTENER_RIB)
	{
		
		int result = checkAddress(addr, ADDR_PASSIVE);
		if(result == ADDR_VALID)
		{
			if( strcmp(ClientControls.listenRAddr, addr) != 0 )
			{
				ClientControls.rebindRFlag = TRUE;
				strncpy(ClientControls.listenRAddr, addr, ADDR_MAX_CHARS);
			}
		}
		return result;
	}	
	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the maximum allowed clients for clients control
 * Input:   which listener
 * Output: returns less than 0 on error
 * Cathie Olschanowsky @ April 2012
 * -------------------------------------------------------------------------------------*/
int
getClientsControlMaxConnections(int client_listener)
{
  if (client_listener == CLIENT_LISTENER_UPDATA){
    return ClientControls.maxUClients;
  }else if(client_listener == CLIENT_LISTENER_RIB){
    return ClientControls.maxRClients;
  }else{
    return -1;
  }
}


/*--------------------------------------------------------------------------------------
 * Purpose: Set the maximum allowed clients for clients control
 * Input:   the new max, which listener
 * Output: returns non-zero on error
 * Cathie Olschanowsky @ April 2012
 * -------------------------------------------------------------------------------------*/
int 
setClientsControlMaxConnections(int newMax, int client_listener)
{
  if (client_listener == CLIENT_LISTENER_UPDATA){
    ClientControls.maxUClients = newMax;
  }else if(client_listener == CLIENT_LISTENER_RIB){
    ClientControls.maxRClients = newMax;
  }else{
    return 1;
  }
  return 0;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for this thread
 * Input:	
 * Output: a timevalue indicating the last time the thread was active
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
time_t 
getClientsControlLastAction()
{
	return ClientControls.lastAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: the main function of clients control module
 *  listens for client connections and starts new thread for each client
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void *
clientsControlThread( void *arg )
{

	fd_set read_fds; 	// file descriptor list for select()
	int fdmax = 0;		// maximum file descriptor number
	int listenUSocket = -1;	// socket to listen for UPDATA connections
	int listenRSocket = -1;  // socket to listen for RIB connections

	// timer to periodically check thread status
	struct timeval timeout; 
	timeout.tv_usec = 0;
	timeout.tv_sec = THREAD_CHECK_INTERVAL; 

	log_msg( "Clients control thread started." );
	
	// listen for connections and start other client threads as needed.
  	while ( ClientControls.shutdown == FALSE ) 
	{
		// update the last active time for this thread
		ClientControls.lastAction = time(NULL);

		// check if clients are disabled
		if( ClientControls.enabled == FALSE )
		{
			// close the UPDATA listening socket if active
			if( listenUSocket >= 0 )
			{
#ifdef DEBUG
				debug( __FUNCTION__, "Close the UPDATA listening socket(%d)!! ", listenUSocket );
#endif
				close( listenUSocket );
				FD_ZERO( &read_fds );
				fdmax = 0;
				listenUSocket = -1;			
			}
			// close the RIB listening socket if active			
			if( listenRSocket >= 0 )
			{
#ifdef DEBUG
				debug( __FUNCTION__, "Close the RIB listening socket(%d)!! ", listenRSocket );
#endif
				close( listenRSocket );
				FD_ZERO( &read_fds );
				fdmax = 0;
				listenRSocket = -1;			
			}			
			
#ifdef DEBUG
			debug( __FUNCTION__, "clients control thread is disabled");
#endif	
		}
		// if it is enabled
		else
		{
			// if addr/port changed, close the old socket
			if( (listenUSocket != -1) && (ClientControls.rebindUFlag == TRUE) )
			{
				close( listenUSocket );
				FD_ZERO( &read_fds );
				fdmax = 0;
				listenUSocket = -1;			
				ClientControls.rebindUFlag = FALSE;
#ifdef DEBUG
				debug( __FUNCTION__, "Close the UPDATA listening socket(%d)!! ", listenUSocket );
#endif
			}
			
			if( (listenRSocket != -1) && (ClientControls.rebindRFlag == TRUE) )
			{
				close( listenRSocket );
				FD_ZERO( &read_fds );
				fdmax = 0;
				listenRSocket = -1;			
				ClientControls.rebindRFlag = FALSE;
#ifdef DEBUG
				debug( __FUNCTION__, "Close the RIB listening socket(%d)!! ", listenRSocket );
#endif
			}
			
			// if socket is down, reopen
			if (listenUSocket == - 1) 
			{
				listenUSocket = startListener(ClientControls.listenUAddr, ClientControls.listenUPort);
				// if listen succeeded, setup FD values
				// otherwise we will try next loop time
				if (listenUSocket != - 1) 
				{
					FD_ZERO( &read_fds );
					FD_SET(listenUSocket, &read_fds);
					fdmax = listenUSocket+1;
#ifdef DEBUG
					debug( __FUNCTION__, "Opened the UPDATA listening socket(%d)!! ", listenUSocket );
#endif
				}
			}
			else
			{
				FD_ZERO( &read_fds );
				FD_SET(listenUSocket, &read_fds);
				fdmax = listenUSocket+1;
			}
			
			if (listenRSocket == - 1) 
			{
				listenRSocket = startListener(ClientControls.listenRAddr, ClientControls.listenRPort);
				// if listen succeeded, setup FD values
				// otherwise we will try next loop time
				if (listenRSocket != - 1) 
				{
				
					FD_SET(listenRSocket, &read_fds);
					fdmax = listenRSocket+1;
#ifdef DEBUG
					debug( __FUNCTION__, "Opened the RIB listening socket(%d)!! ", listenRSocket );
#endif
				}
			}
			else
			{

				FD_SET(listenRSocket, &read_fds);
				fdmax = listenRSocket+1;
			}			
			
#ifdef DEBUG
			debug( __FUNCTION__, "clients control thread is enabled" );
#endif
		}

		if( select(fdmax, &read_fds, NULL, NULL, &timeout) == -1 )
		{
			log_err("clients control thread select error:%s", strerror(errno));
			continue;
		}
		timeout.tv_usec = 0;
		timeout.tv_sec = THREAD_CHECK_INTERVAL; 


		if( listenUSocket >= 0)
		{
			if( FD_ISSET(listenUSocket, &read_fds) )//new UPDATA client
			{
#ifdef DEBUG
				debug( __FUNCTION__, "new UPDATA client attempting to start." );
#endif
				startClient( listenUSocket, CLIENT_LISTENER_UPDATA );
			}
		}
		
		if( listenRSocket >= 0)
		{
			if( FD_ISSET(listenRSocket, &read_fds) )//new RIB client
			{
#ifdef DEBUG
				debug( __FUNCTION__, "new RIB client attempting to start." );
#endif
				startClient( listenRSocket, CLIENT_LISTENER_RIB );
			}
		}		
		
	}
	
	log_warning( "Clients control thread exiting" );	 

	// close UPDATA socket if open
	if( listenUSocket != -1) 
	{
		close(listenUSocket);
		FD_ZERO( &read_fds );
		fdmax = 0;
		listenUSocket = -1;			
#ifdef DEBUG
		debug( __FUNCTION__, "Close the UPDATA listening socket(%d)!! ", listenUSocket );
#endif
	}
	
	// close RIB socket if open
	if( listenRSocket != -1) 
	{
		close(listenRSocket);
		FD_ZERO( &read_fds );
		fdmax = 0;
		listenRSocket = -1;			
#ifdef DEBUG
		debug( __FUNCTION__, "Close the RIB listening socket(%d)!! ", listenRSocket );
#endif
	}	

	return NULL;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Start to listen on the configured addr+port.
 * Input:  listenaddress and listenport  from client (UPDATA or RIB)
 * Output: socket ID of the listener or -1 if listener create fails
 * He Yan @ July 22, 2008
 * Mikhail Strizhov @ June 2, 2009
 * -------------------------------------------------------------------------------------*/
int 
startListener(char *listenaddress, int listenport)
{	 
	// socket to listen for incoming connections
	int listenSocket = 0;

	// create addrinfo struct for the listener
	struct addrinfo *res = createAddrInfo(listenaddress, listenport);
	if( res == NULL ) 
	{
		log_err( "clients control thread createAddrInfo error!" );
		freeaddrinfo(res);
		listenSocket = -1;
		return listenSocket;
	}

	// open the listen socket
	listenSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if ( listenSocket == -1 )
	{
		log_err( "fail to create clients listener socket %s", strerror(errno) );
		freeaddrinfo(res);
		return listenSocket;
	}	

#ifdef DEBUG
	// allow rapid reuse of socket if in debug mode
	int yes=1;
	if(setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,&yes, sizeof(int)) < 0)
		log_err("setsockopt error");
#endif

	// bind to configured address and port
	if (bind(listenSocket, res->ai_addr, res->ai_addrlen) < 0)
	{
		log_err( "clients listener unable to bind %s", strerror(errno) );
		close(listenSocket);
		freeaddrinfo(res);
		listenSocket = -1;
		return listenSocket;
	}	

	//start listening
	if (listen(listenSocket, 0) < 0) {
		log_err( "clients listener unable to listen" );
		freeaddrinfo(res);
		close(listenSocket);
		listenSocket = -1;
		return listenSocket;
	}
	freeaddrinfo(res);

	return(listenSocket);	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Accept the new client and spawn a new thread for the client 
 *          if more clients are allowed and it passes the ACL check.
 * Input:  the socket used for listening,   UPDATA or RIB client trigger
 * Output: none
 * He Yan @ July 22, 2008
 * Mikhail Strizhov @ June 2, 2009
 * -------------------------------------------------------------------------------------*/
void 
startClient( int listenSocket, int client_listener)
{
	// structure to store the client addrress from accept
	struct sockaddr_storage clientaddr;
	socklen_t addrlen = sizeof (clientaddr);
	
	// accept connection
	int clientSocket = accept( listenSocket, (struct sockaddr *) &clientaddr, &addrlen);
	if( clientSocket == -1 ) {
		log_err( "Failed to accept new client" );
		return;
	}

	// convert address into address string and port
	char *addr;
	int port;
	if( getAddressFromSockAddr((struct sockaddr *)&clientaddr, &addr, &port) )
	{
		log_warning( "Unable to get address and port for new connection." );
		return;
	}
#ifdef DEBUG 	
	else
		debug(__FUNCTION__, "connection request from: %s, port: %d ", addr, port);
#endif

	// too many UPDATA clients, close this one
	if (client_listener == CLIENT_LISTENER_UPDATA)
	{
	
		if ( ClientControls.activeUClients >= ClientControls.maxUClients )
		{
			log_warning( "At maximum number of connected clients: connection from %s port %d rejected.", addr, port );
			close(clientSocket);
			free(addr);
			return;
		}
	}

	// too many RIB clients, close this one	
	if (client_listener == CLIENT_LISTENER_RIB)
	{

		if ( ClientControls.activeRClients >= ClientControls.maxRClients )
		{
			log_warning( "At maximum number of connected clients: connection from %s port %d rejected.", addr, port );
			close(clientSocket);
			free(addr);
			return;
		}
	}	

	if (client_listener == CLIENT_LISTENER_UPDATA)
	{
		//check the new client against ACL
		if ( checkACL((struct sockaddr *) &clientaddr, CLIENT_UPDATE_ACL) == FALSE )
		{
			log_msg("client connection from %s port %d rejected by access control list",addr, port);
			close(clientSocket);
			free(addr);
			return;
		}
	}	
	
	if (client_listener == CLIENT_LISTENER_RIB)
	{
		//check the new client against ACL
		if ( checkACL((struct sockaddr *) &clientaddr, CLIENT_RIB_ACL) == FALSE )
		{
			log_msg("client connection from %s port %d rejected by access control list",addr, port);
			close(clientSocket);
			free(addr);
			return;
		}		
	}
	
	if (client_listener == CLIENT_LISTENER_UPDATA)
	{
		// create a UPDATA client node structure
		ClientNode *Ucn = createClientUNode(ClientControls.nextUClientID,addr, port, clientSocket);
		if (Ucn == NULL) 
		{
			log_warning( "Failed to create client structure.   Closing connection from  %s port %d rejected.", addr, port );
			close(clientSocket);
			free(addr);
			return;
		}

		// lock the UPDATA client list
		if ( pthread_mutex_lock( &(ClientControls.clientULock) ) )
			log_fatal( "lock client update list failed" );

		// add the client to the list 
		Ucn->next = ClientControls.firstUNode;
		ClientControls.firstUNode = Ucn;

		//increment the number of active clients
		ClientControls.activeUClients++;
		ClientControls.nextUClientID++;

		// unlock the client list
		if ( pthread_mutex_unlock( &(ClientControls.clientULock ) ) )
			log_fatal( "unlock client update list failed");
	
		// spawn a new thread for this client
		pthread_t clientUThreadID;
		int error;
		Ucn->clientThread = clientUThreadID;
		if ((error = pthread_create( &clientUThreadID, NULL, &clientUThread, Ucn)) > 0) 
		{
			log_warning("Failed to create UPDATA client thread: %s", strerror(error));
			destroyClient(Ucn->id, CLIENT_LISTENER_UPDATA);
		}
	}
	
	if (client_listener == CLIENT_LISTENER_RIB)
	{
		// create a RIB client node structure
		ClientNode *Rcn = createClientRNode(ClientControls.nextRClientID,addr, port, clientSocket);
		if (Rcn == NULL) 
		{
			log_warning( "Failed to create client structure.   Closing connection from  %s port %d rejected.", addr, port );
			close(clientSocket);
			free(addr);
			return;
		}

		// lock the RIB client list
		if ( pthread_mutex_lock( &(ClientControls.clientRLock) ) )
			log_fatal( "lock client rib list failed" );

		// add the client to the list 
		Rcn->next = ClientControls.firstRNode;
		ClientControls.firstRNode = Rcn;

		//increment the number of active clients
		ClientControls.activeRClients++;
		ClientControls.nextRClientID++;

		// unlock the client list
		if ( pthread_mutex_unlock( &(ClientControls.clientRLock ) ) )
			log_fatal( "unlock client rib list failed");
	
		// spawn a new thread for this client
		pthread_t clientRThreadID;
		Rcn->clientThread = clientRThreadID;
		int error;
		if ((error = pthread_create( &clientRThreadID, NULL, &clientRThread, Rcn)) > 0) 
		{
			log_warning("Failed to create RIB client thread: %s", strerror(error));
			destroyClient(Rcn->id, CLIENT_LISTENER_RIB);
		}
	}	
	
// testing
	/*
	if( ClientControls.activeClients == 1 )
	{	
		setPeerAddress(0, "129.82.138.4");
		setPeerAddress(1, "129.88.88.88");
		setLabelAction(1, 0);
	}
	if( ClientControls.activeClients == 2 )
	{
		setPeerAddress(0, "195.28.164.125");
		setPeerPort(0, 179);
		setPeerASNum(0, 23456);
		setPeerBGPID(0, inet_addr("195.28.164.125"));
		setMonitorASNum(0, 6447);
		setPeerAddress(1, "129.11.11.11");	
		setLabelAction(1, 1);
	}	
	if( ClientControls.activeClients == 3 )
	{	
		deletePeer(0);
	}
	*/	
	
	
#ifdef DEBUG
	debug(__FUNCTION__, "client accepted from: %s, port: %d ", addr, port);
#endif
	free(addr);
	return;
}

/*--------------------------------------------------------------------------------------
 * Purpose: print all the active clients
 * Input:  UPDATA or RIB trigger
 * Output: none
 * He Yan @ July 22, 2008
 * Mikhail Strizhov @ June 2, 2009
 * -------------------------------------------------------------------------------------*/
void 
printClients(int client_listener)
{
	if (client_listener == CLIENT_LISTENER_UPDATA)
	{
		
		// print all connected UPDATA clients for debugging
		ClientNode *Ucn = ClientControls.firstUNode;
		while( Ucn != NULL )
		{
			long id = Ucn->id;
			char *Uaddr = getClientAddress(id, CLIENT_LISTENER_UPDATA);
			int Uport = getClientPort(id, CLIENT_LISTENER_UPDATA);
			long Uread = getClientReadItems(id, CLIENT_LISTENER_UPDATA);
			long Uunread = getClientUnreadItems(id, CLIENT_LISTENER_UPDATA);
			time_t UconnectedTime = getClientConnectedTime(id, CLIENT_LISTENER_UPDATA);
			log_msg("Client id: %ld  addr: %s, port: %d, read:%ld, unread:%ld, connectedTime:%d", id, Uaddr, Uport, Uread, Uunread, UconnectedTime);			
			free(Uaddr);
			Ucn = Ucn->next;
		}
	}	
	
	if (client_listener == CLIENT_LISTENER_RIB)
	{
		// print all connected RIB clients for debugging
		ClientNode *Rcn = ClientControls.firstRNode;
		while( Rcn != NULL )
		{
			long id = Rcn->id;
			char *Raddr = getClientAddress(id, CLIENT_LISTENER_RIB);
			int Rport = getClientPort(id, CLIENT_LISTENER_RIB);
			long Rread = getClientReadItems(id, CLIENT_LISTENER_RIB);
			long Runread = getClientUnreadItems(id, CLIENT_LISTENER_RIB);
			time_t RconnectedTime = getClientConnectedTime(id, CLIENT_LISTENER_RIB);
			log_msg("Client id: %ld  addr: %s, port: %d, read:%ld, unread:%ld, connectedTime:%d", id, Raddr, Rport, Rread, Runread, RconnectedTime);			
			free(Raddr);
			Rcn = Rcn->next;
		}
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Signals to the Clients Module that BGPmon is shutting down
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 8, 2009
 * -------------------------------------------------------------------------------------*/
void signalClientsShutdown() {
	// both the client listener and all client connections check this variable
	// to determine when to shutdown
	ClientControls.shutdown = TRUE;

#ifdef DEBUG
	debug(__FUNCTION__, "Client module signaled for shutdown.");
#endif
}

/*--------------------------------------------------------------------------------------
 * Purpose: waits for the client module to finish shutting down
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 8, 2009
 * -------------------------------------------------------------------------------------*/
void waitForClientsShutdown() {
	void * status = NULL;

	// wait for client listener control thread to exit
	pthread_join(ClientControls.clientsListenerThread, status);

	// wait for each update client connection thread to exit
	ClientNode * cn = ClientControls.firstUNode;
	while(cn!=NULL) {
		status = NULL;
		deleteClient(cn->id, CLIENT_LISTENER_UPDATA);
		//pthread_join(cn->clientThread, status);
		//free(cn);
		cn = cn->next;
	}

	// wait for each update client connection thread to exit
	cn = ClientControls.firstRNode;
	while(cn!=NULL) {
		status = NULL;
		deleteClient(cn->id, CLIENT_LISTENER_RIB);
		//pthread_join(cn->clientThread, status);
		//free(cn);
		cn = cn->next;
	}
}
