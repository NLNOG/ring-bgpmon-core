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
 *  File: clients.h
 * 	Authors: He Yan, Dan Massey
 *  Date: Oct 8, 2008 
 */

#ifndef CLIENTS_H_
#define CLIENTS_H_

/* needed for system types such as time_t */
#include <sys/types.h>

/* RIB and UPDATA constants */
#define CLIENT_LISTENER_UPDATA 1
#define CLIENT_LISTENER_RIB 2

// functions related to accepting and managing client connections
// see clientscontrol.c for corresponding functions

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default clients control configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008 
 * -------------------------------------------------------------------------------------*/
int initClientsControlSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Read the clients control settings from the config file.
 * Input: none
 * Output:  returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readClientsControlSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Save the  clients control  settings to the config file.
 * Input:  none
 * Output:  retuns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int saveClientsControlSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: launch clients control thread, called by main.c
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void launchClientsControlThread();

/*--------------------------------------------------------------------------------------
 * Purpose: Get the state of clients control
 * Input:
 * Output: returns TRUE or FALSE
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int isClientsControlEnabled();

/*--------------------------------------------------------------------------------------
 * Purpose: dsiable the clients control
 * Input:
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void enableClientsControl();

/*--------------------------------------------------------------------------------------
 * Purpose: disable the clients control
 * Input:
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void disableClientsControl();

/*--------------------------------------------------------------------------------------
 * Purpose: Get the listening port of clients control
 * Input:
 * Output: the listening port of clients control module
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getClientsControlListenPort(int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the listening port of clients control
 * Input:       the port
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setClientsControlListenPort( int port, int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the listening address of clients control
 * Input:
 * Output: the listening address for client connections
 *          or NULL if no address for client connections 
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
char * getClientsControlListenAddr(int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the listening address of clients control
 * Input:       the addr in string format 
 * Output: ADDR_VALID means address is valid and set successfully.
 *         ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *         ADDR_ERR_INVALID_PASSIVE means this is not a valid passive address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setClientsControlListenAddr( char *addr, int client_listener );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for this thread
 * Input:      
 * Output: a timevalue indicating the last time the thread was active
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getClientsControlLastAction();

/*--------------------------------------------------------------------------------------
 * Purpose: Get and Set the maximum allowed clients for clients control
 * Input:       [the new max], which listener
 * Output:
 * Cathie Olschanowsky @ April 2012
 * -------------------------------------------------------------------------------------*/
int getClientsControlMaxConnections(int client_listener);
int setClientsControlMaxConnections(int newMax, int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose:  delete the socket and memory associated with a client ID
 * Input:  the client ID to destroy
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void destroyClient(long id, int client_listener);


// functions related to a particular client connection
// see clientinstance.c for corresponding functions

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of IDs of all active clients
 * Input: a pointer to an unallocated array
 * Output: the length of the array or -1 on failure
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getActiveClientsIDs(long **IDs, int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote client port (port on the client machine)
 * Input: ID of the client
 * Output: port of this client
 *         or -1 if there is no client with this ID
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getClientPort(long ID, int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote address of a client (address of client machine)
 * Input: ID of the client
 * Output: address of this client in a char array.
 *         or NULL if there is no client with this ID
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it. 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
char * getClientAddress(long ID, int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the connected time of a client
 * Input: ID of the client
 * Output: connected time of this client
 *                or time = 0 if there is no client with this ID
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getClientConnectedTime(long ID, int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the number of read items for a client
 * Input: ID of the client
 * Output: the number of read items for  this client
 *         or -1 if there is no client with this ID
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
long getClientReadItems(long ID, int client_listener);
	
/*--------------------------------------------------------------------------------------
 * Purpose: Get the number of unread items for a client
 * Input: ID of the client
 * Output: the number of unread items for  this client
 *         or -1 if there is no client with this ID
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
long getClientUnreadItems(long ID, int client_listener);

/*-------------------------------------------------------------------------------------- 
 * Purpose: Get the last action time for this thread
 * Input: ID of the client
 * Output: a timevalue indicating the last time the thread was active
 *         or time = 0 if there is no client with this ID
 * He Yan @ Sep 22, 2008
* -------------------------------------------------------------------------------------*/
time_t getClientLastAction(long ID, int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose:  delete the socket and memory associated with a client ID
 * Input:  the client ID to destroy
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void deleteClient(long id, int client_listener);

/*--------------------------------------------------------------------------------------
 * Purpose: Signals to the Clients Module that BGPmon is shutting down
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 8, 2009
 * -------------------------------------------------------------------------------------*/
void signalClientsShutdown();

/*--------------------------------------------------------------------------------------
 * Purpose: waits for the client module to finish shutting down
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 8, 2009
 * -------------------------------------------------------------------------------------*/
void waitForClientsShutdown();

#endif /*CLIENTS_H_*/
