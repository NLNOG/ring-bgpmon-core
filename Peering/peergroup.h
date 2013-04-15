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
 *  File: peergroup.h
 * 	Authors: He Yan, Dan Massey
 *  Date: July 24, 2008 
 */

#ifndef PEERGROUPS_H_
#define PEERGROUPS_H_

/* needed for system types such as time_t */
#include <sys/types.h>
#include "../Util/bgpmon_formats.h"

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the peer groups.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int initPeerGroupsSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Read the peer groups settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readPeerGroupsSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Save the peer groups settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int savePeerGroupsSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: print the peer group settings
 * Input:  the peer group ID
 * Output: NULL
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void printPeerGroupSettings(int peerGroupID);

/*--------------------------------------------------------------------------------------
 * Purpose: find ID of a peer group given group name
 * Input: group name in string
 * Output: ID of the peer group
 * Note: If the peer group with specified name is not exsiting, return -1. 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findPeerGroupID(char *groupName);

/*--------------------------------------------------------------------------------------
 * Purpose: create the default peer group
 * Input:
 * Output: pointer to peer configuration or NULL if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createDefaultPeerGroup();

/*--------------------------------------------------------------------------------------
 * Purpose: Create a peer group structure
 * Input:  the peer configuration struture and the group name
 * Output: id of the new peer group or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeerGroupStruct( PConfiguration config, char *name  );

/*--------------------------------------------------------------------------------------
 * Purpose: create a peer group called by CLI.
 * Input:  name - the group name
 * Output: id of the new peer/peergoup or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeerGroup( char *name );

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of IDs of all peer groups
 * Input: a pointer to an array
 * Output: the length of the array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getAllPeerGroupsIDs(int **peerGroupIDs);

/*--------------------------------------------------------------------------------------
 * Purpose: Print Peer Groups Array
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void printAllPeerGroups();

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer group's name
 * Input:	the peer group's ID
 * Output: the name of the peer group or NULL
 * Note: The caller doesn't need to free memory.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerGroupName(int peerGroupID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote AS number of the peer group
 * Input:   the peer group's ID
 * Output: the peer group's remote AS number
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getPeerGroupRemoteASNum( int peerGroupID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's remote AS number
 * Input:	the peer's ID and AS number
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupRemoteASNum( int peerGroupID, u_int32_t AS );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's remote BGP version
 * Input:	the peer group's ID
 * Output: the remote BGP Versionn
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupRemoteBGPVersion( int peerGroupID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's remote BGP version
 * Input:	the peer group's ID and remote BGP version
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupRemoteBGPVersion( int peerGroupID, int version );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's remote BGP ID
 * Input:	the peer group's ID
 * Output: the remote BGP ID
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getPeerGroupRemoteBGPID(int peerGroupID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's remote BGP ID
 * Input:	the peer group's ID and the remote BGP ID
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int SetPeerGroupRemoteBGPID( int peerGroupID, long long BGPID );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's remote hold time
 * Input:	the peer group's ID
 * Output: the remote hold time
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupRemoteHoldTime( int peerGroupID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's remote hold time
 * Input:	the peer group's ID and remote hold time
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupRemoteHoldTime( int peerGroupID, int holdTime );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's MD5 password
 * Input:	the peer group's ID
 * Output: the MD5 password or NULL
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerGroupMD5Passwd(int peerGroupID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer group's MD5 password
 * Input:	the peer group's ID and the MD5 password in string
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerGroupMD5Passwd( int peerGroupID, char *passwd );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local address
 * Input:	the peer group's ID
 * Output: the local address or NULL
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerGroupLocalAddress(int peerGroupID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer group's local address
 * Input:	the peer group's ID and the local addr in string
 * Output:  ADDR_VALID means address is valid and set successfully.
 *	    ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *          ADDR_ERR_INVALID_ACTIVE means this is not a valid active address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerGroupLocalAddress( int peerGroupID, char *addr );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local port
 * Input:	the peer group's ID
 * Output: the monitor side port
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupLocalPort(int peerGroupID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local port
 * Input:	the peer group 's ID and the port
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLocalPort( int peerGroupID, int port );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local BGP ID
 * Input:	the peer group's ID
 * Output: the local BGP ID
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getPeerGroupLocalBGPID(int peerGroupID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local BGP ID
 * Input:	the peer group's ID and the local BGP ID
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int SetPeerGroupLocalBGPID( int peerGroupID, long long BGPID );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local AS number
 * Input:	the peer group's ID
 * Output: the local AS number
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getPeerGroupLocalASNum( int peerGroupID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local AS number
 * Input:	the peer group's ID and local AS number
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLocalASNum( int peerGroupID, u_int32_t AS );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local hold time
 * Input:	the peer group's ID
 * Output: the local hold time
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupLocalHoldTime( int peerGroupID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local hold time
 * Input:	the peer group's ID and local hold time
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLocalHoldTime( int peerGroupID, int holdTime );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's local BGP version
 * Input:	the peer group's ID
 * Output: the local BGP version
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupLocalBGPVersion( int peerGroupID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's local BGP version
 * Input:	the peer group's ID and local BGP version
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLocalBGPVersion( int peerGroupID, int version );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer Group's label action
 * Input:	the peer group's ID
 * Output: the label action
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupLabelAction( int peerGroupID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer Group's label action
 * Input:	the peer's ID and label action
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupLabelAction( int peerGroupID, int labelAction );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer group's route refresh action
 * Input:   the peer group's ID
 * Output: the route refresh action
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupRouteRefreshAction( int peerGroupID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the route refresh action pf a peer group
 * Input:	the peer group's ID and route refresh action
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerGroupRouteRefreshAction( int peerGroupID, int rrAction );

/*--------------------------------------------------------------------------------------
 * Purpose: get the enabled flag of a peer group
 * Input:	the peer group's ID
 * Output: the enabled flag
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerGroupEnabledFlag(int peerGroupID);

/*--------------------------------------------------------------------------------------
 * Purpose: set the enabled flag of a peer group
 * Input:	the peer group's ID and the enabled flag
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerGroupEnabledFlag( int peerGroupID, int enabledFlag );

/*--------------------------------------------------------------------------------------
 * Purpose: delete a peer group
 * Input:	the peer group's ID
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int  
deletePeerGroup(int peerGroupID);

/*--------------------------------------------------------------------------------------
 * Purpose: add an annouce capabilty to a peer group, called by CLI
 * Input:	the peer group's ID, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerGroupAddAnnounceCapability(int peerGroupID, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: remove an annouce capabilty from a peer group, called by CLI
 * Input:	the peer group's ID, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 mean failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerGroupRemoveAnnounceCapability(int peerGroupID, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: add an receive capabilty to a peer group, called by CLI
 * Input:	the peer group's ID, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerGroupAddReceiveCapability(int peerGroupID, int action, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: remove a receive capabilty from a peer group, called by CLI
 * Input:	the peer group's ID, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty does't exist
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerGroupRemoveReceiveCapability(int peerGroupID, int action, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of rules how to check received capabilites, 
 		    these rules are from the peer group and default peer group
 * Input: the peer group's ID and a pointer to the return array
 * Output: the length of the return array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerGroupReceiveCapabities(int peerGroupID, PeerCapabilityRequirement **rcvCaps);

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of announced capabilities which are from the peer group and default group
 * Input: the peer group's ID and a pointer to the return array
 * Output: the length of the return array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerGroupAnnounceCapabities(int peerGroupID, PBgpCapability **annCaps);


#endif /*PEERGROUPS_H_*/
