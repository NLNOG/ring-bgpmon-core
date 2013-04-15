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
 *  File: peersession.c
 * 	Authors:  He Yan
 *  Date: Jun 19, 2008
 */

#include "../config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <pthread.h> 

#ifdef HAVE_LINUX_TCP_H
#include <linux/tcp.h>
#endif

#include "../Util/log.h"
#include "../Queues/queue.h"
#include "../Util/bgpmon_formats.h"
#include "bgpstates.h"
#include "bgpevents.h"
#include "bgpmessagetypes.h"
#include "bgppacket.h"
#include "peersession.h"
#include "bgpfsm.h"
#include "peers.h"
#include "../Util/unp.h"
#include "../Util/address.h"
#include "../Util/bgpmon_defaults.h"
#include "../Labeling/label.h"
#include "../XML/xml.h"
#include "../Labeling/label.h"
#include "../Mrt/mrt.h"
#include  "../Config/configfile.h"

#define defaultConnectRetryTime 30
#define defaultHoldTime 180
//#define DEBUG


/*--------------------------------------------------------------------------------------
 * Purpose: Create a configInUse structure based on a peer config ID
 * Input:  ID of peer configuation and the session structure
 * Output: 0 means success or -1 means failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int setSessionConfigInUse( int peerID, Session_structp session )
{
	if( getPeerLocalHoldTime(peerID) >= 0 )
		session->configInUse.localHoldtime = getPeerLocalHoldTime(peerID);
	else
		return -1;

	if( getPeerLocalAddress(peerID) != NULL )
		strcpy(session->configInUse.localAddr, getPeerLocalAddress(peerID));
	else
		return -1;

	if( getPeerLocalPort(peerID) >= 0 )
		session->configInUse.localPort = getPeerLocalPort(peerID);
	else
		return -1;

	if( getPeerLocalASNum(peerID) >= 0 )
		session->configInUse.localAS2 = getPeerLocalASNum(peerID);
	else
		return -1;

	if( getPeerLocalBGPVersion(peerID) >= 0 )
		session->configInUse.localBGPVer = getPeerLocalBGPVersion(peerID);
	else
		return -1;

	if( getPeerLocalBGPID(peerID) >= 0 )
		session->configInUse.localBGPID = getPeerLocalBGPID(peerID);
	else
		return -1;

	if( getPeerRemoteASNum(peerID) >= 0 )
		session->configInUse.remoteAS2 = getPeerRemoteASNum(peerID);
	else
		return -1;		

	if( getPeerRemoteBGPVersion(peerID) >= 0 )
		session->configInUse.remoteBGPVer = getPeerRemoteBGPVersion(peerID);
	else
		return -1;

	if( getPeerRemoteBGPID(peerID) >= 0 )
		session->configInUse.remoteBGPID = getPeerRemoteBGPID(peerID);
	else
		return -1;

	if( getPeerRemoteHoldTime(peerID) >= 0 )
		session->configInUse.remoteMinHoldtime = getPeerRemoteHoldTime(peerID);
	else
		return -1;

	if( getPeerRemoteAddress(peerID) != NULL )
		strcpy( session->configInUse.remoteAddr, getPeerRemoteAddress(peerID) );
	else
		return -1;	
	
	if( getPeerRemotePort(peerID) >= 0 )
		session->configInUse.remotePort = getPeerRemotePort(peerID);
	else
		return -1;	

	if( getPeerMD5Passwd(peerID) != NULL )
		strcpy( session->configInUse.md5Passwd, getPeerMD5Passwd(peerID) );
	else
		return -1;	

	if( getPeerRouteRefreshAction(peerID) >= 0 )
		session->configInUse.routeRefreshAction = getPeerRouteRefreshAction(peerID);
	else
		return -1;

	if( getPeerLabelAction(peerID) >= 0 )
		session->configInUse.labelAction = getPeerLabelAction(peerID);
	else
		return -1;
	
	int i;
	for( i=0; i<session->configInUse.numOfAnnCaps; i++ )
	{
		if( session->configInUse.announceCaps[i] != NULL )
			free( session->configInUse.announceCaps[i] );
	}
	free( session->configInUse.announceCaps );

	if( session->configInUse.capRquirements != NULL )
		free( session->configInUse.capRquirements );
	
	session->configInUse.numOfAnnCaps= getPeerAnnounceCapabities(peerID,  &session->configInUse.announceCaps);
	if( session->configInUse.numOfAnnCaps < 0 )
		return -1;
	session->configInUse.numOfCapReqs= getPeerReceiveCapabities(peerID,  &session->configInUse.capRquirements);
	if( session->configInUse.numOfCapReqs < 0 )
		return -1;

	return 0;
}	

/*--------------------------------------------------------------------------------------
 * Purpose: Create a session structure
 * Input:  
 * Output: id of the new session or -1 if creation failed
 * Cathie Olschanowsky December, 2011
 * -------------------------------------------------------------------------------------*/
int createSessionStruct( )
{

	// find a empty slot for this new session
	int i;
	for(i = 0; i < MAX_SESSION_IDS; i++)
	{
		if(Sessions[i] == NULL)
			break;
	}

	// if the number of sessions exceeds MAX_SESSION_IDS, return -1
	if( i == MAX_SESSION_IDS )
	{
	 	log_err("Unable to create new session, max number of sessions exceeded");
		return -1;
	}

	
	// Returns a new session structure or NULL if not enough memory.
	Session_structp session = (Session_structp)malloc( sizeof( struct SessionStruct ) );
	if( session == NULL )
	{
		log_err("createSessionStruct: malloc failed");
		return -1;
	}

	memset(session, 0, sizeof( struct SessionStruct) );

	// session ID
	session->sessionID= i;
	// insert it into the array
	Sessions[i] = session;

        return i;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Create a session structure
 * Input:  ID of peer configuation
 * Output: id of the new session or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeerSessionStruct( int peerID, int downCount, time_t lastDownTime )
{

        int i = createSessionStruct();

        if(i<0){
          return -1;
        }
        
        Session_structp session = Sessions[i];
        if(session == NULL){
          return -1;
        }

	// set configuration in use
	if( setSessionConfigInUse(peerID, session) < 0 )
	{
		log_err("setSessionConfigInUse failed");
		free(session);
		return -1;
	}		

	// set fsm structure	
	session->fsm.state = stateIdle;
	session->fsm.reason = eventNone;
	session->fsm.socket = -1;	
	// determine the time intervals for the various timers
	session->fsm.connectRetryInt = defaultConnectRetryTime;
			
	// reset all the timers.
	session->fsm.connectRetryTimer = 0;
	session->fsm.keepaliveTimer =0;
	session->fsm.holdTimer = 0;
	
	// create queue writer
	session->peerQueueWriter = createQueueWriter(peerQueue);;
	if( session->peerQueueWriter == NULL )
	{
		log_err("peer thead %d failed to create Queue Writer. peer thread exiting.", peerID);
		free(session);
		return -1;
	}
	
	// update real address in use
	strcpy(session->sessionRealSrcAddr, session->configInUse.localAddr);
	
	// set thread related fields
	session->lastAction = 0;
	session->reconnectFlag = FALSE;

	// set default AS number length as 2 bytes
	session->fsm.ASNumlen = 2;

	// set statistics related fields
	session->stats.sessionDownCount = downCount;
	session->stats.lastDownTime = lastDownTime;


	//initial prefix and attribute table
	if( session->configInUse.labelAction != NoAction )
	{
		createPrefixTable(session->sessionID, PREFIX_TABLE_SIZE, MAX_HASH_COLLISION);
		createAttributeTable(session->sessionID, ATTRIBUTE_TABLE_SIZE, MAX_HASH_COLLISION);
	}
	

	if ( session->configInUse.localHoldtime == 0 )
		session->fsm.holdTime = defaultHoldTime;
	else
		session->fsm.holdTime = session->configInUse.localHoldtime;
	session->fsm.largeHoldTime = session->fsm.holdTime * 3;
	session->fsm.keepaliveInt = session->fsm.holdTime / 3;
	
#ifdef DEBUG
	debug(__FUNCTION__, "Created a new session with id %d for peer %d", i, peerID);
#endif
		
	return i;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Create a session for mrt connection
 * Input: source address, AS and destination address, AS
 * Output: id of the new session or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createMrtSessionStruct( u_int32_t srcAS, char *scrAddr, char *collAddr, int labelAction, int ASNumLen )
{

        // create the actual object
        int i = createSessionStruct();
        if(i< 0){
          return -1;
        }
        if(Sessions[i] == NULL){
          return -1;
        }
        Session_structp session = Sessions[i];
       
	
 	// set six tuples
	strcpy(session->configInUse.localAddr, collAddr);
	strcpy(session->configInUse.remoteAddr, scrAddr);
	strcpy(session->configInUse.collectorAddr, collAddr);
	session->configInUse.localPort = 179;
	session->configInUse.remotePort = 179;
	session->configInUse.remoteAS2 = srcAS;
	session->configInUse.routeRefreshAction = TRUE;

	// set fsm structure	
	session->fsm.state = stateMrtEstablished;
	session->fsm.reason = eventNone;
	session->fsm.ASNumlen = ASNumLen;
	session->fsm.socket = -1;	
	
	// create queue writer
	session->peerQueueWriter = createQueueWriter(peerQueue);
	if( session->peerQueueWriter == NULL )
	{
		log_err("createMrtSessionStruct: failed to create Queue Writer");
		free(session);
		return -1;
	}
	
	// update real address in use
	strcpy(session->sessionRealSrcAddr, session->configInUse.localAddr);	
	
	// set thread related fields
	session->lastAction = 0;
	session->reconnectFlag = FALSE;

	//initial prefix and attribute table
	session->configInUse.labelAction = labelAction;
	if( session->configInUse.labelAction != NoAction )
	{
		createPrefixTable(session->sessionID, PREFIX_TABLE_SIZE, MAX_HASH_COLLISION);
		createAttributeTable(session->sessionID, ATTRIBUTE_TABLE_SIZE, MAX_HASH_COLLISION);
	}


	debug(__FUNCTION__, "Created a new mrt session with id %d", i);
	
	return i;
}

/*--------------------------------------------------------------------------------------
 * Purpose: change state of session with ID
 * Input: sessionID, state
 * Output: 
 * Mikhail Strizhov @ Sept 16, 2009
 * -------------------------------------------------------------------------------------*/
void changeMrtState( int sessionID, int state )
{
  if ( Sessions[sessionID] )
  {
	  		Sessions[sessionID]->fsm.state = state;	
  }
}	

/*--------------------------------------------------------------------------------------
 * Purpose: find a session based three attributes
 * Input: peerAS, peerIP, collector IP
 * Output: id of the found session or -1 if couldn't find any
 * Cathie Olschanowsky @ Dec 2011
 * -------------------------------------------------------------------------------------*/
int
findSession_R_ASNIP_C_IP( u_int32_t remoteAS, char *remoteIP, char *collIP )
{
  int i;
  for(i = 0; i < MAX_SESSION_IDS; i++)
  {
    if( Sessions[i] != NULL )
    {
      if( Sessions[i]->configInUse.remoteAS2 == remoteAS
          && strcmp(Sessions[i]->configInUse.collectorAddr, collIP)== 0 && strcmp(Sessions[i]->configInUse.remoteAddr, remoteIP)== 0) 
        return i;
    }
  }
  return -1;
}
/*--------------------------------------------------------------------------------------
 * Purpose: find a session based on 3 attributes or create a new session based on them
 * Input: peer AS, peer IP collector IP label action, and as num length
 * Output: id of the found session or -1 if couldn't find any or ran into error
 * Cathie Olschanowsky @ Dec 2011
 * -------------------------------------------------------------------------------------*/
int
findOrCreateMRTSessionStruct( u_int32_t remoteAS, char *remoteIP, char *collIP, int labelAction, int ASNumLen,int state, int reason)
{
      if ( pthread_mutex_lock( &sessionArrayLock ) ){
        log_err("createSessionStruct: Unable to get lock\n");
        return -1;
      }

      int sessionID = findSession_R_ASNIP_C_IP(remoteAS, remoteIP, collIP);
      if(sessionID < 0 ){
        sessionID = createMrtSessionStruct(remoteAS, remoteIP, collIP, labelAction, ASNumLen );
        // set Session state as stateERROR
        setSessionState(getSessionByID(sessionID), state,reason );
      }

      if ( pthread_mutex_unlock( &sessionArrayLock ) ){
        log_err("createSessionStruct: Unable to surrender lock\n");
        return -1;
      }
      return sessionID;
}


/*--------------------------------------------------------------------------------------
 * Purpose: find a session based on six tuples
 * Input: source address, AS, port and destination address, AS, port
 * Output: id of the found session or -1 if couldn't find any
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int findSession( u_int32_t srcAS, u_int32_t dstAS, int srcPort, int dstPort, char *scrAddr, char *dstAddr )
{
	int i;
	for(i = 0; i < MAX_SESSION_IDS; i++)
	{
		if( Sessions[i] != NULL )
		{
			if( Sessions[i]->configInUse.localAS2 == dstAS && Sessions[i]->configInUse.remoteAS2 == srcAS
				&& Sessions[i]->configInUse.localPort == dstPort && Sessions[i]->configInUse.remotePort == srcPort
				&& strcmp(Sessions[i]->configInUse.localAddr, dstAddr)== 0 && strcmp(Sessions[i]->configInUse.remoteAddr, scrAddr)== 0) 
			return i;
		}
	}
	
	return -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose:delete a session
 * Input:  sessionID - ID of the session
 * Output: 
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
destroySession( int sessionID )
{
/* Frees the memory associated with the session.
 * Do not use the sesssion after this operation.
 */
  if ( Sessions[sessionID] )
  {
  	int i;
	for( i=0; i<Sessions[sessionID]->configInUse.numOfAnnCaps; i++ )
	{
		if( Sessions[sessionID]->configInUse.announceCaps[i] != NULL )
			free( Sessions[sessionID]->configInUse.announceCaps[i] );
	}
	if( Sessions[sessionID]->configInUse.announceCaps != NULL )
		free( Sessions[sessionID]->configInUse.announceCaps );

	if( Sessions[sessionID]->configInUse.capRquirements != NULL )
		free( Sessions[sessionID]->configInUse.capRquirements );
	
	if(Sessions[sessionID]->peerQueueWriter != NULL)
		destroyQueueWriter(Sessions[sessionID]->peerQueueWriter);

	if(Sessions[sessionID]->sessionStringIncoming != NULL)
		free( Sessions[sessionID]->sessionStringIncoming);
	if(Sessions[sessionID]->sessionStringOutgoing != NULL)
		free( Sessions[sessionID]->sessionStringOutgoing);
	
	free( Sessions[sessionID]);
	Sessions[sessionID] = NULL;
  }
}


/*--------------------------------------------------------------------------------------
 * Purpose: Zero the session's connect retry timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
zeroSessionConnectRetryTimer( Session_structp s )
{
	s->fsm.connectRetryTimer = 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Zero the session's keepalive timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
zeroSessionKeepaliveTimer( Session_structp s )
{
	s->fsm.keepaliveTimer = 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Zero the session's hold timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
zeroSessionHoldTimer( Session_structp s )
{
	s->fsm.holdTimer = 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Zero the session's connect retry count
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
zeroSessionConnectRetryCount( Session_structp s )
{
	s->stats.connectRetryCount = 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: implements jitter for the timers in the range 75%-100% of the expected time.
 * Input:	the time in seconds
 * Output: jitter time
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
jitter( int seconds )
{
	int factor = 75 + (rand()%26);  // a number in the interval [75,100]
	int delay = (seconds * factor) / 100;
	return( delay );
}

/*--------------------------------------------------------------------------------------
 * Purpose: Restart the session's connect retry timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
restartSessionConnectRetryTimer( Session_structp s )
{
	int backoff = s->fsm.connectRetryInt * s->stats.connectRetryCount;
	if ( backoff > 60 ){
		backoff = 60;
        }
	int delay = jitter( backoff  ); 
	log_msg("Session(%d):Connect retry delay %d ", s->sessionID, delay);
	s->fsm.connectRetryTimer = time(NULL) + delay; 
}

/*--------------------------------------------------------------------------------------
 * Purpose: Restart the session's keepalive timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
 void 
restartSessionKeepaliveTimer( Session_structp s )
{
	int delay = jitter( s->fsm.keepaliveInt );
	s->fsm.keepaliveTimer = time(NULL) + delay;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Restart the session's largre hold timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
restartLargeSessionHoldTimer( Session_structp s )
{
	int delay = jitter( s->fsm.largeHoldTime );
	s->fsm.holdTimer = time(NULL) + delay;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Restart the session's hold timer
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
 void 
restartSessionHoldTimer( Session_structp s )
{
	int delay = jitter( s->fsm.holdTime );
	s->fsm.holdTimer = time(NULL) + delay;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Returns the earliest timer for this session (smallest nonzero) for use in calls to select.
 * Input:	the session structure
 * Output: the value of earliest timer
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
sessionTimer( Session_structp s )
{
	int t = 0;
	//log_msg( "sessionTimer %d %d %d %d", s->connectRetryTimer, s->holdTimer, s->keepaliveTimer, s->routeRefreshTimer);
	if ( s->fsm.connectRetryTimer > 0 && ( t == 0 || s->fsm.connectRetryTimer < t ) )
		t = s->fsm.connectRetryTimer;
	if ( s->fsm.holdTimer > 0 && ( t == 0 || s->fsm.holdTimer < t ) )
		t = s->fsm.holdTimer;
	if ( s->fsm.keepaliveTimer > 0 && ( t == 0 || s->fsm.keepaliveTimer < t ) )
		t = s->fsm.keepaliveTimer;
	return( t );
}

/*--------------------------------------------------------------------------------------
 * Purpose: check which timer expired
 * Input:	the session structure
 * Output: event indicate which time expired
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
checkTimers( Session_structp s )
{
	/* Checks the events and returns an event for any timer that expired.
	 * If multiple timers expire, only returns the first.
	 * Returns eventNone if no timers have expired.
	 */
	time_t now = time(NULL);
	if ( s->fsm.connectRetryTimer > 0 && s->fsm.connectRetryTimer <= now + 1 )
		return( eventConnectRetryTimer_Expires );
	if ( s->fsm.keepaliveTimer > 0 && s->fsm.keepaliveTimer <= now + 1)
		return( eventKeepaliveTimer_Expires );
	if ( s->fsm.holdTimer > 0 && s->fsm.holdTimer <= now + 1)
		return( eventHoldTimer_Expires );
	return( eventNone );
}


/*--------------------------------------------------------------------------------------
 * Purpose: create a state change BMF message
 * Input:	the session ID, old state, new state and reason of changing state
 * Output: a BMF message
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
BMF
createStateChangeMsg( int sessionID, int	oldState, int newState, int reason )
{
	BMF m = createBMF(sessionID, BMF_TYPE_FSM_STATE_CHANGE);
	StateChangeMsg stateChangMsg;
	stateChangMsg.newState = newState;
	stateChangMsg.oldState = oldState;
	stateChangMsg.reason = reason;
	bgpmonMessageAppend( m, (void *)&stateChangMsg, sizeof(StateChangeMsg));
	return m;
}

/*--------------------------------------------------------------------------------------
 * Purpose: set session state
 * Input:	the session structure, state to set and reason of changing state
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
setSessionState( Session_structp session, int state, int reason)
{
#ifdef DEBUG
	log_msg("setSessionState from %d to %d", session->fsm.state, state);
#endif

	if( session->fsm.state == stateEstablished && state == stateIdle ) // session reset
	{ 
		session->stats.sessionDownCount++;
		session->stats.lastDownTime = time( NULL );
	}
	
	if( state != session->fsm.state )
	{
		session->fsm.state = state;	
		session->fsm.reason = reason;
	}
	
	if( state == stateEstablished )
	{
		session->stats.messageRcvd = 0;
		session->stats.establishTime = time( NULL );	
		session->stats.lastRouteRefresh = time( NULL );
		session->stats.lastDownTime = 0;
	}		
	else
	{
		session->stats.messageRcvd = 0;
		session->stats.establishTime = 0;
		session->stats.lastRouteRefresh = 0;
	}


}

/*--------------------------------------------------------------------------------------
 * Purpose: stub function of initialize connection
 * Input:
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int
initiateConnection( Session_structp s )
{
/* Since connection is performed in a single operation, 
 * the entire process takes place in completeConnection.
 * Always returns successfully.
 */
#ifdef DEBUG
	debug("initiateConnection", "");
#endif	
	return( eventNone );	 
}

/*--------------------------------------------------------------------------------------
 * Purpose: set socket options
 * Input:
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void
Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	if (setsockopt(fd, level, optname, optval, optlen) < 0)
		log_err("setsockopt error");
}


/*--------------------------------------------------------------------------------------
 * Purpose: complete a tcp connection of a session
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
completeConnection( Session_structp session )
{
	#ifdef DEBUG
	debug("completeConnection", "");
	#endif

	// create monitor side addrinfo struct
	struct addrinfo *localRes = createAddrInfo( session->configInUse.localAddr, session->configInUse.localPort );
	if( localRes == NULL )
	{
		// could not create an addrinfo??
		log_err("Session(%d): Unable to create monitor side addrinfo with addr %s and port %d", session->sessionID, session->configInUse.localAddr, session->configInUse.localPort);
		return( eventTcpConnectionFails );
	}

	// create the socket	
	session->fsm.socket = socket(localRes->ai_family, localRes->ai_socktype, localRes->ai_protocol);
	if ( session->fsm.socket == -1 )
	{
		freeaddrinfo(localRes);
		log_fatal( "Session(%d): fail to create socket %s", session->sessionID, strerror(errno) );
	}

	// create remote side addrinfo struct
	struct addrinfo *remoteRes = createAddrInfo( session->configInUse.remoteAddr, session->configInUse.remotePort );
	if( remoteRes == NULL )
	{
		// could not create an addrinfo??
		log_err("Session(%d): Unable to create remote side addrinfo with addr %s and port %d", session->sessionID, session->configInUse.remoteAddr, session->configInUse.remotePort );
		return( eventTcpConnectionFails );
	}
	
	// set some socket options
	struct timeval tv;
	tv.tv_sec = 120;
	tv.tv_usec = 0;
	Setsockopt(session->fsm.socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	int opt = 1;
	Setsockopt(session->fsm.socket, SOL_SOCKET, SO_REUSEADDR,(void *)&opt, sizeof(opt));
#ifdef HAVE_LINUX_TCP_H
	struct tcp_md5sig md5sig;
	if( strlen(session->configInUse.md5Passwd)!= 0 )
	{		
		log_msg("Session(%d): connection md5 passwd(%d): %s", session->sessionID, strlen(session->configInUse.md5Passwd), session->configInUse.md5Passwd);
		bzero((char *)&md5sig, sizeof(md5sig));
		memcpy(&md5sig.tcpm_addr, remoteRes->ai_addr, remoteRes->ai_addrlen);
		md5sig.tcpm_keylen = strlen(session->configInUse.md5Passwd);
		memcpy(md5sig.tcpm_key, session->configInUse.md5Passwd, md5sig.tcpm_keylen);	
		Setsockopt (session->fsm.socket, IPPROTO_TCP, TCP_MD5SIG, &md5sig, sizeof(md5sig));
	}	
#endif
	
	// bind the socket to the configured monitor's addr and port
	if (bind(session->fsm.socket, localRes->ai_addr, localRes->ai_addrlen) < 0)
	{
		log_err( "Session(%d): local socket is unable to bind %s", session->sessionID, strerror(errno) );
		freeaddrinfo(remoteRes);
		freeaddrinfo(localRes);
		return( eventTcpConnectionFails );
	}
	
	// connect to the peer
  	int connection = connect(session->fsm.socket, remoteRes->ai_addr, remoteRes->ai_addrlen);
	if (connection == -1)
  	{
  		log_err("Session(%d): tcp connection error:%s", session->sessionID, strerror(errno));
		close(session->fsm.socket);
  		session->fsm.socket = -1;
		freeaddrinfo(remoteRes);
		freeaddrinfo(localRes);
  		return( eventTcpConnectionFails );
  	}
	
	if( strcmp( session->configInUse.localAddr, IPv4_ANY ) == 0 ||
		strcmp( session->configInUse.localAddr, IPv6_ANY ) == 0 )
	{
		struct sockaddr sock;
		int slen = sizeof(struct sockaddr); 
		getsockname(session->fsm.socket, &sock, (socklen_t *)&slen);
		char *address;
		int port;
		if( getAddressFromSockAddr(&sock, &address, &port) )
		{
			log_warning( "Unable to get address and port for new connection." );
			freeaddrinfo(remoteRes);
			freeaddrinfo(localRes);
			return eventTcpConnectionFails;
		}
		setSessionString(session->sessionID,session->configInUse.remoteAddr, session->configInUse.remotePort, session->configInUse.remoteAS2, 
							session->configInUse.localAddr, session->configInUse.localPort, session->configInUse.localAS2);
		strcpy(session->sessionRealSrcAddr, address);
		free(address);
	}
	freeaddrinfo(localRes);
	freeaddrinfo(remoteRes);
	log_msg("Session(%d): tcp connection ok ", session->sessionID);
	return( eventTcpConnectionConfirmed);
}

/*--------------------------------------------------------------------------------------
 * Purpose: close the connection of a session
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
dropConnection( Session_structp s )
{
	/* Closes the socket connection and voids the socket for future use.
	 */
	
#ifdef DEBUG
	debug("dropConnection", "");
#endif	
	close( s->fsm.socket );
	s->fsm.socket = -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: send a BGP open message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
sendOpenMessage( Session_structp session )
{
	/* Packages an BGP Open message in 3 parts (header, open, and parameters)
	 * and sends them back to back.  Detects and returns a connection failure event.
	 */
#ifdef DEBUG
	debug("sendOpenMessage","");
#endif	
	int event = eventNone;

	BMF bmf = NULL;
	PBgpHeader hdr = NULL;
	int i;
	
	// create the open parameters
	PBgpOpenParameters prm = createBGPOpenParameters( );

	// add the annouce capabilities into open parameters
	for( i=0; i<session->configInUse.numOfAnnCaps; i++ )
	{
		addOneBGPNewCapability( prm, session->configInUse.announceCaps[i]);
	}

	// create the open message with version, as number, holdtime, bgp ID and the open parameters 
	PBgpOpen opn = NULL;
	if( session->configInUse.localBGPID == 0 )
	{
		struct sockaddr sock;
		int slen = sizeof(struct sockaddr); 
		getsockname(session->fsm.socket, &sock, (socklen_t *)&slen);
		char *address;
		int port;
		getAddressFromSockAddr(&sock, &address, &port);
		opn = createBGPOpen(session->configInUse.localBGPVer, session->configInUse.localAS2,
						session->configInUse.localHoldtime, inet_addr(address), prm);		
	}
	else
		opn = createBGPOpen(session->configInUse.localBGPVer, session->configInUse.localAS2,
						session->configInUse.localHoldtime, session->configInUse.localBGPID, prm);
	free(prm);
	hdr = createBGPHeader( typeOpen );
	setBGPHeaderLength( hdr, lengthBGPOpen( opn )); 

	/*send the open message at once*/
	u_char *testOpen = malloc(sizeof(struct BGPHeaderStruct)+ sizeof( struct BGPOpenStruct )+ opn->optionalParameterLength); 
	memcpy(testOpen, hdr, sizeof(struct BGPHeaderStruct));
	memcpy(testOpen+sizeof(struct BGPHeaderStruct), opn, sizeof( struct BGPOpenStruct )+ opn->optionalParameterLength);
	log_msg("sendOpenMessage with length %d", sizeof(struct BGPHeaderStruct)+ sizeof( struct BGPOpenStruct )+ opn->optionalParameterLength );
	int n = writen(session->fsm.socket, testOpen, sizeof(struct BGPHeaderStruct)+ sizeof( struct BGPOpenStruct )+ opn->optionalParameterLength);
#ifdef DEBUG	
	hexdump(LOG_INFO, testOpen, n );
#endif	

	if ( n == sizeof(struct BGPHeaderStruct)+ sizeof( struct BGPOpenStruct )+ opn->optionalParameterLength )
	{
		log_msg("sendOpenMessage Successfully");
		event = eventNone;
		// place outgoing open in queue
		bmf = createBMF( session->sessionID, BMF_TYPE_MSG_TO_PEER );
		bgpmonMessageAppend( bmf, hdr, 19);
		if( lengthBGPOpen( opn ) != 0)
			bgpmonMessageAppend( bmf, opn, lengthBGPOpen( opn ));		
		writeQueue( session->peerQueueWriter, bmf );
	}
	else
	{	
		log_msg("sendOpenMessage failed");
		event = eventTcpConnectionFails;
	}
	//destroyBGPOpen( opn );
	destroyBGPHeader( hdr );

	return( event );
}


 /*--------------------------------------------------------------------------------------
 * Purpose: Check version, as, ID and holdtime in received open message
 * Input:	the session structure
 * Output Events:
 * eventNone - no error
 * eventBGPOpenMsgErr - find errors
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int checkOpenMessage(Session_structp session, PBgpOpen opn, PBgpCapabilities pbcs)
{
	int errFlag = 0;
	if(session->configInUse.remoteBGPVer != 0)
	{
		if(session->configInUse.remoteBGPVer != opn->version)
		{
			u_int16_t supportedVersion = session->configInUse.remoteBGPVer;
			sendNotificationMessage( session, 2, 1, (char*)&supportedVersion, 2);	
			errFlag = 1;
		}
	}
	if(session->configInUse.remoteAS2 != 0)
	{
	        if ( session->configInUse.remoteAS2 != getASfromBgpCapability( pbcs, opn->as) )
		{
			log_warning("Sending Notification message to the peer: ASN in BGP open message does not match to ASN in BGPmonconfiguration.");
			sendNotificationMessage( session, 2, 2, NULL, 0);
			errFlag = 1;
		}
	}
	if(session->configInUse.remoteBGPID != 0)
	{
		if(session->configInUse.remoteBGPID != opn->identifier)
		{
			sendNotificationMessage( session, 2, 3, NULL, 0);
			errFlag = 1;
		}
	}
	if(session->configInUse.remoteMinHoldtime != 0)
	{
		if(session->configInUse.remoteMinHoldtime > ntohs(opn->holdTime))
		{
			sendNotificationMessage( session, 2, 6, NULL, 0);
			errFlag = 1;
		}
	}
	if(errFlag == 1)
		return eventBGPOpenMsgErr;
	else
		return eventNone;	
}


/*--------------------------------------------------------------------------------------
 * Purpose: Check the capabilites in received open message
 * Input:	the session structure
 * Output Events:
 * eventNone - no error
 * eventBGPUnsupportedCapability - find errors
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int checkCapabilities(Session_structp session, int len, PeerCapabilityRequirement *capRequirments, PBgpCapabilities capabilities)
{
	int i,j;
	int errFlag = FALSE;
	PBgpOpenParameters notifyPrm = createBGPOpenParameters();
	log_msg("count: %d, len:%d", capabilities->count, len);
	for(i=0; i<capabilities->count; i++)
	{
		int longestMatchFound = FALSE;
		int refusedFlag = FALSE;
		for(j=0; j<len; j++)
		{
			log_msg("code1: %d, code2:%d", capabilities->capalities[i].code, capRequirments[j].code);
			if(capabilities->capalities[i].code == capRequirments[j].code)
			{
				if(capRequirments[j].checkValueLen == -1)
				{
					log_msg("find the generic matched capability %d", capabilities->capalities[i].code);
					if(capRequirments[j].action == actionRefuse)
					{
						addOneBGPNewCapability(notifyPrm, &capabilities->capalities[i]);
						if( longestMatchFound ==  FALSE)
							refusedFlag = TRUE;
					}
					else if(capRequirments[j].action == actionAllow)
					{
						if( longestMatchFound ==  FALSE)
							refusedFlag = FALSE;
					}
					else if(capRequirments[j].action == actionRequire)
						capRequirments[j].flag = TRUE;
				}
				else if( capRequirments[j].checkValueLen == capabilities->capalities[i].length )
				{
					if( capRequirments[j].checkValueLen == 0
						|| memcmp(capRequirments[j].checkValue, capabilities->capalities[i].value, capRequirments[j].checkValueLen) == 0)
					{
						log_msg("find the longest matched capability %d", capabilities->capalities[i].code);
						if(capRequirments[j].action == actionRefuse)	
						{
							addOneBGPNewCapability(notifyPrm, &capabilities->capalities[i]);
							if( longestMatchFound ==  FALSE)
							{
								refusedFlag = TRUE;
								longestMatchFound = TRUE;
							}

						}
						else if(capRequirments[j].action == actionAllow)	
						{
							if( longestMatchFound == FALSE)
							{
								refusedFlag = FALSE;
								longestMatchFound = TRUE;
							}

						}						
						else if(capRequirments[j].action == actionRequire)
							capRequirments[j].flag = TRUE;
					}		
				}
			}
		}
		if( refusedFlag == TRUE )
		{
			log_warning("Capability NO %d is refused: code %d, len %d!", i, capabilities->capalities[i].code, capabilities->capalities[i].length);
#ifdef DEBUG			
			hexdump(LOG_INFO, capabilities->capalities[i].value, capabilities->capalities[i].length);
#endif			
			errFlag = TRUE;
		}
	}	

	// check if receive all required capabilities	
	for(i=0; i<len; i++)
	{
		if(capRequirments[i].action == actionRequire && capRequirments[i].flag == FALSE)
		{
			errFlag = TRUE;
			log_warning("Required capability NO %d missed!", i);
		}						
	}

	if(errFlag == TRUE)
	{
		log_warning("checkCapabilities failed!");
		sendNotificationMessage( session, 2, 7, (char *)notifyPrm->list, notifyPrm->length);
		destroyBGPOpenParameters( notifyPrm );
		return eventBGPUnsupportedCapability;
	}
	else
		return eventNone;
}

/*--------------------------------------------------------------------------------------
 * Purpose: receive a BGP open message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
receiveOpenMessage( Session_structp session )
{
#ifdef DEBUG
	debug("receiveOpenMessage","");
#endif	
	// first check for expired timers
	int event = checkTimers( session );
	if ( event != eventNone )
		return( event );
	
	PBgpHeader hdr = NULL;
	PBgpOpen opn = NULL;
	PBgpOpenParameters prm = NULL;
	PBgpCapabilities capabilities = NULL;
	PBgpNotification ntf = NULL;

	BMF bmf = NULL;
	int len;
	hdr = readBGPHeader( session->fsm.socket, session->fsm.connectRetryInt);
	if ( hdr == NULL )
	{
		event = eventTcpConnectionFails;
	}
	else
		switch ( getBGPHeaderType( hdr ) )
		{
			case typeOpen:
				#ifdef DEBUG
				log_msg("receiveOpenMessage: open");
				#endif
				// read the rest of the open message
				opn = readBGPOpen( session->fsm.socket );
				if ( opn == NULL )
				{
					event = eventTcpConnectionFails;
					break;
				}				
				prm = extractBGPOpenParameters( opn );
				if ( prm == NULL )
				{
					event = eventTcpConnectionFails;
					break;				
				}

				// check the received capabilities
				capabilities = extractBgpCapabilities(prm);
				
				// check the received open message
				event = checkOpenMessage(session, opn, capabilities);
				if ( event != eventNone )
					break;
			
				PeerCapabilityRequirement *capRquirements = NULL;
				event = checkCapabilities(session, session->configInUse.numOfCapReqs, session->configInUse.capRquirements, capabilities);
				free(capRquirements);
				if ( event != eventNone )
					break;
				
				// decide the holdtime and keepalivetime 
				if ( session->fsm.holdTime > getBGPOpenHoldTime( opn ))
				{
					session->fsm.holdTime = getBGPOpenHoldTime( opn );
					session->fsm.keepaliveInt = session->fsm.holdTime/3;
				}	
				
				session->fsm.peerCapabilities = capabilities;

				// check routerefresh capability
				if(checkOneBgpCapability(capabilities, routeRefreshCapability, 0, NULL) == 0)
					session->fsm.routeRefreshType= routeRefreshCapability;
				if(checkOneBgpCapability(capabilities, ciscoRefreshCapability, 0, NULL) == 0)
					session->fsm.routeRefreshType = ciscoRefreshCapability;

				// check the AS number length: 2 bytes or 4 bytes
				if(checkOneBgpCapability(capabilities, fourbytesASnumber, 0, NULL) == 0)
				{
					if(checkOneBgpAnnCapability(session->configInUse.announceCaps, session->configInUse.numOfAnnCaps, fourbytesASnumber, 0, NULL) == 0)
					{
						session->fsm.ASNumlen = 4;
					}	
					else
					{
						session->fsm.ASNumlen = 2;
					}	
				}
				else
				{
					session->fsm.ASNumlen = 2;
				}	
				
				
				// place Bgpmon in queue
				bmf = createBMF( session->sessionID, BMF_TYPE_MSG_FROM_PEER);
				bgpmonMessageAppend( bmf, hdr, 19);
				bgpmonMessageAppend( bmf, opn, 10);
				if( getBGPOpenOptionalParameterLength( opn ) != 0 )
					bgpmonMessageAppend( bmf, prm, getBGPOpenOptionalParameterLength( opn ));
				writeQueue( session->peerQueueWriter, bmf );		
#ifdef DEBUG				
				hexdump( LOG_INFO, bmf->message, bmf->length );
#endif				
			
				event = eventBGPOpen;
				break;
			
			case typeNotification:
				#ifdef DEBUG
				log_msg("receiveOpenMessage: notificatiob");
				#endif				
				len = getBGPHeaderLength( hdr );
				ntf = readBGPNotification( session->fsm.socket, len - 19 );
				if ( ntf == NULL )
				{
					event = eventTcpConnectionFails;
					break;
				}
				log_msg("code:%d subcode:%d len:%d", ntf->errorCode, ntf->errorSubcode, ntf->datalen);
				
				// place Bgpmon in queue
				bmf = createBMF( session->sessionID, BMF_TYPE_MSG_FROM_PEER);
				bgpmonMessageAppend( bmf, hdr, 19);
				bgpmonMessageAppend( bmf, ntf, len - 19);
				writeQueue( session->peerQueueWriter, bmf );
				
				event = eventNotificationMessage;
				break;
			
			default:
				#ifdef DEBUG
				log_msg("receiveOpenMessage: others");
				#endif				
				// not an expected type of message...
				event = eventBGPFSMErr;
				break;
		} 	
	destroyBGPOpen(opn);
	destroyBGPOpenParameters(prm);
	destroyBGPNotification( ntf );
	destroyBGPHeader( hdr );
	
	return( event );
}

/*--------------------------------------------------------------------------------------
 * Purpose: send a BGP keepalive message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int
sendKeepaliveMessage( Session_structp session )
{
#ifdef DEBUG
	debug("sendKeepaliveMessage","");
#endif

	int event = eventNone;
	PBgpHeader hdr = NULL;
	BMF bmf = NULL;
	 
	hdr = createBGPHeader( typeKeepalive );
	if ( writeBGPHeader( hdr, session->fsm.socket ) < 0 )
	{
		event = eventTcpConnectionFails;
	}
	else
	{
		bmf = createBMF(session->sessionID,  BMF_TYPE_MSG_TO_PEER);
		bgpmonMessageAppend( bmf, hdr, 19);
		writeQueue( session->peerQueueWriter, bmf );
	}	
	
	destroyBGPHeader( hdr );	
	return( event );
}


/*--------------------------------------------------------------------------------------
 * Purpose: receive a BGP keepalive message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
receiveKeepaliveMessage( Session_structp session )
{
	/* Processes an expected keep alive message during peer initialization.
	 */
#ifdef DEBUG
	debug("receiveKeepaliveMessage", "");
#endif	
	// first check for expired timers
	int event = checkTimers( session );
	if ( event != eventNone )
		return( event );
		
	PBgpHeader hdr = NULL;
	BMF bmf = NULL;
	PBgpNotification ntf = NULL;
	int len = 0;
	
	hdr = readBGPHeader( session->fsm.socket, session->fsm.keepaliveInt );
	if ( hdr == NULL )
	{
		event = eventTcpConnectionFails;
	}
	else
		switch ( getBGPHeaderType( hdr ) )
		{
			case typeKeepalive:
				bmf = createBMF(session->sessionID,  BMF_TYPE_MSG_FROM_PEER);
				bgpmonMessageAppend( bmf, hdr, 19);
				writeQueue( session->peerQueueWriter, bmf );
				event = eventKeepaliveMsg;	
				break;

			case typeNotification:
				len = getBGPHeaderLength( hdr );
				ntf = readBGPNotification( session->fsm.socket, len - 19 );
				if ( ntf == NULL )
				{
					event = eventTcpConnectionFails;
					break;
				}	

				bmf = createBMF(session->sessionID,  BMF_TYPE_MSG_FROM_PEER);
				bgpmonMessageAppend( bmf, hdr, 19);
				bgpmonMessageAppend( bmf, ntf, len - 19);
				writeQueue( session->peerQueueWriter, bmf );
			
				event = eventNotificationMessage;
				break;
			
			default:
				event = eventBGPHeaderErr;
		} 
	
	// clean up memory
	destroyBGPNotification( ntf );
	destroyBGPHeader( hdr );
	
	return( event );
}

/*--------------------------------------------------------------------------------------
 * Purpose: receive a route refresh message
 * Input:	the session structure
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
 int
sendRouteRefreshMessage( Session_structp session )
{
	/* Sends a route refresh message with appropriate afi/safi combinations
	 * that were specified in the open parameters.
	 * 
	 * NOTE: Two types of route refreshes are possible, one defined in the 
	 * standard and a CISCO specific version that is advertised in the 
	 * capabilities of older CISCO routers.
	 */
#ifdef DEBUG
	debug("sendrouterefreshMessage",""); 
#endif
	int event = eventNone;
	if ( session->fsm.routeRefreshType == 0 ) 
		return( event ); // do not send route refresh requests if capability not advertised
	
	int i;
	BMF bmf = NULL;
	for ( i = 0; event == eventNone && i < numOfOneBgpCapability( session->fsm.peerCapabilities, multiprotocolCapability); i++ )
	{
		u_int32_t afisafi = afisafiBGPOpenParameter( session->fsm.peerCapabilities, i );
		 
		PBgpHeader hdr = NULL; 
		PBgpRefresh refresh = NULL;
		hdr = createBGPHeader( session->fsm.routeRefreshType );  	
		refresh = createBGPRefresh( afisafi );	
		setBGPHeaderLength( hdr, lengthBGPRefresh( refresh ) );	
		
		if ( 	writeBGPHeader( hdr, session->fsm.socket ) >= 0 &&
					writeBGPRefresh( refresh, session->fsm.socket ) >= 0 	 )  
		{
			event = eventNone;
				
			bmf = createBMF(session->sessionID,  BMF_TYPE_MSG_TO_PEER);
			bgpmonMessageAppend( bmf, hdr, 19);
			bgpmonMessageAppend( bmf, refresh, 4);
			writeQueue( session->peerQueueWriter, bmf );
		
			session->stats.lastRouteRefresh = time( NULL );
		}
		else
		{
			event = eventTcpConnectionFails;
		}
		destroyBGPRefresh( refresh );	
		destroyBGPHeader( hdr );
	}	
	return( event );
}


/*--------------------------------------------------------------------------------------
 * Purpose: receive a BGP message
 * Input:	the session structure
 * Output: Event to indicate the received message or the error
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
receiveBGPMessage( Session_structp session )
{
#ifdef DEBUG
	debug("receiveBGPMessage", "");
#endif
	int event = checkTimers( session );
	
	if ( event != eventNone )
		return( event );
	
	// must have something to read, get the header first.
	PBgpHeader hdr = NULL;
	PBgpUpdate update = NULL;
	PBgpNotification notify = NULL;
	PBgpRefresh refresh = NULL;

	BMF bmf = NULL;	

	hdr = readBGPHeader( session->fsm.socket, session->fsm.keepaliveInt);
	if ( hdr == NULL )
	{
#ifdef DEBUG
log_err("receiveBGPMessage: readBGPHeader failed!");
#endif
		event = eventTcpConnectionFails;
	}
	else
		switch ( getBGPHeaderType( hdr ) )
		{	 
			case typeKeepalive:
				#ifdef DEBUG
				log_msg("receiveBGPMessage: keepalive");
				#endif
				event = eventKeepaliveMsg;
				
				bmf = createBMF(session->sessionID,  BMF_TYPE_MSG_FROM_PEER);
				bgpmonMessageAppend( bmf, hdr, 19);
				writeQueue( session->peerQueueWriter, bmf );	
				
				session->stats.messageRcvd++;
				break;
				
			case typeUpdate:
				#ifdef DEBUG
				log_msg("receiveBGPMessage: update");
				#endif
				event = eventUpdateMsg;
				// read the rest of the update message
				update = readBGPUpdate( session->fsm.socket, getBGPHeaderLength( hdr )-19 );
				if ( update == NULL)
				{
					event = eventTcpConnectionFails;
				}
				else
				{
					bmf = createBMF(session->sessionID,  BMF_TYPE_MSG_FROM_PEER);
					bgpmonMessageAppend( bmf, hdr, 19);
					bgpmonMessageAppend( bmf, update, getBGPHeaderLength(hdr)-19);
					//if the message creation failed, don't enqueue the message!
					if(bmf->length < getBGPHeaderLength(hdr)) break;

					writeQueue( session->peerQueueWriter, bmf );
					
					session->stats.messageRcvd++;	
				}
				break;
				
			case typeNotification:
				
				event = eventNotificationMessage;
				// read the rest of the notification message
				notify = readBGPNotification( session->fsm.socket, getBGPHeaderLength( hdr )-19 );
				if ( notify == NULL )
				{
					event = eventTcpConnectionFails;
					break;
				}
				
				bmf = createBMF(session->sessionID,  BMF_TYPE_MSG_FROM_PEER);
				bgpmonMessageAppend( bmf, hdr, 19);
				bgpmonMessageAppend( bmf, notify, getBGPHeaderLength(hdr)-19);
				writeQueue( session->peerQueueWriter, bmf );	
				
				session->stats.messageRcvd++;	
				#ifdef DEBUG
				log_msg("receiveBGPMessage: notification: %d %d", notify->errorCode, notify->errorSubcode);
				#endif
				break;
				
			case typeRouteRefresh:
				// we don't support
				event = eventUpdateMsgErr;
				session->stats.messageRcvd++;
				break;
				
			case typeOpen:
				event = eventUpdateMsgErr;
				session->stats.messageRcvd++;
				break;
				
			default:
				event = eventUpdateMsgErr;
				session->stats.messageRcvd++;
				break;
		}
	
	// cleanup memory
	destroyBGPRefresh(refresh);
	destroyBGPNotification(notify);
	destroyBGPUpdate(update);
	destroyBGPHeader( hdr );
	
	return( event );	
}
	
	
/*--------------------------------------------------------------------------------------
 * Purpose: send out a BGP notification message
 * Input:	the session structure and notification code, subcode, value and length
 * Output: Event to indicate if it successes
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
sendNotificationMessage( Session_structp session, u_int8_t code, u_int8_t subcode, char *msg, int len )
{
	/*
	 */
#ifdef DEBUG
	debug("sendNotificationMessage", "");
#endif
	int event = eventNone;

	if ( session->fsm.socket == -1 ) // can we send the message?
		return( event );
	
	PBgpHeader hdr = NULL;
	PBgpNotification ntf = NULL;
	BMF bmf = NULL;
	 
	hdr = createBGPHeader( typeNotification );
	ntf = createBGPNotification( code, subcode, msg, len );
	
	setBGPHeaderLength( hdr, lengthBGPNotification( ntf ) );	
	
	if ( 	writeBGPHeader( hdr, session->fsm.socket ) >= 0 &&
				writeBGPNotification( ntf, session->fsm.socket ) >= 0 )
		{
#ifdef DEBUG
	debug("sendNotificationMessage: %x%x","",hdr,ntf);
#endif
			event = eventNone;
			bmf = createBMF(session->sessionID,  BMF_TYPE_MSG_TO_PEER);
			bgpmonMessageAppend( bmf, hdr, 19);
			bgpmonMessageAppend( bmf, ntf, lengthBGPNotification( ntf ));
			writeQueue( session->peerQueueWriter, bmf );	
		}
	else
		event = eventTcpConnectionFails;
	
	destroyBGPNotification( ntf );
	destroyBGPHeader( hdr );
	
	return( event );
}


/*--------------------------------------------------------------------------------------
 * Purpose: decide the nex step of a session
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void
nextStepOfSession( Session_structp session )
{
	// timeout values
	time_t now = time(NULL);
	struct timeval timeout; 
	timeout.tv_usec = 0;
	timeout.tv_sec = 0; 
	int select_status = 0;
	// sockets monitoring values
	int fdMax = 0;
	fd_set sockets;
	FD_ZERO(&sockets); 

	if ( session->fsm.state == stateIdle )
		return;
		
	if ( sessionTimer( session ) <= now )
	{
		// timeout already occurred
		return; 
	}
	else
	{
		// earliest future timeout
		timeout.tv_sec = sessionTimer( session) - now ;
	}

	if ( session->fsm.socket> 0 )
	{
		// check for data if the Session_structp has an open socket.
		FD_SET( session->fsm.socket, &sockets);
		if ( session->fsm.socket + 1 > fdMax )
			fdMax = session->fsm.socket + 1;
	}
	//log_msg( "nextStepOfSession %d", timeout.tv_sec);
	// wait for timeout or available data
	if ( (select_status = select(fdMax, &sockets, NULL, NULL, &timeout)) < 0 )
		log_fatal( "NextStepOfSession: select error");	
	//log_msg( "nextStepOfSession");
	return;
}

/*--------------------------------------------------------------------------------------
 * Purpose: stub function to initialize resources
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void initializeResources( Session_structp seer )
{
}


/*--------------------------------------------------------------------------------------
 * Purpose: stub function to complete initialization
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
 void completeInitialization( Session_structp seer )
{
}


/*--------------------------------------------------------------------------------------
 * Purpose: reset resource of a session
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void releaseResources( Session_structp session)
{
	if( session->fsm.peerCapabilities != NULL )
	{
		free(session->fsm.peerCapabilities);
		session->fsm.peerCapabilities = NULL;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: reset a session to state idle
 * Input:	the session structure and the reason of reseting a session
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void resetSession( Session_structp session, int reason )
{
#ifdef DEBUG
	debug(__FUNCTION__, "");
#endif
	dropConnection( session ); 
	releaseResources( session);
	session->stats.connectRetryCount += 1;
	zeroSessionConnectRetryTimer( session );
	zeroSessionHoldTimer( session );
	zeroSessionKeepaliveTimer( session );
	setSessionState( session, stateIdle, reason );
}


/*--------------------------------------------------------------------------------------
 * Purpose: clean up a session when it ends
 * Input:	the session structure
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void cleanupSession( Session_structp session )
{
#ifdef DEBUG
	debug(__FUNCTION__, "");
#endif

	BMF bmf = NULL;
	int oldState = session->fsm.state;

	// send the cease notification 
	sendNotificationMessage( session, 6, 0, NULL, 0 );

	// reset the session as usual
	resetSession( session, eventManualStop );

	// clear the thread control flags
	session->reconnectFlag = FALSE;
	session->lastAction = time(NULL);
	
	// write state change message to the queue
	bmf = createStateChangeMsg( session->sessionID, oldState, session->fsm.state, session->fsm.reason );
	writeQueue( session->peerQueueWriter, bmf );		

	// peer queue writer cleanup
	//destroyQueueWriter(session->peerQueueWriter);
	//session->peerQueueWriter = NULL;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the 6 tuple string of a session
 * Input:	the session's ID and the direction flag
 * Output: the 6 tuple string 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
char *getSessionString(int sessionID, int type)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionString: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return NULL;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionString: couldn't find a session with ID:%d", sessionID);
		return NULL;
	}
	
	if( type == BMF_TYPE_MSG_TO_PEER )
	{
		if( Sessions[sessionID]->sessionStringOutgoing != NULL )
			return Sessions[sessionID]->sessionStringOutgoing;
		else
			return NULL;
	}
	else
	{
		if( Sessions[sessionID]->sessionStringIncoming!= NULL )
			return Sessions[sessionID]->sessionStringIncoming;
		else
			return NULL;
	}	
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the route refresh type of the specified session
 * Input:	the session ID
 * Output: FALSE means this session doesn't support route refresh.
 *		  TRUE means this session supports route refresh.	
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionRouteRefreshType( int sessionID )
{	
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionRouteRefreshType: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionRouteRefreshType: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	
	// return the type
	if( Sessions[sessionID]->fsm.routeRefreshType == 0)
		return FALSE;
	else
		return TRUE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the route refresh flag of a session
 * Input:	the session ID
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setSessionRouteRefreshFlag( int sessionID )
{	
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("setSessionRouteRefreshFlag: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("setSessionRouteRefreshFlag: couldn't find a session with ID:%d", sessionID);
		return -1;
	}

	// set the flag
	Sessions[sessionID]->fsm.routeRefreshFlag = TRUE;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: check if the state change message is from stateOpenSent, stateOpenConfirm or stateEstablished
 		 to stateIdle
 * Input:	the state change message in BMF
 * Output: FALSE means no
 *         	   TRUE means yes
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int checkStateChangeMessage( BMF bmf )
{
	StateChangeMsg *stateChangMsg;
	stateChangMsg = (StateChangeMsg *)(bmf->message);
	if( stateChangMsg->oldState != stateConnect && stateChangMsg->newState == stateIdle )
		return TRUE;
	else
		return FALSE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: set the 6 tuple string of a session
 * Input:	the session's ID and the six tuple
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setSessionString(int sessionID, char *srcAddr, u_int16_t srcPort, u_int32_t srcAS, char *dstAddr, u_int16_t dstPort, u_int32_t dstAS)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("setSessionString: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("setSessionString: couldn't find a session with ID:%d", sessionID);
		return;
	}
	
	// free the peering strings
	free(Sessions[sessionID]->sessionStringIncoming);
	free(Sessions[sessionID]->sessionStringOutgoing);

	// Set session string
	// Note: No long need to set the session string in advance. Session string would be generated dynamically. 
}

/*--------------------------------------------------------------------------------------
 * Purpose: get label action of a session
 * Input:	the session's ID
 * Output: label action
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionLabelAction(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLabelAction: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionLabelAction: couldn't find a session with ID:%d", sessionID);
		return -1;
	}	
	return Sessions[sessionID]->configInUse.labelAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: set label action of a session
 * Input:	the session's ID and label action
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setSessionLabelAction(int sessionID, int labelAction)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("setSessionLabelAction: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("setSessionLabelAction: couldn't find a session with ID:%d", sessionID);
		return;
	}
	
	if( Sessions[sessionID]->configInUse.labelAction != labelAction )
	{
		log_msg("setSessionLabelAction: from %d to %d", Sessions[sessionID]->configInUse.labelAction, labelAction);
		if( Sessions[sessionID]->configInUse.labelAction == NoAction )
		{
			createPrefixTable(Sessions[sessionID]->sessionID, PREFIX_TABLE_SIZE, MAX_HASH_COLLISION);
			createAttributeTable(Sessions[sessionID]->sessionID, ATTRIBUTE_TABLE_SIZE, MAX_HASH_COLLISION);
		}
		else
		{
			if( labelAction == NoAction )
				deleteRibTable(sessionID);
		}
		Sessions[sessionID]->configInUse.labelAction = labelAction;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: get route refresh action of a session
 * Input:	the session's ID
 * Output: route refresh action
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionRouteRefreshAction(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionRouteRefreshAction: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionRouteRefreshAction: couldn't find a session with ID:%d", sessionID);
		return -1;
	}	
	return Sessions[sessionID]->configInUse.routeRefreshAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: set route refresh action of a session
 * Input:	the session's ID and route refresh action
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setSessionRouteRefreshAction(int sessionID, int rrAction)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("setSessionRouteRefreshAction: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("setSessionRouteRefreshAction: couldn't find a session with ID:%d", sessionID);
		return;
	}
	
	Sessions[sessionID]->configInUse.routeRefreshAction = rrAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the last action time of a session
 * Input:	the session's ID
 * Output: last action time
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getSessionLastActionTime(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLastActionTime: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionLastActionTime: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->lastAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get current state of a session
 * Input:	the session's ID
 * Output: the current state
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionState(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionState: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionState: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->fsm.state;
}

/*--------------------------------------------------------------------------------------
 * Purpose: set the connect retry count of a session
 * Input:	the session's ID and connect retry count
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void setSessionConnectRetryCount(int sessionID, int count)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("setSessionConnectRetryCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("setSessionConnectRetryCount: couldn't find a session with ID:%d", sessionID);
		return;
	}
	Sessions[sessionID]->stats.connectRetryCount = count;
}

/*--------------------------------------------------------------------------------------
 * Purpose: set the connect retry count of a session
 * Input:	the session's ID
 * Output: the connect retry count
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionConnectRetryCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionConnectRetryCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionConnectRetryCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.connectRetryCount;
}

/*--------------------------------------------------------------------------------------
 * Purpose: increment the message count of a session by 1
 * Input:	the session's ID and the state
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void incrementSessionMsgCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("incrementSessionMsgCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("incrementSessionMsgCount: couldn't find a session with ID:%d", sessionID);
		return;
	}

	Sessions[sessionID]->stats.messageRcvd++;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the message count of a session
 * Input:	the session's ID
 * Output: the message count
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
long getSessionMsgCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionMsgCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionMsgCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.messageRcvd;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the down count of a session
 * Input:	the session's ID
 * Output: session down count 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionDownCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionDownCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionDownCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.sessionDownCount;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the last session down time
 * Input:	the session's ID
 * Output: the last session down time
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getSessionLastDownTime(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLastDownTime: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionLastDownTime: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.lastDownTime;
}


/*--------------------------------------------------------------------------------------
 * Purpose: get the last route refresh time of a session
 * Input:	the session's ID
 * Output: last route refresh time
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getSessionLastRouteRefreshTime(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLastRouteRefreshTime: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionLastRouteRefreshTime: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.lastRouteRefresh;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the prefix count of a session
 * Input:	the session's ID
 * Output: the prefix count 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionPrefixCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionPrefixCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionPrefixCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.prefixCount;
}


/*--------------------------------------------------------------------------------------
 * Purpose: get the attribute count of a session
 * Input:	the session's ID
 * Output: the attribute count 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionAttributeCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionAttributeCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionAttributeCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.attrCount;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the memory usage of a session
 * Input:	the session's ID
 * Output: the number of bytes used by the session
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
long getSessionMemoryUsage(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionMemoryUsage: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionMemoryUsage: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.memoryUsed;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of new announcements since session gets established
 * Input:	the session's ID
 * Output: the number of new announcements
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionNannCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionNannCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionNannCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.nannRcvd;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of new announcements since last status message
 * Input:	the session's ID
 * Output: the number of new announcements
 * Peichun Cheng @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionCurrentNannCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionNannCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionNannCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}

	int current = Sessions[sessionID]->stats.nannRcvd - Sessions[sessionID]->stats.lastSentNannRcvd;
	Sessions[sessionID]->stats.lastSentNannRcvd = Sessions[sessionID]->stats.nannRcvd;

	return current;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of duplicate announcements since session gets established
 * Input:	the session's ID
 * Output: the number of duplicate announcements
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionDannCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionDannCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionDannCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.dannRcvd;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of duplicate announcements since last status message
 * Input:	the session's ID
 * Output: the number of duplicate announcements
 * Peichun Cheng @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionCurrentDannCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionDannCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionDannCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	int current = Sessions[sessionID]->stats.dannRcvd - Sessions[sessionID]->stats.lastSentDannRcvd;
	Sessions[sessionID]->stats.lastSentDannRcvd = Sessions[sessionID]->stats.dannRcvd;

	return current;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of announcements with same AS path since session gets established
 * Input:	the session's ID
 * Output: the number of announcements with same AS path
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionSPathCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionSPathCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionSPathCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.spathRcvd;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of announcements with same AS path since last status message
 * Input:	the session's ID
 * Output: the number of announcements with same AS path
 * Peichun Cheng Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionCurrentSPathCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionSPathCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionSPathCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	int current = Sessions[sessionID]->stats.spathRcvd - Sessions[sessionID]->stats.lastSentSpathRcvd;
	Sessions[sessionID]->stats.lastSentSpathRcvd = Sessions[sessionID]->stats.spathRcvd;

	return current;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of announcements with different AS path since session gets established
 * Input:	the session's ID
 * Output: the number of announcements with different AS path
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionDPathCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionDPathCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionDPathCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.dpathRcvd;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of announcements with different AS path since last status message
 * Input:	the session's ID
 * Output: the number of announcements with different AS path
 * Peichun Cheng @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionCurrentDPathCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionDPathCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionDPathCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}

	int current = Sessions[sessionID]->stats.dpathRcvd - Sessions[sessionID]->stats.lastSentDpathRcvd;
	Sessions[sessionID]->stats.lastSentDpathRcvd = Sessions[sessionID]->stats.dpathRcvd;

	return current;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of withdrawals since session gets established
 * Input:	the session's ID
 * Output: the number of withdrawals
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionWithCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionWithCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionWithCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.withRcvd;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of withdrawals since last status message
 * Input:	the session's ID
 * Output: the number of withdrawals
 * Peichun Cheng @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionCurrentWithCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionWithCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionWithCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}

	int current = Sessions[sessionID]->stats.withRcvd - Sessions[sessionID]->stats.lastSentWithRcvd;
	Sessions[sessionID]->stats.lastSentWithRcvd = Sessions[sessionID]->stats.withRcvd;

	return current;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of duplicate withdrawals since session gets established
 * Input:	the session's ID
 * Output: the number of duplicate withdrawals
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionDWithCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionDWithCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionDWithCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.duwiRcvd;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the number of duplicate withdrawals since last status message
 * Input:	the session's ID
 * Output: the number of duplicate withdrawals
 * Peichun Cheng @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionCurrentDWithCount(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionDWithCount: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionDWithCount: couldn't find a session with ID:%d", sessionID);
		return -1;
	}

	int current = Sessions[sessionID]->stats.duwiRcvd - Sessions[sessionID]->stats.lastSentDuwiRcvd;
	Sessions[sessionID]->stats.lastSentDuwiRcvd = Sessions[sessionID]->stats.duwiRcvd;

	return current;
}




/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's remote address
 * Input:	the session's ID
 * Output: the address in string
 *             NULL if there is no session with this ID or the session doesn't have a remote address
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getSessionRemoteAddr(int sessionID)
{
	// check the session ID is valid
	if( sessionID >= MAX_SESSION_IDS )
	{
		log_err("getSessionRemoteAddr: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return NULL;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionRemoteAddr: couldn't find a session with ID:%d", sessionID);
		return NULL;
	}
	if( strlen(Sessions[sessionID]->configInUse.remoteAddr) == 0 )
		return NULL;
	else
		return Sessions[sessionID]->configInUse.remoteAddr;			
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote port of the session
 * Input:   the session's ID
 * Output: the session's remote port
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionRemotePort(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionRemotePort: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionRemotePort: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->configInUse.remotePort;		
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote BGP ID of the session
 * Input:   the session's ID
 * Output: the session's remote BGP ID
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getSessionRemoteBGPID(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionRemoteBGPID: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionRemoteBGPID: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->configInUse.remoteBGPID;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote AS number of the session
 * Input:   the session's ID
 * Output: the session's remote AS number
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getSessionRemoteASNum( int sessionID )
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionRemoteASNum: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return 0;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionRemoteASNum: couldn't find a session with ID:%d", sessionID);
		return 0;
	}
	return Sessions[sessionID]->configInUse.remoteAS2;		
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's remote BGP version
 * Input:	the session's ID
 * Output: the remote BGP Versionn
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionRemoteBGPVersion( int sessionID )
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionRemoteBGPVersion: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionRemoteBGPVersion: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->configInUse.remoteBGPVer;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's required min hold time
 * Input:	the session's ID
 * Output: the required min hold time
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionRemoteHoldTime( int sessionID )
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionRemoteHoldTime: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionRemoteHoldTime: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->configInUse.remoteMinHoldtime;		
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's MD5 password
 * Input:	the session's ID
 * Output: the lMD5 password
 *             NULL if there is no session with this ID or the session doesn't have MD5 password
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getSessionMD5Passwd(int sessionID)
{
	// check the session ID is valid
	if( sessionID >= MAX_SESSION_IDS )
	{
		log_err("getSessionMD5Passwd: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return NULL;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionMD5Passwd: couldn't find a session with ID:%d", sessionID);
		return NULL;
	}
	if( strlen(Sessions[sessionID]->configInUse.md5Passwd) == 0 )
		return NULL;
	else
		return Sessions[sessionID]->configInUse.md5Passwd;		
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the session's local address
 * Input:	the session's ID
 * Output: the local address
 *            NULL if there is no session with this ID the session has no local address
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getSessionLocalAddr(int sessionID)
{	
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLocalAddr: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return NULL;
	}
	if( Sessions[sessionID] == NULL )
	{	
		log_err("getSessionLocalAddr: couldn't find a session with ID:%d", sessionID);
		return NULL;
	}

	if( strlen(Sessions[sessionID]->configInUse.localAddr) == 0)
	{
		return NULL;
	}
	else
	{
		return Sessions[sessionID]->configInUse.localAddr;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local port of the session
 * Input:   the session's ID
 * Output: the session's local port
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionLocalPort(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLocalPort: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionLocalPort: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->configInUse.localPort;		
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local BGP ID of the session
 * Input:   the session's ID
 * Output: the session's local BGP ID
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getSessionLocalBGPID(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLocalBGPID: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionLocalBGPID: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->configInUse.localBGPID;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local AS number of the session
 * Input:   the session's ID
 * Output: the session's local AS number
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getSessionLocalASNum( int sessionID )
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLocalASNum: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return 0;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionLocalASNum: couldn't find a session with ID:%d", sessionID);
		return 0;
	}
	return Sessions[sessionID]->configInUse.localAS2;		
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local BGP version of the session
 * Input:   the session's ID
 * Output: the session's local BGP version
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionLocalBGPVersion( int sessionID )
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLocalBGPVersion: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionLocalBGPVersion: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->configInUse.localBGPVer;		
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the local holdtime of the session
 * Input:   the session's ID
 * Output: the session's local holdtime
 *         or -1 if there is no session with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getSessionLocalHoldTime( int sessionID )
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionLocalHoldTime: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionLocalHoldTime: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->configInUse.localHoldtime;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get session by ID
 * Input:	the session's ID
 * Output: the pointer to session  
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
Session_structp getSessionByID(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionByID: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return NULL;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionByID: couldn't find a session with ID:%d", sessionID);
		return NULL;
	}
	return Sessions[sessionID];
}

/*--------------------------------------------------------------------------------------
 * Purpose: check if the specified session is established
 * Input:	the session's ID
 * Output: FALSE means this session is not established
 *		  TRUE means the session is established
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int isSessionEstablished( int sessionID )
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("isSessionEstablished: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is configured
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("isSessionEstablished: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	
	if( (Sessions[sessionID]->fsm.state == stateEstablished) ||
	    (Sessions[sessionID]->fsm.state == stateMrtEstablished) )
		return TRUE;
	else
		return FALSE;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the establish time of a session
 * Input:	the session's ID
 * Output: the establish time 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getSessionEstablishTime(int sessionID)
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionEstablishTime: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionEstablishTime: couldn't find a session with ID:%d", sessionID);
		return -1;
	}
	return Sessions[sessionID]->stats.establishTime;
}

/*--------------------------------------------------------------------------------------
 * Purpose: return up time of a specified session
 * Input:	the session's ID
 * Output: the up time
 		  -1 if the session is not existing or is not established
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long getSessionUPTime( int sessionID )
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("getSessionUPTime: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is configured
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("getSessionUPTime: couldn't find a session with ID:%d", sessionID);
		return -1;
	}

	// check if the session is established or mrtEstablished
	if( (Sessions[sessionID]->fsm.state == stateMrtEstablished) || (Sessions[sessionID]->fsm.state == stateEstablished) ) 
	{
		return (time(NULL)-Sessions[sessionID]->stats.establishTime);
	}
	else
	{
			log_err("getSessionUPTime: the sessioon %d is not established", sessionID);
			return -1;
	}
	

	
}

/*--------------------------------------------------------------------------------------
 * Purpose: return AS number length of a specified session
 * Input:       the session's ID
 * Output: the up time
 * 	2 or 4
 *      -1 if the session is not existing or is not established
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getSessionASNumberLength( int sessionID )
{
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
        {
                log_err("getSessionASNumberLength: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
                return -1;
        }
	// check if the session is configured
	if( Sessions[sessionID] == NULL )
        {
                log_err("getSessionASNumberLength: couldn't find a session with ID:%d", sessionID);
                return -1;
        }

	// return AS number length
	return Sessions[sessionID]->fsm.ASNumlen;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the ASNumLen of a session
 * Input:	the session ID, value of ASNumLen, 2 or 4
 * Output: 
 * Mikhail Strizhov @ Mar 2, 2010
 * -------------------------------------------------------------------------------------*/ 
int setSessionASNumberLength( int sessionID, int ASNumLen )
{	
	// check the session ID is valid
	if (sessionID >= MAX_SESSION_IDS)
	{
		log_err("setSessionASNumberLenght: session ID %d exceeds max %d", sessionID, MAX_SESSION_IDS);
		return -1;
	}
	// check if the session is existing
	if( Sessions[sessionID] == NULL ) 
	{
		log_err("setSessionASNumberLenght: couldn't find a session with ID:%d", sessionID);
		return -1;
	}

	// check if we've got correct value of ASNumLen
	if ( (ASNumLen != 2) && (ASNumLen !=4))
	{
		log_err("setSessionMrtTableFlag: ASNum is %d", ASNumLen);
		return -1;
	}
	// set value
	Sessions[sessionID]->fsm.ASNumlen = ASNumLen;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Print Sessions Array
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void printAllSessions()
{
	int i;
	for(i = 0; i < MAX_SESSION_IDS; i++)
	{
		if( Sessions[i] != NULL )
		{
			log_msg("Session %d", Sessions[i]->sessionID);
			log_msg("Incoming String %s", Sessions[i]->sessionStringIncoming);
			log_msg("Outgoing String %s", Sessions[i]->sessionStringOutgoing);
		}
	}
}


/*--------------------------------------------------------------------------------------
 * Purpose: the main function of a peer
 * Input:	the peer ID
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void *
peerThreadFunction( void *arg )
{
	int *ppeerID = arg;

	int peerID = *ppeerID;
	// peer thread started
	log_msg("The thread for peer(peerID:%d) starting!",peerID);
	// create a new session
	int sessionID = createPeerSessionStruct( peerID, 0, 0 );	
	if( sessionID == -1 )
	{
		log_msg("closing thread as peer %d is deleted!", peerID);	
		pthread_exit( NULL );
	}
	

	// main loop starts
	while( getPeerEnabledFlag(peerID) == TRUE )
	{
		// set the session ID
		if( setPeerSessionID(peerID, sessionID) < 0 )
		{			
			log_msg("closing thread as peer %d is deleted!", peerID);
			break;
		}
		
		// set the last action time
		Sessions[sessionID]->lastAction = time(NULL);

		// store the state before running FSM
		int oldState = Sessions[sessionID]->fsm.state;
		
		// run BGP finite state machine
		nextStepOfSession( Sessions[sessionID] );
  		bgpFiniteStateMachine( Sessions[sessionID] );

		// display the state change 
		if( Sessions[sessionID]->fsm.state != oldState )
		{
			log_msg("peer %d's is changed from %d to %d", peerID, oldState, Sessions[sessionID]->fsm.state);
		}

		// check the reconnect flag is true or session state is changed to idle
		if( Sessions[sessionID]->reconnectFlag == TRUE || ( oldState != stateConnect && Sessions[sessionID]->fsm.state == stateIdle ))
		{
			// clean up the current session
			int downCount = Sessions[sessionID]->stats.sessionDownCount;
			time_t lastDownTime = Sessions[sessionID]->stats.lastDownTime;
			cleanupSession( Sessions[sessionID] );

			// create a new session with the latest peer configuration
			sessionID = createPeerSessionStruct( peerID, downCount, lastDownTime );
			if( sessionID == -1 )
			{
				log_msg("session(%d): closing thread as peer %d is deleted!", sessionID, peerID);	
				pthread_exit( NULL );	
			}
		}
		else if( Sessions[sessionID]->fsm.state != oldState )
		{
			BMF bmf = NULL;
			bmf = createStateChangeMsg( sessionID, oldState, Sessions[sessionID]->fsm.state, Sessions[sessionID]->fsm.reason );
			writeQueue( Sessions[sessionID]->peerQueueWriter, bmf );		
		}
		
		// update the session configuration in use, for the session which bounces between idle and connect 
		if( Sessions[sessionID]->fsm.state == stateConnect && Sessions[sessionID]->stats.connectRetryCount != 0 )
		{
			if( setSessionConfigInUse(peerID, Sessions[sessionID]) < 0 )
			{
				log_msg("session(%d): closing thread as peer %d is deleted!", sessionID, peerID);
				break;
			}
			else
			{
				setSessionString(sessionID, Sessions[sessionID]->configInUse.remoteAddr, Sessions[sessionID]->configInUse.remotePort, Sessions[sessionID]->configInUse.remoteAS2, 
									Sessions[sessionID]->configInUse.localAddr, Sessions[sessionID]->configInUse.localPort, Sessions[sessionID]->configInUse.localAS2);
				strcpy(Sessions[sessionID]->sessionRealSrcAddr, Sessions[sessionID]->configInUse.localAddr);
			}
			if( getPeerLabelAction(peerID) < 0 )
			{
				log_msg("session(%d): closing thread as peer %d is deleted!", sessionID, peerID);
				break;
			}
			else
				setSessionLabelAction(sessionID, getPeerLabelAction(peerID));
		}

		// check the route refresh flag
		if( Sessions[sessionID]->fsm.routeRefreshFlag == 1 && Sessions[sessionID]->fsm.state == stateEstablished )
		{
			log_msg( "session(%d): Send a route refresh!", sessionID);
			Sessions[sessionID]->fsm.routeRefreshFlag = 0;
			int event = sendRouteRefreshMessage( Sessions[sessionID] );
			if( event != eventNone )
			{
				log_err("session(%d) was reset! Reason:%d", sessionID, event);
				resetSession( Sessions[sessionID], event);
			}
		}			
	}

	log_msg("peer %d! is closing", peerID);	
	
	// if the peer is disabled, cleanup the session
	cleanupSession(Sessions[sessionID]);
	
	pthread_exit( NULL );	
}


/*--------------------------------------------------------------------------------------
 * Purpose: Change the session state to error, if mrt update client disconnect
 * 	 	and try to erase prefixes and attributes associated with this session ID
 * Input:  mrt node ID
 * Output:  0 - success, -1 failure
 * Mikhail Strizhov @ Sept 27, 2010
 * -------------------------------------------------------------------------------------*/
void deleteMrtSession(int mrtid  )
{

	int i = 0;
	int establishedSessions[MAX_SESSION_IDS];
	memset(establishedSessions, 0, MAX_SESSION_IDS);
	int establishedSessionCount=0;		
	Session_structp session = NULL;


	// get array of all established(live) sessions
	for(i=0; i<MAX_SESSION_IDS; i++ )
	{
		// check Sessions and check Sessions trigger = established or mrtestablished
		if( Sessions[i] != NULL && isSessionEstablished(Sessions[i]->sessionID) == TRUE )
		{
			establishedSessions[establishedSessionCount] = Sessions[i]->sessionID;
			establishedSessionCount++;
		}
	}
	
	// for degug
	// char mrtAddr[] = "128.223.51.102";
	
	// get MRT address node which all sessions were created	
	char * mrtAddr = NULL;
	mrtAddr = getMrtAddress(mrtid);
	if (mrtAddr != NULL)
	{
		mrtAddr[ADDR_MAX_CHARS] = '\n';
#ifdef DEBUG	
		log_warning("Client Mrt IP address: %s", mrtAddr);
		log_msg("deleteMrtSession, Number of established SessionIDs is %d", establishedSessionCount);
#endif	

		for (i=0; i<establishedSessionCount; i++)
		{
			session = getSessionByID(establishedSessions[i]);
			if (session)
			{
				// check if the node ip address is 
				if( (strcmp(getSessionLocalAddr(establishedSessions[i]),mrtAddr)==0))
				{
					log_warning("Changing state of Session ID %d to stateError", establishedSessions[i]);
					setSessionState(session, stateError, eventManualStop);
					log_warning("Erasing prefix and BGP attribute table for Session ID %d", establishedSessions[i]);
					// try to delete prefix and attribute tables
					if (cleanRibTable(session->sessionID)<0)
					{
						log_warning("Could not clear table for Session ID %d", establishedSessions[i]);
					}
				} 
			}
		}
	free(mrtAddr);	
	} // NULL check
}



