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
 *  File: client_commands.h
 * 	Authors: Kevin Burnett, Dan Massey
 *
 *  Date: November 5, 2008
 */
 
#ifndef CLIENT_COMMANDS_H_
#define CLIENT_COMMANDS_H_

#include "commandprompt.h"

// client commands
int cmdShowClients(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdKillClient(commandArgument * ca, clientThreadArguments * client, commandNode * cn);

// client-listener commands
int cmdSetClientListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowClientListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdClientListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowClientListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowClientListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdClientListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowClientListenerSummary(commandArgument * ca, clientThreadArguments * client, commandNode * cn);
int cmdClientListenerMaxConnections(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowClientListenerMaxConnections(commandArgument * ca, clientThreadArguments * client, commandNode * root);

int cmdClientListenerEnable(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdClientListenerDisable(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowClientListenerStatus(commandArgument * ca, clientThreadArguments * client, commandNode * root);

#endif
