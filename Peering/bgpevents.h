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
 *	File: bgpevents.h
 * 	Authors: Dave Matthews
 *  Data: May 31, 2007  
 */

#ifndef BGPEVENTS_H_
#define BGPEVENTS_H_

/* 
 * BGP Finite State Machine events defined in RFC 4271
 * 
 * Implementation specific events:
 * 
 * eventNone - marker used to signify no event has occurred.
 * 
 * eventRouteRefresh.. for route refresh specific timers to 
 * operate similar to the other timers (ie. when the route
 * refresh timer expires and when a route refresh message
 * arrives).
 * 
 * eventLast - marker used to give the upper bound on events
 */

enum bgpEvent { 
	eventNone = 0,
	/* administrative events */
	eventManualStart = 1, 
	eventManualStop,
	eventAutomaticStart,
	eventManualStart_Passive,
	eventAutomaticStart_Passive,
	eventAutomaticStart_DampOscillations,
	eventAutomaticStart_DampOscillationsPassive,
	eventAutomaticStop,
	/* timer events */
  	eventConnectRetryTimer_Expires = 10,
	eventHoldTimer_Expires,
	eventKeepaliveTimer_Expires,
	eventDelayOpenTimer_Expires,
	eventIdleHoldTimer_Expires,
	/* TCP connection events */
	eventTcpConnection_Valid = 15,
	eventTcp_CR_Invalid,
	eventTcp_CR_Acked,
	eventTcpConnectionConfirmed,
	eventTcpConnectionFails,
	/* BGP message-based events */
	eventBGPOpen = 20,
	eventBGPOpen_DelayOpenTimer,
	eventBGPHeaderErr,
	eventBGPOpenMsgErr,
	eventBGPUnsupportedCapability,	
	eventBGPFSMErr,
	eventOpenCollisionDump,
	eventNotificationMessageVerErr,
	eventNotificationMessage,
	eventKeepaliveMsg,
	eventUpdateMsg,
	eventUpdateMsgErr,
	/* implementation specific events */
	eventRouteRefreshTimer_Expires = 64,
	eventRouteRefreshMsg,
	/* use for range checking */
	eventLast
};

#endif /*BGPEVENTS_H_*/
