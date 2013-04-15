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
 *  Date: July 30, 2008 
 */

// Needed for atoi() 
#include <stdlib.h>
// Needed for memset,strncmp
#include <string.h>
// Needed for strerror(errno)
#include <errno.h>

// Needed for the function definitions
#include "commands.h"
// Provides the definition for clientThreadArguments struct
#include "login.h"
// Provides the definition for commandArgument and commandNode structs
#include "commandprompt.h"
// Provides defaults used and Queue names
#include "../site_defaults.h"
// needed for TRUE/FALSE definitions
#include "../Util/bgpmon_defaults.h"
// Provides the definition for the ChainStruct
#include "../Chains/chains.h"
// Provides access to ACL structs and definitions
#include "../Util/acl.h"
// provides access to ACL management functions
#include "../Util/aclmanagement.h"
// provides access to Client related functions
#include "../Clients/clients.h"
// provides access to config file related functions
#include "../Config/configfile.h"
// provides address verification
#include "../Util/address.h"
// provides queue
#include "../Queues/queue.h"
// needed for to start peers
#include "../Peering/peers.h"
// needed for the closeBgpmon()
#include "../Util/signals.h"
 
/*----------------------------------------------------------------------------------------
 * Purpose: Changes the security access level of the use to "enable" from general access.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int enable(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int msg_len = 0;
	char pwd[PASSWORD_MAX_CHARS];
	memset(pwd, 0, PASSWORD_MAX_CHARS);

	if(isEnableModeActive()==TRUE) {
		sendMessage(client->socket, "enable mode is already active for another user.\n");
	}
	else {
		// check enable password
		sendMessage(client->socket, "enable password: ");
		msg_len = getMessage(client->socket, pwd, PASSWORD_MAX_CHARS);
		if (msg_len < 0)
		{
			log_fatal("enable(): could not read from socket");
			return 1;
		}
		if(strcmp(pwd, LoginSettings.enablePassword)==0) {
			// active enable flag so other users can't enter mode
			enableModeActive();

			// authentication successful so change priviledges to ENABLE
			client->commandMask = ENABLE;
			return 0;
		}
		sendMessage(client->socket, "incorrect enable password\n");
	}
	return 1;
}

/*----------------------------------------------------------------------------------------
 * Purpose:  
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int disable(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	// inactive enable flag so other users can't enter mode
	enableModeInactive();

	client->commandMask = ACCESS;
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose:  
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int configure(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	client->commandMask = CONFIGURE;
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Change the context of the current use to ROUTER_BGP to allow them configure
 * 	peers.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int router(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	client->commandMask = ROUTER_BGP;
	if(ca!=NULL)
		client->localASNumber = atoi(ca->commandArgument);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose:  
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int showRunning(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	sendMessage(client->socket, "show running!\n");
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose:  
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ August 1, 2008   
 * -------------------------------------------------------------------------------------*/
int exitEnable(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	// inactive enable flag so other users can't enter mode
	enableModeInactive();

	client->commandMask = ACCESS;
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose:  
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ August 1, 2008   
 * -------------------------------------------------------------------------------------*/
int exitConfigure(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	client->commandMask = ENABLE;
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose:  
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ August 1, 2008   
 * -------------------------------------------------------------------------------------*/
int endConfigure(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	client->commandMask = ENABLE;
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Change the cli context back to CONFIGURE from ROUTER_BGP
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: 
 * Kevin Burnett @ August 1, 2008   
 * -------------------------------------------------------------------------------------*/
int exitRouter(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	client->commandMask = CONFIGURE;
	client->localASNumber = -1;
	launchAllPeers();
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Change the cli context back to CONFIGURE from ROUTER_BGP
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: 
 * Kevin Burnett @ January 1, 2009   
 * -------------------------------------------------------------------------------------*/
int endRouter(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	client->commandMask = ENABLE;
	client->localASNumber = -1;
	launchAllPeers();
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: prints out the current node in the command structure then recurses on all 
 * 	    the children for the node. 
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: none
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int showHelp(commandArgument * ca, clientThreadArguments * client, commandNode * node) {
	int recurse = FALSE;
	if(strcmp(ca->commandArgument, SHOW_HELP_ARGUMENT_SINGLE)==0) {
		recurse = FALSE;
	}
	if(strcmp(ca->commandArgument, SHOW_HELP_ARGUMENT_MULTI)==0) {
		recurse = TRUE;
	}
	printCommandTree(client->socket, node, client->commandMask, 1, recurse);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: prints out the current node in the command structure then recurses on all 
 * 	    the children for the node. 
 * Input: a pointer to a node in the tree and the level. 
 * Output: none
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int printCommandTree(int clientSocket, commandNode * node, int mask, int level, int recurse) {
	int i=0, j=0;
	char line[100];
	for(i=0; i<MAX_SUB_COMMANDS; i++) {
		if(node->subcommands[i]!=NULL && (node->subcommands[i]->commandMask & mask)==mask) {
			memset(line, 0, 100);
			for(j=0; j<level; j++)
				strcat(line, " - ");

			strcat(line, node->subcommands[i]->description);
			strcat(line, " \n");

			sendMessage(clientSocket, line);

			if(recurse==TRUE) {
				printCommandTree(clientSocket, node->subcommands[i], mask, level+1, TRUE);
			}
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This command will create a file with the current configuration of BGPmon.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ September 2, 2008   
 * -------------------------------------------------------------------------------------*/
int copyRunningConfig(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int results = 0;
	char * destination = NULL;
	char * backup = NULL;
    

	if(ca==NULL) {
                backup = LoginSettings.scratchDir;
		destination = LoginSettings.configFile;
		backupConfigFile(destination, backup);
	}
	else {
		destination = ca->commandArgument;
	}

	results = saveConfigFile(destination);
	if(results!=0) {
		sendMessage(client->socket, "failed to copy running config to [%s]\n", destination);
	}
	return results;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Starts the bgpmon shutdown process
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ July 30, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShutdown(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	closeBgpmon(NULL);
	return 0;
}
