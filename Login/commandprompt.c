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
 *	OTHER DEALINGS IN THE SOFTWARE.\
 * 
 * 
 *  File: commandprompt.c
 * 	Authors: Kevin Burnett
 *
 *  Date: July 30, 2008 
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h> 
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>

#include "login.h" 
#include "commandprompt.h"
#include "commands.h"
#include "acl_commands.h"
#include "chain_commands.h"
#include "login_commands.h"
#include "client_commands.h"
#include "periodic_commands.h"
#include "peer_commands.h"
#include "queue_commands.h"
#include "mrt_commands.h"
#include "../site_defaults.h"
#include "../Util/log.h"
#include "../Util/bgpmon_defaults.h"

//#define DEBUG

// frees all memory associated with a command
void freeCommandArguments(commandArgument * ca);

// adds an argument to a command
void addCommandArgument(commandArgument ** ca, char * command);

// this function will step through the commandNode structure to find the commandNode 
commandNode* findCommandNode(commandNode * cs, char * input, char * pch, int count, int returnCommandArguments, int mask, commandArgument ** ca);

// creates and returns a pointer to a commandNode
commandNode* buildCommand(char * command, char * description, COMMAND_MASK mask, int (*cmdPtr)(commandArgument *, clientThreadArguments *, commandNode *)); 

// This function will add subcommands to the commandNode passed in.
commandNode* buildCommandTree(commandNode * cs, char * path_temp, int subCommandsCount, ... );

// This function will walk up the commandNode structure and set the current and 
// parent nodes to include the mask passed in.  
void setParentLinkMask(commandNode* cs, COMMAND_MASK mask);

// destroy the socket and memory associated with a cli ID
void destroyCli(long id);

// initialize pieces of the command tree
int initACLRuleCommands(commandNode * root);
int initQueueCommands(commandNode * root);
int initLoginCommands(commandNode * root);
int initClientCommands(commandNode * root);
int initChainCommands(commandNode * root);
int initPeriodicCommands(commandNode * root);
int initPeerCommands(commandNode * root);
int initPeerNeighborAnnounceReceiveCommands(commandNode * root);
int initNoPeerNeighborAnnounceReceiveCommands(commandNode * root);
int initMrtCommands(commandNode * root);
 
 /*----------------------------------------------------------------------------------------
 * Purpose: initializes the command tree 
 * Input: The root node for the tree 
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int initCommandTree(commandNode * root) {
	// construct the root commandNode
	strncpy(root->command, "root", MAX_ARGUMENT_LENGTH);

	commandNode * temp = NULL;

	// setup base commands that a lot of other subcommands use
	if ( (temp = buildCommandTree(root, "root", 1,
			buildCommand("show", "show", ACCESS, NULL))) == NULL)
	{
		log_fatal("initCommandTree(): could not init the command tree.");
	}
	temp = buildCommandTree(root, "root", 1,
			buildCommand("no", "no", CONFIGURE | ROUTER_BGP, NULL));

	// setup access-level change commands (list does not include ACL or BGP commands)
	temp = buildCommandTree(root, "root", 3,
			buildCommand("enable", "enable", ACCESS, &enable), 
			buildCommand("configure", "configure", ENABLE, &configure),
			buildCommand("disable", "disable", ENABLE, &disable));

	// setup exit commands
	temp = buildCommandTree(root, "root", 8,
			buildCommand("exit", "exit", ACCESS, NULL),	// no function pointer since clientThread() handles program exit
			buildCommand("exit", "exit", ENABLE, &exitEnable),
			buildCommand("exit", "exit", CONFIGURE, &exitConfigure),
			buildCommand("end", "end", CONFIGURE, &endConfigure),
			buildCommand("exit", "exit", ACCESS_CONTROL_LIST, &exitACL),
			buildCommand("end", "end", ACCESS_CONTROL_LIST, &endACL),
			buildCommand("exit", "exit", ROUTER_BGP, &exitRouter),
			buildCommand("end", "end", ROUTER_BGP, &endRouter));

	// setup bgpmon shutdown command
	temp = buildCommandTree(root, "root", 1,
			buildCommand("shutdown", "shutdown", ENABLE, &cmdShutdown));

	// ACCESS CONTROL LISTS
		// create access list [acl *]
		temp = buildCommandTree(root, "root", 1,
				buildCommand("acl", "acl", CONFIGURE, NULL));
		temp = buildCommandTree(root, "acl", 1,
				buildCommand("*", "[acl name]", CONFIGURE, &ACL));
		
		// delete access list [no acl *]
		temp = buildCommandTree(root, "no", 1,
				buildCommand("acl", "acl", CONFIGURE, NULL));
		temp = buildCommandTree(root, "no acl", 1,
				buildCommand("*", "[acl name]", CONFIGURE, &noACL));

		// show access control list [show acl]
		temp = buildCommandTree(root, "show", 1,
				buildCommand("acl", "acl", ACCESS | ENABLE | CONFIGURE | ACCESS_CONTROL_LIST, &showACL));
		temp = buildCommandTree(root, "show acl", 1,
				buildCommand("*", "[acl name]", ACCESS | ENABLE | CONFIGURE | ACCESS_CONTROL_LIST, &showACL));

	// BGP commands
		temp = buildCommandTree(root, "root", 1,
				buildCommand("router", "router", CONFIGURE, NULL));
		temp = buildCommandTree(root, "router", 1,
				buildCommand("bgp", "bgp", CONFIGURE, NULL));
		temp = buildCommandTree(root, "router bgp", 1,
				buildCommand("*", "[montor AS number]", CONFIGURE, &router));

	// [copy running-config *] command
	temp = buildCommandTree(root, "root", 1,
			buildCommand("copy", "copy", ENABLE, NULL));
	temp = buildCommandTree(root, "copy", 1,
			buildCommand("running-config", "running-config", ENABLE, NULL));
	temp = buildCommandTree(root, "copy running-config", 1,
			buildCommand("*", "[filename]", ENABLE, &copyRunningConfig));
	temp = buildCommandTree(root, "copy running-config", 1,
			buildCommand("startup-config", "startup-config", ENABLE, &copyRunningConfig));
	
	// initialize other parts of the command tree
	initLoginCommands(root);
	initACLRuleCommands(root);
	initQueueCommands(root);
	initClientCommands(root);
	initChainCommands(root);
	initPeriodicCommands(root);
	initPeerCommands(root);
	initPeerNeighborAnnounceReceiveCommands(root);
	initNoPeerNeighborAnnounceReceiveCommands(root);
	initMrtCommands(root);

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: intialize the login commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ November 3, 2008
 * -------------------------------------------------------------------------------------*/
int initLoginCommands(commandNode * root) {
	commandNode * temp = NULL;

	// [enable password *]
	temp = buildCommandTree(root, "enable", 1,
			buildCommand("password", "password", CONFIGURE, NULL));
	temp = buildCommandTree(root, "enable password", 1,
			buildCommand("*", "[enable password]", CONFIGURE, &cmdEnablePassword)); 

	// [password *] command
	temp = buildCommandTree(root, "root", 1,
			buildCommand("password", "password", CONFIGURE, NULL));
	temp = buildCommandTree(root, "password", 1,
			buildCommand("*", "[access password]", CONFIGURE, &cmdAccessPassword));

	// SETUP LOGIN-LISTENER COMMANDS
		// base [login-listener] command
		temp = buildCommandTree(root, "root", 1,
				buildCommand("login-listener", "login-listener", CONFIGURE, NULL));

		// [login-listener acl *] command
		temp = buildCommandTree(temp, "login-listener", 1,
				buildCommand("acl", "acl", CONFIGURE, NULL));
		temp = buildCommandTree(temp, "acl", 1,
				buildCommand("*", "[acl name]", CONFIGURE, &cmdLoginListenerAcl));

		// [login-listener address *]
		temp = buildCommandTree(root, "login-listener", 1,
				buildCommand("address", "address", CONFIGURE, NULL));
		temp = buildCommandTree(root, "login-listener address", 1,
				buildCommand("*", "[address]", CONFIGURE, &cmdLoginListenerAddress));

		// [login-listener port *]
		temp = buildCommandTree(root, "login-listener", 1,
				buildCommand("port", "port", CONFIGURE, NULL));
		temp = buildCommandTree(root, "login-listener port", 1,
				buildCommand("*", "[port number]", CONFIGURE, &cmdLoginListenerPort));

		// [show login-listener]
		temp = buildCommandTree(root, "show", 1,
				buildCommand("login-listener", "login-listener", ACCESS | ENABLE | CONFIGURE, NULL));

		// [show login-listener acl] command
		temp = buildCommandTree(temp, "login-listener", 1,
				buildCommand("acl", "acl", ACCESS | ENABLE | CONFIGURE, &cmdShowLoginListenerAcl));

		// [show login-listener port]
		temp = buildCommandTree(root, "show login-listener", 1,
				buildCommand("port", "port", ACCESS | ENABLE | CONFIGURE, &cmdShowLoginListenerPort));

		// [show login-listener address]
		temp = buildCommandTree(root, "show login-listener", 1,
				buildCommand("address", "address", ACCESS | ENABLE | CONFIGURE, &cmdShowLoginListenerAddress));

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: intialize the permit/deny commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ August 24, 2008   
 * -------------------------------------------------------------------------------------*/
int initACLRuleCommands(commandNode * root) {
	commandNode * temp = NULL;
	
	// base [permit], [deny], [label], and [ribonly] commands
	temp = buildCommandTree(root, "root", 4,
			buildCommand("permit", "permit", ACCESS_CONTROL_LIST, NULL),
			buildCommand("deny", "deny", ACCESS_CONTROL_LIST, NULL),
			buildCommand("label", "label", ACCESS_CONTROL_LIST, NULL),
			buildCommand("ribonly", "ribonly", ACCESS_CONTROL_LIST, NULL));

	// base [no] command
	temp = buildCommandTree(root, "no", 1,
			buildCommand("*", "[rule #]", ACCESS_CONTROL_LIST, &noACLRule));
	
	// PERMIT
		// [no permit *]
		temp = buildCommandTree(root, "no", 1,
				buildCommand("permit", "permit", ACCESS_CONTROL_LIST, &noACLRule));
		temp = buildCommandTree(root, "no permit", 1,
				buildCommand("*", "[rule #]", ACCESS_CONTROL_LIST, &noACLRule));

		// [permit any]
		temp = buildCommandTree(root, "permit", 1,
				buildCommand("any", "any", ACCESS_CONTROL_LIST, &ACLRuleAny));
		temp = buildCommandTree(root, "permit any", 1,
				buildCommand("*", "[index]", ACCESS_CONTROL_LIST, &ACLRuleAny));

		// [permit * *]
		temp = buildCommandTree(root, "permit", 1,
				buildCommand("*", "[address]", ACCESS_CONTROL_LIST, NULL));
		temp = buildCommandTree(root, "permit *", 1,
				buildCommand("*", "[mask]", ACCESS_CONTROL_LIST, &ACLRuleAddress));
		temp = buildCommandTree(temp, "*", 1, 
				buildCommand("*", "[index]", ACCESS_CONTROL_LIST, &ACLRuleAddress));

	// DENY 
		// [no deny *]
		temp = buildCommandTree(root, "no", 1,
				buildCommand("deny", "deny", ACCESS_CONTROL_LIST, &noACLRule));
		temp = buildCommandTree(root, "no deny", 1,
				buildCommand("*", "[rule #]", ACCESS_CONTROL_LIST, &noACLRule));
	
		// [deny any]
		temp = buildCommandTree(root, "deny", 1,
				buildCommand("any", "any", ACCESS_CONTROL_LIST, &ACLRuleAny));
		temp = buildCommandTree(root, "deny any", 1,
				buildCommand("*", "[index]", ACCESS_CONTROL_LIST, &ACLRuleAny));

		// [deny * *]
		temp = buildCommandTree(root, "deny", 1,
				buildCommand("*", "[address]", ACCESS_CONTROL_LIST, NULL));	
		temp = buildCommandTree(root, "deny *", 1,
				buildCommand("*", "[mask]", ACCESS_CONTROL_LIST, &ACLRuleAddress));
		temp = buildCommandTree(temp, "*", 1, 
				buildCommand("*", "[index]", ACCESS_CONTROL_LIST, &ACLRuleAddress));
	
	// LABEL
		// [no label *]
		temp = buildCommandTree(root, "no", 1,
				buildCommand("label", "label", ACCESS_CONTROL_LIST, &noACLRule));
		temp = buildCommandTree(root, "no label", 1,
				buildCommand("*", "[rule #]", ACCESS_CONTROL_LIST, &noACLRule));

		// [label any]
		temp = buildCommandTree(root, "label", 1,
				buildCommand("any", "any", ACCESS_CONTROL_LIST, &ACLRuleAny));
		temp = buildCommandTree(root, "label any", 1,
				buildCommand("*", "[index]", ACCESS_CONTROL_LIST, &ACLRuleAny));

		// [label * *]
		temp = buildCommandTree(root, "label", 1,
				buildCommand("*", "[address]", ACCESS_CONTROL_LIST, NULL));
		temp = buildCommandTree(root, "label *", 1,
				buildCommand("*", "[mask]", ACCESS_CONTROL_LIST, &ACLRuleAddress));
		temp = buildCommandTree(temp, "*", 1, 
				buildCommand("*", "[index]", ACCESS_CONTROL_LIST, &ACLRuleAddress));
	
	// RIBONLY
		// [no ribonly *]
		temp = buildCommandTree(root, "no", 1,
				buildCommand("ribonly", "ribonly", ACCESS_CONTROL_LIST, &noACLRule));
		temp = buildCommandTree(root, "no ribonly", 1,
				buildCommand("*", "[rule #]", ACCESS_CONTROL_LIST, &noACLRule));

		// [ribonly any]
		temp = buildCommandTree(root, "ribonly", 1,
				buildCommand("any", "any", ACCESS_CONTROL_LIST, &ACLRuleAny));
		temp = buildCommandTree(root, "ribonly any", 1,
				buildCommand("*", "[index]", ACCESS_CONTROL_LIST, &ACLRuleAny));

		// [ribonly * *]
		temp = buildCommandTree(root, "ribonly", 1,
				buildCommand("*", "[address]", ACCESS_CONTROL_LIST, NULL));
		temp = buildCommandTree(root, "ribonly *", 1,
				buildCommand("*", "[mask]", ACCESS_CONTROL_LIST, &ACLRuleAddress));
		temp = buildCommandTree(temp, "*", 1, 
				buildCommand("*", "[index]", ACCESS_CONTROL_LIST, &ACLRuleAddress));

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: intialize the clients commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ November 3, 2008
 * -------------------------------------------------------------------------------------*/
int initClientCommands(commandNode * root) {
	commandNode * temp = NULL;

	// SETUP CLIENT COMMANDS
		// [show clients] command
		temp = buildCommandTree(root, "show", 1,
				buildCommand("clients", "clients", ACCESS | ENABLE | CONFIGURE, &cmdShowClients));

		// [kill client *] command
		temp = buildCommandTree(root, "root", 1,
				buildCommand("kill", "kill", ENABLE, NULL));
		temp = buildCommandTree(temp, "kill", 1,
				buildCommand("client", "client", ENABLE, NULL));
		temp = buildCommandTree(temp, "client", 1,
				buildCommand("*", "[client id]", ENABLE, &cmdKillClient));

	// SETUP CLIENT-LISTENER COMMANDS
		// base [client-listener] commands
		temp = buildCommandTree(root, "root", 1,
				buildCommand("client-listener", "client-listener", CONFIGURE, NULL));

		// [client-listener enable]
		temp = buildCommandTree(root, "client-listener", 1,
				buildCommand("enable", "enable", CONFIGURE, &cmdClientListenerEnable));
		
		// [client-listener disable]
		temp = buildCommandTree(root, "client-listener", 1,
				buildCommand("disable", "disable", CONFIGURE, &cmdClientListenerDisable));

		//[client-listener rib], and [client-listener update]
		temp = buildCommandTree(root, "client-listener", 2,
				buildCommand("rib", "rib", CONFIGURE, NULL),
				buildCommand("update", "update", CONFIGURE, NULL));

		// CLIENT-LISTENER UPDATE commands
			// [client-listener update acl *] command
			temp = buildCommandTree(root, "client-listener update", 1,
					buildCommand("acl", "acl", CONFIGURE, NULL));
			temp = buildCommandTree(temp, "acl", 1,
					buildCommand("*", "[acl name]", CONFIGURE, &cmdSetClientListenerAcl));

			// [client-listener update port *]
			temp = buildCommandTree(root, "client-listener update", 1,
					buildCommand("port", "port", CONFIGURE, NULL));
			temp = buildCommandTree(temp, "port", 1,
					buildCommand("*", "[port number]", CONFIGURE, &cmdClientListenerPort));

			// [client-listener update address *]
			temp = buildCommandTree(root, "client-listener update", 1,
					buildCommand("address", "address", CONFIGURE, NULL));
			temp = buildCommandTree(temp, "address", 1,
					buildCommand("*", "[address]", CONFIGURE, &cmdClientListenerAddress));

			// [client-listener update limit *]
			temp = buildCommandTree(root, "client-listener update", 1,
					buildCommand("limit", "limit", CONFIGURE, NULL));
			temp = buildCommandTree(temp, "limit", 1,
					buildCommand("*", "[max connections]", CONFIGURE, &cmdClientListenerMaxConnections));

		// CLIENT-LISTENER RIB commands
			// [client-listener rib acl *] command
			temp = buildCommandTree(root, "client-listener rib", 1,
					buildCommand("acl", "acl", CONFIGURE, NULL));
			temp = buildCommandTree(temp, "acl", 1,
					buildCommand("*", "[acl name]", CONFIGURE, &cmdSetClientListenerAcl));

			// [client-listener rib port *]
			temp = buildCommandTree(root, "client-listener rib", 1,
					buildCommand("port", "port", CONFIGURE, NULL));
			temp = buildCommandTree(temp, "port", 1,
					buildCommand("*", "[port number]", CONFIGURE, &cmdClientListenerPort));

			// [client-listener rib address *]
			temp = buildCommandTree(root, "client-listener rib", 1,
					buildCommand("address", "address", CONFIGURE, NULL));
			temp = buildCommandTree(temp, "address", 1,
					buildCommand("*", "[address]", CONFIGURE, &cmdClientListenerAddress));

			// [client-listener rib limit *]
			temp = buildCommandTree(root, "client-listener rib", 1,
					buildCommand("limit", "limit", CONFIGURE, NULL));
			temp = buildCommandTree(temp, "limit", 1,
					buildCommand("*", "[max connections]", CONFIGURE, &cmdClientListenerMaxConnections));


		// [show client-listener]
		temp = buildCommandTree(root, "show", 1,
				buildCommand("client-listener", "client-listener", ACCESS | ENABLE | CONFIGURE, NULL));

		// [show client-listener status]
		temp = buildCommandTree(root, "show client-listener", 1,
				buildCommand("status", "status", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerStatus));

		// [show client-listener update] and [show client-listener rib] commands
		temp = buildCommandTree(root, "show client-listener", 2,
				buildCommand("update", "update", ACCESS | ENABLE | CONFIGURE, NULL),
				buildCommand("rib", "rib", ACCESS | ENABLE | CONFIGURE, NULL));

		// [show client-listener summary]
		temp = buildCommandTree(root, "show client-listener", 1,
				buildCommand("summary", "summary", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerSummary));

		// SHOW CLIENT-LISTENER UPDATE commands
			// [show client-listener update acl] command
			temp = buildCommandTree(root, "show client-listener update", 1,
					buildCommand("acl", "acl", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerAcl));

			// [show client-listener update port]
			temp = buildCommandTree(root, "show client-listener update", 1,
					buildCommand("port", "port", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerPort));

			// [show client-listener update address]
			temp = buildCommandTree(root, "show client-listener update", 1,
					buildCommand("address", "address", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerAddress));

			// [show client-listener update limit]
			temp = buildCommandTree(root, "show client-listener update", 1,
					buildCommand("limit", "limit", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerMaxConnections));

		// SHOW CLIENT-LISTENER RIB commands
			// [show client-listener rib acl] command
			temp = buildCommandTree(root, "show client-listener rib", 1,
					buildCommand("acl", "acl", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerAcl));

			// [show client-listener rib port]
			temp = buildCommandTree(root, "show client-listener rib", 1,
					buildCommand("port", "port", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerPort));

			// [show client-listener rib address]
			temp = buildCommandTree(root, "show client-listener rib", 1,
					buildCommand("address", "address", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerAddress));

			// [show client-listener update limit]
			temp = buildCommandTree(root, "show client-listener rib", 1,
					buildCommand("limit", "limit", ACCESS | ENABLE | CONFIGURE, &cmdShowClientListenerMaxConnections));


	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: intialize the queue commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int initQueueCommands(commandNode * root) {
	commandNode * temp = NULL;

	// [queue]
	if ( (temp = buildCommandTree(root, "root", 1,
			buildCommand("queue", "queue", CONFIGURE, NULL))) == NULL)
	{
		log_fatal("initQueueCommands(): could not init the command tree.");
	}
	
	// [queue pacingOnThresh *]
	temp = buildCommandTree(root, "queue", 1,
			buildCommand("pacingOnThresh", "pacingOnThresh", CONFIGURE, NULL));
	temp = buildCommandTree(root, "queue pacingOnThresh", 1,
			buildCommand("*", "[pacingOnThresh]", CONFIGURE, &queuePacingOnThresh));
	
	// [queue pacingOffThresh *]
	temp = buildCommandTree(root, "queue", 1,
			buildCommand("pacingOffThresh", "pacingOffThresh", CONFIGURE, NULL));
	temp = buildCommandTree(root, "queue pacingOffThresh", 1,
			buildCommand("*", "[pacingOffThresh]", CONFIGURE, &queuePacingOffThresh));
	
	// [queue alpha *]
	temp = buildCommandTree(root, "queue", 1,
			buildCommand("alpha", "alpha", CONFIGURE, NULL));
	temp = buildCommandTree(root, "queue alpha", 1,
			buildCommand("*", "[alpha]", CONFIGURE, &queueAlpha));
	
	// [queue minWritesLimit *]
	temp = buildCommandTree(root, "queue", 1,
			buildCommand("minWritesLimit", "minWritesLimit", CONFIGURE, NULL));
	temp = buildCommandTree(root, "queue minWritesLimit", 1,
			buildCommand("*", "[minimum write limit]", CONFIGURE, &queueMinWritesLimit));
	
	// [queue pacingInterval *]
	temp = buildCommandTree(root, "queue", 1,
			buildCommand("pacingInterval", "pacingInterval", CONFIGURE, NULL));
	temp = buildCommandTree(root, "queue pacingInterval", 1,
			buildCommand("*", "[pacing interval]", CONFIGURE, &queuePacingInterval));

	// [show queue peer], [show queue ribonly], and [show queue xml] commands
	temp = buildCommandTree(root, "show", 1,
			buildCommand("queue", "queue", ACCESS | ENABLE | CONFIGURE, &showQueue));
	temp = buildCommandTree(root, "show queue", 4,
			buildCommand(PEER_QUEUE_NAME, PEER_QUEUE_NAME, ACCESS | ENABLE | CONFIGURE, &showQueue),
			buildCommand(LABEL_QUEUE_NAME, LABEL_QUEUE_NAME, ACCESS | ENABLE | CONFIGURE, &showQueue),
			buildCommand(XML_U_QUEUE_NAME, XML_U_QUEUE_NAME, ACCESS | ENABLE | CONFIGURE, &showQueue),
			buildCommand(XML_R_QUEUE_NAME, XML_R_QUEUE_NAME, ACCESS | ENABLE | CONFIGURE, &showQueue));

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: initialize the chaining commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int initChainCommands(commandNode * root) {
	commandNode * temp = NULL;

	// base [chain] and [no chain] commands
	temp = buildCommandTree(root, "root", 1,
			buildCommand("chain", "chain", CONFIGURE, NULL));

	// create chain [chain * * * *]
	temp = buildCommandTree(root, "chain", 1, 
			buildCommand("*", "[address]", CONFIGURE, &cmdChain));
	temp = buildCommandTree(temp, "*", 1,
			buildCommand("*", "[update port]", CONFIGURE, &cmdChain));
	temp = buildCommandTree(temp, "*", 1,
			buildCommand("*", "[rib port]", CONFIGURE, &cmdChain));
	temp = buildCommandTree(temp, "*", 1,
			buildCommand("*", "[enabled]", CONFIGURE, &cmdChain));
	temp = buildCommandTree(temp, "*", 1,
			buildCommand("*", "[retry interval]", CONFIGURE, &cmdChain));

	// delete chain [no chain * *]
	temp = buildCommandTree(root, "no", 1,
			buildCommand("chain", "chain", CONFIGURE, NULL));
	temp = buildCommandTree(temp, "chain", 1,
			buildCommand("*", "[address]", CONFIGURE, &cmdNoChain));
	temp = buildCommandTree(temp, "*", 1,
			buildCommand("*", "[update port]", CONFIGURE, &cmdNoChain));
	temp = buildCommandTree(temp, "*", 1,
			buildCommand("*", "[rib port]", CONFIGURE, &cmdNoChain));

	// show chain [show chain] and [show chain *]
	temp = buildCommandTree(root, "show", 1,
			buildCommand("chains", "chains", ACCESS | ENABLE | CONFIGURE, &cmdShowChain));
	temp = buildCommandTree(root, "show chains", 1,
			buildCommand("*", "[chain id]", ACCESS | ENABLE | CONFIGURE, &cmdShowChain));

	return 0;

}

/*----------------------------------------------------------------------------------------
 * Purpose: initialize the periodic commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int initPeriodicCommands(commandNode * root) {
	commandNode * temp = NULL;

	// base [periodic] command
	temp = buildCommandTree(root, "root", 1,
			buildCommand("periodic", "periodic", CONFIGURE, NULL));

	// set router refresh interval [periodic route-refresh *]
	temp = buildCommandTree(root, "periodic", 1,
			buildCommand("route-refresh", "route-refresh", CONFIGURE, NULL));
	temp = buildCommandTree(temp, "route-refresh", 1,
			buildCommand("*", "[route refresh]", CONFIGURE, &cmdPeriodicRouteRefresh));

	// set status report interval [periodic status-message *]
	temp = buildCommandTree(root, "periodic", 1,
			buildCommand("status-message", "status-message", CONFIGURE, NULL));
	temp = buildCommandTree(temp, "status-message", 1,
			buildCommand("*", "[status message]", CONFIGURE, &cmdPeriodicStatusMessage));

	// set status report interval [periodic route-refresh enable] and [periodic route-refresh disable]
	temp = buildCommandTree(root, "periodic route-refresh", 1,
			buildCommand("enable", "enable", CONFIGURE, &cmdPeriodicRouteRefreshEnableDisable));
	temp = buildCommandTree(temp, "route-refresh", 1,
			buildCommand("disable", "disable", CONFIGURE, &cmdPeriodicRouteRefreshEnableDisable));

	// base [show periodic] command
	temp = buildCommandTree(root, "show", 1,
			buildCommand("periodic", "periodic", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, NULL));

	// [show periodic route-refresh]
	temp = buildCommandTree(root, "show periodic", 1,
			buildCommand("route-refresh", "route-refresh", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowPeriodicRouteRefresh));
	
	// [show periodic status-message]
	temp = buildCommandTree(root, "show periodic", 1,
			buildCommand("status-message", "status-message", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowPeriodicStatusMessage));

	// [show periodic transfer-type]
	temp = buildCommandTree(root, "show periodic route-refresh", 1,
			buildCommand("status", "status", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowPeriodicRouteRefreshStatus));

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: initialize the peer commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int initPeerCommands(commandNode * root) {
	commandNode * temp = NULL;
	
	// base [neighbor *] command
	temp = buildCommandTree(root, "root", 1,
			buildCommand("neighbor", "neighbor", ROUTER_BGP, NULL));
	temp = buildCommandTree(temp, "neighbor", 1,
			buildCommand("*", "[neighbor address]", ROUTER_BGP, NULL));

	// [neighbor * port *] command
	temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("port", "port", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port", 1,
			buildCommand("*", "[neighbor port number]", ROUTER_BGP, NULL));
	
	// [neighbor * peer-group] command - creating a peer group
	temp = buildCommandTree(root, "neighbor *", 1, 
			buildCommand("peer-group", "peer-group", ROUTER_BGP, &cmdNeighborPeerGroupCreate));
	
	// [neighbor * port * peer-group] command - creating a peer group
	temp = buildCommandTree(root, "neighbor * port *", 1, 
			buildCommand("peer-group", "peer-group", ROUTER_BGP, &cmdNeighborPeerGroupCreate));

	// [neighbor * peer-group *] command - assigning a neighbor to a peer group
	temp = buildCommandTree(root, "neighbor * peer-group", 1, 
			buildCommand("*", "[peer-group-name]", ROUTER_BGP, &cmdNeighborPeerGroupAssign));
	
	// [neighbor * port * peer-group *] command - assigning a neighbor to a peer group
	temp = buildCommandTree(root, "neighbor * port * peer-group", 1, 
			buildCommand("*", "[peer-group-name]", ROUTER_BGP, &cmdNeighborPeerGroupAssign));
	
	// [neighbor * remote] command
	temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("remote", "remote", ROUTER_BGP, NULL));

	// [neighbor * remote as *] command
	temp = buildCommandTree(root, "neighbor * remote", 1,
			buildCommand("as", "as", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * remote as", 1,
			buildCommand("*", "[remote-as-number]", ROUTER_BGP, &cmdNeighborRemoteAsNum));


	// [neighbor * remote bgpid *] command
	temp = buildCommandTree(root, "neighbor * remote", 1,
			buildCommand("bgpid", "bgpid", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * remote bgpid", 1,
			buildCommand("*", "[remote-bgpid]", ROUTER_BGP, &cmdNeighborRemoteBGPId));

	// [neighbor * remote bgp-version *] command
	temp = buildCommandTree(root, "neighbor * remote", 1,
			buildCommand("bgp-version", "bgp-version", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * remote bgp-version", 1,
			buildCommand("*", "[bgp version]", ROUTER_BGP, &cmdNeighborRemoteBGPVersion));

	// [neighbor * remote hold-time *] command
	temp = buildCommandTree(root, "neighbor * remote", 1,
			buildCommand("hold-time", "hold-time", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * remote hold-time", 1,
			buildCommand("*", "[hold time]", ROUTER_BGP, &cmdNeighborRemoteHoldTime));
	
	// [neighbor * port * remote] command
	temp = buildCommandTree(root, "neighbor * port *", 1,
			buildCommand("remote", "remote", ROUTER_BGP, NULL));

	// [neighbor * port * remote as *] command
	temp = buildCommandTree(root, "neighbor * port * remote", 1,
			buildCommand("as", "as", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * remote as", 1,
			buildCommand("*", "[remote-as-number]", ROUTER_BGP, &cmdNeighborRemoteAsNum));


	// [neighbor * port * remote bgpid *] command
	temp = buildCommandTree(root, "neighbor * port * remote", 1,
			buildCommand("bgpid", "bgpid", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * remote bgpid", 1,
			buildCommand("*", "[remote-bgpid]", ROUTER_BGP, &cmdNeighborRemoteBGPId));

	// [neighbor * port * remote bgp-version *] command
	temp = buildCommandTree(root, "neighbor * port * remote", 1,
			buildCommand("bgp-version", "bgp-version", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * remote bgp-version", 1,
			buildCommand("*", "[bgp version]", ROUTER_BGP, &cmdNeighborRemoteBGPVersion));

	// [neighbor * port * remote hold-time *] command
	temp = buildCommandTree(root, "neighbor * port * remote", 1,
			buildCommand("hold-time", "hold-time", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * remote hold-time", 1,
			buildCommand("*", "[hold time]", ROUTER_BGP, &cmdNeighborRemoteHoldTime));

	// base [neighbor * local] command
	temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("local", "local", ROUTER_BGP, NULL));

	// [neighbor * local as *] command
	temp = buildCommandTree(root, "neighbor * local", 1,
			buildCommand("as", "as", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * local as", 1,
			buildCommand("*", "[local-as-number]", ROUTER_BGP, &cmdNeighborLocalAsNum));

	// [neighbor * local bgpid *] command
	temp = buildCommandTree(root, "neighbor * local", 1,
			buildCommand("bgpid", "bgpid", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * local bgpid", 1,
			buildCommand("*", "[local-bgpid]", ROUTER_BGP, &cmdNeighborLocalBGPId));
				
	// [neighbor * local_port *] command
	temp = buildCommandTree(root, "neighbor * local", 1,
			buildCommand("port", "port", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * local port", 1,
			buildCommand("*", "[local-port]", ROUTER_BGP, &cmdNeighborLocalPort));

	// [neighbor * local_port *] command
	temp = buildCommandTree(root, "neighbor * local", 1,
			buildCommand("address", "address", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * local address", 1,
			buildCommand("*", "[local-address]", ROUTER_BGP, &cmdNeighborLocalAddr));

	// [neighbor * local bgp-version *] command
	temp = buildCommandTree(root, "neighbor * local", 1,
			buildCommand("bgp-version", "bgp-version", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * local bgp-version", 1,
			buildCommand("*", "[bgp version]", ROUTER_BGP, &cmdNeighborLocalBGPVersion));

	// [neighbor * local hold-time *] command
	temp = buildCommandTree(root, "neighbor * local", 1,
			buildCommand("hold-time", "hold-time", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * local hold-time", 1,
			buildCommand("*", "[hold time]", ROUTER_BGP, &cmdNeighborLocalHoldTime));
	
	// [neighbor * port * local] command
	temp = buildCommandTree(root, "neighbor * port *", 1,
	 		buildCommand("local", "local", ROUTER_BGP, NULL));

	// [neighbor * local as *] command
	temp = buildCommandTree(root, "neighbor * port * local", 1,
			buildCommand("as", "as", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * local as", 1,
			buildCommand("*", "[local-as-number]", ROUTER_BGP, &cmdNeighborLocalAsNum));

	// [neighbor * local bgpid *] command
	temp = buildCommandTree(root, "neighbor * port * local", 1,
			buildCommand("bgpid", "bgpid", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * local bgpid", 1,
			buildCommand("*", "[local-bgpid]", ROUTER_BGP, &cmdNeighborLocalBGPId));

	// [neighbor * local_port *] command
	temp = buildCommandTree(root, "neighbor * port * local", 1,
			buildCommand("port", "port", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * local port", 1,
			buildCommand("*", "[local-port]", ROUTER_BGP, &cmdNeighborLocalPort));

	// [neighbor * local_port *] command
	temp = buildCommandTree(root, "neighbor * port * local", 1,
			buildCommand("address", "address", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * local address", 1,
			buildCommand("*", "[local-address]", ROUTER_BGP, &cmdNeighborLocalAddr));

	// [neighbor * local bgp-version *] command
	temp = buildCommandTree(root, "neighbor * port * local", 1,
			buildCommand("bgp-version", "bgp-version", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * local bgp-version", 1,
			buildCommand("*", "[bgp version]", ROUTER_BGP, &cmdNeighborLocalBGPVersion));

	// [neighbor * local hold-time *] command
	temp = buildCommandTree(root, "neighbor * port * local", 1,
			buildCommand("hold-time", "hold-time", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * local hold-time", 1,
			buildCommand("*", "[hold time]", ROUTER_BGP, &cmdNeighborLocalHoldTime));

	// [neighbor * label-action NoAction], [neighbor * label-action Label], [neighbor * label-action StoreRibOnly] commands
	temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("label-action", "label-action", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * label-action", 3,
			buildCommand("NoAction", "NoAction", ROUTER_BGP, &cmdNeighborLabelAction),
			buildCommand("Label", "Label", ROUTER_BGP, &cmdNeighborLabelAction),
			buildCommand("StoreRibOnly", "StoreRibOnly", ROUTER_BGP, &cmdNeighborLabelAction));
	
	// [neighbor * port * label-action NoAction], [neighbor * port * label-action Label], [neighbor * port * label-action StoreRibOnly] commands
	temp = buildCommandTree(root, "neighbor * port *", 1,
			buildCommand("label-action", "label-action", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * label-action", 3,
			buildCommand("NoAction", "NoAction", ROUTER_BGP, &cmdNeighborLabelAction),
			buildCommand("Label", "Label", ROUTER_BGP, &cmdNeighborLabelAction),
			buildCommand("StoreRibOnly", "StoreRibOnly", ROUTER_BGP, &cmdNeighborLabelAction));

	// [neighbor * route-refresh] command
	temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("route-refresh", "route-refresh", ROUTER_BGP, &cmdNeighborRouteRefresh));

	// [neighbor * port * route-refresh] command
	temp = buildCommandTree(root, "neighbor * port *", 1,
			buildCommand("route-refresh", "route-refresh", ROUTER_BGP, &cmdNeighborRouteRefresh));

	// [neighbor * enable] command
	temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("enable", "enable", ROUTER_BGP, &cmdNeighborEnableDisable));
	
	// [neighbor * port * enable] command
	temp = buildCommandTree(root, "neighbor * port *", 1,
			buildCommand("enable", "enable", ROUTER_BGP, &cmdNeighborEnableDisable));

	// [neighbor * disable] command
	temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("disable", "disable", ROUTER_BGP, &cmdNeighborEnableDisable));
	
	// [neighbor * port * disable] command
	temp = buildCommandTree(root, "neighbor * port *", 1,
			buildCommand("disable", "disable", ROUTER_BGP, &cmdNeighborEnableDisable));

	// [neighbor * md5 *] command
	temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("md5", "md5", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * md5", 1,
			buildCommand("*", "[md5 password]", ROUTER_BGP, &cmdNeighborMD5Password));

	// [neighbor * port * md5 *] command
	temp = buildCommandTree(root, "neighbor * port *", 1,
			buildCommand("md5", "md5", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "neighbor * port * md5", 1,
			buildCommand("*", "[md5 password]", ROUTER_BGP, &cmdNeighborMD5Password));

	// base [no neighbor *] and [no neighbor * port *] commands
	temp = buildCommandTree(root, "no", 1,
			buildCommand("neighbor", "neighbor", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "no neighbor", 1,
			buildCommand("*", "[neighbor address]", ROUTER_BGP, &cmdNoNeighbor));
	temp = buildCommandTree(root, "no neighbor *", 1,
			buildCommand("peer-group", "peer-group", ROUTER_BGP, &cmdNoNeighbor));
	temp = buildCommandTree(root, "no neighbor *", 1,
			buildCommand("port", "port", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "no neighbor * port", 1,
			buildCommand("*", "[neighbor port number]", ROUTER_BGP, &cmdNoNeighbor));

	// [clear neighbor *] and [clear neighbor * port *] command
	temp = buildCommandTree(root, "root", 1,
			buildCommand("clear", "clear", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "clear", 1,
			buildCommand("neighbor", "neighbor", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "clear neighbor", 1,
			buildCommand("*", "[neighbor address]", ROUTER_BGP, &cmdClearNeighbor));
	temp = buildCommandTree(root, "clear neighbor *", 1,
			buildCommand("port", "port", ROUTER_BGP, NULL));
	temp = buildCommandTree(root, "clear neighbor * port", 1,
			buildCommand("*", "[neighbor port number]", ROUTER_BGP, &cmdClearNeighbor));
	
	// base [show bgp] command
	temp = buildCommandTree(root, "show", 1,
			buildCommand("bgp", "bgp", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, NULL));

		// [show bgp neighbor]/[show bgp neighbor * port *] 
		temp = buildCommandTree(root, "show bgp", 1,
				buildCommand("neighbor", "neighbor", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowBGPNeighbor));
		temp = buildCommandTree(root, "show bgp neighbor", 1,
				buildCommand("*", "[neighbor address]", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowBGPNeighbor));
		temp = buildCommandTree(root, "show bgp neighbor *", 1,
				buildCommand("port", "port", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "show bgp neighbor * port", 1,
				buildCommand("*", "[neighbor port]", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowBGPNeighbor));
		// s[show bgp routes]/[show bgp routes *]
		temp = buildCommandTree(root, "show bgp", 1,
				buildCommand("routes", "routes", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowBGPRoutes));

		temp = buildCommandTree(root, "show bgp routes", 1,
				buildCommand("*", "[neighbor address]", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowBGPRoutes));

		temp = buildCommandTree(root, "show bgp routes *", 1,
				buildCommand("prefix", "prefix", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "show bgp routes * prefix", 1,
				buildCommand("*", "[prefix]", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowBGProutesASpath));

		// show all AS paths based on entered prefix
		temp = buildCommandTree(root, "show bgp", 1,
				buildCommand("prefix", "prefix", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "show bgp prefix", 1,
				buildCommand("*", "[prefix]", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowBGPprefix));
	// setup [SHOW RUNNING] command
	temp = buildCommandTree(root, "show", 1,
			buildCommand("running", "running", ACCESS | ENABLE | CONFIGURE | ROUTER_BGP, &cmdShowRunning));

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: initialize the peer neighbor announce/receive commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int initPeerNeighborAnnounceReceiveCommands(commandNode * root) {
	commandNode * temp = NULL;

	// [neighbor * announce] command
	if ( (temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("announce", "announce", ROUTER_BGP, NULL))) == NULL)
	{
		log_fatal("initPeerNeighborAnnounceReceiveCommands(): could not init the command tree.");
	}
	
		// [neighbor * announce * * *] command
		temp = buildCommandTree(root, "neighbor * announce", 1,
				buildCommand("*", "[code]", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "neighbor * announce *", 1,
				buildCommand("*", "[length]", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "neighbor * announce * *", 1,
				buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborAnnounce));
		
		// [neighbor * announce ipv4 unicast] and [neighbor * announce ipv4 multicast]
		temp = buildCommandTree(root, "neighbor * announce", 1,
				buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "neighbor * announce ipv4", 1,
				buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		temp = buildCommandTree(root, "neighbor * announce ipv4", 1,
				buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		
		// [neighbor * announce ipv6 unicast] and [neighbor * announce ipv6 multicast]
		temp = buildCommandTree(root, "neighbor * announce", 1,
				buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "neighbor * announce ipv6", 1,
				buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		temp = buildCommandTree(root, "neighbor * announce ipv6", 1,
				buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
	
	// [neighbor * port * announce] command
	temp = buildCommandTree(root, "neighbor * port *", 1,
			buildCommand("announce", "announce", ROUTER_BGP, NULL));
	
		// [neighbor * port * announce * * *] command
		temp = buildCommandTree(root, "neighbor * port * announce", 1,
				buildCommand("*", "[code]", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "neighbor * port * announce *", 1,
				buildCommand("*", "[length]", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "neighbor * port * announce * *", 1,
				buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborAnnounce));
		
		// [neighbor * port * announce ipv4 unicast] and [neighbor * announce ipv4 multicast]
		temp = buildCommandTree(root, "neighbor * port * announce", 1,
				buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "neighbor * port * announce ipv4", 1,
				buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		temp = buildCommandTree(root, "neighbor * port * announce ipv4", 1,
				buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		
		// [neighbor * port * announce ipv6 unicast] and [neighbor * announce ipv6 multicast]
		temp = buildCommandTree(root, "neighbor * port * announce", 1,
				buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "neighbor * port * announce ipv6", 1,
				buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		temp = buildCommandTree(root, "neighbor * port * announce ipv6", 1,
				buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
	
	// [neighbor * receive ] command
	temp = buildCommandTree(root, "neighbor *", 1,
			buildCommand("receive", "receive", ROUTER_BGP, NULL));

		// [neighbor * receive require] command
		temp = buildCommandTree(root, "neighbor * receive", 1,
				buildCommand("require", "require", ROUTER_BGP, NULL));

			// [neighbor * receive require * * *] command
			temp = buildCommandTree(root, "neighbor * receive require", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * receive require *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "neighbor * receive require * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [neighbor * receive require ipv4 unicast] and [neighbor * receive require ipv4 multicast]
			temp = buildCommandTree(root, "neighbor * receive require", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * receive require ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * receive require ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [neighbor * receive require ipv6 unicast] and [neighbor * receive require ipv6 multicast]
			temp = buildCommandTree(root, "neighbor * receive require", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * receive require ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * receive require ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));


		// [neighbor * receive refuse] command
		temp = buildCommandTree(root, "neighbor * receive", 1,
				buildCommand("refuse", "refuse", ROUTER_BGP, NULL));
			
			// [neighbor * receive refuse * * *] command
			temp = buildCommandTree(root, "neighbor * receive refuse", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * receive refuse *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "neighbor * receive refuse * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [neighbor * receive refuse ipv4 unicast] and [neighbor * receive refuse ipv4 multicast]
			temp = buildCommandTree(root, "neighbor * receive refuse", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * receive refuse ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * receive refuse ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [neighbor * receive refuse ipv6 unicast] and [neighbor * receive refuse ipv6 multicast]
			temp = buildCommandTree(root, "neighbor * receive refuse", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * receive refuse ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * receive refuse ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));

		// [neighbor * receive allow] command
		temp = buildCommandTree(root, "neighbor * receive", 1,
				buildCommand("allow", "allow", ROUTER_BGP, NULL));

			// [neighbor * receive allow * * *] command
			temp = buildCommandTree(root, "neighbor * receive allow", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * receive allow *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "neighbor * receive allow * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [neighbor * receive allow ipv4 unicast] and [neighbor * receive allow ipv4 multicast]
			temp = buildCommandTree(root, "neighbor * receive allow", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * receive allow ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * receive allow ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [neighbor * receive allow ipv6 unicast] and [neighbor * receive allow ipv6 multicast]
			temp = buildCommandTree(root, "neighbor * receive allow", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * receive allow ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * receive allow ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
	
	// [neighbor * port * receive ] command
	temp = buildCommandTree(root, "neighbor * port *", 1,
			buildCommand("receive", "receive", ROUTER_BGP, NULL));

		// [neighbor * port * receive require] command
		temp = buildCommandTree(root, "neighbor * port * receive", 1,
				buildCommand("require", "require", ROUTER_BGP, NULL));

			// [neighbor * port * receive require * * *] command
			temp = buildCommandTree(root, "neighbor * port * receive require", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * port * receive require *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "neighbor * port * receive require * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [neighbor * port * receive require ipv4 unicast] and [neighbor * receive require ipv4 multicast]
			temp = buildCommandTree(root, "neighbor * port * receive require", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * port * receive require ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * port * receive require ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [neighbor * port * receive require ipv6 unicast] and [neighbor * receive require ipv6 multicast]
			temp = buildCommandTree(root, "neighbor * port * receive require", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * port * receive require ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * port * receive require ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));


		// [neighbor * port * receive refuse] command
		temp = buildCommandTree(root, "neighbor * port * receive", 1,
				buildCommand("refuse", "refuse", ROUTER_BGP, NULL));
			
			// [neighbor * port * receive refuse * * *] command
			temp = buildCommandTree(root, "neighbor * port * receive refuse", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * port * receive refuse *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "neighbor * port * receive refuse * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [neighbor * port * receive refuse ipv4 unicast] and [neighbor * receive refuse ipv4 multicast]
			temp = buildCommandTree(root, "neighbor * port * receive refuse", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * port * receive refuse ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * port * receive refuse ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [neighbor * port * receive refuse ipv6 unicast] and [neighbor * receive refuse ipv6 multicast]
			temp = buildCommandTree(root, "neighbor * port * receive refuse", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * port * receive refuse ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * port * receive refuse ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));

		// [neighbor * port * receive allow] command
		temp = buildCommandTree(root, "neighbor * port * receive", 1,
				buildCommand("allow", "allow", ROUTER_BGP, NULL));

			// [neighbor * port * receive allow * * *] command
			temp = buildCommandTree(root, "neighbor * port * receive allow", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * port * receive allow *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "neighbor * port * receive allow * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [neighbor * port * receive allow ipv4 unicast] and [neighbor * receive allow ipv4 multicast]
			temp = buildCommandTree(root, "neighbor * port * receive allow", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * port * receive allow ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * port * receive allow ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [neighbor * port * receive allow ipv6 unicast] and [neighbor * receive allow ipv6 multicast]
			temp = buildCommandTree(root, "neighbor * port * receive allow", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "neighbor * port * receive allow ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "neighbor * port * receive allow ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: initialize the no peer neighbor announce/receive commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ September 18, 2008   
 * -------------------------------------------------------------------------------------*/
int initNoPeerNeighborAnnounceReceiveCommands(commandNode * root) {
	commandNode * temp = NULL;

	// [no neighbor * announce] and [no neighbor * port * announce] command
	if ( (temp = buildCommandTree(root, "no neighbor *", 1,
			buildCommand("announce", "announce", ROUTER_BGP, NULL))) == NULL)
	{
		log_fatal("initNoPeerNeighborAnnounceReceiveCommands(): could not init the command tree.");
	}
	
		// [no neighbor * announce * * *] command
		temp = buildCommandTree(root, "no neighbor * announce", 1,
				buildCommand("*", "[code]", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "no neighbor * announce *", 1,
				buildCommand("*", "[length]", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "no neighbor * announce * *", 1,
				buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborAnnounce));
		
		// [no neighbor * announce ipv4 unicast] and [no neighbor * announce ipv4 multicast]
		temp = buildCommandTree(root, "no neighbor * announce", 1,
				buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "no neighbor * announce ipv4", 1,
				buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		temp = buildCommandTree(root, "no neighbor * announce ipv4", 1,
				buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		
		// [no neighbor * announce ipv6 unicast] and [no neighbor * announce ipv6 multicast]
		temp = buildCommandTree(root, "no neighbor * announce", 1,
				buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "no neighbor * announce ipv6", 1,
				buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		temp = buildCommandTree(root, "no neighbor * announce ipv6", 1,
				buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
	
	// [no neighbor * port * announce] command
	temp = buildCommandTree(root, "no neighbor * port *", 1,
			buildCommand("announce", "announce", ROUTER_BGP, NULL));
	
		// [no neighbor * port * announce * * *] command
		temp = buildCommandTree(root, "no neighbor * port * announce", 1,
				buildCommand("*", "[code]", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "no neighbor * port * announce *", 1,
				buildCommand("*", "[length]", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "no neighbor * port * announce * *", 1,
				buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborAnnounce));
		
		// [no neighbor * port * announce ipv4 unicast] and [no neighbor * announce ipv4 multicast]
		temp = buildCommandTree(root, "no neighbor * port * announce", 1,
				buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "no neighbor * port * announce ipv4", 1,
				buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		temp = buildCommandTree(root, "no neighbor * port * announce ipv4", 1,
				buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		
		// [no neighbor * port * announce ipv6 unicast] and [no neighbor * announce ipv6 multicast]
		temp = buildCommandTree(root, "no neighbor * port * announce", 1,
				buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
		temp = buildCommandTree(root, "no neighbor * port * announce ipv6", 1,
				buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
		temp = buildCommandTree(root, "no neighbor * port * announce ipv6", 1,
				buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborAnnounceIpv4Ipv6UnicastMulticast));
	
	// [no neighbor * receive ] command
	temp = buildCommandTree(root, "no neighbor *", 1,
			buildCommand("receive", "receive", ROUTER_BGP, NULL));

		// [no neighbor * receive require] command
		temp = buildCommandTree(root, "no neighbor * receive", 1,
				buildCommand("require", "require", ROUTER_BGP, NULL));

			// [no neighbor * receive require * * *] command
			temp = buildCommandTree(root, "no neighbor * receive require", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * receive require *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "no neighbor * receive require * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [no neighbor * receive require ipv4 unicast] and [no neighbor * receive require ipv4 multicast]
			temp = buildCommandTree(root, "no neighbor * receive require", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * receive require ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * receive require ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [no neighbor * receive require ipv6 unicast] and [no neighbor * receive require ipv6 multicast]
			temp = buildCommandTree(root, "no neighbor * receive require", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * receive require ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * receive require ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));


		// [no neighbor * receive refuse] command
		temp = buildCommandTree(root, "no neighbor * receive", 1,
				buildCommand("refuse", "refuse", ROUTER_BGP, NULL));
			
			// [no neighbor * receive refuse * * *] command
			temp = buildCommandTree(root, "no neighbor * receive refuse", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * receive refuse *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "no neighbor * receive refuse * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [no neighbor * receive refuse ipv4 unicast] and [no neighbor * receive refuse ipv4 multicast]
			temp = buildCommandTree(root, "no neighbor * receive refuse", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * receive refuse ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * receive refuse ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [no neighbor * receive refuse ipv6 unicast] and [no neighbor * receive refuse ipv6 multicast]
			temp = buildCommandTree(root, "no neighbor * receive refuse", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * receive refuse ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * receive refuse ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));

		// [no neighbor * receive allow] command
		temp = buildCommandTree(root, "no neighbor * receive", 1,
				buildCommand("allow", "allow", ROUTER_BGP, NULL));

			// [no neighbor * receive allow * * *] command
			temp = buildCommandTree(root, "no neighbor * receive allow", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * receive allow *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "no neighbor * receive allow * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [no neighbor * receive allow ipv4 unicast] and [no neighbor * receive allow ipv4 multicast]
			temp = buildCommandTree(root, "no neighbor * receive allow", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * receive allow ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * receive allow ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [no neighbor * receive allow ipv6 unicast] and [no neighbor * receive allow ipv6 multicast]
			temp = buildCommandTree(root, "no neighbor * receive allow", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * receive allow ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * receive allow ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
	
	// [no neighbor * port * receive ] command
	temp = buildCommandTree(root, "no neighbor * port *", 1,
			buildCommand("receive", "receive", ROUTER_BGP, NULL));

		// [no neighbor * port * receive require] command
		temp = buildCommandTree(root, "no neighbor * port * receive", 1,
				buildCommand("require", "require", ROUTER_BGP, NULL));

			// [no neighbor * port * receive require * * *] command
			temp = buildCommandTree(root, "no neighbor * port * receive require", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * port * receive require *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "no neighbor * port * receive require * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [no neighbor * port * receive require ipv4 unicast] and [no neighbor * receive require ipv4 multicast]
			temp = buildCommandTree(root, "no neighbor * port * receive require", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * port * receive require ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * port * receive require ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [no neighbor * port * receive require ipv6 unicast] and [no neighbor * receive require ipv6 multicast]
			temp = buildCommandTree(root, "no neighbor * port * receive require", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * port * receive require ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * port * receive require ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));


		// [no neighbor * port * receive refuse] command
		temp = buildCommandTree(root, "no neighbor * port * receive", 1,
				buildCommand("refuse", "refuse", ROUTER_BGP, NULL));
			
			// [no neighbor * port * receive refuse * * *] command
			temp = buildCommandTree(root, "no neighbor * port * receive refuse", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * port * receive refuse *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "no neighbor * port * receive refuse * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [no neighbor * port * receive refuse ipv4 unicast] and [no neighbor * receive refuse ipv4 multicast]
			temp = buildCommandTree(root, "no neighbor * port * receive refuse", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * port * receive refuse ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * port * receive refuse ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [no neighbor * port * receive refuse ipv6 unicast] and [no neighbor * receive refuse ipv6 multicast]
			temp = buildCommandTree(root, "no neighbor * port * receive refuse", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * port * receive refuse ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * port * receive refuse ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));

		// [no neighbor * port * receive allow] command
		temp = buildCommandTree(root, "no neighbor * port * receive", 1,
				buildCommand("allow", "allow", ROUTER_BGP, NULL));

			// [no neighbor * port * receive allow * * *] command
			temp = buildCommandTree(root, "no neighbor * port * receive allow", 1,
					buildCommand("*", "[code]", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * port * receive allow *", 1,
					buildCommand("*", "[length]", ROUTER_BGP, &cmdNeighborReceive));
			temp = buildCommandTree(root, "no neighbor * port * receive allow * *", 1,
					buildCommand("*", "[value]", ROUTER_BGP, &cmdNeighborReceive));
			
			// [no neighbor * port * receive allow ipv4 unicast] and [no neighbor * receive allow ipv4 multicast]
			temp = buildCommandTree(root, "no neighbor * port * receive allow", 1,
					buildCommand("ipv4", "ipv4", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * port * receive allow ipv4", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * port * receive allow ipv4", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			
			// [no neighbor * port * receive allow ipv6 unicast] and [no neighbor * receive allow ipv6 multicast]
			temp = buildCommandTree(root, "no neighbor * port * receive allow", 1,
					buildCommand("ipv6", "ipv6", ROUTER_BGP, NULL));
			temp = buildCommandTree(root, "no neighbor * port * receive allow ipv6", 1,
					buildCommand("unicast", "unicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));
			temp = buildCommandTree(root, "no neighbor * port * receive allow ipv6", 1,
					buildCommand("multicast", "multicast", ROUTER_BGP, &cmdNeighborReceiveIpv4Ipv6UnicastMulticast));

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: initialize the no peer neighbor announce/receive commands
 * Input: A pointer to a commandNode where the commands will be added onto
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ May 25, 2009
 * -------------------------------------------------------------------------------------*/
int initMrtCommands(commandNode * root) {
	commandNode * temp = NULL;

	// base [mrt-listener] and [mrt] command
	temp = buildCommandTree(root, "root", 2,
			buildCommand("mrt", "mrt", CONFIGURE, NULL),
			buildCommand("mrt-listener", "mrt-listener", CONFIGURE, NULL));

	// base [show mrt-listener] and [show mrt] command
	temp = buildCommandTree(root, "show", 2,
			buildCommand("mrt", "mrt", CONFIGURE, NULL),
			buildCommand("mrt-listener", "mrt-listener", CONFIGURE, NULL));

	// MRT-LISTENER
		// [mrt-listener port]
		temp = buildCommandTree(root, "mrt-listener", 1,
				buildCommand("port", "port", CONFIGURE , NULL));
		temp = buildCommandTree(root, "mrt-listener port", 1,
				buildCommand("*", "*", CONFIGURE , &cmdMrtListenerPort));
		
		// [mrt-listener address]
		temp = buildCommandTree(root, "mrt-listener", 1,
				buildCommand("address", "address", CONFIGURE , NULL));
		temp = buildCommandTree(root, "mrt-listener address", 1,
				buildCommand("*", "*", CONFIGURE , &cmdMrtListenerAddress));

		// [mrt-listener enable] 
		temp = buildCommandTree(root, "mrt-listener", 1,
				buildCommand("enable", "enable", CONFIGURE , &cmdMrtListenerEnable));
		
		// [mrt-listener disable]
		temp = buildCommandTree(root, "mrt-listener", 1,
				buildCommand("disable", "disable", CONFIGURE , &cmdMrtListenerDisable));
	
		// [mrt-listener acl *] command
		temp = buildCommandTree(root, "mrt-listener", 1,
				buildCommand("acl", "acl", CONFIGURE, NULL));
		temp = buildCommandTree(temp, "acl", 1,
				buildCommand("*", "[acl name]", CONFIGURE, &cmdSetMrtListenerAcl));
		
		// [mrt-listener limit *] command
		temp = buildCommandTree(root, "mrt-listener", 1,
				buildCommand("limit", "limit", CONFIGURE, NULL));
		temp = buildCommandTree(temp, "limit", 1,
				buildCommand("*", "[max connections]", CONFIGURE, &cmdMrtListenerLimit));
		
		// [show mrt-listener acl] command
		temp = buildCommandTree(root, "show mrt-listener", 1,
				buildCommand("acl", "acl", ACCESS | ENABLE | CONFIGURE, &cmdShowMrtListenerAcl));

		// [show mrt-listener status]
		temp = buildCommandTree(root, "show mrt-listener", 1,
				buildCommand("status", "status", ACCESS | ENABLE | CONFIGURE , &cmdShowMrtListenerStatus));

		// [show mrt-listener port]
		temp = buildCommandTree(root, "show mrt-listener", 1,
				buildCommand("port", "port", ACCESS | ENABLE | CONFIGURE , &cmdShowMrtListenerPort));

		// [show mrt-listener address]
		temp = buildCommandTree(root, "show mrt-listener", 1,
				buildCommand("address", "address", ACCESS | ENABLE | CONFIGURE , &cmdShowMrtListenerAddress));

		// [show mrt-listener summary]
		temp = buildCommandTree(root, "show mrt-listener", 1,
				buildCommand("summary", "summary", ACCESS | ENABLE | CONFIGURE , &cmdShowMrtListenerSummary));

		// [show mrt-listener limit]
		temp = buildCommandTree(root, "show mrt-listener", 1,
				buildCommand("limit", "limit", ACCESS | ENABLE | CONFIGURE , &cmdShowMrtListenerLimit));

	// MRT CLIENT
		// [show mrt client] and [show mrt client *]
		temp = buildCommandTree(root, "show mrt", 1,
				buildCommand("clients", "clients", ACCESS | ENABLE | CONFIGURE , &cmdShowMrtClient));
		temp = buildCommandTree(root, "show mrt", 1,
				buildCommand("neighbor", "neighbor", ACCESS | ENABLE | CONFIGURE , &cmdShowMrtNeighbor));
			
//		temp = buildCommandTree(root, "show mrt client", 1,
//				buildCommand("*", "*", CONFIGURE , &cmdShowMrtClientId));
	
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This is the main loop for a cli thread.  It receives commands from the 
 * 	client and will execute the appropriate command.
 * Input: arg - A struct containing all the arguments to start a thread (clientThreadArguments)
 * Note:        
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
void * cliThread( void * arg) {
	// initialize the client settings
	clientThreadArguments * client = (clientThreadArguments *)arg;
	client->commandMask = NONE;
	client->currentACL = NULL;
	
	log_msg("Command line connection [%d] from [%s:%d]", LoginSettings.activeConnections, client->addr, client->port); 	//report connection

	char input_buffer[MAX_COMMAND_LENGTH], prompt[MAX_PROMPT_LENGTH], hostname[MAX_HOSTNAME_LENGTH], temp[MAX_COMMAND_LENGTH];
	memset(input_buffer, 0, MAX_COMMAND_LENGTH);
	memset(prompt, 0, MAX_PROMPT_LENGTH);
	memset(hostname, 0, MAX_HOSTNAME_LENGTH);
	int bytes_read=0, bytes_sent=0;
	commandNode* cs = NULL;
	char * pch = NULL;

	// get the host name for the prompt
	if(gethostname(hostname, MAX_HOSTNAME_LENGTH)==-1) {
		log_fatal("Unable to get the host name: %s", strerror(errno));
	}

	// create reference for command arguments
	commandArgument * ca = NULL;

	// check access password
	sendMessage(client->socket, "Password: ");
	bytes_read = getMessage(client->socket, input_buffer, MAX_COMMAND_LENGTH);

	// trap errors when reading from socket
	if(bytes_read>0) {
		if(strcmp(input_buffer, LoginSettings.accessPassword)==0) {
			// ACCESS authentication was successful
			client->commandMask = ACCESS;
			memset(input_buffer, 0, MAX_COMMAND_LENGTH);
		
			// THE infinite loop
			while(LoginSettings.shutdown == FALSE) {
				// update last action
				client->lastAction = time(NULL);

				// write out prompt
				strncpy(prompt, hostname, MAX_HOSTNAME_LENGTH);
				switch(client->commandMask) {
					case ACCESS:
						strncat(prompt, PROMPT_ACCESS, MAX_PROMPT_LENGTH);
						break;
					case ENABLE:
						strncat(prompt, PROMPT_ENABLE, MAX_PROMPT_LENGTH);
						break;
					case CONFIGURE:
						strncat(prompt, PROMPT_CONFIGURE, MAX_PROMPT_LENGTH);
						break;
					case ACCESS_CONTROL_LIST:
						strncat(prompt, PROMPT_ACCESS_CONTROL_LIST, MAX_PROMPT_LENGTH);
						break;
					case ROUTER_BGP:
						strncat(prompt, PROMPT_ROUTER_BGP, MAX_PROMPT_LENGTH);
						break;
				}
				bytes_sent = sendMessage(client->socket, prompt);
				if (bytes_sent < 0)
				{
					log_err("cliThread(): could not send message to the client!");
				}

				// get command from user
				bytes_read = getMessage(client->socket, input_buffer, MAX_COMMAND_LENGTH);

				if(bytes_read>=0) {
					memcpy(temp, input_buffer, MAX_COMMAND_LENGTH);

					// if exit statement seen then exit exit loop
					if(strcmp(input_buffer, COMMAND_EXIT)==0 && (client->commandMask==ACCESS ))
						break;

					// find the correct command
					cs = findCommandNode(LoginSettings.rootCommandNode, input_buffer, pch, 0, TRUE, client->commandMask, &ca);
					if(cs!=NULL) {
						// check to see if a function pointer was found
						if(cs->cmdPtr!=NULL) {
							// call the command
							(*cs->cmdPtr)(ca, client, cs);
						}
						else {
							// check to see if the help command was called
							if(ca!=NULL && (strcmp(ca->commandArgument, SHOW_HELP_ARGUMENT_SINGLE)==0 || strcmp(ca->commandArgument, SHOW_HELP_ARGUMENT_MULTI)==0)) {
								showHelp(ca, client, cs);
							}
							else {
								sendMessage(client->socket, "command [%s] not found\n", temp);
							}
						}
					}
					else {
						sendMessage(client->socket, "command [%s] not found\n", temp);
					}
				}
				else {
					// an error occured or a user closed the connection.

					// make sure enableMode is turned off
					enableModeInactive();
					break;
				}

				// reset input variables
				freeCommandArguments(ca);
				ca = NULL;
				memset(prompt, 0, MAX_PROMPT_LENGTH);
				memset(input_buffer, 0, MAX_COMMAND_LENGTH);
				cs = NULL;
			}
		}
		else {
			sendMessage(client->socket, "incorrect access password\n");
		}
	}

	// release all allocated memory for arguments
	freeCommandArguments(ca);
	
	// close connection then exit
	log_msg("Connection closed from [%s:%d]", client->addr, client->port); 	

	// close connection and clean up memory
	destroyCli(client->id);
	pthread_exit(NULL);
}



/*----------------------------------------------------------------------------------------
 * Purpose: If clientSocket is greater than 0 then this function will send a message
 * 	over the open socket to the client.  If clientSocket is not greater than 0
 * 	then this function will just print the message out to stdout.
 * Input: clientSocket - an open tcp socket.
 * 	msg - the actual format of the message to be sent
 * 	... - the parameters to be substituted into the formatted string
 * Output: The total number of bytes sent over the socket or printed to stdout.
 * Note:        
 * Kevin Burnett @ July 30, 2008   
 * -------------------------------------------------------------------------------------*/
int sendMessage(int clientSocket, char * msg, ... ) {
	int bytes_sent=0, msg_length=0;
	char buffer[MAX_COMMAND_LENGTH];
	va_list args;

	// get the arguments from the variable length array
	va_start(args, msg);
	
	// generate a string from the arguments
	msg_length = vsprintf(buffer, msg, args);
	va_end(args);

	// send string to client
	if(clientSocket>0) {
		bytes_sent = send(clientSocket, buffer, msg_length, 0);
	}
	else {
		bytes_sent = printf("%s", buffer);
	}

	return bytes_sent;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads a message from the open clientSocket and writes the message, based on
 * 	message length, into the msg buffer.
 * Input: clientSocket - An open socket to a client
 * 	char * msg - A pointer to a character array where the message will be written
 * 	int msg_len - the maximum length of the message
 * Output: The number of bytes received from the client
 * Note:        
 * Kevin Burnett @ July 30, 2008   
 * -------------------------------------------------------------------------------------*/
int getMessage(int clientSocket, char * msg, int msg_len) {
	char * temp = malloc(msg_len);
	int bytes_read=0;
	int socket = 0;
	memset(temp, 0, msg_len);

	// file descriptor list for select()
	fd_set readfds;

	// timer to periodically check thread status
	struct timeval timeout; 
	timeout.tv_usec = 0;
	timeout.tv_sec = THREAD_CHECK_INTERVAL; 

	while( LoginSettings.shutdown == FALSE ) {
		FD_ZERO(&readfds);
		FD_SET(clientSocket, &readfds);

		socket=select(clientSocket+1, &readfds, NULL, NULL, &timeout);
 		if ( socket < 0 )
		{
                	log_fatal( "getMessage(): select error");
		}


		if(FD_ISSET(clientSocket, &readfds)) {

			// get message
			bytes_read = recv(clientSocket, temp, msg_len, 0) - 2;

			if(bytes_read<0) {

				// free temporary allocated space
				free(temp);

				// connection closed by user or error in socket
				return bytes_read;
			}

			// trim off trailing \n and \r from telnet
			memset(&temp[bytes_read], 0, msg_len-bytes_read);

			// copy value into return array
			strncpy(msg, temp, msg_len);

			// free temporary allocated space
			free(temp);

			// return amount read
			return bytes_read;
		}
		else {
			// free temporary allocated space
			free(temp);
		}
	}

	sendMessage(clientSocket, "\nBGPmon server is shutting down, disconnecting.\n");

	// free temporary allocated space
	free(temp);

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This function will add subcommands to the commandNode passed in.  It will add
 * 	the new commands at the location specified in the path specified by path_temp.
 * Input: cs - the root command node where all the sub commands will be added
 * 	path_temp - the path to the location in the commandNode where the new commands
 * 		will be added
 * 	subCommandsCount - the number of subcommands to add
 * 	... - the sub commands
 * Output: Returns a pointer to the node where the new commands were added
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
commandNode* buildCommandTree(commandNode * cs, char * path_temp, int subCommandsCount, ... ) {
	commandNode * ret = NULL;
	char * pch = NULL;

	// make a writable string so strtok works	
	char path[MAX_COMMAND_LENGTH];
	memset(path, 0, MAX_COMMAND_LENGTH);
	strncpy(path, path_temp, MAX_COMMAND_LENGTH);

	// find the correct commandNode in tree
	if(strcmp(path, "root")!=0) {
		// find the correct node in the tree
		ret = findCommandNode(cs, path, pch, 0, FALSE, NONE, NULL);
	}
	else {
		// return base node 
		ret = cs;
	}

	// if node isn't found then log fatal error
	if(ret==NULL) {
		log_fatal("The path [%s] was not found while initializing the command tree.", path_temp);
	}

	// parse variable array list and create commands structs for each string
	va_list argptr;
	va_start(argptr,subCommandsCount);
	commandNode * c = NULL;
	int i=0, empty_node_index=0;

	// find first empty index in subcommands
	for(empty_node_index=0; empty_node_index<MAX_SUB_COMMANDS; empty_node_index++) {
		if(ret->subcommands[empty_node_index]==NULL) {
			break;
		}
	}
	
	// if there isn't room for the subcommands then exit program 
	if(ret->subcommands[empty_node_index + subCommandsCount]!=NULL || empty_node_index + subCommandsCount >= MAX_SUB_COMMANDS) {
		log_fatal("%s node in command tree has max number of subnodes.", ret->command);
	}	

	// loop through commandNodes to add
	for (i=empty_node_index;i<subCommandsCount + empty_node_index;i++) {
		// get the sub command then attach them to parent
		c = va_arg(argptr, commandNode*);
		c->parent = ret;
		ret->subcommands[i] = c;

		// add the mask to the parent nodes so the command can be reached
		setParentLinkMask(c->parent, c->commandMask);
	}
	va_end(argptr);

	return ret;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Based on parameters will create and return pointer to a commandNode
 * Input:
 * 	command - the actual commmand in the command tree
 * 	description - the description of the command
 * 	mask - the mask that should be allowed to see the command
 * 	cmdPtr - the function that will be called  
 * Output: A pointer to the commandNode that was constructed
 * Note: The caller doesn't need to allocate memory but will need to free it when done
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
commandNode* buildCommand(char * command, char * description, COMMAND_MASK mask, int (*cmdPtr)(commandArgument *, clientThreadArguments *, commandNode *)) {
	commandNode * c = malloc(sizeof(commandNode));
	memset(c, 0, sizeof(commandNode));
	strncpy(c->command, command, MAX_ARGUMENT_LENGTH);
	strncpy(c->description, description, MAX_ARGUMENT_LENGTH);
	c->commandMask = mask;
	c->cmdPtr = cmdPtr;

	return c;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Recursively frees allocated memory associated with commandNodes.
 * Input: commandNode* - pointer to a commandNode struct
 * Output: NA
 * Note:        
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
void freeCommandNode(commandNode* cs) {
	// clean up child nodes first
	int i = 0;
	for(i=0; i<MAX_SUB_COMMANDS; i++) {
		if(cs->subcommands[i]!=NULL) {
			// recurse on children
			freeCommandNode(cs->subcommands[i]);
		}
	}

	// then clean up node
	free(cs);
}

/*----------------------------------------------------------------------------------------
 * Purpose: Recursively frees allocated memory associated with commandArguments.
 * Input: commandArgument* - pointer to a commandArgument struct
 * Output: NA 
 * Note:    
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
void freeCommandArguments(commandArgument * root) {
	commandArgument * ca_next = NULL, * ca = NULL;
	if(root!=NULL) {
		ca = root;

		// loop through linked list and clean up nodes
		while(ca!=NULL) {
			// get references to the next node
			ca_next = ca->nextArgument;
			// free current node
			free(ca);
			// set current node to the next node
			ca = ca_next;
		}

		root = NULL;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Based on the input and commandNode, this function will recursively step through the
 * 	commandNode structure and try to find the corresponding commandNode for the input string.
 * Input: commandNode* - A pointer to the command tree structure
 * 	char * input - A string indicating what node to return.  Example "show chain ex1"
 * 	char * pch - Used by the string tokenizer, this is a pointer to the current command
 * 		that had been tokenized.
 * 	int count - An integer representing how many levels down the recursive function has been called
 * 	commandArgument* - A pointer to the a commandArgument struct which will be populated with user 
 * 		input as wildcard (*) nodes are encountered in the command tree structure.
 * 	mask - The bit mask to be used when determining which commands are available for a client 
 * 	ca - a double pointer to the CommandArguments that will be returned when the command
 * 		is done being parsed
 * Output: A pointer to the commandNode* is returned if found.  If not then commandNode* is NULL.
 * Note:        
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
commandNode* findCommandNode(commandNode* cs, char * input, char * pch, int count, int returnCommandArguments, int mask, commandArgument ** ca) {
	// very useful debug code I'm keeping in here for now
	//int y=0;
	//for(y=0; y<count; y++)
	//	printf("\t");
	//printf("cs[%p] cs[%s] input[%s] pch[%s]\n", cs, cs->command, input, pch);

	commandNode * retval = NULL;
	int wildcardIndex = -1;
	if(count==0) {
		// initial case
		// establish the tokenizer
		pch=strtok(input, " ");
	}
	else {
		// increment token
		pch=strtok(NULL, " ");
	}

	if(pch==NULL) {
		// base case (exit case)
		// no more arguments found
		return cs;
	}
	else {
		// check to see if argument is the show help command
		// if show help command is found then stop looking for
		// more arguments
		if(returnCommandArguments==TRUE && (strcmp(pch, SHOW_HELP_ARGUMENT_SINGLE)==0 || strcmp(pch, SHOW_HELP_ARGUMENT_MULTI)==0)) {
			// append ? or ?? to command arguments so showHelp() knows what was called
			addCommandArgument(ca, pch);
			retval = cs;
		}
		else {
			int i = 0;
			// loop through sub comands
			for(i=0; i<MAX_SUB_COMMANDS; i++) {
				// make sure sub command exists and matches our command
				if(cs->subcommands[i]!=NULL) {
					// check to see if command matches and the command is available
					if(strcmp(cs->subcommands[i]->command, pch)==0
						&& ((cs->subcommands[i]->commandMask & mask) - mask)==0) {
						// found a command
						retval = findCommandNode(cs->subcommands[i], input, pch, count+1, returnCommandArguments, mask, ca);

						if(retval!=NULL) {
							wildcardIndex = -1;
							break;
						}
					}
					else {
						// see if a wildcard is found...if it is found keep looking for a matching
						// command before processing the command as a wildcard
						if(strcmp(cs->subcommands[i]->command, WILDCARD_ARGUMENT)==0) {
							wildcardIndex = i;
						}
					}
				}
			}

			// A wildcard node was found and no matching subcommand was found
			if(wildcardIndex>=0) {
				if(returnCommandArguments==TRUE) {
					// create commandArgument node
					addCommandArgument(ca, pch);
					retval = findCommandNode(cs->subcommands[wildcardIndex], input, pch, count+1, returnCommandArguments, mask, ca);
				} 
				else {
					// no command arguments are to be returned since ca==NULL 
					retval = findCommandNode(cs->subcommands[wildcardIndex], input, pch, count+1, returnCommandArguments, mask, NULL);
				}
				wildcardIndex = -1;
			}
		}
	}

	return retval;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This function will take in a linked list and a command and add the command
 * 	to the linked list, handling all the corner cases.
 * Input: ca - the linked list of commandArguments
 * 	command - the command to be added to the list
 * Output: NA
 * Note: The caller doesn't need to allocate memory but will need to free it when done
 * Kevin Burnett @ August 10, 2008   
 * -------------------------------------------------------------------------------------*/
void addCommandArgument(commandArgument ** ca, char * command) {
	commandArgument * ca_cur = NULL;

	// create new argument
	commandArgument * ca_new = malloc(sizeof(commandArgument));
	memset(ca_new, 0, sizeof(commandArgument));
	strncpy(ca_new->commandArgument, command, MAX_ARGUMENT_LENGTH);

	if(*ca==NULL) {
		// first node in the list
		ca_new->firstArgument = ca_new;
		ca_new->nextArgument = NULL;
		*ca = ca_new;
	}
	else {
		// loop to the end of the list
		ca_cur = *ca;
		while(ca_cur->nextArgument!=NULL) {
			ca_cur = ca_cur->nextArgument;
		}
		
		// linking on a node to an existing node
		ca_new->firstArgument = ca_cur->firstArgument;
		ca_cur->nextArgument = ca_new;
		ca_new->previousArgument = ca_cur;
	}
}


/*----------------------------------------------------------------------------------------
 * Purpose: This function will walk up the commandNode structure and set the current and 
 * 	parent nodes to include the mask passed in.  Very useful when trying to grant
 * 	access to a command burried deep in the command structure
 * Input: cs - A command node that wil have the mask applied to it.
 * 	mask - the mask to be applied to the command
 * Output: NA
 * Kevin Burnett @ August 30, 2008   
 * -------------------------------------------------------------------------------------*/
void setParentLinkMask(commandNode* cs, COMMAND_MASK mask) {
	do {
		cs->commandMask |= mask;
		cs = cs->parent;
	}
	while(cs!=NULL);
}

/*----------------------------------------------------------------------------------------
 * Purpose: Walks from leaf to root through the command structure looking for a 
 * 	specific command
 * Input: commandNode - A pointer to a node in the command tree structure.
 * Output: 1 if the nodes is found, 0 if it was not found
 * Kevin Burnett @ December 2, 2008   
 * -------------------------------------------------------------------------------------*/
int 
listContainsCommand(commandNode * node, char * command) {
  while(node!=NULL) {
    if(strcmp(node->description, command)==0){
      return 1;
    }
    node = node->parent;
  }
  return 0;
}

/*******************************************************************************
 * Purpose: Walks from leaf to root through the command structure looking for a 
 * 	specific pair of commands (must be neighbors)
 * Input: commandNode - A pointer to a node in the command tree structure.
 * Output: 1 if the nodes are found, 0 if it was not found
 * Cathie Olschanowsky May 2012
*******************************************************************************/
int 
listContainsCommands2(commandNode *node, const char *command1, 
                      const char *command2){

  while(node!=NULL && node->parent != NULL) {
    if(strcmp(node->description, command2)==0 && 
       strcmp(node->parent->description, command1)==0){
      return 1;
    }
    node = node->parent;
  }
  return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for this thread
 * Input:	
 * Output: a timevalue indicating the last time the thread was active
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/ 
time_t getCliLastAction(long id) {
	time_t ans = 0;
	CliNode * cn = LoginSettings.firstNode;
	while( cn != NULL )
	{
		if(cn->id == id)
			return  cn->lastAction;
		else	
			cn = cn->next;
	}
	log_err("getCliLastAction: couldn't find a cli connection with ID: %d", id);
	return ans;			
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for this thread
 * Input:	
 * Output: a timevalue indicating the last time the thread was active
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/ 
int getActiveCliIds(long **cliIds) {
	// allocate an array whose size depends on the active clients
	long *Ids = malloc(sizeof(long)*LoginSettings.activeConnections);
	if (Ids == NULL) 
	{
		log_err("Failed to allocate memory for getActiveCliIds");
		*cliIds = NULL; 
		return -1;
	}

	// lock the client list
	if ( pthread_mutex_lock( &(LoginSettings.loginLock ) ) )
		log_fatal("lock login list failed");

	// for each active client, add its ID to the array
	CliNode *cn = LoginSettings.firstNode;
	int i = 0;
	while( cn!=NULL )
	{
		Ids[i] = cn->id;
		i++;
		cn = cn->next;
	}
	*cliIds = Ids; 

	//sanity check how many IDs we found
	if ( i!=LoginSettings.activeConnections )
	{
		log_err("Unable to get Active CLI IDs!");
		free(Ids);
		*cliIds = NULL; 
		i = -1;
	}

	// unlock the client list
	if ( pthread_mutex_unlock( &(LoginSettings.loginLock ) ) )
		log_fatal( "unlock cli list failed");

	return i;
}

/*--------------------------------------------------------------------------------------
 * Purpose:  destroy the socket and memory associated with a cli ID
 * Input:  the cli ID to destroy
 * Output: none
 * Kevin Burnett @ August 1, 2009
 * -------------------------------------------------------------------------------------*/
void 
destroyCli(long id) {
	// lock the cli list
	if ( pthread_mutex_lock( &(LoginSettings.loginLock ) ) )
		log_fatal( "lock cli list failed");

	CliNode *prev = NULL;
	CliNode *cn = LoginSettings.firstNode;

	while(cn != NULL) {
		if (cn->id == id ) {
			// close this cli connection
			log_msg("Deleting cli id (%d)", cn->id);		
			// close the clie socket
			close( cn->socket );
			// remove from the cli list
			LoginSettings.activeConnections--;
			if (prev == NULL) 
				LoginSettings.firstNode = cn->next;
			else
				prev->next = cn->next;
			// clean up the memory
			free(cn);
			// unlock the client list
			if ( pthread_mutex_unlock( &(LoginSettings.loginLock ) ) )
				log_fatal( "unlock cli list failed");			
			return;
		}
		else
		{
			prev = cn;
			cn = cn->next;
		}
	}
}

