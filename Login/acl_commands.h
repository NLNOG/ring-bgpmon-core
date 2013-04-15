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

#ifndef ACL_COMMANDS_H_
#define ACL_COMMANDS_H_

// needed for commandArgument, clientThreadArguments, commandNode structures
#include "commandprompt.h"
// needed for ACCESS_RULE enum
#include "../Util/acl.h"

// show acl commands
int showACL(commandArgument * ca, clientThreadArguments * client, commandNode * root);

// access control commands
int ACL(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int noACL(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int exitACL(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int endACL(commandArgument * ca, clientThreadArguments * client, commandNode * root);

// ACL - PERMIT/DENY/LABEL/RIBONLY commands
int noACLRule(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int ACLRuleAny(commandArgument * ca, clientThreadArguments * client, commandNode * root);
int ACLRuleAddress(commandArgument * ca, clientThreadArguments * client, commandNode * cn);

ACCESS_RULE getAccessRule(commandNode * cn);

#endif
