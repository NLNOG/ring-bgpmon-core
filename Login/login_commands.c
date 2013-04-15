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

// Provides the definition for clientThreadArguments struct
#include "login.h"
// Provides the definition for commandArgument and commandNode structs
#include "commandprompt.h"
// needed for TRUE/FALSE definitions
#include "../Util/bgpmon_defaults.h"
// needed for address related functions
#include "../Util/address.h"

/*----------------------------------------------------------------------------------------
 * Purpose: Changes the enabled password
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ January 24, 2009
 * -------------------------------------------------------------------------------------*/
int cmdEnablePassword(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	setEnablePassword(ca->commandArgument);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Changes the Access password
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
int cmdAccessPassword(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	setAccessPassword(ca->commandArgument);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: set the active ACL for the Command Line Interface module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
int cmdLoginListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	AccessControlList * acl = NULL;
	acl = getACL(ca->commandArgument);
	if(acl==NULL) {
		sendMessage(client->socket, "Access Control List [%s] was not found.\n", ca->commandArgument);
		return 1;
	}
	LoginSettings.cliAcl= acl;
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Sets the address for the login listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
int cmdLoginListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	char * address = NULL;
	int result = 0;

	// get the address
	address = ca->commandArgument;
	if(checkAddress(address, ADDR_PASSIVE)!=ADDR_VALID) {
		sendMessage(client->socket, "Address [%s] is not a valid listening address.\n", address);
		return 1;
	}

	// set the address
	result = setListenAddr(address);

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Sets the port for the login listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
int cmdLoginListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	long port = 0;
	port = strtol(ca->commandArgument,NULL,10);
        if(port < 1 || port > 65535){
          sendMessage(client->socket, "Port [%s] is out of range.\n",ca->commandArgument);
          return 1;
        }
	setListenPort((int)port);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: show the active Access Control List for the login-listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowLoginListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	AccessControlList * acl = NULL;
	acl = LoginSettings.cliAcl;
	if(acl==NULL) {
		sendMessage(client->socket, "No Access Control List is defined for login-listener.\n", ca->commandArgument);
		return 1;
	}
	sendMessage(client->socket, "Current acl: %s\n", acl->name);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: show the active port or the login listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowLoginListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int port = 0;
	port = getListenPort();
	sendMessage(client->socket, "login-listener socket is %d\n", port);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: show the active address for the login listener
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowLoginListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	char * addr = NULL;
	addr = getListenAddr();
	sendMessage(client->socket, "login-listener address is %s\n", addr);
	free(addr);
	return 0;
}
