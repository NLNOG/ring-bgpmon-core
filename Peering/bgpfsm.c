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
 *  File: bgpfsm.c
 * 	Authors: He Yan
 *  Date: Jun 20, 2008
 */

/* 
 * Implements BGP finite state machine described in section 8.2 of RFC 4271.
 */

#include "bgpstates.h" 
#include "bgpevents.h"
#include "peersession.h"
#include "../Util/log.h"

#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <pthread.h>

//#define DEBUG
void 
fsmIdle( Session_structp s )
{
	/*
	 * Initial and restart state.
	 */
#ifdef DEBUG
	debug( "fsmIdle", "");
#endif	
	int event = eventAutomaticStart;
	
	switch (event)
	{
		case eventAutomaticStart:
			initializeResources( s );
			restartSessionConnectRetryTimer( s );
			zeroSessionKeepaliveTimer( s );
			zeroSessionHoldTimer( s );
			initiateConnection( s );   
			setSessionState( s, stateConnect, event );
			break; 
		
		default:
			//ignored
			break;
	}
}

void 
fsmConnect(Session_structp s)
{
	/*
	 * TCP connection is made.
	 */		
#ifdef DEBUG
	debug( "fsmConnect", "");
#endif	
	int event = checkTimers( s );
	switch ( event )
	{

		case eventConnectRetryTimer_Expires:
			event = completeConnection( s );
			switch ( event )
			{
				case eventTcpConnectionConfirmed: 
					zeroSessionConnectRetryTimer( s );
					zeroSessionConnectRetryCount( s );
					restartLargeSessionHoldTimer( s );
					completeInitialization( s );
					setSessionState( s, stateOpenSent, eventTcpConnectionConfirmed );
					if ( sendOpenMessage( s ) != eventNone )
						resetSession( s, eventTcpConnectionFails); 
					break;
					
				
				case eventTcpConnectionFails:
				default:
					resetSession( s, eventTcpConnectionFails);
					break;
			}
			break;
			
		default:
			log_fatal( "fsmConnect(%d): Unexpected(%d) timer timed out!!!", s->sessionID, event );
			break;			
	}	
	return ;
}

void 
fsmActive( Session_structp s )
{
	/*
	 * FSM is trying to acquire a peer by listening for, and accepting a TCP connection.
	 * 
	 */
	 
#ifdef DEBUG
	debug( "fsmActive", "" );
#endif
	return ;
}

void 
fsmOpenSent( Session_structp s )
{
	/*
	 * BGP Opens are exchanged.
	 * Expecting to receive a valid BGP Open message.
	 */
#ifdef DEBUG
	debug("fsmOpenSent", "");
#endif
	int event = receiveOpenMessage( s );
			
	switch ( event )
	{
		case eventBGPOpen:
			zeroSessionConnectRetryTimer( s );
			restartSessionKeepaliveTimer( s );
			restartSessionHoldTimer( s );
			setSessionState( s, stateOpenConfirm, event);
			if ( sendKeepaliveMessage( s ) != eventNone )
				resetSession( s, eventTcpConnectionFails );
			break;
		
		default:
			//send notification with appropriate error
			//NOTE: might be better to send where it occurred
			switch ( event )
			{
				case eventBGPHeaderErr:
					sendNotificationMessage( s, 1, 0, NULL, 0);
					break;
				
				case eventBGPOpenMsgErr:
					// do nothing as the corresponding  notifications have been sent in 'receiveOpenMessage'.
					log_msg("fsmOpenSent: eventBGPOpenMsgErr");
					break;	
					
				case eventBGPUnsupportedCapability:
					// do nothing as the corresponding  notifications have been sent in 'receiveOpenMessage'.
					log_msg("fsmOpenSent: eventBGPUnsupportedCapability");
					break;					
				
				case eventHoldTimer_Expires:
					sendNotificationMessage( s, 4, 0, NULL, 0);
					break;

				case eventBGPFSMErr:
				default:
					sendNotificationMessage( s, 5, 0, NULL, 0);
					break;
			}
			resetSession( s, event );
			break;
	}		
	 
	return ;
}

void 
fsmOpenConfirm( Session_structp s )
{
	/*
	 * BGP Keepalives are exchanged after the opens are exchanged.
	 * Notifications may also occur.
	 */
#ifdef DEBUG
	debug( "fsmOpenConfirm", "");	
#endif
	int event = receiveKeepaliveMessage( s );
	
	switch (event)
	{
		case eventKeepaliveMsg:
			restartSessionHoldTimer( s );
			setSessionState( s, stateEstablished, event );	
			log_msg("Session:%d is established , holdtime: %d , keepalivetime: %d", s->sessionID, s->fsm.holdTime, s->fsm.keepaliveInt);		
			break;
			
		case eventKeepaliveTimer_Expires:
			restartSessionKeepaliveTimer( s );
			if ( sendKeepaliveMessage( s ) != eventNone )
				resetSession( s, event);
			break;
		
		default:
			switch ( event )
			{
				case eventNotificationMessage:
				case eventNotificationMessageVerErr:
					break;
				
				case eventBGPHeaderErr:
					sendNotificationMessage( s, 1, 0, NULL, 0 );
					break;

				case eventHoldTimer_Expires:
					sendNotificationMessage( s, 4, 0, NULL, 0);
					break;

				default:
					sendNotificationMessage( s, 5, 0, NULL, 0 );
					break;
			}			
			resetSession( s, event );			
			break;
	}	
	
	return ;
}

void 
fsmEstablished( Session_structp s )
{
	int event;
	
	/*
	 * BGP Updates and KeepAlives are exchanged once the session
	 * is established.
	 */
	#ifdef DEBUG
		debug(__FUNCTION__, "fsmEstablished"); 
	#endif	 
	event= receiveBGPMessage( s );
		
	switch (event)
	{

		case eventKeepaliveMsg:
			restartSessionHoldTimer( s );
			break;

		case eventUpdateMsg:
			restartSessionHoldTimer( s );
			zeroSessionConnectRetryCount( s ); // success at least once!
			break;

		case eventKeepaliveTimer_Expires:
			restartSessionKeepaliveTimer( s );			
			if ( sendKeepaliveMessage( s ) != eventNone )
			{
				log_err("Session %d was reset! Reason:%d", s->sessionID, event);
				resetSession( s, eventTcpConnectionFails);
			}
			break;
		
		default:
			switch ( event )
			{
				case eventNotificationMessage:
				case eventNotificationMessageVerErr:
					// no response needed
					break;
					
				case eventBGPHeaderErr:
					sendNotificationMessage( s, 1, 0, NULL, 0 );
					break;
				
				case eventUpdateMsgErr:
					sendNotificationMessage( s, 3, 0, NULL, 0 );
					break;
				
				case eventHoldTimer_Expires:
					sendNotificationMessage( s, 4, 0, NULL, 0 );
					break;
				
				//case eventManualStop:
				//case eventAutomaticStop:
					//sendNotificationMessage( s, 6, 0, NULL, 0 );
					//break;
					
				default:
					sendNotificationMessage( s, 5, 0, NULL, 0 );
					break;
			}
			log_err("Session %d was reset! Reason:%d", s->sessionID, event);
			resetSession( s , event );			
			break;
	}
	
	return ;
}
 
void 
bgpFiniteStateMachine( Session_structp s )
{	
	/* Performs the appropriate processing for a Session_structp 
	 * given its current state.
	 */
	int state = s->fsm.state;
		
	switch ( state )
	{
		case stateIdle:
			fsmIdle( s );
			break;
		
		case stateConnect:
			fsmConnect( s );
			break;
		
		case stateActive:
			fsmActive( s );
			break;
		
		case stateOpenSent:
			fsmOpenSent( s );
			break;
		
		case stateOpenConfirm:
			fsmOpenConfirm( s );
			break;
		
		case stateEstablished:
			fsmEstablished( s );
			break;
	}
	
	return;	
}
