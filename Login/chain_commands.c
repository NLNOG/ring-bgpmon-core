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
 *  File: chain_commands.c
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
// needed for address related functions
#include "../Util/address.h"

/*----------------------------------------------------------------------------------------
 * Purpose:  This function is called to edit an exiting or create a new chain.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ August 4, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdChain(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	char * addr = NULL, * pch = NULL;

	// CLIENTS_UPDATES_LISTEN_PORT is also the default connection port for chains
	int uport = CLIENTS_UPDATES_LISTEN_PORT, rport = CLIENTS_RIB_LISTEN_PORT, enable = -1, retryInterval = -1;
	int chainId = 0;

	// get address
	addr = ca->commandArgument;
	ca = ca->nextArgument;

	// get the arguments
	while(ca!=NULL) {
		if(strstr(ca->commandArgument, "uport:")!=NULL) {
			// get the update port
			pch = strchr(ca->commandArgument, ':');
			uport = atoi(pch + 1);
		} else if(strstr(ca->commandArgument, "rport:")!=NULL) {
			// get the rib port
			pch = strchr(ca->commandArgument, ':');
			rport = atoi(pch + 1);
		} if(strstr(ca->commandArgument, "retry:")!=NULL) {
			// look for retry interval
			pch = strchr(ca->commandArgument, ':');
			retryInterval = atoi(pch + 1);
		} else if(strcmp(ca->commandArgument, "enable")==0) {
			// look for enable command
			enable = TRUE;
		}
		else if(strcmp(ca->commandArgument, "disable")==0) {
			// look for disable command
			enable = FALSE;
		}

		ca = ca->nextArgument;
	}


	if(checkAddress(addr, ADDR_ACTIVE)==ADDR_VALID && !checkPort(uport) && !checkPort(rport)) {
		// determine if we edit or create the chain
		if((chainId=getChainID(addr, uport, rport))>=0) {
			// update an existing chain

			if(enable!=-1) {
				// update the chains enable/disable status
				if(enable==TRUE) {
					enableChain(chainId);
				}
				if(enable==FALSE) {
					disableChain(chainId);
				}
			}

			if(retryInterval!=-1) {
				// update the chains retry interval
				setChainConnectionRetryInterval(chainId, retryInterval);
			}
		}
		else {	
			// create a new chain

			// default the enable status
			if(enable==-1) {
				enable = CHAINS_ENABLED;
			}
			// default the retry interval
			if(retryInterval==-1) {
				retryInterval = CHAINS_RETRY_INTERVAL;
			}

			// create the chain
			createChain(addr, uport, rport, enable, retryInterval);
		}
	}
	else {
		sendMessage(client->socket, "The address [%s], update port [%d] combination are not valid.\n" , addr, uport, rport);
		return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Deletes an existing chain based on an ID that is pulled out of the
 * 	commandArguments.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ August 4, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdNoChain(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int chainId = 0;
	char * chainAddress = NULL;
	int chainUPort = 0;
	int chainRPort = 0;

	chainAddress = ca->commandArgument;
	if(ca->nextArgument!=NULL) {
		ca = ca->nextArgument;
		chainUPort = atoi(ca->commandArgument);
		ca = ca->nextArgument;
		chainRPort = atoi(ca->commandArgument);
	}
	else {
		chainUPort = CLIENTS_UPDATES_LISTEN_PORT;
		chainRPort = CLIENTS_RIB_LISTEN_PORT;
	}

	if((chainId = getChainID(chainAddress, chainUPort, chainRPort))<0) {
		sendMessage(client->socket, "Chain %s update port %d rib port %d wasn't found.\n" , chainAddress, chainUPort, chainRPort);
		return 1;
	}

	deleteChain(chainId);

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Loops through and will print out the chains
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ August 4, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdShowChain(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int *chainID, len = 0;
	char * chainAddress = NULL;
	int chainUPort = 0, chainRPort = 0;
	int chainEnabled = FALSE;
	int retryInterval = 0;
	unsigned int i = 0;

	if((len=getActiveChainsIDs(&chainID))<0) {
		log_warning("Unable to get list of chains.");
		return 1;
	}

	if(len>0) {
		sendMessage(client->socket, "ID\taddress\t\tuport\trport\tretry\tstatus\n");
		for(i=0; i<len; i++) {
			chainAddress = getChainAddress(chainID[i]);
			chainUPort = getUChainPort(chainID[i]);
			chainRPort = getRChainPort(chainID[i]);
			chainEnabled = isChainEnabled(chainID[i]);
			retryInterval = getChainConnectionRetryInterval(chainID[i]);

			sendMessage(client->socket, "%d\t%s\t%d\t%d\t\t%d", chainID[i], chainAddress, chainUPort, chainRPort, retryInterval);
			if(chainEnabled==TRUE)
				sendMessage(client->socket, "enabled\n");
			else
				sendMessage(client->socket, "disabled\n");
		}
	}
	else {
		sendMessage(client->socket, "No active chains.\n");
	}

	free(chainAddress);

	return 0;
}


