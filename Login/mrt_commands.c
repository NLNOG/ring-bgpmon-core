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
 *  File: commands.c
 * 	Authors: Kevin Burnett
 *
 *  Date: May 25th, 2009
 */

// Needed for atoi() 
#include <stdlib.h>
// Needed for memset,strncmp
#include <string.h>
// Needed for strerror(errno)
#include <errno.h>
// needed for time_t functions
#include <time.h>



// Needed for the function definitions
#include "mrt_commands.h"
// Provides the definition for clientThreadArguments struct
#include "login.h"
// Provides the definition for commandArgument and commandNode structs
#include "commandprompt.h"
// provdes the interface into the Mrt module
#include "../Mrt/mrt.h"
// needed for address related functions
#include "../Util/address.h"
// needed for peerLabelAction
#include "../Labeling/label.h"

// needed for all the peering commands
#include "../Peering/peers.h"
// needed for peering session commands
#include "../Peering/peersession.h"
// needed for all peer group commands
#include "../Peering/peergroup.h"
// needed for all states
#include "../Peering/bgpstates.h"

/*----------------------------------------------------------------------------------------
 * Purpose: Returns the status used by the Mrt-listener to a client's screen
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ May 25, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowMrtListenerStatus(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	sendMessage(client->socket, "mrt-listener is ");
	if(isMrtControlEnabled()) {
		sendMessage(client->socket, "enabled\n");
	}
	else {
		sendMessage(client->socket, "disabled\n");
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Returns the port used by the Mrt-listener to a client's screen
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ May 25, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowMrtListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int port = 0;
	port = getMrtControlListenPort();
	sendMessage(client->socket, "mrt-listener socket is %d\n", port);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Returns the address used by the Mrt-listener to a client's screen
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ May 25, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowMrtListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	char * addr = NULL;
	addr = getMrtControlListenAddr();
	sendMessage(client->socket, "mrt-listener address is %s\n", addr);
	free(addr);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Returns the current maximum number of mrt connections allowed
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Cathie Olschanowsky April 2012
 * -------------------------------------------------------------------------------------*/
int cmdShowMrtListenerLimit(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
  int maxConn = getMrtMaxConnections();
  sendMessage(client->socket,"mrt-listener limit is %d\n",maxConn);
  return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Set a new Mrt maximum number of connections
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Cathie Olschanowsky April 2012
 * -------------------------------------------------------------------------------------*/
int cmdMrtListenerLimit(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
  if(setMrtMaxConnections(atoi(ca->commandArgument))){
    sendMessage(client->socket, "mrt-listener new connection limit is not valid\n");
    return 1;
  }
  return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Show summary info for Mrt-listener 
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Mikhail Strizhov @ January 29, 2010
 * -------------------------------------------------------------------------------------*/

int cmdShowMrtListenerSummary(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	if(isMrtControlEnabled()) 
	{
		sendMessage(client->socket, "mrt-listener is enabled\n");
		
		AccessControlList * acl = NULL;
		acl = LoginSettings.mrtAcl;
		if(acl==NULL) {
			sendMessage(client->socket, "No Access Control List is defined for the Mrt module.\n", ca->commandArgument);
			return 1;
		}
		else
		{
			sendMessage(client->socket, "mrt-listener acl: %s\n", acl->name);
		}

		int port = 0;
		char * addr = NULL;
                int maxConnections = 0;

		addr = getMrtControlListenAddr();
		sendMessage(client->socket, "mrt-listener address is %s\n", addr);
		port = getMrtControlListenPort();
		sendMessage(client->socket, "mrt-listener socket is %d\n", port);
                maxConnections = getMrtMaxConnections();
		sendMessage(client->socket, "mrt-listener maximum connections allowed is %d\n", maxConnections);

		free(addr);
	}
	else {
		sendMessage(client->socket, "mrt-listener is disabled\n");
	}
	return 0;
}




/*----------------------------------------------------------------------------------------
 * Purpose: Used by the CLI to se the port for the mrt listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ May 25, 2009
 * -------------------------------------------------------------------------------------*/
int cmdMrtListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int port = 0;
	port = atoi(ca->commandArgument);
	setMrtControlListenPort(port);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Used by the CLI to se the address for the mrt listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ May 25, 2009
 * -------------------------------------------------------------------------------------*/
int cmdMrtListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	char * address = NULL;
	int result = 0;

	// get the address
	address = ca->commandArgument;
	if(checkAddress(address, ADDR_PASSIVE)!=ADDR_VALID) {
		sendMessage(client->socket, "Address [%s] is not a valid listening address.\n", address);
		return 1;
	}

	// set the address
	result = setMrtControlListenAddr(address);

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Used by the CLI to se the port for the mrt listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ May 25, 2009
 * -------------------------------------------------------------------------------------*/
int cmdMrtListenerEnable(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	enableMrtControl();
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Used by the CLI to se the port for the mrt listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ May 25, 2009
 * -------------------------------------------------------------------------------------*/
int cmdMrtListenerDisable(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	disableMrtControl();
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: 
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ May 26, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowMrtClient(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	long *ids = NULL;
	int count = getActiveMrtsIDs(&ids);

	if(count<0) {
		log_err("error retrieve mrt clients in cmdShowMrtClient()\n");
	}
	else {
		if(count==0) {
			sendMessage(client->socket, "no active mrt clients\n");
		}
		else {
			int i = 0, mrtID = 0, mrtPort = 0, mrtAction ;
			char * mrtAddr = NULL;
			time_t mrtTime, mrtLastTime;

			for(i=0; i<count; i++) {
				mrtID = ids[i];
                                	
				mrtAddr = getMrtAddress(mrtID);
				mrtPort = getMrtPort(mrtID);
				mrtTime = getMrtConnectedTime(mrtID);
				mrtAction = getMrtLabelAction(mrtID);
				mrtLastTime = getMrtLastAction(mrtID);

				sendMessage(client->socket, "Mrt peers:\n");
//				sendMessage(client->socket, "%d: %d\n", i, ids[i]);
				sendMessage(client->socket, "\tpeer ID: %d\n", mrtID);
				sendMessage(client->socket, "\tpeer address: %s\n", mrtAddr);
				sendMessage(client->socket, "\tpeer port: %d\n", mrtPort);

				sendMessage(client->socket, "\tpeer label action: ");
				switch(mrtAction)
				{
					case NoAction:
						sendMessage(client->socket, "NoAction\n");
						break;
					case Label:
						sendMessage(client->socket, "Label\n");
						break;
					case StoreRibOnly:
						sendMessage(client->socket, "StoreRibOnly\n");
						break;
				}

				struct tm * timeinfo;
				timeinfo = localtime ( &mrtTime );
				sendMessage(client->socket, "\tpeer connection time: %s", asctime (timeinfo));
				timeinfo = localtime ( &mrtLastTime );
				sendMessage(client->socket, "\tpeer last action time: %s", asctime(timeinfo));
			}

                        free(mrtAddr);
		}
	}


	free(ids);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Shows specific details for a mrt client
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ May 26, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowMrtClientId(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: set the active ACL for the mrt module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ May 27, 2009
 * -------------------------------------------------------------------------------------*/
int cmdSetMrtListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	AccessControlList * acl = NULL;
	acl = getACL(ca->commandArgument);
	if(acl==NULL) {
		sendMessage(client->socket, "Access Control List [%s] was not found.\n", ca->commandArgument);
		return 1;
	}
	LoginSettings.mrtAcl = acl;
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: show the active Access Control List for the Mrt module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ May 27, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowMrtListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	AccessControlList * acl = NULL;
	acl = LoginSettings.mrtAcl;
	if(acl==NULL) {
		sendMessage(client->socket, "No Access Control List is defined for the Mrt module.\n", ca->commandArgument);
		return 1;
	}
	sendMessage(client->socket, "Current Mrt acl: %s\n", acl->name);
	return 0;
}


/*----------------------------------------------------------------------------------------
 * Purpose: show all data received from Mrt connection 
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Mikhail Strizhov @ Jan 31, 2010
 * -------------------------------------------------------------------------------------*/

int cmdShowMrtNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * root) {

	int i = 0;
	int establishedSessions[MAX_SESSION_IDS];
	int establishedSessionCount;
	int mrtId;
	
	char * peerRemoteAddress = NULL, * peerLocalAddress = NULL;
	int peerRemotePort = 0, peerRemoteASNum = 0;
	int peerLocalPort = 0, peerLocalASNum = 0;
	
	establishedSessionCount = 0;
	memset(establishedSessions, 0, sizeof(int)*MAX_SESSION_IDS);
 
	// get the established session count and list
	for( i=0; i<MAX_SESSION_IDS; i++ )
	{
		// check Sessions and check Sessions trigger = established or mrtestablished
		if( Sessions[i] != NULL && getSessionState(Sessions[i]->sessionID) == stateMrtEstablished )
		{
			establishedSessions[establishedSessionCount] = Sessions[i]->sessionID;
			establishedSessionCount++;
		}
	}

	if (establishedSessionCount != 0)
	{
		sendMessage(client->socket, "Mrt session peers\n");
		sendMessage(client->socket, "ID\tRemote\t\t\t\tLocal\t\t\n");
		sendMessage(client->socket,   "\tADDRESS\t\tPORT\tASNum\tADDRESS\t\tPORT\tASNum\n");
		for (i=0; i<establishedSessionCount; i++)
		{
			mrtId = establishedSessions[i];
			
			peerRemoteAddress = getSessionRemoteAddr(mrtId);
			peerRemotePort = getSessionRemotePort(mrtId);
			//peerRemoteBGPID = getSessionRemoteBGPID(mrtId);
			peerRemoteASNum = getSessionRemoteASNum(mrtId);
	
			peerLocalAddress = getSessionLocalAddr(mrtId);
			peerLocalPort = getSessionLocalPort(mrtId);
			//peerLocalBGPID = getSessionLocalBGPID(mrtId);
			peerLocalASNum = getSessionLocalASNum(mrtId);
	 
			sendMessage(client->socket, "%d\t", mrtId);
			sendMessage(client->socket, "%s\t", peerRemoteAddress);
			sendMessage(client->socket, "%d\t", peerRemotePort);
			//sendMessage(client->socket, "%d\t", peerRemoteBGPID);
			sendMessage(client->socket, "%lu\t", peerRemoteASNum);
			sendMessage(client->socket, "%s\t", peerLocalAddress);
			sendMessage(client->socket, "%d\t", peerLocalPort);
			//sendMessage(client->socket, "%d\t", peerLocalBGPID);
			sendMessage(client->socket, "%lu\n", peerLocalASNum);
		}
	}
	else
	{
		sendMessage(client->socket, "NO Mrt session peers\n");
	}

	return 0;
}



