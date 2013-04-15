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
 *  File: commands.c
 * 	Authors: Kevin Burnett
 *
 *  Date: November 5th, 2008
 */

// Needed for atoi() 
#include <stdlib.h>
// Needed for memset,strncmp
#include <string.h>
// Needed for strerror(errno)
#include <errno.h>

// Needed for the function definitions
#include "acl_commands.h"
// Provides the definition for clientThreadArguments struct
#include "login.h"
// Provides the definition for commandArgument and commandNode structs
#include "commandprompt.h"
// Provides defaults used and Queue names
#include "../site_defaults.h"
// needed for TRUE/FALSE definitions
#include "../Util/bgpmon_defaults.h"
// Provides access to ACL structs and definitions
#include "../Util/acl.h"
// provides access to ACL management functions
#include "../Util/aclmanagement.h"
// provides address verification
#include "../Util/address.h"

/*----------------------------------------------------------------------------------------
 * Purpose: changes the current CLI-user context to editing an ACL 
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int ACL(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	client->commandMask = ACCESS_CONTROL_LIST;

	// get reference to ACL
	AccessControlList * acl = NULL;
	acl = getACL(ca->commandArgument);
	if(acl!=NULL) {
		sendMessage(client->socket, "Editing ACL: ");
	}
	else {
		// if ACL doesn't exist then create it
		acl = createACL(ca->commandArgument);
		sendMessage(client->socket, "Created ACL, now editing: ");
	}
	sendMessage(client->socket, "%s\n", acl->name);

	// set client context variable
	client->currentACL = acl;

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Command called to remove an ACL.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int noACL(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	removeACL(ca->commandArgument);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Displays the information for an ACL
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
int showACL(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	char * buffer = NULL;

	AccessControlList * list = LoginSettings.rootAcl;
	ACLRule * rule = NULL;
	while(list!=NULL) {
		if(ca->commandArgument==NULL || strcmp(ca->commandArgument, list->name)==0) {
			// get reference to first rule in the list
			rule = list->head;

			// display ACL info
			if((buffer=displayAccessControlList(list))==NULL) {
				log_fatal("Error reading Access Control List [%s]:%s", list->name, strerror(errno));
				continue;
			}
			sendMessage(client->socket, "ACL name:");
			sendMessage(client->socket, buffer);
			free(buffer);
			buffer=NULL;

			// print ACL rule header
			buffer=displayACLRuleHeader();
			sendMessage(client->socket, buffer);
			free(buffer);
			buffer=NULL;

			// loop through all the rules in ACL
			while(rule!=NULL) {
				// display rule info
				if((buffer=displayACLRule(rule))==NULL) {
					log_fatal("Error reading Access Control List [%s]:%s", list->name, strerror(errno));
				}
				sendMessage(client->socket, buffer);
				free(buffer);
				buffer=NULL;

				// increment rules
				rule = rule->nextRule;
			}

			// we should only reach this if single list was queried by the user (ie. commandArgument!=NULL)
			if(ca->commandArgument!=NULL)
				break;
		}

		sendMessage(client->socket, "\n");

		// increment lists
		list = list->nextList;
	}
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Sets the command level back to CONFIGURE from ACL
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ August 1, 2008   
 * -------------------------------------------------------------------------------------*/
int exitACL(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	client->commandMask = CONFIGURE;
	client->currentACL = NULL;
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Sets the command level back to ENABLE from ACL
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ August 1, 2008   
 * -------------------------------------------------------------------------------------*/
int endACL(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	client->commandMask = ENABLE;
	client->currentACL = NULL;
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Looks through the command arguments and determines if the command
 * 	is a permit or deny command
 * Input: cn - a node to the Command Arguments
 * Output: Returns PERMIT or DENY
 * Kevin Burnett @ September 6, 2008   
 * -------------------------------------------------------------------------------------*/
ACCESS_RULE getAccessRule(commandNode * cn) {
	ACCESS_RULE r = PERMIT;

	do {
		if(strcmp(cn->command, "permit")==0) {
			r = PERMIT;
			break;
		}

		if(strcmp(cn->command, "deny")==0) {
			r = DENY;
			break;
		}

		if(strcmp(cn->command, "label")==0) {
			r = LABEL;
			break;
		}

		if(strcmp(cn->command, "ribonly")==0) {
			r = RIBONLY;
			break;
		}

		cn = cn->parent;
	} while (cn->parent!=NULL);

	return r;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This function is called to create a permit or deny ACL rule. 
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ August 7, 2008   
 * -------------------------------------------------------------------------------------*/
int ACLRuleAny(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	// check to see if an index was specified
	int targetIndex = -1;
	ACCESS_RULE r = getAccessRule(cn);

	if(ca!=NULL) {
		targetIndex = atoi(ca->commandArgument);
	}

	// create new rule
	if(addACLRule(client->currentACL, targetIndex, NULL, NULL, r)==NULL) {
		sendMessage(client->socket, "Error creating access control list rule.");
		return 1;
	}
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This function is called to create a permit or deny ACL rule. This function 
 * 	also extracts the address and mask from the arguments that are passed in.
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ October 24, 2008   
 * -------------------------------------------------------------------------------------*/
int ACLRuleAddress(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int targetIndex = -1;
	ACCESS_RULE r = getAccessRule(cn);

	char * address = NULL;
	char * mask = NULL;

	// get the address
	address = ca->commandArgument;
	if(checkAddress(address, ADDR_ACTIVE)!=ADDR_VALID) {
		sendMessage(client->socket, "Address [%s] is not a valid address.\n", address);
		return 1;
	}
	ca = ca->nextArgument;
	
	// get the bit mask
	mask = ca->commandArgument;
	if(checkAddress(mask, ADDR_ACTIVE)!=ADDR_VALID) {
		sendMessage(client->socket, "Mask [%s] is not a valid mask.\n", mask);
		return 1;
	}
	ca = ca->nextArgument;
	
	// get the targetIndex if available
	if(ca!=NULL) {
		targetIndex = atoi(ca->commandArgument);
	}

	// create new rule
	if(addACLRule(client->currentACL, targetIndex, address, mask, r)==NULL) {
		sendMessage(client->socket, "Error creating access control list rule.");
		return 1;
	}
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This function will is called to remove an ACL rule at a specific index
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ August 7, 2008   
 * -------------------------------------------------------------------------------------*/
int noACLRule(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int index = 0;
	index = atoi(ca->commandArgument);
	removeACLRule(client->currentACL, index);
	return 0;
}

