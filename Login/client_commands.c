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
 *  File: client_commands.c
 * 	Authors: Kevin Burnett
 *
 *  Date: November 5, 2008
 */

// Needed for atoi() 
#include <stdlib.h>
// Needed for memset,strncmp
#include <string.h>
// Needed for strerror(errno)
#include <errno.h>

// Needed for the function definitions
#include "client_commands.h"
// Provides the definition for clientThreadArguments struct
#include "login.h"
// Provides the definition for commandArgument and commandNode structs
#include "commandprompt.h"
// Provides defaults used and Queue names
#include "../site_defaults.h"
// needed for TRUE/FALSE definitions
#include "../Util/bgpmon_defaults.h"
// provides access to Client related functions
#include "../Clients/clients.h"
// needed for address related functions
#include "../Util/address.h"
// needed for system types such as time_t
#include <sys/types.h>

/*----------------------------------------------------------------------------------------
 * Purpose: This will update the port of the client-listener module based on a
 *	command from the CLI.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 2, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdClientListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int port = 0;
	port = atoi(ca->commandArgument);
	int type = -1;

	if(listContainsCommand(cn, "update")) {
		type = CLIENT_LISTENER_UPDATA;
	} else if(listContainsCommand(cn, "rib")) {
		type = CLIENT_LISTENER_RIB;
	}

	if(type!=-1)
		setClientsControlListenPort(port, type);

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This will Enable the client-listener for the client module.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 2, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdClientListenerEnable(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	enableClientsControl();
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This will Disable the client-listener for the client module.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 2, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdClientListenerDisable(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	disableClientsControl();
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This will show the currently configured port for the client-listener module.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 2, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdShowClientListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	u_int16_t port = 0;
	int type = -1;

	if(listContainsCommand(cn, "update")) {
		type = CLIENT_LISTENER_UPDATA;
	} else if(listContainsCommand(cn, "rib")) {
		type = CLIENT_LISTENER_RIB;
	}

	if(type!=-1) {
		port = getClientsControlListenPort(type);
		sendMessage(client->socket, "port is %d\n", port);
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This will show the current statue (enabled/disabled) for the client-listener module.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ November 3, 2008
 * -------------------------------------------------------------------------------------*/
int cmdShowClientListenerStatus(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	sendMessage(client->socket, "client-listener is ");
	if(isClientsControlEnabled()) {
		sendMessage(client->socket, "enabled\n");
	}
	else {
		sendMessage(client->socket, "disabled\n");
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Set the address for the client-listener module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ November 3, 2008
 * -------------------------------------------------------------------------------------*/
int cmdClientListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	char * address = NULL;
	int result = 0;
	int type = -1;

	if(listContainsCommand(cn, "update")) {
		type = CLIENT_LISTENER_UPDATA;
	} else if(listContainsCommand(cn, "rib")) {
		type = CLIENT_LISTENER_RIB;
	}

	// get the address
	address = ca->commandArgument;
	if(checkAddress(address, ADDR_PASSIVE)!=ADDR_VALID) {
		sendMessage(client->socket, "Address [%s] is not a valid listening address.\n", address);
		return 1;
	}

	if(type!=-1)
		result = setClientsControlListenAddr(address, type);

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Display the address for the client listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ November 3, 2008
 * -------------------------------------------------------------------------------------*/
int cmdShowClientListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	char * address = NULL;
	int type = -1;

	if(listContainsCommand(cn, "update")) {
		type = CLIENT_LISTENER_UPDATA;
	} else if(listContainsCommand(cn, "rib")) {
		type = CLIENT_LISTENER_RIB;
	}

	if(type!=-1) {
		address = getClientsControlListenAddr(type);

		sendMessage(client->socket, "address is %s\n", address);
		free(address);
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Set the max connectionf for the client-listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Cathie Olschanowsky @ April 2012
 * -------------------------------------------------------------------------------------*/
int cmdClientListenerMaxConnections(commandArgument * ca, clientThreadArguments * client, commandNode * root) {

  int newMax = atoi(ca->commandArgument);

  if(listContainsCommand(root, "update")) {
    if(setClientsControlMaxConnections(newMax,CLIENT_LISTENER_UPDATA)){
      sendMessage(client->socket, "Update of max connections failed");
      return 1;
    }
  }else if(listContainsCommand(root, "rib")) {
    if(setClientsControlMaxConnections(newMax,CLIENT_LISTENER_RIB)){
      sendMessage(client->socket, "Update of max connections failed");
      return 1;
    }
  }else{
    return 1;
  }
  return 0;
}
/*----------------------------------------------------------------------------------------
 * Purpose: Display the maximum number of connections allowed
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 *              in. This list is in the same order as they were typed.
 *      clientThreadArguments - A struct providing the basic address information for the 
 *              current connection.
 *      commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Cathie Olschanowsky @ April 2012
 * -------------------------------------------------------------------------------------*/
int
cmdShowClientListenerMaxConnections(commandArgument * ca, clientThreadArguments * client, commandNode * cn)
{

  int type = -1;
  if(listContainsCommand(cn, "update")) {
    type = CLIENT_LISTENER_UPDATA;
  } else if(listContainsCommand(cn, "rib")) {
    type = CLIENT_LISTENER_RIB;
  }

  if(type!=-1) {
    int maxConnections = getClientsControlMaxConnections(type);
    sendMessage(client->socket, "Connections are limited to  %d\n", maxConnections);
  }else{
    return 1;
  }

  return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Display summary info for UPDATE and RIB client listener 
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Mikhail Strizhov @ January 29, 2010
 * -------------------------------------------------------------------------------------*/
int cmdShowClientListenerSummary(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {

	if(isClientsControlEnabled()) 
	{
		sendMessage(client->socket, "Client-listener is enabled\n");

		char * address = NULL;
		u_int16_t port = 0;
		AccessControlList * acl = NULL;
                int maxClients = 0;

		sendMessage(client->socket, "\n");
		acl = LoginSettings.clientUpdateAcl;
		if(acl==NULL) 
		{
			sendMessage(client->socket, "No Access Control List is defined for UPDATE Clients.\n", ca->commandArgument);
			return 1;
		}
		else
		{
			sendMessage(client->socket, "UPDATE ACL: %s\n", acl->name);
		}
		address = getClientsControlListenAddr(CLIENT_LISTENER_UPDATA);
		sendMessage(client->socket, "UPDATE address is %s\n", address);
		port = getClientsControlListenPort(CLIENT_LISTENER_UPDATA);
		sendMessage(client->socket, "UPDATE port is %d\n", port);
                maxClients = getClientsControlMaxConnections(CLIENT_LISTENER_UPDATA);
		sendMessage(client->socket, "UPDATE max clients is %d\n", maxClients);


		sendMessage(client->socket, "\n");
		acl = LoginSettings.clientRIBAcl;
		if(acl==NULL) 
		{
			sendMessage(client->socket, "No Access Control List is defined for RIB Clients.\n", ca->commandArgument);
			return 1;
		}
		else
		{
			sendMessage(client->socket, "RIB ACL: %s\n", acl->name);
		}
		address = getClientsControlListenAddr(CLIENT_LISTENER_RIB);
		sendMessage(client->socket, "RIB address is %s\n", address);
		port = getClientsControlListenPort(CLIENT_LISTENER_RIB);
		sendMessage(client->socket, "RIB port is %d\n", port);
                maxClients = getClientsControlMaxConnections(CLIENT_LISTENER_RIB);
		sendMessage(client->socket, "RIB max clients is %d\n", maxClients);

		free(address);
	}
	else 
	{
		sendMessage(client->socket, "Client-listener is disabled\n");
	}

	return 0;
}





/*----------------------------------------------------------------------------------------
 * Purpose: Display the address for the client listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ November 3, 2008
 * -------------------------------------------------------------------------------------*/
int cmdShowClients(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	long* clientIDs = NULL;
	long clientID = 0;
	int clientCount = 0, i = 0;
	int clientPort = 0;
	char * clientAddress = NULL;

	clientCount = getActiveClientsIDs(&clientIDs, CLIENT_LISTENER_UPDATA);

	// display all clients
	if(clientCount>0) { 
		sendMessage(client->socket, "ID\taddress\t\tport\n");
		for(i = 0; i<clientCount; i++) {
			clientID = clientIDs[i];

			clientPort = getClientPort(clientID, CLIENT_LISTENER_UPDATA);
			clientAddress = getClientAddress(clientID, CLIENT_LISTENER_UPDATA);

			sendMessage(client->socket, "%d\t%s\t%d\n", clientID, clientAddress, clientPort);

			free(clientAddress);
		}
	}
	else {
		sendMessage(client->socket, "No active clients.\n");
	}

	free(clientIDs);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: set the active ACL for the CLients module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ October 26, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdSetClientListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	AccessControlList * acl = NULL;
	int type = -1;

	// figure out what the client-listener type is
	if(listContainsCommand(cn, "update")) {
		type = CLIENT_LISTENER_UPDATA;
	} else if(listContainsCommand(cn, "rib")) {
		type = CLIENT_LISTENER_RIB;
	}
	
	if(type!=-1) {
		acl = getACL(ca->commandArgument);

		if(acl==NULL) {
			sendMessage(client->socket, "Access Control List [%s] was not found.\n", ca->commandArgument);
			return 1;
		}

		if(type==CLIENT_LISTENER_UPDATA) {
			LoginSettings.clientUpdateAcl = acl;
		} else if(type==CLIENT_LISTENER_RIB) {
			LoginSettings.clientRIBAcl = acl;
		}

	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: show the active Access Control List for the Client module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ October 26, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdShowClientListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	AccessControlList * acl = NULL;
	int type = -1;

	if(listContainsCommand(cn, "update")) {
		type = CLIENT_LISTENER_UPDATA;
	} else if(listContainsCommand(cn, "rib")) {
		type = CLIENT_LISTENER_RIB;
	}

	if(type!=-1) {
		if(type==CLIENT_LISTENER_UPDATA) {
			acl = LoginSettings.clientUpdateAcl;
		} else if(type==CLIENT_LISTENER_RIB) {
			acl = LoginSettings.clientRIBAcl;
		}

		if(acl==NULL) {
			sendMessage(client->socket, "No Access Control List is defined for Clients.\n", ca->commandArgument);
			return 1;
		}
		sendMessage(client->socket, "acl: %s\n", acl->name);
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Deletes an active client based on the clientId
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ November 10, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdKillClient(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	long clientID = 0;
	clientID = atol(ca->commandArgument);
	deleteClient(clientID, CLIENT_LISTENER_UPDATA);
	return 0;
}

