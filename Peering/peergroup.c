/* 
 * 	Copyright (c) 2010 Colorado State University
 * 
 *	Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
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
 *  	File: peergroup.c
 *  	Authors:  He Yan, Dan Massey
 *  	Date: Oct 13, 2008 
 */

/* externally visible structures and functions for peers */
#include "peers.h"
/* externally visible structures and functions for peer groups */
#include "peergroup.h"
/* required for logging functions */
#include "../Util/log.h"
/* needed for reading and saving configuration */
#include "../Config/configdefaults.h"
#include "../Config/configfile.h"

/* needed for ADDR_MAX_CHARS and MAX_PEER_IDS */ 
#include "../site_defaults.h"

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
/* needed for generatePeeringString */
#include "../XML/xml.h"
/* needed for createLabelingStruct*/
#include "../Labeling/label.h"
/* needed for bgp states*/
#include "bgpstates.h"
/* needed for LOG_INFO*/
#include <syslog.h>




//#define DEBUG

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the peer groups.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int initPeerGroupsSettings()
{
	// initialize the list of peer groups
	int i;
	for( i = 0; i < MAX_PEER_GROUP_IDS; i++)
		PeerGroups[i] = NULL;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Read the peer groups settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readPeerGroupsSettings()
{	
	debug(__FUNCTION__, "readPeerGroupsSettings" );
	int 	err = 0;	// indicates whether read was successful
	int 	valid;		// is the current peer configuration valid
	char	*name = NULL; // group's name
	int 	result; 	// indicates whether XML read was successful

	// get the count of peer groups
	int peerGroupsCount = getConfigListCount(XML_PEER_GROUPS_PATH);
#ifdef DEBUG
	debug(__FUNCTION__, "found %d configured peer groups", peerGroupsCount);
#endif

	// check if the count of peers exceeds MAX_PEER_IDS. If so, trim it.
	if(peerGroupsCount > MAX_PEER_GROUP_IDS)
	{
		peerGroupsCount = MAX_PEER_GROUP_IDS;
		log_warning("Configured peer groups(%d) exceeds maximum allowable number of peers groups(%d).", peerGroupsCount, MAX_PEER_GROUP_IDS);
		log_warning("using only first %d configured peer groups", MAX_PEER_GROUP_IDS);	
	}

	// read each peer group's configuration and create the peer group structure
	int i;
	for(i = 0; i < peerGroupsCount; i++)
	{
		PConfiguration peerConf;
		valid = readPeerConf( &peerConf, XML_PEER_GROUPS_PATH, i );

		// read group name
		name = NULL;
		result = getConfigValueFromListAsString(&name, XML_PEER_GROUPS_PATH, XML_PEER_GROUP_NAME_TAG, i, PEER_GROUP_MAX_CHARS);
		if ( ( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) ) 
		{
			log_warning("Invalid configuration of peer group %d name.", i);
			valid = FALSE;
		}
		
		// if configuration is valid, create peer group struct
		if (valid == TRUE)
		{
			//create the peer group structure
			int newPeerGroupID = createPeerGroupStruct(peerConf, name);
			if (newPeerGroupID == -1) 
			{ 
				err = 1;
				log_warning("Unable to create structure for peer group %d", i);
			}
			else
			{
				log_msg("created new peer group with id (%d)", newPeerGroupID);
			#ifdef DEBUG
				printPeerGroupSettings(newPeerGroupID);
			#endif
			}
		}
		else
			err = 1;

		// free name if defined
		if (name != NULL) 
			free(name); 		
	}
	return err;

}

/*--------------------------------------------------------------------------------------
 * Purpose: Save the peer groups settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int savePeerGroupsSettings()
{	
	int err = 0;
	int i;

	// open the peer groups list
	if (openConfigElement(XML_PEER_GROUP_LIST_TAG)) 
	{
		err = 1;
		log_warning("Failed to save peer groups list tag to config file.");
	}

	for(i = 0; i < MAX_PEER_GROUP_IDS; i++)
	{
		if( PeerGroups[i] != NULL && i > 0) // skip default group
		{
			// open a peer group
			if (openConfigElement(XML_PEER_GROUP_TAG)) 
			{
				err = 1;
				log_warning("Failed to save peer group tag to config file.");
			}

			// write peer group's name
			if ( setConfigValueAsString(XML_PEER_GROUP_NAME_TAG, PeerGroups[i]->name) )
			{
				err = 1;
				log_warning("Failed to save peer group's name to config file.");
			}			
			// write a peer group
			err = saveOnePeerSetting(PeerGroups[i]->configuration);
			
			// close a peer group
			if (closeConfigElement(XML_PEER_GROUP_TAG)) 
			{
				err = 1;
				log_warning("Failed to save peer group tag to config file.");
			}
		}
	}

	// close the peer groups list
	if (closeConfigElement(XML_PEER_GROUP_LIST_TAG)) 
	{
		err = 1;
		log_warning("Failed to save peer groups list tag to config file.");
	}
	return err;

}

/*--------------------------------------------------------------------------------------
 * Purpose: print the peer group settings
 * Input:  the peer group ID
 * Output: NULL
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void printPeerGroupSettings(int peerGroupID)
{	
	debug(__FUNCTION__, "Peer Group %d: name is %s", peerGroupID, getPeerGroupName(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: Local Addr: %s", peerGroupID, getPeerGroupLocalAddress(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: Local port set to %d", peerGroupID, getPeerGroupLocalPort(peerGroupID));	
	debug(__FUNCTION__, "Peer Group %d: Local BGP version set to %d", peerGroupID, getPeerGroupLocalBGPVersion(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: Local BGP AS number set to %d", peerGroupID, getPeerGroupLocalASNum(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: Local BGP hold time set to %d", peerGroupID, getPeerGroupLocalHoldTime(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: Local BGP ID set to %d", peerGroupID, getPeerGroupLocalBGPID(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: remote BGP version set to %d", peerGroupID, getPeerGroupRemoteBGPVersion(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: remote BGP AS number set to %d", peerGroupID, getPeerGroupRemoteASNum(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: remote min hold time set to %d", peerGroupID, getPeerGroupRemoteHoldTime(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: remote BGP ID set to %d", peerGroupID, getPeerGroupRemoteBGPID(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: MD5 passwd: %s", peerGroupID, getPeerGroupMD5Passwd(peerGroupID) );
	debug(__FUNCTION__, "Peer Group %d: label action set to %d", peerGroupID, getPeerGroupLabelAction(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: route refresh action set to %d", peerGroupID, getPeerGroupRouteRefreshAction(peerGroupID));
	debug(__FUNCTION__, "Peer Group %d: group ID set to %s", peerGroupID, PeerGroups[peerGroupID]->configuration->groupID);
	if(getPeerGroupEnabledFlag(peerGroupID) == TRUE) 
		debug(__FUNCTION__, "Peer Group %d is enabled", peerGroupID);
	else
		debug(__FUNCTION__, "Peer Group %d is disabled", peerGroupID);
}

/*--------------------------------------------------------------------------------------
 * Purpose: find ID of a peer group given group name
 * Input: group name in string
 * Output: ID of the peer group
 * Note: If the peer group with specified name is not exsiting, return -1. 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findPeerGroupID(char *groupName)
{
	int i;
	for(i = 0; i < MAX_PEER_GROUP_IDS; i++)
	{
		if( PeerGroups[i] != NULL && !strcmp(PeerGroups[i]->name, groupName) )
			return PeerGroups[i]->ID;
	}
	return -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: create the default peer group
 * Input:
 * Output: pointer to peer configuration or NULL if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createDefaultPeerGroup()
{
	PConfiguration peerConf = malloc( sizeof( struct ConfigurationStruct ));
	if( peerConf == NULL ) 
	{
		log_err( "createDefaultPeerGroup: malloc failed" );
		return -1;
	}
	memset( peerConf, 0, sizeof( struct ConfigurationStruct ));
	strcpy( peerConf->localSettings.addr, MONITOR_ADDRESS );
	peerConf->localSettings.port = MONITOR_PORT;
	peerConf->localSettings.AS2 = 0;
	peerConf->localSettings.BGPVer = MONITOR_BGP_VERSION;
	peerConf->localSettings.holdtime = MONITOR_ANNOUNCE_HOLDTIME;
	peerConf->localSettings.BGPID = 0;

	peerConf->remoteSettings.port = PEER_PORT;
	peerConf->remoteSettings.AS2 = 0;
	peerConf->remoteSettings.MinHoldtime = PEER_MIN_HOLDTIME;
	peerConf->remoteSettings.BGPVer = PEER_BGP_VERSION;
	peerConf->remoteSettings.BGPID = 0;

	peerConf->enabled = PEERS_ENABLED;
	peerConf->routeRefreshAction = PEERS_RR_ACTION;
	peerConf->labelAction= PEERS_LABEL_ACTION;
	
	peerConf->numOfReceiveCaps = 0;
	peerConf->numOfAnnCaps = 0;
	peerConf->groupID = -1;

	PPeerGroup pg = malloc( sizeof( struct PeerGroupStruct ) );
	if( pg == NULL )
	{
		log_err("createDefaultPeerGroup: malloc failed");
		return -1;
	}

	memset(pg, 0, sizeof( struct PeerGroupStruct) );
	
	// peer group ID
	pg->ID = 0;
	// peer group name
	strcpy(pg->name, DEFAULT_PEER_GROUP_NAME);
	pg->configuration = peerConf;

	PeerGroups[0] = pg;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Create a peer group structure
 * Input:  the peer configuration struture and the group name
 * Output: id of the new peer group or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeerGroupStruct( PConfiguration config, char *name  )
{

	// check if it is existing
	if( findPeerGroupID(name) != -1 )
	{
	 	log_err("Unable to create new peer group as it is already existing");
		return -1;		
	}
	// find a empty slot for this new peer group
	int i;
	for(i = 0; i < MAX_PEER_GROUP_IDS; i++)
	{
		if(PeerGroups[i] == NULL)
			break;
	}

	// if the number of peer groups exceeds MAX_PEER_GROUP_IDS, return -1
	if( i == MAX_PEER_GROUP_IDS )
	{
	 	log_err("Unable to create new peer group, max peer groups exceeded");
		return -1;
	}
	
	// Returns a new Session_structp or NULL if not enough memory.
	PPeerGroup pg = malloc( sizeof( struct PeerGroupStruct ) );
	if( pg == NULL )
	{
		log_err("createPeerGroupStruct: malloc failed");
		return -1;
	}

	memset(pg, 0, sizeof( struct PeerGroupStruct) );
	
	// peer group ID
	pg->ID = i;
	// peer group name
	if( name != NULL )
	{
		strcpy(pg->name, name);
	}
	pg->configuration = config;

	PeerGroups[i] = pg;
 	return i;
	
}


/*--------------------------------------------------------------------------------------
 * Purpose: create a peer group called by CLI.
 * Input:  name - the group name
 * Output: id of the new peer/peergoup or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeerGroup( char *name )
{
	PConfiguration peerConf = createPeerConf( NULL, -1 );
	if( peerConf != NULL)
	{
		int peerGoupID = createPeerGroupStruct( peerConf, name );
		if( peerGoupID == -1 ) 
		{
			log_err( "Creation of peer group structure failed" );
			return -1;
		}
		return peerGoupID;
	}
	else
		return -1;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of IDs of all peer groups
 * Input: a pointer to an array
 * Output: the length of the array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getAllPeerGroupsIDs(int **peerGroupIDs)
{
	int i;
	// get a count of peer groups
	int peerGroupCount = 0;
	for(i = 0; i < MAX_PEER_GROUP_IDS; i++)
	{
		if( PeerGroups[i] != NULL )
			peerGroupCount++;
	}
	
	// allocate memory for the resulting ids
	int *IDs = malloc(sizeof(int)*peerGroupCount);
	if (IDs == NULL) 
	{
		log_warning("getAllPeerGroupsIDs: Unable to allocate memory");
		return 0; 
	}

	// note is possible some peer groups were added since we counted above
	int actualPeerGroups = 0;
	for(i = 0; i < MAX_PEER_GROUP_IDS; i++)
	{
		if( (PeerGroups[i] != NULL ) && (actualPeerGroups <= peerGroupCount ) )
		{
			IDs[actualPeerGroups] = i; 
			actualPeerGroups++;
		}
	}
	*peerGroupIDs = IDs; 
	return actualPeerGroups;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Print Peer Groups Array
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void printAllPeerGroups()
{
	int i;
	for(i = 0; i < MAX_PEER_GROUP_IDS; i++)
	{
		if( PeerGroups[i] != NULL )
		{
			log_msg("PeerGroup %d, name %s", PeerGroups[i]->ID, PeerGroups[i]->name);
		}
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer group's name
 * Input:	the peer group's ID
 * Output: the name of the peer group or NULL
 * Note: The caller doesn't need to free memory.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerGroupName(int peerGroupID)
{	
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupName: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return NULL;
	}
	if( PeerGroups[peerGroupID] == NULL )
	{	
		log_err("getPeerGroupName: couldn't find a peer group with ID:%d", peerGroupID);
		return NULL;
	}
	return PeerGroups[peerGroupID]->name;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote AS number of the peer group
 * Input:   the peer group's ID
 * Output: the peer group's remote AS number
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getPeerGroupRemoteASNum( int peerGroupID )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupRemoteASNum: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return 0;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupRemoteASNum: couldn't find a peer group with ID:%d", peerGroupID);
		return 0;
	}
	// if the remote AS number is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->remoteSettings.AS2 == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->remoteSettings.AS2;
	else
		return PeerGroups[peerGroupID]->configuration->remoteSettings.AS2;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's remote AS number
 * Input:	the peer's ID and AS number
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupRemoteASNum( int peerGroupID, u_int32_t AS )
{	
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("setPeerGroupRemoteASNum: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupRemoteASNum: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if the AS number changed
	if( PeerGroups[peerGroupID]->configuration->remoteSettings.AS2 == AS )
		return 0;
	
	// if changed, set it
	PeerGroups[peerGroupID]->configuration->remoteSettings.AS2 = AS;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's remote BGP version
 * Input:	the peer group's ID
 * Output: the remote BGP Versionn
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupRemoteBGPVersion( int peerGroupID )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupRemoteBGPVersion: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupRemoteBGPVersion: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// if the remote BGP version is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->remoteSettings.BGPVer == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->remoteSettings.BGPVer;
	else
		return PeerGroups[peerGroupID]->configuration->remoteSettings.BGPVer;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's remote BGP version
 * Input:	the peer group's ID and remote BGP version
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupRemoteBGPVersion( int peerGroupID, int version )
{	
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("setPeerGroupRemoteBGPVersion: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupRemoteBGPVersion: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if the supplied remote BGP version is different from current
	if( PeerGroups[peerGroupID]->configuration->remoteSettings.BGPVer== version )
		return 0;
	
	// if different, set it
	PeerGroups[peerGroupID]->configuration->remoteSettings.BGPVer = version;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's remote BGP ID
 * Input:	the peer group's ID
 * Output: the remote BGP ID
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getPeerGroupRemoteBGPID(int peerGroupID)
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupRemoteBGPID: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupRemoteBGPID: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// if the remote BGP ID is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->remoteSettings.BGPID == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0)
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->remoteSettings.BGPID;
	else
		return PeerGroups[peerGroupID]->configuration->remoteSettings.BGPID;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's remote BGP ID
 * Input:	the peer group's ID and the remote BGP ID
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int SetPeerGroupRemoteBGPID( int peerGroupID, long long BGPID )
{	
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("SetPeerGroupRemoteBGPID: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("SetPeerGroupRemoteBGPID: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if the supplied BGP ID is different from current
	if( PeerGroups[peerGroupID]->configuration->remoteSettings.BGPID == BGPID )
		return -1;
	
	// if different, set it
	PeerGroups[peerGroupID]->configuration->remoteSettings.BGPID = BGPID;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's remote hold time
 * Input:	the peer group's ID
 * Output: the remote hold time
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupRemoteHoldTime( int peerGroupID )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupRemoteHoldTime: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupRemoteHoldTime: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// if the remote hold time is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->remoteSettings.MinHoldtime== -1 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->remoteSettings.MinHoldtime;
	else
		return PeerGroups[peerGroupID]->configuration->remoteSettings.MinHoldtime;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's remote hold time
 * Input:	the peer group's ID and remote hold time
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupRemoteHoldTime( int peerGroupID, int holdTime )
{	
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("setPeerGroupRemoteHoldTime: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupRemoteHoldTime: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if the supplied holdtime is different from current
	if( PeerGroups[peerGroupID]->configuration->remoteSettings.MinHoldtime == holdTime )
		return 0;
	
	// if different, set it
	PeerGroups[peerGroupID]->configuration->remoteSettings.MinHoldtime = holdTime;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's MD5 password
 * Input:	the peer group's ID
 * Output: the MD5 password or NULL
 * Note: The caller doesn't need to free memory.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerGroupMD5Passwd(int peerGroupID)
{	
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupMD5Passwd: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return NULL;
	}
	if( PeerGroups[peerGroupID] == NULL )
	{	
		log_err("getPeerGroupMD5Passwd: couldn't find a peer group with ID:%d", peerGroupID);
		return NULL;
	}

	// if the local address is not set, use the value from default group
	char *passwd = NULL;
	if( strlen(PeerGroups[peerGroupID]->configuration->remoteSettings.md5Passwd) == 0 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		passwd = PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->remoteSettings.md5Passwd;
	else
		passwd = PeerGroups[peerGroupID]->configuration->remoteSettings.md5Passwd;
	
	return passwd;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer group's MD5 password
 * Input:	the peer group's ID and the MD5 password in string
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerGroupMD5Passwd( int peerGroupID, char *passwd )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("setPeerGroupMD5Passwd: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}

	// check if the peer group is defined
	if( PeerGroups[peerGroupID] == NULL )
	{
		log_err("setPeerGroupMD5Passwd: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if the supplied address is different from current
	if( strcmp(PeerGroups[peerGroupID]->configuration->remoteSettings.md5Passwd, passwd) == 0 )
		return 0;

	// if different, set it
	strncpy( PeerGroups[peerGroupID]->configuration->remoteSettings.md5Passwd, passwd, PASSWORD_MAX_CHARS);
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local address
 * Input:	the peer group's ID
 * Output: the local address or NULL
 * Note: The caller doesn't need to free memory.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerGroupLocalAddress(int peerGroupID)
{	
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupLocalAddress: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return NULL;
	}
	if( PeerGroups[peerGroupID] == NULL )
	{	
		log_err("getPeerGroupLocalAddress: couldn't find a peer group with ID:%d", peerGroupID);
		return NULL;
	}

	// if the local address is not set, use the value from default group
	char *addr = NULL;
	if( strlen(PeerGroups[peerGroupID]->configuration->localSettings.addr) == 0 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		addr = PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->localSettings.addr;
	else
		addr = PeerGroups[peerGroupID]->configuration->localSettings.addr;

	return addr;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer group's local address
 * Input:	the peer group's ID and the local addr in string
 * Output:  ADDR_VALID means address is valid and set successfully.
 *	    ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *          ADDR_ERR_INVALID_ACTIVE means this is not a valid active address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerGroupLocalAddress( int peerGroupID, char *addr )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("setPeerGroupLocalAddress: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return ADDR_ERR_INVALID_FORMAT;
	}

	// check if the peer group is defined
	if( PeerGroups[peerGroupID] == NULL )
	{
		log_err("setPeerGroupLocalAddress: couldn't find a peer group with ID:%d", peerGroupID);
		return ADDR_ERR_INVALID_FORMAT;
	}

	// check if the supplied address is valid
	int result = checkAddress(addr, ADDR_PASSIVE);
	if(result != ADDR_VALID)
		return result;

	// check if the supplied address is different from current
	if( strcmp(PeerGroups[peerGroupID]->configuration->localSettings.addr, addr) == 0 )
		return ADDR_VALID;

	// if different, set it
	strncpy( PeerGroups[peerGroupID]->configuration->localSettings.addr, addr, ADDR_MAX_CHARS);
	return ADDR_VALID;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local port
 * Input:	the peer group's ID
 * Output: the monitor side port
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupLocalPort(int peerGroupID)
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupLocalPort: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupLocalPort: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// if the local port is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->localSettings.port == 0 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->localSettings.port;
	else
		return PeerGroups[peerGroupID]->configuration->localSettings.port;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local port
 * Input:	the peer group 's ID and the port
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLocalPort( int peerGroupID, int port )
{	
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("setPeerGroupLocalPort: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupLocalPort: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}
	//  check if the port is valid
	if ( (port < 0) || (port > 65536) ) 
	{
		log_err("setPeerGroupLocalPort: invalid port number of %d", port);
		return -1;
	}
	
	// check if the supplied port is different from current
	if( PeerGroups[peerGroupID]->configuration->localSettings.port == port )
		return 0;

	// if different, set it
	PeerGroups[peerGroupID]->configuration->localSettings.port = port;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local BGP ID
 * Input:	the peer group's ID
 * Output: the local BGP ID
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getPeerGroupLocalBGPID(int peerGroupID)
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupLocalBGPID: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupLocalBGPID: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// if the local BGP ID is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->localSettings.BGPID == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0)
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->localSettings.BGPID;
	else
		return PeerGroups[peerGroupID]->configuration->localSettings.BGPID;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local BGP ID
 * Input:	the peer group's ID and the local BGP ID
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int SetPeerGroupLocalBGPID( int peerGroupID, long long BGPID )
{	
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("SetPeerGroupLocalBGPID: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("SetPeerGroupLocalBGPID: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if the supplied BGP ID is different from current
	if( PeerGroups[peerGroupID]->configuration->localSettings.BGPID == BGPID )
		return -1;
	
	// if different, set it
	PeerGroups[peerGroupID]->configuration->localSettings.BGPID = BGPID;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local AS number
 * Input:	the peer group's ID
 * Output: the local AS number
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getPeerGroupLocalASNum( int peerGroupID )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupLocalASNum: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return 0;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupLocalASNum: couldn't find a peer group with ID:%d", peerGroupID);
		return 0;
	}

	// if the local AS number is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->localSettings.AS2 == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->localSettings.AS2;
	else
		return PeerGroups[peerGroupID]->configuration->localSettings.AS2;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local AS number
 * Input:	the peer group's ID and local AS number
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLocalASNum( int peerGroupID, u_int32_t AS )
{	
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("setPeerGroupLocalASNum: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupLocalASNum: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if the supplied AS number is different from current
	if( PeerGroups[peerGroupID]->configuration->localSettings.AS2 == AS )
		return 0;
	
	// if different, set it
	PeerGroups[peerGroupID]->configuration->localSettings.AS2 = AS;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local hold time
 * Input:	the peer group's ID
 * Output: the local hold time
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupLocalHoldTime( int peerGroupID )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupLocalHoldTime: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupLocalHoldTime: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// if the local hold time is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->localSettings.holdtime == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->localSettings.holdtime;
	else
		return PeerGroups[peerGroupID]->configuration->localSettings.holdtime;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local hold time
 * Input:	the peer group's ID and local hold time
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLocalHoldTime( int peerGroupID, int holdTime )
{	
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("setPeerGroupLocalHoldTime: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupLocalHoldTime: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if the supplied holdtime is different from current
	if( PeerGroups[peerGroupID]->configuration->localSettings.holdtime == holdTime )
		return 0;
	
	// if different, set it
	PeerGroups[peerGroupID]->configuration->localSettings.holdtime = holdTime;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local BGP version
 * Input:	the peer group's ID
 * Output: the local BGP version
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupLocalBGPVersion( int peerGroupID )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupLocalBGPVersion: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupLocalBGPVersion: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// if the local BGP version is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->localSettings.BGPVer == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->localSettings.BGPVer;
	else
		return PeerGroups[peerGroupID]->configuration->localSettings.BGPVer;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local BGP version
 * Input:	the peer group's ID and local BGP version
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLocalBGPVersion( int peerGroupID, int version )
{	
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("setPeerGroupLocalHoldTime: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupLocalHoldTime: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if the supplied BGP version is different from current
	if( PeerGroups[peerGroupID]->configuration->localSettings.BGPVer== version )
		return 0;
	
	// if different, set it
	PeerGroups[peerGroupID]->configuration->localSettings.BGPVer = version;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's label action
 * Input:	the peer group's ID
 * Output: the label action
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupLabelAction( int peerGroupID )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupLabelAction: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupLabelAction: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// if the label action is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->labelAction == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->labelAction;
	else
		return PeerGroups[peerGroupID]->configuration->labelAction;		 
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's label action
 * Input:	the peer's ID and label action
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLabelAction( int peerGroupID, int labelAction )
{
	// check the peer group ID is valid
	if( peerGroupID >= MAX_PEER_GROUP_IDS )
	{
		log_err("setPeerGroupLabelAction: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupLabelAction: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if label action changes
	if( PeerGroups[peerGroupID]->configuration->labelAction == labelAction )
		return 0;
	
	// if changed ...
	log_msg("setPeerGroupLabelAction: from %d to %d", PeerGroups[peerGroupID]->configuration->labelAction, labelAction);
	PeerGroups[peerGroupID]->configuration->labelAction = labelAction;
	return 0;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer group's route refresh action
 * Input:   the peer group's ID
 * Output: the route refresh action
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupRouteRefreshAction( int peerGroupID )
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupRouteRefreshAction: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupRouteRefreshAction: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// if the route refresh action is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->routeRefreshAction == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->routeRefreshAction;
	else
		return PeerGroups[peerGroupID]->configuration->routeRefreshAction;		
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the route refresh action pf a peer group
 * Input:	the peer group's ID and route refresh action
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupRouteRefreshAction( int peerGroupID, int rrAction )
{
	// check the peer group ID is valid
	if( peerGroupID >= MAX_PEER_GROUP_IDS )
	{
		log_err("setPeerGroupRouteRefreshAction: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupRouteRefreshAction: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if RR action changes
	if( PeerGroups[peerGroupID]->configuration->routeRefreshAction == rrAction )
		return 0;
	
	// if changed ...
	log_msg("setPeerGroupRouteRefreshAction: from %d to %d", PeerGroups[peerGroupID]->configuration->routeRefreshAction, rrAction);
	PeerGroups[peerGroupID]->configuration->routeRefreshAction = rrAction;
	return 0;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the enabled flag of a peer group
 * Input:	the peer group's ID
 * Output: the enabled flag
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupEnabledFlag(int peerGroupID)
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupEnabledFlag: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupEnabledFlag: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}
	
	// if the enabled flag is not set, use the value from default group
	if( PeerGroups[peerGroupID]->configuration->enabled == -1 && PeerGroups[peerGroupID]->configuration->groupID >=0 )
		return PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->enabled;
	else
		return PeerGroups[peerGroupID]->configuration->enabled;
	
}

/*--------------------------------------------------------------------------------------
 * Purpose: set the enabled flag of a peer group
 * Input:	the peer group's ID and the enabled flag
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerGroupEnabledFlag( int peerGroupID, int enabledFlag )
{
	// check the peer group ID is valid
	if( peerGroupID >= MAX_PEER_GROUP_IDS )
	{
		log_err("setPeerGroupEnabledFlag: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("setPeerGroupEnabledFlag: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	// check if enabled flag changes
	if( PeerGroups[peerGroupID]->configuration->enabled == enabledFlag )
		return 0;
	
	// if changed ...
	log_msg("setPeerGroupEnabledFlag: from %d to %d", PeerGroups[peerGroupID]->configuration->enabled, enabledFlag);
	PeerGroups[peerGroupID]->configuration->enabled = enabledFlag;
	return 0;	
} 

/*--------------------------------------------------------------------------------------
 * Purpose: delete a peer group
 * Input:	the peer group's ID
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int  
deletePeerGroup(int peerGroupID)
{
	// check the peer group ID is valid
	if( peerGroupID >= MAX_PEER_GROUP_IDS )
	{
		log_err("deletePeerGroup: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("deletePeerGroup: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	int i;
	for( i=0; i<PeerGroups[peerGroupID]->configuration->numOfAnnCaps; i++ )
	{
		if( PeerGroups[peerGroupID]->configuration->announceCaps[i] != NULL )
			free( PeerGroups[peerGroupID]->configuration->announceCaps[i] );
	}
	free( PeerGroups[peerGroupID]->configuration);
	PPeerGroup tmp = PeerGroups[peerGroupID];
	PeerGroups[peerGroupID] = NULL;
	free( tmp );
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: add an annouce capabilty to a peer group, called by CLI
 * Input:	the peer group's ID, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerGroupAddAnnounceCapability(int peerGroupID, int code, int len, u_char *value)
{
	// check the peer group ID is valid
	if( peerGroupID >= MAX_PEER_GROUP_IDS )
	{
		log_err("peerAddAnnounceCapability: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("peerAddAnnounceCapability: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	return addAnnounceCapability(PeerGroups[peerGroupID]->configuration, code, len, value);
}

/*--------------------------------------------------------------------------------------
 * Purpose: remove an annouce capabilty from a peer group, called by CLI
 * Input:	the peer group's ID, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 mean failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerGroupRemoveAnnounceCapability(int peerGroupID, int code, int len, u_char *value)
{
	// check the peer group ID is valid
	if( peerGroupID >= MAX_PEER_GROUP_IDS )
	{
		log_err("peerRemoveAnnounceCapability: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("peerRemoveAnnounceCapability: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}

	return removeAnnounceCapability(PeerGroups[peerGroupID]->configuration, code, len, value);
}

/*--------------------------------------------------------------------------------------
 * Purpose: add an receive capabilty to a peer group, called by CLI
 * Input:	the peer group's ID, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerGroupAddReceiveCapability(int peerGroupID, int action, int code, int len, u_char *value)
{
	// check the peer group ID is valid
	if( peerGroupID >= MAX_PEER_GROUP_IDS )
	{
		log_err("peerGroupAddReceiveCapability: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("peerGroupAddReceiveCapability: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}
	
	return addReceiveCapability(PeerGroups[peerGroupID]->configuration, action, code, len, value);
}

/*--------------------------------------------------------------------------------------
 * Purpose: remove a receive capabilty from a peer group, called by CLI
 * Input:	the peer group's ID, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty does't exist
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerGroupRemoveReceiveCapability(int peerGroupID, int action, int code, int len, u_char *value)
{
	// check the peer group ID is valid
	if( peerGroupID >= MAX_PEER_GROUP_IDS )
	{
		log_err("peerGroupRemoveReceiveCapability: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("peerGroupRemoveReceiveCapability: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}
	return removeReceiveCapability(PeerGroups[peerGroupID]->configuration, action, code, len, value);
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of rules how to check received capabilites, 
 		    these rules are from the peer group and default peer group
 * Input: the peer group's ID and a pointer to the return array
 * Output: the length of the return array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerGroupReceiveCapabities(int peerGroupID, PeerCapabilityRequirement **rcvCaps)
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupReceiveCapabities: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupReceiveCapabities: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;
	}
	
	// first allocate memory for the peer's receive capabilites
	int len = 0;
	PeerCapabilityRequirement *caps = NULL;

	// put all the peer group receive capabilites in the list at first
	int i = 0;
	for(i = 0; i < PeerGroups[peerGroupID]->configuration->numOfReceiveCaps; i++)
	{	
		PeerCapabilityRequirement tmp = PeerGroups[peerGroupID]->configuration->receiveCapRequirements[i];
		if( findReceiveCapabilityWithoutAction(len, caps, tmp.code, tmp.checkValueLen, tmp.checkValue) < 0 )
		{
			len++;
			caps = (PeerCapabilityRequirement *)realloc (caps, len * sizeof(PeerCapabilityRequirement));
			if (caps == NULL) 
			{
				log_warning("getPeerGroupReceiveCapabities: Unable to allocate memory");
				return -1; 
			}			
			caps[len-1] = tmp;
		}
	}

	// then put all the receive capabilites from default peer group
	if(PeerGroups[peerGroupID]->configuration->groupID >= 0)
	{
		for(i = 0; i < PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->numOfReceiveCaps; i++)
		{
			PeerCapabilityRequirement tmp = PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->receiveCapRequirements[i];
			if( findReceiveCapabilityWithoutAction(len, caps, tmp.code, tmp.checkValueLen, tmp.checkValue) < 0 )
			{
				len++;
				caps = (PeerCapabilityRequirement *) realloc (caps, len * sizeof(PeerCapabilityRequirement));
				if (caps == NULL) 
				{
					log_warning("getPeerGroupReceiveCapabities: Unable to allocate memory");
					return -1; 
				}					
				caps[len-1] = tmp;
			}
		}	
	}
	
#ifdef DEBUG
	for(i = 0; i < len; i++)
	{	
		debug(__FUNCTION__, "Peer Group %d: Capability Code %d, len %d, action %d", peerGroupID, caps[i].code, caps[i].checkValueLen, caps[i].action);
	}
#endif
		
	*rcvCaps = caps; 
	return len;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of announced capabilities which are from the peer group and default group
 * Input: the peer group's ID and a pointer to the return array
 * Output: the length of the return array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerGroupAnnounceCapabities(int peerGroupID, PBgpCapability **annCaps)
{
	// check the peer group ID is valid
	if (peerGroupID >= MAX_PEER_GROUP_IDS)
	{
		log_err("getPeerGroupAnnounceCapabities: peer group ID %d exceeds max %d", peerGroupID, MAX_PEER_GROUP_IDS);
		return -1;
	}
	// check if the peer group is configured
	if( PeerGroups[peerGroupID] == NULL ) 
	{
		log_err("getPeerGroupAnnounceCapabities: couldn't find a peer group with ID:%d", peerGroupID);
		return -1;	}

	
	int len = 0;
	PBgpCapability *caps = NULL;

	// put all the peer group's announce capabilites in the list at first
	int i = 0;
	for(i = 0; i < PeerGroups[peerGroupID]->configuration->numOfAnnCaps; i++)
	{	
		PBgpCapability tmp = PeerGroups[peerGroupID]->configuration->announceCaps[i];
		if( findAnnounceCapabilityInArray(len, caps, tmp->code, tmp->length, tmp->value) < 0 )
		{
			len++;
			caps = (PBgpCapability *)realloc (caps, len * sizeof(PBgpCapability));
			if (caps == NULL) 
			{
				log_warning("getPeerAnnounceCapabities: Unable to allocate memory");
				return -1; 
			}			
			caps[len-1] = createGenericBGPCapability( tmp->code, tmp->length, tmp->value);
		}
	}

	// then put all the receive capabilites from default peer group
	if(PeerGroups[peerGroupID]->configuration->groupID >= 0)
	{
		for(i = 0; i < PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->numOfAnnCaps; i++)
		{
			PBgpCapability tmp = PeerGroups[PeerGroups[peerGroupID]->configuration->groupID]->configuration->announceCaps[i];
			if( findAnnounceCapabilityInArray(len, caps, tmp->code, tmp->length, tmp->value) < 0 )
			{
				len++;
				caps = (PBgpCapability *) realloc (caps, len * sizeof(PBgpCapability));
				if (caps == NULL) 
				{
					log_warning("getPeerAnnounceCapabities: Unable to allocate memory");
					return -1; 
				}					
				caps[len-1] = createGenericBGPCapability( tmp->code, tmp->length, tmp->value);
			}
		}	
	}
	
#ifdef DEBUG
	for(i = 0; i < len; i++)
	{	
		debug(__FUNCTION__, "Peer Group %d: Annouce Capability Code %d, len %d", peerGroupID, caps[i]->code, caps[i]->length);
	}
#endif
	*annCaps = caps; 
	return len;
}

