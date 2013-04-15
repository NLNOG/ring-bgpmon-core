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
 *  File: peers.h
 * 	Authors: He Yan, Dan Massey
 *  Date: July 24, 2008 
 */

#ifndef PEERS_H_
#define PEERS_H_

// needed for pthread id
#include <pthread.h>
/* needed for system types such as time_t */
#include <sys/types.h>
#include "../Util/bgpmon_formats.h"
#include "../Util/bgpmon_defaults.h"
#include "../site_defaults.h"
#include "bgppacket.h"

/*----------------------------------------------------------------------------------------
 * Configuration Substructure Definition
 * -------------------------------------------------------------------------------------*/
 
/* Peer Capability Action */
enum PeerCapabilityActionType { 
	actionAllow = 0,
	actionRequire,
	actionRefuse
};	

/* Peer Capability Info in Configuration File */
struct PeerCapabilityRequirementStruct
{
	u_char	code;
	int		action;
	int		checkValueLen; //-1 means only check code
	u_char	checkValue[maxCapabilityValueLen];
	u_char	flag;
};
typedef struct PeerCapabilityRequirementStruct PeerCapabilityRequirement;

/*local side settings of a peer in Configuration File */
struct LocalSettingsStrut
{
  int        holdtime;
  char       addr[ADDR_MAX_CHARS];
  int        port;
  u_int32_t  AS2;
  int        BGPVer;
  long long  BGPID;
};
typedef struct LocalSettingsStrut LocalSettings;

/*remote side settings of a peer in Configuration File */
struct RemoteSettingsStrut
{
	/*tcp_md5 passwd if available*/
	char  		md5Passwd[PASSWORD_MAX_CHARS];
	/*expected router 2 byte AS number, 0 means accept any value */
	u_int32_t		AS2;
	/*expected router 's BGP version*/
	int			BGPVer;
	/*expected router 's BGP Identifer,  0 means accept any value*/
	long long	BGPID;
	/*expected min router 's BGP Holdtime*/
	int			MinHoldtime;
	char		addr[ADDR_MAX_CHARS];
	int			port;	
};
typedef struct RemoteSettingsStrut RemoteSettings;

/* Peer Configuration */
struct ConfigurationStruct
{
	LocalSettings		localSettings;
	RemoteSettings		remoteSettings;
	int					routeRefreshAction;
	int					labelAction;
	int					enabled;
	int					numOfAnnCaps;	
	PBgpCapability 		announceCaps[maxNumOfCapabilities];	
	int					numOfReceiveCaps;	
	PeerCapabilityRequirement receiveCapRequirements[maxNumOfCapabilities];
	/*group name*/
	char				groupName[PEER_GROUP_MAX_CHARS];
	int					groupID;
};
typedef struct ConfigurationStruct *PConfiguration;

/* Peer Structure */
struct PeerStruct
{
	/*peer ID*/
	int				peerID;
	/*session ID*/
	int				sessionID;
	/*thread reference*/
	pthread_t			peerThread;
	/*configuration data*/
	PConfiguration configuration;

};
typedef struct PeerStruct *PPeer;

/* Peer Group Configuration */
struct PeerGroupStruct
{
	/*group identification*/
	int				ID;
	/*group name*/
	char			name[PEER_GROUP_MAX_CHARS];
	/*configuration data*/
	PConfiguration configuration;

};
typedef struct PeerGroupStruct *PPeerGroup;

/* An array of peer configuration structures.  Each structure contains the
 * settings need to create and maintain a BGP peering session. 
 */
PPeer Peers[MAX_PEER_IDS];

/* An array of peer group configuration structures.  
 */
PPeerGroup PeerGroups[MAX_PEER_GROUP_IDS];

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default peers configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int initPeersSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Read the peers settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readPeersSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Read the peers settings from the config file.
 * Input: a double poniter to the returned configuration structure
 *		xpath to a peer or peer group
 *		the index of the element to read
 * Output: TRUE means success, FALSE means failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readPeerConf( PConfiguration *conf, char * xpath, int i);

/*--------------------------------------------------------------------------------------
 * Purpose: print the peer settings
 * Input:  the peer ID
 * Output: NULL
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void printPeerSettings(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Save one peer settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int saveOnePeerSetting(PConfiguration conf);

/*--------------------------------------------------------------------------------------
 * Purpose: Save the peers settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int savePeersSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: create the configuration structure
 * Input:  the peer 's address and port
 * Output: pointer to configuration or NULL if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
PConfiguration createPeerConf( char *peerAddr, int port );

/*--------------------------------------------------------------------------------------
 * Purpose: find the ID of a peer given addr and port
 * Input: addr in string and port number
 * Output: ID of the peer
 * Note: If the peer with specified addr and port is not exsiting, return -1. 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findPeerID(char *addr, int port);

/*--------------------------------------------------------------------------------------
 * Purpose: Create a peer structure
 * Input:  the peer configuration struture
 * Output: id of the new peer group or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeerStruct( PConfiguration config );

/*--------------------------------------------------------------------------------------
 * Purpose: create a peer, called by CLI.
 * Input:  peerAddress - could be a peer's address
 *		 port - the peer's port	
 * Output: id of the new peer or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeer( char *peerAddr, int port );

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of IDs of all active peers
 * Input: a pointer to an array
 * Output: the length of the array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getAllPeersIDs(int **peerIDs);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the session ID of a peer
 * Input: peer ID and session ID
 * Output:  0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerSessionID(int peerID, int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session ID of a peer
 * Input: peer ID
 * Output: the corresponding session ID
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerSessionID(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's address
 * Input:	the peer's ID
 * Output: the address of the peer
 *          or NULL if no address for this peer 
 * Note: The caller doesn't need to free memory.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerRemoteAddress(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's remote address
 * Input:	the peer's ID and the local addr in string
 * Output:  ADDR_VALID means address is valid and set successfully.
 *	    ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *          ADDR_ERR_INVALID_ACTIVE means this is not a valid active address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerRemoteAddress( int peerID, char *addr );


/*--------------------------------------------------------------------------------------
 * Purpose: Get the port of the peer
 * Input:   the peer's ID
 * Output: the peer's port
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerRemotePort(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's remote port
 * Input:	the peer's ID and the local addr in string
 * Output:  ADDR_VALID means address is valid and set successfully.
 *	    ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *          ADDR_ERR_INVALID_ACTIVE means this is not a valid active address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerRemotePort( int peerID, int port );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's group name
 * Input:	the peer's ID
 * Output: the group name of the peer
 *          or NULL 
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeer_GroupName(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's group name
 * Input:	the peer's ID and the group name in string
 * Output:  0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeer_GroupName( int peerID, char *groupName );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote BGP ID of the peer
 * Input:   the peer's ID
 * Output: the peer's remote BGP ID
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getPeerRemoteBGPID(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's remote BGP ID
 * Input:	the peer's ID and remote BGP ID
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRemoteBGPID( int peerID, long long BGPID );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote AS number of the peer
 * Input:   the peer's ID
 * Output: the peer's remote AS number
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getPeerRemoteASNum( int peerID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's AS number
 * Input:	the peer's ID and AS number
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRemoteASNum( int peerID, u_int32_t AS );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer 's remote BGP version
 * Input:	the peer 's ID
 * Output: the remote BGP Versionn
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerRemoteBGPVersion( int peerID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's remote BGP version
 * Input:	the peer's ID and and remote BGP version
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRemoteBGPVersion( int peerID, int version );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer 's required min hold time
 * Input:	the peer 's ID
 * Output: the required min hold time
 *         or -1 if there is no peer group with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerRemoteHoldTime( int peerID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's required min hold time
 * Input:	the peer's ID and and the required min hold time
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRemoteHoldTime( int peerID, int holdtime );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's MD5 password
 * Input:	the peer's ID
 * Output: the lMD5 password
 *          or NULL if no address for this peer 
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerMD5Passwd(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's MD5 password
 * Input:	the peer's ID and the MD5 password in string
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerMD5Passwd( int peerID, char *passwd );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local address
 * Input:	the peer's ID
 * Output: the local address
 *          or NULL if no address for this peer 
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerLocalAddress(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local address
 * Input:	the peer's ID and the local addr in string
 * Output:  ADDR_VALID means address is valid and set successfully.
 *	    ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *          ADDR_ERR_INVALID_ACTIVE means this is not a valid active address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerLocalAddress( int peerID, char *addr );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local port number
 * Input:   the peer's ID
 * Output: the local port
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerLocalPort(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local port number
 * Input:	the peer's ID and the port
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalPort( int peerID, int port );

/*--------------------------------------------------------------------------------------
 * Purpose: Get peer's local BGP ID
 * Input:   the peer's ID
 * Output: the monitor side BGP ID
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getPeerLocalBGPID(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose:Set peer's local BGP ID
 * Input:	the peer's ID and the local BGP ID
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalBGPID( int peerID, long long BGPID );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local AS number
 * Input:   the peer's ID
 * Output: the local AS number
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getPeerLocalASNum( int peerID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local AS number
 * Input:	the peer's ID and local AS number
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalASNum( int peerID, u_int32_t AS );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local BGP version
 * Input:   the peer's ID
 * Output: the local BGP version
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerLocalBGPVersion( int peerID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local BGP version
 * Input:	the peer's ID and local BGP version
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalBGPVersion( int peerID, int version );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local holdtime
 * Input:   the peer's ID
 * Output: the local hold time
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerLocalHoldTime( int peerID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local hold time
 * Input:	the peer's ID and local hold time
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalHoldTime( int peerID, int holdtime );

/*--------------------------------------------------------------------------------------
 * Purpose: Get peer's the label action
 * Input:   the peer's ID
 * Output: the label action
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerLabelAction( int peerID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set peer's the label action
 * Input:	the peer's ID and label action
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLabelAction( int peerID, int labelAction );

/*--------------------------------------------------------------------------------------
 * Purpose: Get peer's the route refresh action
 * Input:   the peer's ID
 * Output: the route refresh action
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerRouteRefreshAction( int peerID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set peer's the route refresh action
 * Input:	the peer's ID and route refresh action
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRouteRefreshAction( int peerID, int rrAction );

/*--------------------------------------------------------------------------------------
 * Purpose: Get peer's the enabled flag
 * Input:	the peer's ID
 * Output: TRUE or FALSE
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerEnabledFlag(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: Set peer's the enabled flag
 * Input:	the peer's ID and the enabled flag
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerEnabledFlag( int peerID, int enabled );

/*--------------------------------------------------------------------------------------
 * Purpose: check if the peer's configuration is complete to launch the thread
 * Input:	the peer's ID
 * Output: FALSE means Ok, TRUE means incomplete configuration
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int checkPeerConfiguration(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: launch all peers
 * Input:	
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void launchAllPeers();

/*--------------------------------------------------------------------------------------
 * Purpose: Signals all peer threads to shutdown
 * Input:	
 * Output:
 * Kevin Burnett @ July 9, 2009
 * -------------------------------------------------------------------------------------*/ 
void signalPeersShutdown();

/*--------------------------------------------------------------------------------------
 * Purpose: Waits until all peer threads have been stopped before returning
 * Input:	
 * Output:
 * Kevin Burnett @ July 9, 2009
 * -------------------------------------------------------------------------------------*/ 
void waitForPeersShutdown();

/*--------------------------------------------------------------------------------------
 * Purpose: reconnectPeer a peer
 * Input:	the peer's ID
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int reconnectPeer(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: set the delete flag of a peer as TRUE, the real deletetion will be done by thread management module later.
 * Input:	the peer's ID
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
deletePeer(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: get peer's session ID by a peer ID
 * Input:	the peer's sesssion ID
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
getSessionIDByPeerID(int peerID);

/*--------------------------------------------------------------------------------------
 * Purpose: add an announce capabilty
 * Input:	the configuration structure, cap code, cap value len and cap value.
 * Output: 0 means success, -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int addAnnounceCapability(PConfiguration conf, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: remove an annouce capabilty
 * Input:	the configuration structure, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 mean failure because the capabilty doesn't exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int removeAnnounceCapability(PConfiguration conf, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: add an annouce capabilty to a peer, called by CLI
 * Input:	the peer's ID, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerAddAnnounceCapability(int peerID, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: remove an annouce capabilty from a peer, called by CLI
 * Input:	the peer's ID, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 mean failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerRemoveAnnounceCapability(int peerID, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: add a receive capabilty
 * Input:	the configuration structure, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int addReceiveCapability(PConfiguration conf, int action, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: remove a receive capabilty
 * Input:	the configuration structure, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty does't exist
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int removeReceiveCapability(PConfiguration conf, int action, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: add an receive capabilty to a peer, called by CLI
 * Input:	the peer's ID, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerAddReceiveCapability(int peerID, int action, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: remove a receive capabilty from a peer, called by CLI
 * Input:	the peer's ID, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty does't exist
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerRemoveReceiveCapability(int peerID, int action, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: Print Peers Array
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void printAllPeers();

/*--------------------------------------------------------------------------------------
 * Purpose: check if the state change message is from stateOpenSent, stateOpenConfirm or stateEstablished
 		 to stateIdle
 * Input:	the state change message in BMF
 * Output: FALSE means no
 *         	   TRUE means yes
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int checkStateChangeMessage( BMF bmf );

/*--------------------------------------------------------------------------------------
 * Purpose: find a receive capabilty without looking at action field
 * Input:	cap code, cap value len and cap value.
 * Output: the index of the found capability or -1 means not found
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findReceiveCapabilityWithoutAction( int numOfCaps, PeerCapabilityRequirement *caps, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of rules how to check received capabilites, 
 		    these rules are from peer, peer group and default peer group
 * Input: the peer's ID and a pointer to the return array
 * Output: the length of the return array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerReceiveCapabities(int peerID, PeerCapabilityRequirement **rcvCaps);

/*--------------------------------------------------------------------------------------
 * Purpose: find an announce capabilty
 * Input:	cap code, cap value len and cap value.
 * Output: the index of the found capability or -1 means not found
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findAnnounceCapabilityInArray(int numOfCaps, PBgpCapability *caps, int code, int len, u_char *value);

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of announced capabilities which are from peer, peer group and default group
 * Input: the peer's ID and a pointer to the return array
 * Output: the length of the return array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerAnnounceCapabities(int peerID, PBgpCapability **annCaps);


#endif /*PEERS_H_*/
