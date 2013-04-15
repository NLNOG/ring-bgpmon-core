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
 *  File: login_commands.h
 * 	Authors: Kevin Burnett, Dan Massey
 *
 *  Date: February 5, 2009
 */
 
#ifndef LOGIN_COMMANDS_H_
#define LOGIN_COMMANDS_H_

#include "commandprompt.h"

// login-listener commands
int cmdEnablePassword(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdAccessPassword(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdLoginListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdLoginListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdLoginListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowLoginListenerAcl(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowLoginListenerPort(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int cmdShowLoginListenerAddress(commandArgument * ca, clientThreadArguments * client, commandNode * root);

#endif
