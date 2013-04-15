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
 *  File: commands.h
 * 	Authors: Kevin Burnett
 *
 *  Date: November 5, 2008
 */

#ifndef MRT_COMMANDS_H_
#define MRT_COMMANDS_H_

// needed for commandArgument, clientThreadArguments, commandNode structures
#include "commandprompt.h"

// show mrt-listener commands
int cmdShowMrtListenerStatus(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowMrtListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowMrtListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowMrtListenerSummary(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowMrtListenerLimit(commandArgument * ca, clientThreadArguments * client, commandNode * root);

// mrt-listener commands
int cmdMrtListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdMrtListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdMrtListenerEnable(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdMrtListenerDisable(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdMrtListenerLimit(commandArgument * ca, clientThreadArguments * client, commandNode * root);

// show mrt client commands
int cmdShowMrtClient(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowMrtClientId(commandArgument * ca, clientThreadArguments * client, commandNode * root);

// mrt acl commands
int cmdSetMrtListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * cn);
int cmdShowMrtListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * cn);

// mrt neighbor
int cmdShowMrtNeighbor(commandArgument * ca, clientThreadArguments * client, commandNode * cn);


#endif
