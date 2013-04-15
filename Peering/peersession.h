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
 *  File: peersession.h
 * 	Authors: He Yan
 *  Date: Jun 24, 2008 
 */

#ifndef PEERSESSION_H_
#define PEERSESSION_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "../Queues/queue.h"
#include "bgppacket.h"
#include "peers.h"
#include "../Labeling/rtable.h"
/* required for TRUE/FALSE defines  */
#include "../Util/bgpmon_defaults.h"

/*----------------------------------------------------------------------------------------
 * FSM Substructure Definition
 * -------------------------------------------------------------------------------------*/
struct FSMStruct
{
	int 			socket;	
	int 			state; // the state of BGP FSM
	int				reason;	// the reason of changing state
	int 			connectRetryInt;
	time_t			connectRetryTimer;
	int 			keepaliveInt;
	time_t			keepaliveTimer;
	int 			largeHoldTime;
	int 			holdTime;
	time_t			holdTimer;
	int 			routeRefreshType;
	int 			routeRefreshFlag;	// set by periodic module
        int 			ASNumlen; 		// 2 bytes or 4 bytes AS number 
	PBgpCapabilities	peerCapabilities; // received Peer Capabilities	
};
typedef struct FSMStruct FSM;


/*Structure for BMF_TYPE_FSM_STATE_CHANGE*/
struct StateChangeStruct
{
	u_int16_t		oldState;
	u_int16_t 		newState;
	u_int16_t		reason;
};
typedef struct StateChangeStruct StateChangeMsg;

 
 /*----------------------------------------------------------------------------------------
  *  Stats Substructure Definition
  * -------------------------------------------------------------------------------------*/
 struct StatisticsStruct
 {
	int				connectRetryCount;
	int 			sessionDownCount;
	time_t			lastDownTime;
	long			messageRcvd;
	time_t			establishTime;
	time_t 			lastRouteRefresh;
	int				nannRcvd;
	int				lastSentNannRcvd;
	int				lastHourNannRcvd;
	int				dannRcvd;
	int				lastSentDannRcvd;
	int				lastHourDannRcvd;
	int				spathRcvd;
	int				lastSentSpathRcvd;
	int				lastHourSpathRcvd;
	int				dpathRcvd;
	int				lastSentDpathRcvd;
	int				lastHourDpathRcvd;
	int				withRcvd;
	int				lastSentWithRcvd;
	int				lastHourWithRcvd;
	int				duwiRcvd;
	int				lastSentDuwiRcvd;
	int				lastHourDuwiRcvd;
	long			memoryUsed;
	int				prefixCount;
	int				attrCount;
 };
 typedef struct StatisticsStruct Statistics;

/*----------------------------------------------------------------------------------------
 *  ConfigInUse Substructure Definition
 * -------------------------------------------------------------------------------------*/
struct ConfigInUseStruct
{
	int			localHoldtime;
	char		localAddr[ADDR_MAX_CHARS];
	int			localPort;
	u_int32_t		localAS2;
	int			localBGPVer;
	long long	localBGPID;
	u_int32_t 		remoteAS2;
	int			remoteBGPVer;
	long long	remoteBGPID;
	int			remoteMinHoldtime;
	char		remoteAddr[ADDR_MAX_CHARS];
        char            collectorAddr[ADDR_MAX_CHARS];
	int			remotePort;	
	char  		md5Passwd[PASSWORD_MAX_CHARS];
	// route refresh action
	int			routeRefreshAction;
	// label action
	int			labelAction;
	// a list of announce capabilities
	int					numOfAnnCaps;	
	PBgpCapability 		*announceCaps;		
	// a list of requirements of received capabilities
	int					numOfCapReqs;
	PeerCapabilityRequirement *capRquirements;
};
typedef struct ConfigInUseStruct ConfigInUse;

/*----------------------------------------------------------------------------------------
 * Session Structure Definition 
 * -------------------------------------------------------------------------------------*/
struct SessionStruct
{
	/*session identification*/
	int				sessionID;

	/* Config in use substructure */
	ConfigInUse 	configInUse;
	
	/*FSM substructure*/
	FSM				fsm;

	/*Statistics substructure*/
	Statistics		stats;

	/*peer queue writer*/
	QueueWriter		peerQueueWriter;
		
	/*outgoing peering string for XML module*/
	char				*sessionStringOutgoing;

	/*incoming peering string for XML module*/
	char				*sessionStringIncoming;
	
	/*real src address string for XML module*/
	char				sessionRealSrcAddr[ADDR_MAX_CHARS];
	
	/*Prefix Table*/
	PrefixTable		*prefixTable;

	/*Attribute Table*/
	AttrTable		*attributeTable;

	/* thread related fields */
	int			reconnectFlag;
	time_t		lastAction;

};
typedef struct SessionStruct  *Session_structp;


/* An array of session structures.  Each structure contains the
 * data need to label updates from a peer and also contains 
 * statistics associated withe the peer and labels, as well as 
 * instructions on whether to maintain a RIB for the peer and the
 * actuall RIB table if stored.
 */
Session_structp Sessions[MAX_SESSION_IDS];
pthread_mutex_t 	sessionArrayLock;

/*--------------------------------------------------------------------------------------
 * Purpose: Create a configInUse structure based on a peer config ID
 * Input:  ID of peer configuation and the session structure
 * Output: 0 means success or -1 means failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int setSessionConfigInUse( int peerID, Session_structp session );

/*--------------------------------------------------------------------------------------
 * Purpose: Create a session structure
 * Input:  
 * Output: id of the new session or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createSessionStruct( );

/*--------------------------------------------------------------------------------------
 * Purpose: Create a session structure and populates it with peer specific information
 * Input:  ID of peer configuation, the session down couts and last session down time
 * Output: id of the new session or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeerSessionStruct( int peerID, int downCount, time_t lastDownTime );

/*--------------------------------------------------------------------------------------
 * Purpose: Create a session for mrt connection
 * Input: source address, AS and destination address, AS
 * Output: id of the new session or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createMrtSessionStruct( u_int32_t srcAS, char *scrAddr, char *collAddr, int labelAction, int ASNumLen );

/*--------------------------------------------------------------------------------------
 * Purpose: change state of session with ID
 * Input: sessionID, state
 * Output: 
 * Mikhail Strizhov @ Sept 16, 2009
 * -------------------------------------------------------------------------------------*/
void changeMrtState( int sessionID, int state );

/*--------------------------------------------------------------------------------------
 * Purpose: find a session based on six tuples
 * Input: source address, AS, port and destination address, AS, port
 * Output: id of the found session or -1 if couldn't find any
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int findSession( u_int32_t srcAS, u_int32_t dstAS, int srcPort, int dstPort, char *scrAddr, char *dstAddr );

/*--------------------------------------------------------------------------------------
 * Purpose: find a session based on 3 tuple
 * Input: source address, source AS, collector address 
 * Output: id of the found session or -1 if couldn't find any
 * Cathie Olschanowsky @ December, 2011 
 * -------------------------------------------------------------------------------------*/
int findSession_R_ASNIP_C_IP( u_int32_t remoteAS, char *remoteIP, char *collIP );

/*--------------------------------------------------------------------------------------
 * Purpose: find a session based on 3 tuple
 * Input: peer AS, peer IP, collector IP, label action, as num len, session state and reason for state
 * Output: id of the found session or -1 if couldn't find any
 * Cathie Olschanowsky @ December, 2011 
 * -------------------------------------------------------------------------------------*/
int findOrCreateMRTSessionStruct( u_int32_t remoteAS, char *remoteIP, char *collIP, int labelAction, int ASNumLen,int state, int reason);

/*--------------------------------------------------------------------------------------
 * Purpose:delete a session
 * Input:  sessionID - ID of the session
 * Output: 
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
destroySession( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: Zero the session's connect retry timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
zeroSessionConnectRetryTimer( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: Zero the session's keepalive timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
zeroSessionKeepaliveTimer( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: Zero the session's hold timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
zeroSessionHoldTimer( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: Zero the session's connect retry count
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
zeroSessionConnectRetryCount( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: implements jitter for the timers in the range 75%-100% of the expected time.
 * Input:	the time in seconds
 * Output: jitter time
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
jitter( int seconds );

/*--------------------------------------------------------------------------------------
 * Purpose: Restart the session's connect retry timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
restartSessionConnectRetryTimer( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: Restart the session's keepalive timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
 void 
restartSessionKeepaliveTimer( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: Restart the session's largre hold timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
restartLargeSessionHoldTimer( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: Restart the session's hold timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
 void 
restartSessionHoldTimer( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: Returns the earliest timer for this session (smallest nonzero) for use in calls to select.
 * Input:	the session structure
 * Output: the value of earliest timer
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
sessionTimer( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: check which timer expired
 * Input:	the session structure
 * Output: event indicate which time expired
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
checkTimers( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: create a state change BMF message
 * Input:	the session ID, old state, new state and reason of changing state
 * Output: a BMF message
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
BMF
createStateChangeMsg( int sessionID, int	oldState, int newState, int reason );

/*--------------------------------------------------------------------------------------
 * Purpose: set session state
 * Input:	the session structure, state to set and reason of changing state
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
setSessionState( Session_structp session, int state, int reason);

/*--------------------------------------------------------------------------------------
 * Purpose: stub function of initialize connection
 * Input:
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int
initiateConnection( Session_structp s );


/*--------------------------------------------------------------------------------------
 * Purpose: set socket options
 * Input:
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void
Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);

/*--------------------------------------------------------------------------------------
 * Purpose: complete a tcp connection of a session
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
completeConnection( Session_structp session );

/*--------------------------------------------------------------------------------------
 * Purpose: close the connection of a session
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
dropConnection( Session_structp s );

/*--------------------------------------------------------------------------------------
 * Purpose: send a BGP open message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
sendOpenMessage( Session_structp session );

 /*--------------------------------------------------------------------------------------
 * Purpose: Check version, as, ID and holdtime in received open message
 * Input:	the session structure
 * Output Events:
 * eventNone - no error
 * eventBGPOpenMsgErr - find errors
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int checkOpenMessage(Session_structp session, PBgpOpen opn, PBgpCapabilities pbcs);

 
/*--------------------------------------------------------------------------------------
* Purpose: Check the capabilites in received open message
* Input:	 the session structure
* Output Events:
* eventNone - no error
* eventBGPUnsupportedCapability - find errors
* He Yan @ Sep 22, 2008
* -------------------------------------------------------------------------------------*/
int checkCapabilities(Session_structp session, int len, PeerCapabilityRequirement *capRequirments, PBgpCapabilities capabilities);


/*--------------------------------------------------------------------------------------
 * Purpose: receive a BGP open message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
receiveOpenMessage( Session_structp session );

/*--------------------------------------------------------------------------------------
 * Purpose: send a BGP keepalive message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int
sendKeepaliveMessage( Session_structp session );

/*--------------------------------------------------------------------------------------
 * Purpose: receive a BGP keepalive message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
receiveKeepaliveMessage( Session_structp session );

/*--------------------------------------------------------------------------------------
 * Purpose: receive a route refresh message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
 int
sendRouteRefreshMessage( Session_structp session );

/*--------------------------------------------------------------------------------------
 * Purpose: receive a BGP message
 * Input:	the session structure
 * Output: Event to indicate the received message or the error
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
receiveBGPMessage( Session_structp session );

/*--------------------------------------------------------------------------------------
 * Purpose: send out a BGP notification message
 * Input:	the session structure and notification code, subcode, value and length
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
sendNotificationMessage( Session_structp session, u_int8_t code, u_int8_t subcode, char *msg, int len );

/*--------------------------------------------------------------------------------------
 * Purpose: decide the nex step of a session
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void
nextStepOfSession( Session_structp session );

/*--------------------------------------------------------------------------------------
 * Purpose: stub function to initialize resources
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void initializeResources( Session_structp seer );

/*--------------------------------------------------------------------------------------
 * Purpose: stub function to complete initialization
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
 void completeInitialization( Session_structp seer );

/*--------------------------------------------------------------------------------------
 * Purpose: reset resource of a session
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void releaseResources( Session_structp session);

/*--------------------------------------------------------------------------------------
 * Purpose: reset a session to state idle
 * Input:	the session structure and the reason of reseting a session
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void resetSession( Session_structp session, int reason );

/*--------------------------------------------------------------------------------------
 * Purpose: clean up a session when it ends
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void cleanupSession( Session_structp session );

/*--------------------------------------------------------------------------------------
 * Purpose: get the route refresh type of the specified session
 * Input:	the session ID
 * Output: FALSE means this session doesn't support route refresh.
 *		  TRUE means this session supports route refresh.	
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionRouteRefreshType( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: Set the route refresh flag of a session
 * Input:	the session ID
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setSessionRouteRefreshFlag( int sessionID );

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
 * Purpose: get the 6 tuple string of a session
 * Input:	the session's ID and the direction flag
 * Output: the 6 tuple string 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
char *getSessionString(int sessionID, int type);

/*--------------------------------------------------------------------------------------
 * Purpose: set the 6 tuple string of a session
 * Input:	the session's ID and the six tuple
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setSessionString(int sessionID, char *srcAddr, u_int16_t srcPort, u_int32_t srcAS, char *dstAddr, u_int16_t dstPort, u_int32_t dstAS);

/*--------------------------------------------------------------------------------------
 * Purpose: get label action of a session
 * Input:	the session's ID
 * Output: label action
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionLabelAction(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: set label action of a session
 * Input:	the session's ID and label action
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setSessionLabelAction(int sessionID, int labelAction);

/*--------------------------------------------------------------------------------------
 * Purpose: get route refresh action of a session
 * Input:	the session's ID
 * Output: route refresh action
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionRouteRefreshAction(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: set route refresh action of a session
 * Input:	the session's ID and route refresh action
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setSessionRouteRefreshAction(int sessionID, int rrAction);

/*--------------------------------------------------------------------------------------
 * Purpose: get the last action time of a session
 * Input:	the session's ID
 * Output: last action time
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getSessionLastActionTime(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get current state of a session
 * Input:	the session's ID
 * Output: the current state
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionState(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: set the connect retry count of a session
 * Input:	the session's ID
 * Output: the connect retry count
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionConnectRetryCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: set the connect retry count of a session
 * Input:	the session's ID and connect retry count
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setSessionConnectRetryCount(int sessionID, int count);

/*--------------------------------------------------------------------------------------
 * Purpose: get the message count of a session
 * Input:	the session's ID
 * Output: the message count
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
long getSessionMsgCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: increment the message count of a session by 1
 * Input:	the session's ID and the state
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void incrementSessionMsgCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the peer down count of a session
 * Input:	the session's ID
 * Output: peer down count 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionDownCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the last peer down time of a session
 * Input:	the session's ID
 * Output: the last peer down time
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getSessionLastDownTime(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the establish time of a session
 * Input:	the session's ID
 * Output: the establish time 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getSessionEstablishTime(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the last route refresh time of a session
 * Input:	the session's ID
 * Output: peer down count 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getSessionLastRouteRefreshTime(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the prefix count of a session
 * Input:	the session's ID
 * Output: the prefix count 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionPrefixCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the attribute count of a session
 * Input:	the session's ID
 * Output: the attribute count 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionAttributeCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the memory usage of a session
 * Input:	the session's ID
 * Output: the number of bytes used by the session
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
long getSessionMemoryUsage(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of new announcements since session gets established
 * Input:	the session's ID
 * Output: the number of new announcements
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionNannCount(int sessionID);
int getSessionCurrentNannCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of duplicate announcements since session gets established
 * Input:	the session's ID
 * Output: the number of duplicate announcements
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionDannCount(int sessionID);
int getSessionCurrentDannCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of announcements with same AS path since session gets established
 * Input:	the session's ID
 * Output: the number of announcements with same AS path
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionSPathCount(int sessionID);
int getSessionCurrentSPathCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of announcements with different AS path since session gets established
 * Input:	the session's ID
 * Output: the number of announcements with different AS path
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionDPathCount(int sessionID);
int getSessionCurrentDPathCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of withdrawals since session gets established
 * Input:	the session's ID
 * Output: the number of withdrawals
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionWithCount(int sessionID);
int getSessionCurrentWithCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of duplicate withdrawals since session gets established
 * Input:	the session's ID
 * Output: the number of duplicate withdrawals
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionDWithCount(int sessionID);
int getSessionCurrentDWithCount(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's remote address
 * Input:	the session's ID
 * Output: the address in string
 *             NULL if there is no session with this ID or the session doesn't have a remote address
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getSessionRemoteAddr(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote port of the session
 * Input:   the session's ID
 * Output: the session's remote port
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionRemotePort(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote BGP ID of the session
 * Input:   the session's ID
 * Output: the session's remote BGP ID
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getSessionRemoteBGPID(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote AS number of the session
 * Input:   the session's ID
 * Output: the session's remote AS number
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getSessionRemoteASNum( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's remote BGP version
 * Input:	the session's ID
 * Output: the remote BGP Versionn
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionRemoteBGPVersion( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's required min hold time
 * Input:	the session's ID
 * Output: the required min hold time
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionRemoteHoldTime( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's MD5 password
 * Input:	the session's ID
 * Output: the lMD5 password
 *             NULL if there is no session with this ID or the session doesn't have MD5 password
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getSessionMD5Passwd(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's local address
 * Input:	the session's ID
 * Output: the local address
 *            NULL if there is no session with this ID the session has no local address
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getSessionLocalAddr(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local port of the session
 * Input:   the session's ID
 * Output: the session's local port
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionLocalPort(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local BGP ID of the session
 * Input:   the session's ID
 * Output: the session's local BGP ID
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getSessionLocalBGPID(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local AS number of the session
 * Input:   the session's ID
 * Output: the session's local AS number
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getSessionLocalASNum( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local BGP version of the session
 * Input:   the session's ID
 * Output: the session's local BGP version
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionLocalBGPVersion( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local holdtime of the session
 * Input:   the session's ID
 * Output: the session's local holdtime
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionLocalHoldTime( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: get session by ID
 * Input:	the session's ID
 * Output: the pointer to session  
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
Session_structp getSessionByID(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose: check if the specified session is established
 * Input:	the session's ID
 * Output: FALSE means this session is not established
 *		  TRUE means the session is established
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int isSessionEstablished( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: return up time of a specified session
 * Input:	the session's ID
 * Output: the up time
 		  -1 if the session is not existing or is not established
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long getSessionUPTime( int sessionID );

/*--------------------------------------------------------------------------------------
 * Purpose: return AS number length of a specified session
 * Input:       the session's ID
 * Output: the up time
 *    2 or 4
 *    -1 if the session is not existing or is not established
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionASNumberLength( int sessionID );
int setSessionASNumberLength( int sessionID, int ASNumLen);

/*--------------------------------------------------------------------------------------
 * Purpose: Print Peers Array
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void printAllSessions();

/*--------------------------------------------------------------------------------------
 * Purpose: the main function of a peer
 * Input:	the peer ID
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void *
peerThreadFunction( void *arg );


/*--------------------------------------------------------------------------------------
 * Purpose: Change the session state to error, if mrt update client disconnect
 * 	 	and try to erase prefixes and attributes associated with this session ID
 * Input:  mrt node ID
 * Output:  0 - success, -1 failure
 * Mikhail Strizhov @ Sept 27, 2010
 * -------------------------------------------------------------------------------------*/
void deleteMrtSession(int mrtid  );


#endif /*PEERSESSION_H_*/

