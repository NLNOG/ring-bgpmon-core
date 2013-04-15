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
 *  File: queue_commands.c
 * 	Authors: Kevin Burnett
 *
 *  Date: November 26, 2008
 */

// Needed for atoi() 
#include <stdlib.h>
// Needed for memset,strncmp
#include <string.h>
// Needed for strerror(errno)
#include <errno.h>

// Needed for the function definitions
#include "queue_commands.h"
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

/*----------------------------------------------------------------------------------------
 * Purpose: Show information relating to the queues (peer, label, xml)
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int showQueue(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	u_int16_t minWritesLimit = 0, pacingState = 0, currentWritesLimit = 0;
	u_int32_t pacingInterval = 0, bytesUsed = 0, itemsUsed = 0, itemsTotal = 0, readerCount = 0, writerCount = 0;
	float pacingOnThresh = 0, pacingOffThresh = 0, alpha = 0;

	// get the values
	pacingOnThresh = getPacingOnThresh();
	pacingOffThresh = getPacingOffThresh();
	alpha = getAlpha();
	minWritesLimit = getMinimumWritesLimit();
	pacingInterval = getPacingInterval();
	
	// print them out
	sendMessage(client->socket, "pacing on threshold: %f\n", pacingOnThresh);
	sendMessage(client->socket, "pacing off threshold: %f\n", pacingOffThresh);
	sendMessage(client->socket, "alpha : %f\n", alpha);
	sendMessage(client->socket, "minimum writes limit: %d\n", minWritesLimit);
	sendMessage(client->socket, "pacing interval: %d\n", pacingInterval);

	// check to see if this is a specific queue
	if(strcmp(cn->command, "queue")!=0) {
		bytesUsed = getBytesUsed(cn->command);
		itemsUsed = getItemsUsed(cn->command);
		itemsTotal = getItemsTotal(cn->command);
		pacingState = getPacingState(cn->command);
		currentWritesLimit = getCurrentWritesLimit(cn->command);
		readerCount = getReaderCount(cn->command);
		writerCount = getWriterCount(cn->command);

		sendMessage(client->socket, "\ntotal bytes used: %d\n", bytesUsed);
		sendMessage(client->socket, "items used: %d\n", itemsUsed);
		sendMessage(client->socket, "items total: %d\n", itemsTotal);
		sendMessage(client->socket, "pacing state: %d\n", pacingState);
		sendMessage(client->socket, "current writes limit: %d\n",currentWritesLimit);
		sendMessage(client->socket, "reader position: \n");
		sendMessage(client->socket, "readers count: %d\n", readerCount);
		sendMessage(client->socket, "writers count: %d\n", writerCount);
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Set the pacing-on threshold for queues
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int queuePacingOnThresh(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	float pacingOnThresh = atof(ca->commandArgument);
	setPacingOnThresh(pacingOnThresh);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Set the pacing-off threshold
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int queuePacingOffThresh(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	float pacingOffThresh = atof(ca->commandArgument);
	setPacingOffThresh(pacingOffThresh);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: set queue's alpha
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int queueAlpha(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	float alpha = atof(ca->commandArgument);
	setAlpha(alpha);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: set the minimum writes limit
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int queueMinWritesLimit(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	u_int16_t minWritesLimit = atoi(ca->commandArgument);
	setMinimumWritesLimit(minWritesLimit);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: set the pacing interval
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int queuePacingInterval(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	u_int32_t pacingInterval = atoi(ca->commandArgument);
	setPacingInterval(pacingInterval);
	return 0;
}


