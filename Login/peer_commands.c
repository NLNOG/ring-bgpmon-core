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
 *  File: peer_commands.c
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
// needed for time_t functions
#include <time.h>

// Needed for the function definitions
#include "commands.h"
// Provides the definition for clientThreadArguments struct
#include "login.h"
// Provides the definition for commandArgument and commandNode structs
#include "commandprompt.h"
// provides the peer command definitions
#include "peer_commands.h"
// Provides defaults used 
#include "../site_defaults.h"
// needed for TRUE/FALSE definitions
#include "../Util/bgpmon_defaults.h"
// needed for address related functions
#include "../Util/address.h"
// needed for all the peering commands
#include "../Peering/peers.h"
// needed for peering session commands
#include "../Peering/peersession.h"
// needed for all peer group commands
#include "../Peering/peergroup.h"
// needed for peerLabelAction
#include "../Labeling/label.h"
// needed for hexStringToByteArray function
#include "../Config/configfile.h"
// needed for routing table
//#include "../Labeling/rtable.h"


/*----------------------------------------------------------------------------------------
 * Purpose: Looks through the commandArugments and will try to find either the peerId
 * 	or groupId if they don't exist.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * 	int - an integer indicating whethr to create the peer if it is not found
 * Output: The id, which should be greater than 0, or -1 if an error occured
 * Kevin Burnett @ December 23, 2008   
 * -------------------------------------------------------------------------------------*/
void 
getId(commandArgument ** ca, clientThreadArguments * client, commandNode * root, int autoCreatePeer, int *peerId, int *groupId) {
	char * addr = NULL;
	int port = 0;
	*peerId = -1, *groupId = -1;

	// get the address
	addr = (*ca)->commandArgument;
	*ca = (*ca)->nextArgument;

	// check to make sure the address is valid, if valid then it's a peer
	if(checkAddress(addr, ADDR_ACTIVE)==ADDR_VALID) {
		// if the port exists get it, otherwise use default peer port
                // a search for port directly is problematic because the key
                // word port may appear more than once in a command
                // here we want port only if it is directly after the 
                // address
		if(listContainsCommands2(root,"neighbor","port")) {
			port = atoi((*ca)->commandArgument);
			*ca = (*ca)->nextArgument;
		}
		else {
			port = PEER_PORT;
		}

		// get peer id
		*peerId = findPeerID(addr, port);

		// if no peer was found then create peer
		if(*peerId==-1 && autoCreatePeer==TRUE) {
			*peerId = createPeer(addr, port);
			setPeerLocalASNum(*peerId, client->localASNumber);
		}
	}
	else {
		// if not a valid address then assume it's a peer
		*groupId = findPeerGroupID(addr);
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Creates a peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborPeerGroupCreate(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	char * groupName = NULL;
	int groupId = -1;

	groupName = ca->commandArgument;
	groupId = createPeerGroup(groupName);

	return groupId;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Assigns a peer to a peer group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborPeerGroupAssign(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	char * groupName = NULL, * addr = NULL;
	int groupId = -1, peerId = -1, result = 0;

	// get peer id
	addr = ca->commandArgument;
	getId(&ca, client, root, FALSE, &peerId, &groupId);
	
	// if peer isn't found then return an error message
	if(peerId==-1) {
		sendMessage(client->socket, "Unable to find peer [%s].\n", addr);
		return 1;
	}

	// get group id
	groupName = ca->commandArgument;
	groupId = findPeerGroupID(groupName);

	// if peer group isn't found then return an error message
	if(groupId==-1) {
		sendMessage(client->socket, "Unable to find peer group [%s].\n", groupName);
		return 1;
	}

	if(setPeer_GroupName(peerId, groupName))
		sendMessage(client->socket, "unable to assign peer [%s] to peer-group [%s]\n", addr, groupName);

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the local AS number for a neighbor or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborLocalAsNum(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, asNum = 0, result = 0;
	getId(&ca, client, root, TRUE, &peerId, &groupId);
	asNum = atoi(ca->commandArgument);

	if(peerId>=0) {
		result = setPeerLocalASNum(peerId, asNum);
	}
	else if(groupId>=0) {
		result = setPeerGroupLocalASNum(groupId, asNum);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the local BGP ID for a neighbor or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborLocalBGPId(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, result = 1;
	long bgpId = 0;
	getId(&ca, client, root, TRUE, &peerId, &groupId);
	bgpId = inet_addr(ca->commandArgument);

	if(peerId>=0) {
		result = setPeerLocalBGPID(peerId, bgpId);
	}
	return result;
}

/*******************************************************************************
 * Purpose: Function called to update the local BGP Port for a neighbor or 
 *          peer-group
 * Input: commandArgument - A linked list that provides all the parameters the 
 *        users typed in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information
 *                              for the current connection.
 * 	commandNode - A pointer to the current node in the command tree 
 *                    structure.
 * Output:  0 for success or 1 for failure
 * Cathie Olschanowsky @ May 2012
 ******************************************************************************/
int 
cmdNeighborLocalPort(commandArgument * ca, clientThreadArguments * client,
                     commandNode * root)
{
  int peerId = -1, groupId = -1, result = 1;
  int localPort = 0;
  getId(&ca, client, root, TRUE, &peerId, &groupId);
  localPort = atoi(ca->commandArgument);

  if(localPort>=0) {
    result = setPeerLocalPort(peerId, localPort);
  }
  return result;
}

/*******************************************************************************
 * Purpose: Function called to update the local BGP Port for a neighbor or 
 *          peer-group
 * Input: commandArgument - A linked list that provides all the parameters the 
 *        users typed in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information
 *                              for the current connection.
 * 	commandNode - A pointer to the current node in the command tree 
 *                    structure.
 * Output:  0 for success or 1 for failure
 * Cathie Olschanowsky @ May 2012
 ******************************************************************************/
int 
cmdNeighborLocalAddr(commandArgument * ca, clientThreadArguments * client,
                     commandNode * root) {

  int peerId = -1, groupId = -1;
  char *localAddr;
  getId(&ca, client, root, TRUE, &peerId, &groupId);
  localAddr = ca->commandArgument;

  return setPeerLocalAddress(peerId, localAddr);
}


/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the local BGP version for a neighbor or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborLocalBGPVersion(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, version = 0, result = 0;
	getId(&ca, client, root, TRUE, &peerId, &groupId);
	version = atoi(ca->commandArgument);

	if(peerId>=0) {
		result = setPeerLocalBGPVersion(peerId, version);
	}
	else if(groupId>=0) {
		result = setPeerGroupLocalBGPVersion(groupId, version);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the local hold time for a neighbor or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborLocalHoldTime(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, holdtime = 0, result = 0;
	getId(&ca, client, root, TRUE, &peerId, &groupId);
	holdtime = atoi(ca->commandArgument);

	if(peerId>=0) {
		result = setPeerLocalHoldTime(peerId, holdtime);
	}
	else if(groupId>=0) {
		result = setPeerGroupLocalHoldTime(groupId, holdtime);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the remote AS number for a neighbor or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborRemoteAsNum(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, asNum = 0, result = 0;
	getId(&ca, client, root, TRUE, &peerId, &groupId);
	asNum = atoi(ca->commandArgument);

	if(peerId>=0) {
		result = setPeerRemoteASNum(peerId, asNum);
	}
	else if(groupId>=0) {
		result = setPeerGroupRemoteASNum(groupId, asNum);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the remote BGP id number for a neighbor or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborRemoteBGPId(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, result = 1;
	long bgpId = 0;
	getId(&ca, client, root, TRUE, &peerId, &groupId);
	bgpId = inet_addr(ca->commandArgument);

	if(peerId>=0) {
		result = setPeerRemoteBGPID(peerId, bgpId);
	}

	return result;
}

/*******************************************************************************
 * Purpose: Function called to update the remote BGP Port for a neighbor or 
 *          peer-group
 * Input: commandArgument - A linked list that provides all the parameters the 
 *                          users typed in. This list is in the same order as 
 *                          they were typed.
 *        clientThreadArguments - A struct providing the basic address 
 *                                information for the current connection.
 *        commandNode - A pointer to the current node in the command tree 
 *                      structure.
 * Output:  0 for success or 1 for failure
 * Cathie Olschanowsky @ May 2012
 ******************************************************************************/
int
cmdNeighborRemotePort(commandArgument * ca, clientThreadArguments * client,
                      commandNode * root){

  int peerId = -1, groupId = -1, result = 1;
  int remotePort = 0;
  getId(&ca, client, root, TRUE, &peerId, &groupId);
  remotePort = atoi(ca->commandArgument);

  if(remotePort>=0) {
    result = setPeerRemotePort(peerId, remotePort);
  }
  return result;
}

/*******************************************************************************
 * Purpose: Function called to update the local BGP Address for a neighbor or 
 *          peer-group
 * Input: commandArgument - A linked list that provides all the parameters the 
 *        users typed in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information
 *                              for the current connection.
 * 	commandNode - A pointer to the current node in the command tree 
 *                    structure.
 * Output:  0 for success or 1 for failure
 * Cathie Olschanowsky @ May 2012
 ******************************************************************************/
int 
cmdNeighborRemoteAddr(commandArgument * ca, clientThreadArguments * client,
                     commandNode * root) {

  int peerId = -1, groupId = -1;
  char * remoteAddr = NULL;
  getId(&ca, client, root, TRUE, &peerId, &groupId);
  remoteAddr = ca->commandArgument;

  return setPeerRemoteAddress(peerId, remoteAddr);
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the remote BGP version for a neighbor or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborRemoteBGPVersion(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, version = 0, result = 0;
	getId(&ca, client, root, TRUE, &peerId, &groupId);
	version = atoi(ca->commandArgument);

	if(peerId>=0) {
		result = setPeerRemoteBGPVersion(peerId, version);
	}
	else if(groupId>=0) {
		result = setPeerGroupRemoteBGPVersion(groupId, version);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the remote hold time for a neighbor or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborRemoteHoldTime(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, holdtime = 0, result = 0;
	getId(&ca, client, root, TRUE, &peerId, &groupId);
	holdtime = atoi(ca->commandArgument);

	if(peerId>=0) {
		result = setPeerRemoteHoldTime(peerId, holdtime);
	}
	else if(groupId>=0) {
		result = setPeerGroupRemoteHoldTime(groupId, holdtime);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the MD5 password for a peer or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborMD5Password(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, result = 0;
	char * md5 = NULL;
	getId(&ca, client, root, TRUE, &peerId, &groupId);
	md5 = ca->commandArgument;

	if(peerId>=0) {
		result = setPeerMD5Passwd(peerId, md5);
	}
	else if(groupId>=0) {
		result = setPeerGroupMD5Passwd(groupId, md5);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to update the Label Action for a peer or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborLabelAction(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, result = 0, action = NoAction;
	getId(&ca, client, root, TRUE, &peerId, &groupId);

	if(listContainsCommand(root, "NoAction")) {
		action = NoAction;
	} else
	if(listContainsCommand(root, "StoreRibOnly")) {
		action = StoreRibOnly;
	} else
	if(listContainsCommand(root, "Label")) {
		action = Label;
	}

	if(peerId>=0) {
		result = setPeerLabelAction(peerId, action);
	}
	else if(groupId>=0) {
		result = setPeerGroupLabelAction(peerId, action);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to set route refresh to true
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborRouteRefresh(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, result = 0, rrAction = TRUE;
	getId(&ca, client, root, TRUE, &peerId, &groupId);

	if(peerId>=0) {
		result = setPeerRouteRefreshAction(peerId, rrAction);
	} else if(groupId>=0) {
		result = setPeerGroupRouteRefreshAction(peerId, rrAction);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Function called to enable or disable a peer/peer group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 23, 2008
 * -------------------------------------------------------------------------------------*/
int cmdNeighborEnableDisable(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1, result = 0, enableFlag= TRUE;
	getId(&ca, client, root, TRUE, &peerId, &groupId);

	if(listContainsCommand(root, "enable")) {
		enableFlag = TRUE;
	} else if(listContainsCommand(root, "disable")) {
		enableFlag = FALSE;
	} 

	if(peerId>=0) {
		result = setPeerEnabledFlag(peerId, enableFlag);
	} else if(groupId>=0) {
		result = setPeerGroupEnabledFlag(peerId, enableFlag);
	}

	return result;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Loops through all the neighbors and returns information about each
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 1, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdShowRunning(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	cmdShowRunningNeighbor(ca, client, root);
	cmdShowRunningPeerGroup(ca, client, root);
	return 0;
}


/*----------------------------------------------------------------------------------------
 * Purpose: Loops through all the peer groups and returns information about each
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 1, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdShowRunningPeerGroup(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int * peerGroups = NULL;
	int groupCount = 0, i = 0, capCount = 0, j = 0, peerGroupId = 0;
	int peerGroupLocalPort = 0, peerLabelAction = 0, peerRouteRefreshAction = 0, peerEnabledFlag = FALSE;
	int peerGroupRemoteBGPVersion = 0, peerGroupLocalBGPVersion = 0, peerGroupRemoteASNum = 0, peerGroupLocalASNum = 0;
	int peerGroupRemoteHoldTime = 0, peerGroupLocalHoldTime = 0;
	char * peerGroupName = NULL, * peerGroupLocalAddress = NULL, * peerMD5Password = NULL;
	long long peerGroupRemoteBGPID = 0, peerGroupLocalBGPID = 0;
	PeerCapabilityRequirement * rcvCaps = NULL;
	PBgpCapability * annCaps = NULL;
	char * checkValue = NULL;

	if((groupCount=getAllPeerGroupsIDs(&peerGroups))<0) {
		sendMessage(client->socket, "Unable to retrieve the list of peer groups.");
		return 1;
	}

	if(groupCount>0) {
		for(i=0; i<groupCount; i++) {
			peerGroupId = peerGroups[i];

			// get values for a peer group
			peerGroupName = getPeerGroupName(peerGroupId);
			peerMD5Password = getPeerGroupMD5Passwd(peerGroupId);
			peerLabelAction = getPeerGroupLabelAction(peerGroupId);
			peerRouteRefreshAction = getPeerGroupRouteRefreshAction(peerGroupId);
			peerEnabledFlag = getPeerGroupEnabledFlag(peerGroupId);

			peerGroupRemoteBGPID = getPeerGroupRemoteBGPID(peerGroupId);
			peerGroupRemoteASNum = getPeerGroupRemoteASNum(peerGroupId);
			peerGroupRemoteBGPVersion = getPeerGroupRemoteBGPVersion(peerGroupId);
			peerGroupRemoteHoldTime = getPeerGroupRemoteHoldTime(peerGroupId);

			peerGroupLocalAddress = getPeerGroupLocalAddress(peerGroupId);
			peerGroupLocalPort = getPeerGroupLocalPort(peerGroupId);
			peerGroupLocalBGPID = getPeerGroupLocalBGPID(peerGroupId);
			peerGroupLocalASNum = getPeerGroupLocalASNum(peerGroupId);
			peerGroupLocalBGPVersion = getPeerGroupLocalBGPVersion(peerGroupId);
			peerGroupLocalHoldTime = getPeerGroupLocalHoldTime(peerGroupId); 

			// write out values
			sendMessage(client->socket, "peer groups\n");
			sendMessage(client->socket, "\tgroup id: %d\n", peerGroupId);

			sendMessage(client->socket, "\tgroup name: %s\n", peerGroupName);
			sendMessage(client->socket, "\tremote bgp id: %d\n", peerGroupRemoteBGPID);
			sendMessage(client->socket, "\tremote AS number: %d\n", peerGroupRemoteASNum);
			sendMessage(client->socket, "\tremote BGP veresion: %d\n", peerGroupRemoteBGPVersion);
			sendMessage(client->socket, "\tremote hold time: %d\n", peerGroupRemoteHoldTime);

			sendMessage(client->socket, "\n\tlocal address: %s\n", peerGroupLocalAddress);
			sendMessage(client->socket, "\tlocal port: %d\n", peerGroupLocalPort);
			sendMessage(client->socket, "\tlocal bgp id: %d\n", peerGroupLocalBGPID);
			sendMessage(client->socket, "\tlocal as number: %d\n", peerGroupLocalASNum);
			sendMessage(client->socket, "\tlocal BGP version: %d\n", peerGroupLocalBGPVersion);
			sendMessage(client->socket, "\tlocal hold time: %d\n", peerGroupLocalHoldTime);
			if (client->commandMask == ENABLE)
			{
				sendMessage(client->socket, "\n\tMD5 password: %s\n", peerMD5Password);
			}
			else
			{
				sendMessage(client->socket, "\n\tMD5 password: ******\n");
			}
			sendMessage(client->socket, "\tlabel action: ");
			switch(peerLabelAction) {
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
			sendMessage(client->socket, "\troute refresh action: %d\n", peerRouteRefreshAction);

			if(peerEnabledFlag==TRUE)
				sendMessage(client->socket, "\tis enabled: true\n");
			else
				sendMessage(client->socket, "\tis enabled: false\n");

			// peer receive capability
			sendMessage(client->socket, "\n\tpeer group receive capabilities:\n");
			capCount = getPeerGroupReceiveCapabities(peerGroupId, &rcvCaps);
			if(capCount>0) {
				for(j=0; j<capCount; j++) {
					sendMessage(client->socket, "\t\treceive capability %d:\n", j);
					sendMessage(client->socket, "\t\tcode: %c\n", rcvCaps->code);
					sendMessage(client->socket, "\t\taction:");
					switch(rcvCaps->action) {
						case actionRefuse:
							sendMessage(client->socket, "refuse \n");
							break;

						case actionAllow:
							sendMessage(client->socket, "allow \n");
							break;

						case actionRequire:
							sendMessage(client->socket, "require \n");
							break;
					}
					sendMessage(client->socket, "\t\tcheckValueLen: %d\n", rcvCaps->checkValueLen);
					checkValue = byteArrayToHexString(rcvCaps->checkValue, rcvCaps->checkValueLen);
					sendMessage(client->socket, "\t\tcheckValue: %s\n", checkValue);
					sendMessage(client->socket, "\t\tflag: %c\n\n", rcvCaps->flag);
					rcvCaps++;
				}
			}
			else {
				sendMessage(client->socket, "\t\tno receive capabilities\n\n");
			}

			// peer announce capability
			sendMessage(client->socket, "\tpeer group announce capabilities:\n");
			capCount = getPeerGroupAnnounceCapabities(peerGroupId, &annCaps);
			if(capCount>0) {
				for(j=0; j<capCount; j++) {
					sendMessage(client->socket, "\t\tannounce capability %d:\n", j);
					sendMessage(client->socket, "\t\tcode: %c\n", (*annCaps)->code);
					sendMessage(client->socket, "\t\tlength: %d\n", (*annCaps)->length);
					checkValue = byteArrayToHexString((*annCaps)->value, (*annCaps)->length);
					sendMessage(client->socket, "\t\tcheckValue: %s\n\n", checkValue);
					annCaps++;
				}
			}
			else {
				sendMessage(client->socket, "\t\tno announce capabilities\n\n");
			}
		}
	}
	else {
		sendMessage(client->socket, "no peer groups configured\n");
	}

	free(peerGroups);

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Loops through all the neighbors and returns information about each
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 1, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdShowRunningNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int * peers = NULL;
	int peerCount = 0, i = 0, capCount = 0, j = 0, sessionId = 0, peerId = 0, peerRemotePort = 0, peerLocalPort = 0;
	int peerLabelAction = 0, peerRouteRefreshAction = 0, peerEnabledFlag = FALSE;
	int peerRemoteBGPVersion = 0, peerLocalBGPVersion = 0, peerRemoteASNum = 0, peerLocalASNum = 0;
	int peerRemoteHoldTime = 0, peerLocalHoldTime = 0;
	char * peerRemoteAddress = NULL, * peerLocalAddress = NULL, * peerGroupName = NULL;
      	char * peerMD5Password = NULL;
	long long peerRemoteBGPID = 0, peerLocalBGPID = 0;
	PeerCapabilityRequirement * rcvCaps = NULL;
	PBgpCapability * annCaps = NULL;
	char * checkValue = NULL;

	if((peerCount=getAllPeersIDs(&peers))<0) {
		sendMessage(client->socket, "Unable to retrieve the list of peers");
		return 1;
	}

	if(peerCount>0) {
		for(i=0; i<peerCount; i++) {
			peerId = peers[i];
			// get values for a peer
			sessionId = getPeerSessionID(peerId);

			peerGroupName = getPeer_GroupName(peerId);
			peerMD5Password = getPeerMD5Passwd(peerId);
			peerLabelAction = getPeerLabelAction(peerId);
			peerRouteRefreshAction = getPeerRouteRefreshAction(peerId);
			peerEnabledFlag = getPeerEnabledFlag(peerId);

			peerRemoteAddress = getPeerRemoteAddress(peerId);
			peerRemotePort = getPeerRemotePort(peerId);
			peerRemoteBGPID = getPeerRemoteBGPID(peerId);
			peerRemoteASNum = getPeerRemoteASNum(peerId);
			peerRemoteBGPVersion = getPeerRemoteBGPVersion(peerId);
			peerRemoteHoldTime = getPeerRemoteHoldTime(peerId);

			peerLocalAddress = getPeerLocalAddress(peerId);
			peerLocalPort = getPeerLocalPort(peerId);
			peerLocalBGPID = getPeerLocalBGPID(peerId);
			peerLocalASNum = getPeerLocalASNum(peerId);
			peerLocalBGPVersion = getPeerLocalBGPVersion(peerId);
			peerLocalHoldTime = getPeerLocalHoldTime(peerId); 

			// write out values
			sendMessage(client->socket, "peers\n");
			sendMessage(client->socket, "\tpeer id: %d\n", peerId);

			sendMessage(client->socket, "\tremote address: %s\n", peerRemoteAddress);
			sendMessage(client->socket, "\tremote port: %d\n", peerRemotePort);
			sendMessage(client->socket, "\tremote bgp id: %d\n", peerRemoteBGPID);
			sendMessage(client->socket, "\tremote AS number: %d\n", peerRemoteASNum);
			sendMessage(client->socket, "\tremote BGP version: %d\n", peerRemoteBGPVersion);
			sendMessage(client->socket, "\tremote hold time: %d\n", peerRemoteHoldTime);

			sendMessage(client->socket, "\n\tlocal address: %s\n", peerLocalAddress);
			sendMessage(client->socket, "\tlocal port: %d\n", peerLocalPort);
			sendMessage(client->socket, "\tlocal bgp id: %d\n", peerLocalBGPID);
			sendMessage(client->socket, "\tlocal as number: %d\n", peerLocalASNum);
			sendMessage(client->socket, "\tlocal BGP version: %d\n", peerLocalBGPVersion);
			sendMessage(client->socket, "\tlocal hold time: %d\n", peerLocalHoldTime);

			sendMessage(client->socket, "\n\tsession id: %d\n", sessionId);
			sendMessage(client->socket, "\tgroup name: %s\n", peerGroupName);
			if (client->commandMask == ENABLE)
			{
				sendMessage(client->socket, "\tMD5 password: %s\n", peerMD5Password);
			}
			else
			{
				sendMessage(client->socket, "\tMD5 password: ******\n");
			}
			sendMessage(client->socket, "\tlabel action: ");
			switch(peerLabelAction) {
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
			sendMessage(client->socket, "\troute refresh action: %d\n", peerRouteRefreshAction);

			if(peerEnabledFlag==TRUE)
				sendMessage(client->socket, "\tis enabled: true\n");
			else
				sendMessage(client->socket, "\tis enabled: false\n");

			// peer receive capability
			sendMessage(client->socket, "\n\tpeer receive capabilities:\n");
			capCount = getPeerReceiveCapabities(peerId, &rcvCaps);
			if(capCount>0) {
				for(j=0; j<capCount; j++) {
					sendMessage(client->socket, "\t\treceive capability %d:\n", j);
					sendMessage(client->socket, "\t\tcode: %c\n", rcvCaps->code);
					sendMessage(client->socket, "\t\taction:");
					switch(rcvCaps->action) {
						case actionRefuse:
							sendMessage(client->socket, "refuse \n");
							break;

						case actionAllow:
							sendMessage(client->socket, "allow \n");
							break;

						case actionRequire:
							sendMessage(client->socket, "require \n");
							break;
					}
					sendMessage(client->socket, "\t\tcheckValueLen: %d\n", rcvCaps->checkValueLen);
					checkValue = byteArrayToHexString(rcvCaps->checkValue, rcvCaps->checkValueLen);
					sendMessage(client->socket, "\t\tcheckValue: %s\n", checkValue);
					sendMessage(client->socket, "\t\tflag: %c\n\n", rcvCaps->flag);
					rcvCaps++;
				}
			}
			else {
				sendMessage(client->socket, "\t\tno receive capabilities\n\n");
			}

			// peer announce capability
			sendMessage(client->socket, "\tpeer announce capabilities:\n");
			capCount = getPeerAnnounceCapabities(peerId, &annCaps);
			if(capCount>0) {
				for(j=0; j<capCount; j++) {
					sendMessage(client->socket, "\t\tannounce capabilitye %d:\n", j);
					sendMessage(client->socket, "\t\tcode: %c\n", (*annCaps)->code);
					sendMessage(client->socket, "\t\tlength: %d\n", (*annCaps)->length);
					checkValue = byteArrayToHexString((*annCaps)->value, (*annCaps)->length);
					sendMessage(client->socket, "\t\tcheckValue: %s\n\n", checkValue);
					annCaps++;
				}
			}
			else {
				sendMessage(client->socket, "\t\tno announce capabilities\n\n");
			}
		}
	}
	else {
		sendMessage(client->socket, "no peers configured\n");
	}
	
	free(peers);

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Loops through all the neighbors and returns information about each
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 1, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdShowBGPNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int * peers = NULL;
	int peerCount = 0, i = 0, capCount = 0, j = 0, sessionId = 0, peerId = 0, peerRemotePort = 0, peerLocalPort = 0;
	int peerLabelAction = 0, peerRouteRefreshAction = 0;
	int peerRemoteBGPVersion = 0, peerLocalBGPVersion = 0, peerRemoteASNum = 0, peerLocalASNum = 0;
	int peerRemoteHoldTime = 0, peerLocalHoldTime = 0;
	char * peerRemoteAddress = NULL, * peerLocalAddress = NULL, * peerGroupName = NULL;
      	char * peerMD5Password = NULL;
	long long peerRemoteBGPID = 0, peerLocalBGPID = 0;
	PeerCapabilityRequirement * rcvCaps = NULL;
	PBgpCapability * annCaps = NULL;
	char * checkValue = NULL;

	char * peerAddress = NULL;
	int peerPort = PEER_PORT;

	if(ca!=NULL) {
		peerAddress = ca->commandArgument;
		ca = ca->nextArgument;
		if(ca!=NULL) {
			peerPort = atoi(ca->commandArgument);
		}
	}

	if((peerCount=getAllPeersIDs(&peers))<0) {
		sendMessage(client->socket, "Unable to retrieve the list of peers");
		return 1;
	}

	if(peerCount>0) {
		for(i=0; i<peerCount; i++) {
			peerId = peers[i];

			// get values for a peer
			sessionId = getPeerSessionID(peerId);

			if(sessionId!=-1) {
				peerRemoteAddress = getSessionRemoteAddr(sessionId);
				peerRemotePort = getSessionRemotePort(sessionId);

				if(peerAddress==NULL || (strcmp(peerAddress, peerRemoteAddress)==0 && peerPort==peerRemotePort)) {
					peerMD5Password = getSessionMD5Passwd(sessionId);
					peerLabelAction = getSessionLabelAction(sessionId);
					peerRouteRefreshAction = getSessionRouteRefreshAction(sessionId);

					peerRemoteBGPID = getSessionRemoteBGPID(sessionId);
					peerRemoteASNum = getSessionRemoteASNum(sessionId);
					peerRemoteBGPVersion = getSessionRemoteBGPVersion(sessionId);
					peerRemoteHoldTime = getSessionRemoteHoldTime(sessionId);

					peerLocalAddress = getSessionLocalAddr(sessionId);
					peerLocalPort = getSessionLocalPort(sessionId);
					peerLocalBGPID = getSessionLocalBGPID(sessionId);
					peerLocalASNum = getSessionLocalASNum(sessionId);
					peerLocalBGPVersion = getSessionLocalBGPVersion(sessionId);
					peerLocalHoldTime = getSessionLocalHoldTime(sessionId); 

					// write out values
					sendMessage(client->socket, "peer session:\n");
					sendMessage(client->socket, "\tpeer id: %d\n", peerId);

					sendMessage(client->socket, "\tremote address: %s\n", peerRemoteAddress);
					sendMessage(client->socket, "\tremote port: %d\n", peerRemotePort);
					sendMessage(client->socket, "\tremote bgp id: %d\n", peerRemoteBGPID);
					sendMessage(client->socket, "\tremote AS number: %d\n", peerRemoteASNum);
					sendMessage(client->socket, "\tremote BGP version: %d\n", peerRemoteBGPVersion);
					sendMessage(client->socket, "\tremote hold time: %d\n", peerRemoteHoldTime);

					sendMessage(client->socket, "\n\tlocal address: %s\n", peerLocalAddress);
					sendMessage(client->socket, "\tlocal port: %d\n", peerLocalPort);
					sendMessage(client->socket, "\tlocal bgp id: %d\n", peerLocalBGPID);
					sendMessage(client->socket, "\tlocal as number: %d\n", peerLocalASNum);
					sendMessage(client->socket, "\tlocal BGP version: %d\n", peerLocalBGPVersion);
					sendMessage(client->socket, "\tlocal hold time: %d\n", peerLocalHoldTime);

					sendMessage(client->socket, "\n\tsession id: %d\n", sessionId);
					sendMessage(client->socket, "\tgroup name: %s\n", peerGroupName);
					if (client->commandMask == ENABLE)
					{
						sendMessage(client->socket, "\tMD5 password: %s\n", peerMD5Password);
					}
					else
					{
						sendMessage(client->socket, "\tMD5 password: ******\n");
					}
					sendMessage(client->socket, "\tlabel action: ");
					switch(peerLabelAction) {
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
					sendMessage(client->socket, "\troute refresh action: %d\n", peerRouteRefreshAction);

					// peer receive capability
					sendMessage(client->socket, "\n\tpeer receive capabilities:\n");
					capCount = getPeerReceiveCapabities(peerId, &rcvCaps);
					if(capCount>0) {
						for(j=0; j<capCount; j++) {
							sendMessage(client->socket, "\t\treceive capability %d:\n", j);
							sendMessage(client->socket, "\t\tcode: %c\n", rcvCaps->code);
							sendMessage(client->socket, "\t\taction:");
							switch(rcvCaps->action) {
								case actionRefuse:
									sendMessage(client->socket, "refuse \n");
									break;

								case actionAllow:
									sendMessage(client->socket, "allow \n");
									break;

								case actionRequire:
									sendMessage(client->socket, "require \n");
									break;
							}
							sendMessage(client->socket, "\t\tcheckValueLen: %d\n", rcvCaps->checkValueLen);
							checkValue = byteArrayToHexString(rcvCaps->checkValue, rcvCaps->checkValueLen);
							sendMessage(client->socket, "\t\tcheckValue: %s\n", checkValue);
							sendMessage(client->socket, "\t\tflag: %c\n\n", rcvCaps->flag);
							rcvCaps++;
						}
					}
					else {
						sendMessage(client->socket, "\t\tno receive capabilities\n\n");
					}

					// peer announce capability
					sendMessage(client->socket, "\tpeer announce capabilities:\n");
					capCount = getPeerAnnounceCapabities(peerId, &annCaps);
					if(capCount>0) {
						for(j=0; j<capCount; j++) {
							sendMessage(client->socket, "\t\tannounce capability %d:\n", j);
							sendMessage(client->socket, "\t\tcode: %c\n", (*annCaps)->code);
							sendMessage(client->socket, "\t\tlength: %d\n", (*annCaps)->length);
							checkValue = byteArrayToHexString((*annCaps)->value, (*annCaps)->length);
							sendMessage(client->socket, "\t\tcheckValue: %s\n\n", checkValue);
							annCaps++;
						}
					}
					else {
						sendMessage(client->socket, "\t\tno announce capabilities\n\n");
					}

					time_t sessionLastActionTime = getSessionLastActionTime(sessionId);
					int sessionState = getSessionState(sessionId);
					int sessionConnectionRetryCount = getSessionConnectRetryCount(sessionId);
					int sessionMsgCount = getSessionMsgCount(sessionId);
					int sessionDownCount = getSessionDownCount(sessionId);
					time_t sessionLastDownTime = getSessionLastDownTime(sessionId);
					time_t sessionEstablishTime = getSessionEstablishTime(sessionId);
					time_t sessionLastRouteRefreshTime = getSessionLastRouteRefreshTime(sessionId);
					int sessionPrefixCount = getSessionPrefixCount(sessionId);
					int sessionAttributeCount = getSessionAttributeCount(sessionId);

					struct tm * timeinfo;
					timeinfo = localtime ( &sessionLastActionTime );
					sendMessage(client->socket, "\tlast action time: %s", asctime (timeinfo));
					sendMessage(client->socket, "\tstate: %d\n", sessionState);
					sendMessage(client->socket, "\tconnection retry count: %d\n", sessionConnectionRetryCount);
					sendMessage(client->socket, "\tmessage count: %d\n", sessionMsgCount);
					sendMessage(client->socket, "\tdown count: %d\n", sessionDownCount);
					sendMessage(client->socket, "\tlast down time: ");
					if(sessionLastDownTime>0) {
						timeinfo = localtime ( &sessionLastDownTime);
						sendMessage(client->socket, "%s", asctime (timeinfo));
					}
					else {
						sendMessage(client->socket, "\n");
					}
					timeinfo = localtime(&sessionEstablishTime);
					sendMessage(client->socket, "\testablish time: %s", asctime (timeinfo));
					timeinfo = localtime(&sessionLastRouteRefreshTime);
					sendMessage(client->socket, "\tlast route refresh time: %s", asctime (timeinfo));
					sendMessage(client->socket, "\tprefix count: %d\n", sessionPrefixCount);
					sendMessage(client->socket, "\tattribute count: %d\n\n", sessionAttributeCount);
				}
			}
			else {
				sendMessage(client->socket, "no active peers\n");
			}
		}
	}
	else {
		sendMessage(client->socket, "no peers configured\n");
	}
	
	free(peers);

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: adds a generic announce capability to a peer or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 30, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdNeighborAnnounce(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1; 
	getId(&ca, client, root, TRUE, &peerId, &groupId);

	u_char * value = NULL;
	u_char code = '0';
	u_char * p_code = &code;
	int len = 0;

	memcpy((void*)p_code, (void *)ca->commandArgument, 1);
	ca = ca->nextArgument;
	ca = ca->nextArgument; // ignore length so it can be calculated and more accurate
	value = hexStringToByteArray(ca->commandArgument);
	if(value==NULL) {
		sendMessage(client->socket, "invalid value [%s]\n", ca->commandArgument);
		return 1;
	}
	len = strlen((char*)value);
	
	if(peerId>=0) {
		if(peerAddAnnounceCapability(peerId, code, len, value))
			sendMessage(client->socket, "error adding generic announce capability to a peer\n");
	}
	else if(groupId>=0) {
		if(peerGroupAddAnnounceCapability(groupId, code, len, value))
			sendMessage(client->socket, "error adding generic announce capability to a peer-group\n");
	}
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: adds an ipv4/ipv6 unicast/multicast announce capability to a peer or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 30, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdNeighborAnnounceIpv4Ipv6UnicastMulticast(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1;
	getId(&ca, client, root, TRUE, &peerId, &groupId);

	char * value = NULL;
	u_char * u_value = NULL;
	u_char code = '1';
	int length = 4;

	if(listContainsCommand(root, "ipv4") && listContainsCommand(root, "unicast")) {
		value = "00010001";
	} 
	else if(listContainsCommand(root, "ipv4") && listContainsCommand(root, "multicast")) {
		value = "00010002";
	}
	else if(listContainsCommand(root, "ipv6") && listContainsCommand(root, "unicast")) {
		value = "00020001";
	}
	else if(listContainsCommand(root, "ipv6") && listContainsCommand(root, "multicast")) {
		value = "00020002";
	}
	u_value = hexStringToByteArray(value);

	if(peerId>=0) {
		if(!listContainsCommand(root, "no")) {
			// add announce capability
			if(peerAddAnnounceCapability(peerId, code, length, u_value)) {
				sendMessage(client->socket, "error adding announce capabilites to a peer\n");
				return 1;
			}
		}
		else {
			// remove announce capability
			if(peerRemoveAnnounceCapability(peerId, code, length, u_value)) {
				sendMessage(client->socket, "error removing announce capabilites from a peer\n");
				return 1;
			}
		}

	}
	else if(groupId>=0) {
		if(!listContainsCommand(root, "no")) {
			// add announce capability
			if(peerGroupAddAnnounceCapability(groupId, code, length, u_value)) {
				sendMessage(client->socket, "error adding announce capabilites to a peer-group\n");
				return 1;
			}
		}
		else {
			// remove announce capability
			if(peerGroupRemoveAnnounceCapability(groupId, code, length, u_value)) {
				sendMessage(client->socket, "error removing announce capabilites from a peer-group\n");
				return 1;
			}
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: adds a generic receive capability to a peer or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 31, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdNeighborReceive(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1; 
	getId(&ca, client, root, TRUE, &peerId, &groupId);

	u_char * value = NULL;
	u_char code = 0;
	u_char * p_code = &code;
	int len = 0;
	int action = 0;

	if(listContainsCommand(root, "require")) {
		action = actionRequire;
	}
	if(listContainsCommand(root, "refuse")) {
		action = actionRefuse;
	}
	if(listContainsCommand(root, "allow")) {
		action = actionAllow;
	}

	memcpy((void*)p_code, (void *)ca->commandArgument, 1);
	ca = ca->nextArgument;
	len = atoi(ca->commandArgument);
	ca = ca->nextArgument; 
	if(len>0) {
		value = hexStringToByteArray(ca->commandArgument);
		if(value==NULL) {
			sendMessage(client->socket, "invalid value [%s]\n", ca->commandArgument);
			return 1;
		}
	}
	
	if(peerId>=0) {
		if(peerAddReceiveCapability(peerId, action, code, len, value))
			sendMessage(client->socket, "error adding generic announce capability to a peer\n");
	}
	else if(groupId>=0) {
		if(peerGroupAddReceiveCapability(groupId, action, code, len, value))
			sendMessage(client->socket, "error adding generic announce capability to a peer-group\n");
	}
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: adds an ipv4 unicast receive capability to a peer or peer-group
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ December 31, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdNeighborReceiveIpv4Ipv6UnicastMulticast(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1;
	getId(&ca, client, root, TRUE, &peerId, &groupId);

	char * value = NULL;
	u_char * u_value = NULL;
	u_char code = '1';
	int length = 4;
	int action = 0;

	// set the action
	if(listContainsCommand(root, "require")) {
		action = actionRequire;
	}
	if(listContainsCommand(root, "refuse")) {
		action = actionRefuse;
	}
	if(listContainsCommand(root, "allow")) {
		action = actionAllow;
	}

	// set the value
	if(listContainsCommand(root, "ipv4") && listContainsCommand(root, "unicast")) {
		value = "00010001";
	} 
	else if(listContainsCommand(root, "ipv4") && listContainsCommand(root, "multicast")) {
		value = "00010002";
	}
	else if(listContainsCommand(root, "ipv6") && listContainsCommand(root, "unicast")) {
		value = "00020001";
	}
	else if(listContainsCommand(root, "ipv6") && listContainsCommand(root, "multicast")) {
		value = "00020002";
	}
	u_value = hexStringToByteArray(value);

	if(peerId>=0) {
		if(!listContainsCommand(root, "no")) {
			// add announce capability
			if(peerAddReceiveCapability(peerId, action, code, length, u_value)) {
				sendMessage(client->socket, "error adding receive capabilites to a peer\n");
				return 1;
			}
		}
		else {
			// remove announce capability
			if(peerRemoveReceiveCapability(peerId, action, code, length, u_value)) {
				sendMessage(client->socket, "error removing announce capabilites from a peer\n");
				return 1;
			}
		}

	}
	else if(groupId>=0) {
		if(!listContainsCommand(root, "no")) {
			// add announce capability
			if(peerGroupAddReceiveCapability(groupId, action, code, length, u_value)) {
				sendMessage(client->socket, "error adding receive capabilites to a peer-group\n");
				return 1;
			}
		}
		else {
			// remove announce capability
			if(peerGroupRemoveReceiveCapability(groupId, action, code, length, u_value)) {
				sendMessage(client->socket, "error removing announce capabilites from a peer-group\n");
				return 1;
			}
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Marks a neighbor for deletion
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ January 1, 2009
 * -------------------------------------------------------------------------------------*/
int cmdNoNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1;
	getId(&ca, client, root, FALSE, &peerId, &groupId);

	if(peerId>=0) {
		if(deletePeer(peerId)==-1) {
			sendMessage(client->socket, "error deleting peer\n");
			return 1;
		}
	}
	else if(groupId>=0) {
		if(deletePeerGroup(groupId)==-1) {
			sendMessage(client->socket, "error deleting peer group\n");
			return 1;
		}
	} 
	else {
		sendMessage(client->socket, "no peer or peer-group was found\n");
		return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Forces a neighbor to reconnect
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ January 1, 2009
 * -------------------------------------------------------------------------------------*/
int cmdClearNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * root) {
	int peerId = -1, groupId = -1;
	getId(&ca, client, root, FALSE, &peerId, &groupId);
	
	if(peerId>=0) {
		if(reconnectPeer(peerId)==-1) {
			sendMessage(client->socket, "error marking peer for reconnection\n");
			return 1;
		}
	}
	else {
		sendMessage(client->socket, "unable to find peer\n");
		return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: show bgp routes which stored in rtable.c
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Mikhail Strizhov @ Febrary 1, 2010
 * -------------------------------------------------------------------------------------*/

int 
cmdShowBGPRoutes(commandArgument * ca, clientThreadArguments * client, 
                 commandNode * root) {

  int i = 0, showcount = 0;
  u_int32_t j = 0;
  int establishedSessions[MAX_SESSION_IDS];
  int establishedSessionCount;
  PrefixNode    *prefixNode;
  PrefixNode    *nextPrefixNode;

  establishedSessionCount = 0;
  memset(establishedSessions, 0, sizeof(int)*MAX_SESSION_IDS);
	
  char * msg;
  char * aspath;
  char * prefixaddr;
  char * peerAddress = NULL;

  int ASLen=0;

  if(ca!=NULL) {
    peerAddress = ca->commandArgument;
  }

  // get the established session count and list
  for(i=0; i<MAX_SESSION_IDS; i++ ){
    // check Sessions and check Sessions trigger = established or mrtestablished
    if( Sessions[i] != NULL && isSessionEstablished(Sessions[i]->sessionID) == TRUE ){
      establishedSessions[establishedSessionCount] = Sessions[i]->sessionID;
      establishedSessionCount++;
    }
  }

  for (i=0; i<establishedSessionCount; i++){
    Session_structp session = getSessionByID(establishedSessions[i]);
    if (session){
			
      if(peerAddress == NULL || (strcmp(getSessionRemoteAddr(establishedSessions[i]),peerAddress)==0)){

        //sendMessage(client->socket, "Session is %d\n", establishedSessions[i]);
			
        // 2 or 4 bytes AS 
        ASLen = session->fsm.ASNumlen;

        sendMessage(client->socket, "%-44s%-44s%-6s%s","Network","Next Hop",
                                    "ASLen","AS Path\n");
        for( j=0; j < session->prefixTable->tableSize; j++ ){
          if (session->prefixTable->prefixEntries[j].node != NULL){
            prefixNode = session->prefixTable->prefixEntries[j].node;
            while (prefixNode != NULL){
						
              prefixaddr = printPrefix(&(prefixNode->keyPrefix));
              sendMessage(client->socket, "%-44s", prefixaddr);
              free(prefixaddr);
			
              sendMessage(client->socket, "%-44s",
                          getSessionRemoteAddr(establishedSessions[i]));
              sendMessage(client->socket, "%-6d", ASLen);
					
              // check if we have 2 byte lenght of as path
              if (prefixNode->dataAttr->asPath->asPathData.data[0] & 0x10 ){
                aspath = printASPath(prefixNode->dataAttr->asPath->asPathData.data+4,
                                     ASLen);
              } else {
                aspath = printASPath(prefixNode->dataAttr->asPath->asPathData.data+3,
                                      ASLen);
              }
              sendMessage(client->socket, "%s\n", aspath);
              free(aspath);

              nextPrefixNode = prefixNode->next;
              prefixNode = nextPrefixNode;
              showcount++;
              if (showcount > 30) {
                sendMessage(client->socket,
                            "\n\nPress ENTER to see more or Q to leave: ");
                msg = (char *)malloc(sizeof(char));
                getMessage(client->socket, msg, 5);
                if (strcmp(msg,"q")==0 || strcmp(msg,"Q")==0){
                  free(msg);
                  return 0;
                } else {
                  showcount = 0;
                }
                free(msg);
              }
            }
          }
        }
      }
    }// session end
  }
return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: show AS path for entered prefix
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Mikhail Strizhov @ September 5, 2010
 * -------------------------------------------------------------------------------------*/

int cmdShowBGProutesASpath(commandArgument * ca, clientThreadArguments * client, commandNode * root) {

	int i = 0;
	u_int32_t j = 0;
	int establishedSessions[MAX_SESSION_IDS];
	int establishedSessionCount;
        PrefixNode    *prefixNode;
        PrefixNode    *nextPrefixNode;

	establishedSessionCount = 0;
	memset(establishedSessions, 0, sizeof(int)*MAX_SESSION_IDS);
	
	char * aspath;
	char * prefixaddr = NULL;
	char * tempprefixaddr = NULL;
	char * peerAddress = NULL;

	int ASLen=0;

	// look for arguments, there should be two args
	if (ca==NULL)
	{
		sendMessage(client->socket, "Please enter correct arguments\n");
		return 1;
	}
	else
	{
		peerAddress = ca->commandArgument;
	}

	ca = ca->nextArgument;
	if(ca==NULL)
	{
		sendMessage(client->socket, "Please enter correct arguments\n");
		return 1;
	}
	else
	{
		prefixaddr = ca->commandArgument;
	}



	// get the established session count and list
	for(i=0; i<MAX_SESSION_IDS; i++ )
	{
		// check Sessions and check Sessions trigger = established or mrtestablished
		if( Sessions[i] != NULL && isSessionEstablished(Sessions[i]->sessionID) == TRUE )
		{
			establishedSessions[establishedSessionCount] = Sessions[i]->sessionID;
			establishedSessionCount++;
		}
	}

	for (i=0; i<establishedSessionCount; i++)
	{
		Session_structp session = getSessionByID(establishedSessions[i]);
		if (session)
		{
			
			if( (strcmp(getSessionRemoteAddr(establishedSessions[i]),peerAddress)==0))
			{

			//sendMessage(client->socket, "Session is %d\n", establishedSessions[i]);
			
			// 2 or 4 bytes AS 
			ASLen = session->fsm.ASNumlen;

			for( j=0; j < session->prefixTable->tableSize; j++ ) 
			{
				if (session->prefixTable->prefixEntries[j].node != NULL) 
				{
					prefixNode = session->prefixTable->prefixEntries[j].node;
					while (prefixNode != NULL) 
					{
						// get prefix name
						tempprefixaddr = printPrefix(&(prefixNode->keyPrefix));

						if( (strcmp(tempprefixaddr,prefixaddr)==0))
						{
							sendMessage(client->socket, "Network\t\tNext Hop\tASLen\tAS Path\n");
							sendMessage(client->socket, "%s\t", tempprefixaddr);
						
							sendMessage(client->socket, "%s\t", getSessionRemoteAddr(establishedSessions[i]));
							sendMessage(client->socket, "%d\t", ASLen);
					
							// check if we have 2 byte lenght of as path
							if (prefixNode->dataAttr->asPath->asPathData.data[0] & 0x10 )
							{
								aspath = printASPath(prefixNode->dataAttr->asPath->asPathData.data+4, ASLen);
							}
							else
							{
								aspath = printASPath(prefixNode->dataAttr->asPath->asPathData.data+3, ASLen);
							}
							sendMessage(client->socket, "%s\n", aspath);
							free(aspath);
						}
						// free prefix memory
						free(tempprefixaddr);

						nextPrefixNode = prefixNode->next;
						prefixNode = nextPrefixNode;
					}
				}
			}
			}
		}// session end
		
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: show all AS paths for prefix
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Mikhail Strizhov @ September 5, 2010
 * -------------------------------------------------------------------------------------*/

int 
cmdShowBGPprefix(commandArgument * ca, clientThreadArguments * client, 
                 commandNode * root) {
  int i = 0;
  u_int32_t j = 0;
  int establishedSessions[MAX_SESSION_IDS];
  int establishedSessionCount;
  PrefixNode    *prefixNode;
  PrefixNode    *nextPrefixNode;

  establishedSessionCount = 0;
  memset(establishedSessions, 0, sizeof(int)*MAX_SESSION_IDS);
	
  char * aspath;
  char * prefixaddr = NULL;
  PAddress * tempprefixaddr = NULL;

  int ASLen=0;
  int found = 0;

  // look for arguments, there should be two args
  if (ca==NULL) {
    sendMessage(client->socket, "Please enter correct arguments\n");
    return 1;
  } else {
    prefixaddr = ca->commandArgument;
  }

  // create an address object for the prefix
  PAddress *prefix = stringToPrefix(prefixaddr);

  // get the established session count and list
  for(i=0; i<MAX_SESSION_IDS; i++ ) {
    // check Sessions and check Sessions trigger = established or mrtestablished
    if( Sessions[i] != NULL && isSessionEstablished(Sessions[i]->sessionID) == TRUE ){
      establishedSessions[establishedSessionCount] = Sessions[i]->sessionID;
      establishedSessionCount++;
    }
  }

  sendMessage(client->socket, "Network\t\tNext Hop\tASLen\tAS Path\n");
  for (i=0; i<establishedSessionCount; i++) {
    Session_structp session = getSessionByID(establishedSessions[i]);
    if (session) {
      found = 0;
      // 2 or 4 bytes AS 
      ASLen = session->fsm.ASNumlen;

      for( j=0; j < session->prefixTable->tableSize; j++ ){
        if (session->prefixTable->prefixEntries[j].node != NULL){
          prefixNode = session->prefixTable->prefixEntries[j].node;
          while (prefixNode != NULL){ 
            tempprefixaddr = &(prefixNode->keyPrefix.addr);
            if(prefixesEqual(tempprefixaddr,prefix)){
              found = 1;
              sendMessage(client->socket, "%s\t", tempprefixaddr);
	
              sendMessage(client->socket, "%s\t", 
                          getSessionRemoteAddr(establishedSessions[i]));
              sendMessage(client->socket, "%d\t", ASLen);
				
              // check if we have 2 byte lenght of as path
              if (prefixNode->dataAttr->asPath->asPathData.data[0] & 0x10 ) {
                aspath = printASPath(prefixNode->dataAttr->asPath->asPathData.data+4, ASLen);
              } else {
                aspath = printASPath(prefixNode->dataAttr->asPath->asPathData.data+3, ASLen);
              }
              sendMessage(client->socket, "%s\n", aspath);
              free(aspath);
            }

            nextPrefixNode = prefixNode->next;
            prefixNode = nextPrefixNode;
          }
        }
      } // table for loop
	
	if (found != 1)
		{
			sendMessage(client->socket, "%s\t", prefixaddr);
			sendMessage(client->socket, "%s\t", getSessionRemoteAddr(establishedSessions[i]));
			sendMessage(client->socket, "%d\t", ASLen);
			sendMessage(client->socket, "%s\n", "N/A");
		}
		
		}// session end
		
	}

	return 0;
}


