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
 * 	Authors: Kevin Burnett, Dan Massey
 *
 *  Date: July 30, 2008
 */
 
#ifndef COMMANDS_H_
#define COMMANDS_H_

#include "commandprompt.h"

// access control commands
int enable(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int disable(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int configure(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int router(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int exitEnable(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int exitConfigure(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int endConfigure(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int exitRouter(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int endRouter(commandArgument * ca, clientThreadArguments * client, commandNode * root);

// show commands
int showRunning(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int showHelp(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int printCommandTree(int clientSocket, commandNode * root, int mask, int level, int recurse);

// shutdown command
int cmdShutdown(commandArgument * ca, clientThreadArguments * client, commandNode * root);

// command line interface commands
int copyRunningConfig(commandArgument * ca, clientThreadArguments * client, commandNode * root);

#endif	// COMMANDS_H_

