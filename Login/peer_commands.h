/*******************************************************************************
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
 *  File: peer_commands.h
 * 	Authors: Kevin Burnett
 *
 *  Date: November 26, 2008
 ******************************************************************************/
 
#ifndef PEER_COMMANDS_H_
#define PEER_COMMANDS_H_

#include "commandprompt.h"

int cmdShowRunning(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowRunningNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowRunningPeerGroup(commandArgument * ca, clientThreadArguments * client, commandNode * root);

int cmdShowBGPNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowBGPRoutes(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowBGProutesASpath(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowBGPprefix(commandArgument * ca, clientThreadArguments * client, commandNode * root);

int cmdNeighborPeerGroupCreate(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborPeerGroupAssign(commandArgument * ca, clientThreadArguments * client, commandNode * root);

int cmdNeighborLocalAsNum(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborLocalPort(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborLocalAddr(commandArgument * ca, clientThreadArguments * client,
                      commandNode * root);
int cmdNeighborLocalBGPId(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborLocalBGPVersion(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborLocalHoldTime(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborRemoteAsNum(commandArgument * ca, clientThreadArguments * client,
                           commandNode * root);
int cmdNeighborRemotePort(commandArgument * ca, clientThreadArguments * client,
                      commandNode * root);
int cmdNeighborRemoteAddr(commandArgument * ca, clientThreadArguments * client,
                      commandNode * root);
int cmdNeighborRemoteBGPId(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborRemoteBGPVersion(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborRemoteHoldTime(commandArgument * ca, clientThreadArguments * client, commandNode * root);

int cmdNeighborMD5Password(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborLabelAction(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborRouteRefresh(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborEnableDisable(commandArgument * ca, clientThreadArguments * client, commandNode * root);

int cmdNeighborAnnounce(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborAnnounceIpv4Ipv6UnicastMulticast(commandArgument * ca, clientThreadArguments * client, commandNode * root);

int cmdNeighborReceive(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdNeighborReceiveIpv4Ipv6UnicastMulticast(commandArgument * ca, clientThreadArguments * client, commandNode * root);

int cmdNoNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdClearNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * root);


#endif
