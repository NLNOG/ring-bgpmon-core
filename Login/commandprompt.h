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
 *  File: commandprompt.h
 * 	Authors: Kevin Burnett
 *
 *  Date: July 30, 2008
 */
 
#ifndef COMMANDPROMPT_H_
#define COMMANDPROMPT_H_

#include "login.h"

#define MAX_SUB_COMMANDS 40
#define MAX_ARGUMENT_LENGTH 40 
#define MAX_DESCRIPTION_LENGTH 255
#define MAX_COMMAND_LENGTH MAX_ARGUMENT_LENGTH * 20
#define COMMAND_EXIT "exit"
#define WILDCARD_ARGUMENT "*"
#define SHOW_HELP_ARGUMENT_SINGLE "?"
#define SHOW_HELP_ARGUMENT_MULTI "??"
#define PROMPT_ACCESS ">"
#define PROMPT_ENABLE "#"
#define PROMPT_CONFIGURE "(config)" PROMPT_ENABLE
#define PROMPT_ACCESS_CONTROL_LIST "(config-acl)" PROMPT_ENABLE
#define PROMPT_ROUTER_BGP "(config-router)" PROMPT_ENABLE
#define MAX_PROMPT_LENGTH 140
#define MAX_HOSTNAME_LENGTH 100

struct CliStruct
{
	int			id;
	char			addr[ADDR_MAX_CHARS];
	int			port;
	int			socket;
	time_t			connectedTime;
	time_t			lastAction;
	int			commandMask;
	AccessControlList *	currentACL;
	int 			localASNumber;
	pthread_t		cliThread;
	struct CliStruct *	next;
}; 
typedef struct CliStruct CliNode;
typedef struct CliStruct clientThreadArguments;

struct commandArgumentStruct {
	char				commandArgument[MAX_ARGUMENT_LENGTH];
	struct commandArgumentStruct*	nextArgument;
	struct commandArgumentStruct*	previousArgument;
	struct commandArgumentStruct*	firstArgument;
};
typedef struct commandArgumentStruct commandArgument;

enum COMMAND_MASK_ENUM {
	NONE = 0,
	ACCESS = 1,
	ENABLE = 2,
	CONFIGURE = 4,
	ACCESS_CONTROL_LIST = 8,
	ROUTER_BGP = 16,
	ALL = 31
};
typedef enum COMMAND_MASK_ENUM COMMAND_MASK;

struct commandNodeStruct {
	char				command[MAX_ARGUMENT_LENGTH];
	char				description[MAX_DESCRIPTION_LENGTH];
	COMMAND_MASK			commandMask;
	int (*cmdPtr)(commandArgument*, clientThreadArguments*, struct commandNodeStruct* );		
	struct commandNodeStruct* 	subcommands[MAX_SUB_COMMANDS];
	struct commandNodeStruct*	parent;
};
typedef struct commandNodeStruct commandNode;


int listContainsCommand(commandNode *, char *);
int listContainsCommands2(commandNode *, const char *, const char *);
int sendMessage(int socket, char * msg, ... );
int getMessage(int clientSocket, char * msg, int msg_len);
int getPassword(int clientSocket, char * msg, int msg_len);
void * cliThread( void *arg );
time_t getCliLastAction(long id);
int getActiveCliIds(long **cliIds);

// initializes the command tree
int initCommandTree(commandNode * cs);
// recursively frees allocated memory associated with commandNodes
void freeCommandNode(commandNode * cs);

#endif	// COMMANDPROMPT_H_
