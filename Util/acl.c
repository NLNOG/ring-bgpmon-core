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
 *  File: acl.c
 * 	Authors: Kevin Burnett
 *
 *  Date: June 23, 2008 
 */
 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

// needed for TRUE FALSE definition
#include "bgpmon_defaults.h"
// needed for AccessControlList and ACLRule struct definitions
#include "acl.h"
// needed for ACL function prototypes
#include "aclmanagement.h"
// needed for the TRUE/FALSE definition
#include "utils.h"
// needed for access to Login struct
#include "../Login/login.h"
// needed for ADDR_MAX_CHARS definition
#include "../site_defaults.h"
// needed for XML definitions
#include "../Config/configdefaults.h"
// needed for reading XML
#include "../Config/configfile.h"
// needed for address management
#include "address.h"

// function prototyping
ACLRule * createDefaultACLRule();
ACLRule * createACLRule(int index, char * addressString, char * maskString, ACCESS_RULE access);
ACLRule * createACLRuleAny(int index, ACCESS_RULE access);
void reindexACL(ACLRule * rule);
int isRuleApplicable(uint8_t clientAddress[16], sa_family_t family, ACLRule * rule);
int saveACLList(AccessControlList * acl);
int saveACLListRule(ACLRule * rule);
int readACLList(int index, AccessControlList * acl);
int removeAllACLs();
int isAddressInRange(uint8_t clientAddress[16], uint8_t ruleAddress[16], uint8_t mask[16]);

/*----------------------------------------------------------------------------------------
 * Purpose: Based on a sockaddr, this will step through all the rules in a ACL list and
 * 	to determine if an address should be allowed to connect.
 * Input: sa - the sockaddr of the connecting client.
 * Output: Returns the rule type (DENY, PERMIT, LABEL, RIBONLY) of the applicable rule.
 * 	If no applicable rule is found then DENY is returned.
 * Kevin Burnett @ October 24, 2008   
 * -------------------------------------------------------------------------------------*/
int checkACL(struct sockaddr *sa, int type) {
	struct sockaddr_in * sa4 = NULL;
	struct sockaddr_in6 * sa6 = NULL;
	uint32_t temp = 0;
	uint8_t address[16];
	memset(address, 0x00, sizeof(uint8_t)*16);

	// based on the family, get the fully qualified address
	switch(sa->sa_family) {
		case AF_INET:
			sa4 = (struct sockaddr_in*)sa;
			//temp = ntohl((uint32_t)sa4->sin_addr.s_addr);
			temp = (uint32_t)sa4->sin_addr.s_addr;
			memcpy(&address[12], &temp, sizeof(uint32_t));
			break;

		case AF_INET6:
			sa6 = (struct sockaddr_in6*)sa;
			memcpy(address, sa6->sin6_addr.s6_addr, sizeof(uint8_t)*16);
			break;
	}

	// get the correct acl head node
	ACLRule * rule = NULL;
	switch(type) {
		case CLIENT_UPDATE_ACL:
			if(LoginSettings.clientUpdateAcl!=NULL)
				rule = LoginSettings.clientUpdateAcl->head;
			break;

		case CLIENT_RIB_ACL:
			if(LoginSettings.clientRIBAcl!=NULL)
				rule = LoginSettings.clientRIBAcl->head;
			break;

		case CLI_ACL:
			if(LoginSettings.cliAcl!=NULL)
				rule = LoginSettings.cliAcl->head;
			break;

		case MRT_ACL:
			if(LoginSettings.mrtAcl!=NULL)
				rule = LoginSettings.mrtAcl->head;
			break;
	}

	// step through all the rules and find the first applicable rule
	while(rule!=NULL) {
		if(rule->allowAnyAddress==TRUE || isRuleApplicable(address, sa->sa_family, rule)==TRUE) {
			return rule->access;
		}
		rule = rule->nextRule;
	}

	// if no applicable rules are found then deny access
	return DENY;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This function will test to see if a rule is applicable to the address from
 * 	a connecting client.
 * Input: clientAddress - 16*1byte values that make up a 128bit address
 * 	  clientFamily - the family of the address from the client
 *	  rule - a pointer the the rule to be tested
 * Output: Returns TRUE or FALSE (defined in Util/bgpmon_defaults.h) indicating whether
 * 	the rule is applicable to the current address.
 * Kevin Burnett @ October 24, 2008   
 * -------------------------------------------------------------------------------------*/
int isRuleApplicable(uint8_t clientAddress[16], sa_family_t clientFamily, ACLRule * rule) {
	uint8_t mask[16];
	memset(mask, 0x00, sizeof(uint8_t)*16);
	uint8_t ruleAddress[16];
	memset(ruleAddress, 0x00, sizeof(uint8_t)*16);
	uint32_t temp = 0;

	struct sockaddr * sa = NULL;
	struct sockaddr_in * sa4 = NULL;
	struct sockaddr_in6 * sa6 = NULL;

	// see if rule is the same family as client address
	if(rule->address->ai_family==clientFamily) {
		// get the address information from family specific structures
		switch(rule->address->ai_family) {
			case AF_INET:
				// get the address from the rule
				sa = rule->address->ai_addr;
				sa4 = (struct sockaddr_in*)sa;
				//temp = ntohl((uint32_t)sa4->sin_addr.s_addr);
				temp = (uint32_t)sa4->sin_addr.s_addr;
				memcpy(&ruleAddress[12], &temp, sizeof(uint32_t));

				// get the mask from the rule
				sa = rule->mask->ai_addr;
				sa4 = (struct sockaddr_in*)sa;
				//temp = ntohl((uint32_t)sa4->sin_addr.s_addr);
				temp = (uint32_t)sa4->sin_addr.s_addr;
				memcpy(&mask[12], &temp, sizeof(uint32_t));
				break;

			case AF_INET6:
				// get the address from the rule
				sa = rule->address->ai_addr;
				sa6 = (struct sockaddr_in6*)sa;
				memcpy(ruleAddress, sa6->sin6_addr.s6_addr, sizeof(uint8_t)*16);

				// get the mask from the rule
				sa = rule->mask->ai_addr;
				sa6 = (struct sockaddr_in6*)sa;
				memcpy(ruleAddress, sa6->sin6_addr.s6_addr, sizeof(uint8_t)*16);
				break;
		}

		// return whether the address is in range or not
		return isAddressInRange(clientAddress, ruleAddress, mask);
	}

	// current rule is not applicable
	return FALSE;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This function will step through 16 bytes (16 * 8 = 128bits) of an address
 * 	and test to see if the address is in range of another address.  
 * Input: clientAddress - an array of 16 1 byte elements that make up an address
 * 	ruleAddress - an array of 16 1 byte elements that make up an address 
 * 	mask - an array of 16 1 byte elements that make up the mask which will be applied
 * 		to both addresses when determining if the clientAddress is in range of
 * 		the ruleAddress
 * Output:  TRUE if address is in range and FALSE if address is not in range
 * Kevin Burnett @ October 25, 2008   
 * -------------------------------------------------------------------------------------*/
int isAddressInRange(uint8_t clientAddress[16], uint8_t ruleAddress[16], uint8_t mask[16]) {
	// check to see if address is in range
	int i = 0;
	uint8_t r1 = 0, r2 = 0;
	// step through all the elements of the array
	for(i = 0; i<16; i++) {
		// bitwise OR the addresses with the mask
		r1 = clientAddress[i] | mask[i];
		r2 = ruleAddress[i] | mask[i];

		// if the results aren't equal then the address is not in range
		if(r1!=r2)
			return FALSE;
	}
	return TRUE;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Based on an ACL name, this will find the corresponding ACL and return a
 * 	pointer to it.
 * Input: ACLName - The name of the ACL
 * Output: A pointer to the corresponding ACL is returned, if its found, otherwise NULL is
 * 	returned.
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
AccessControlList* getACL(char * ACLName) {
	AccessControlList * acl = LoginSettings.rootAcl;
	AccessControlList * retval = NULL;

	// step through ACL linked list for ACLName
	while(acl!=NULL) {
		if(strcmp(acl->name, ACLName)==0) {
			// return a find
			retval = acl;
			break;
		}
		acl = acl->nextList;
	}

	return retval;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Creates a new ACL and adds it into the linked list containing all
 * 	ACLS (LoginSettings.rootAcl).
 * Input: ACLName - the name for the new ACL
 * Output: A pointer to the AccessControlList structure
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
AccessControlList* createACL(char * ACLName) {
	// create the new acl
	AccessControlList * newAcl = NULL;
	newAcl = malloc(sizeof(AccessControlList));
	if(newAcl==NULL) {
		log_err("createACL: Unable to allocate memory for new Access Control List.");
		return NULL;
	}
	memset(newAcl, 0, sizeof(AccessControlList));
	
	// set the acl name
	newAcl->name = malloc(ACL_NAME_MAX_LEN);
	if(newAcl->name==NULL) {
		log_err("createACL: Unable to allocate memory for name on new Access Control List.");
		return 0;
	}
	memset(newAcl->name, 0, ACL_NAME_MAX_LEN);
	strncpy(newAcl->name, ACLName, ACL_NAME_MAX_LEN);
	newAcl->head = createDefaultACLRule();

	// find the end of the ACL linked list
	AccessControlList * currentAcl = LoginSettings.rootAcl;
	while(currentAcl!=NULL) {
		if(currentAcl->nextList==NULL)
			break;

		currentAcl = currentAcl->nextList;
	}

	// check to see if root node is NULL
	if(LoginSettings.rootAcl!=NULL) {
		// set next node
		currentAcl->nextList = newAcl;
	}
	else {
		// set root node
		LoginSettings.rootAcl = newAcl;
	}

	return newAcl;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Removes an ACL from the ACL Linked List based on a name
 * Input: aclName - the name of the acl to be removed
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
int removeACL(char * aclName) {
	AccessControlList * currentAcl = NULL, * previousAcl = NULL;
	currentAcl = LoginSettings.rootAcl;

	// look for the correct acl
	while(currentAcl!=NULL) {
		if(strcmp(currentAcl->name, aclName)==0)
			break;

		previousAcl = currentAcl;
		currentAcl = currentAcl->nextList;
	}

	// only one list...
	if(previousAcl==NULL && currentAcl->nextList==NULL) {
		LoginSettings.rootAcl = NULL;
	}
	else {
		// first list...
		if(previousAcl==NULL) {
			LoginSettings.rootAcl = currentAcl->nextList;
		}

		// last list...
		if(currentAcl->nextList==NULL) {
			previousAcl->nextList = NULL;
		}
	}

	// middle list...
	if(previousAcl!=NULL && currentAcl->nextList!=NULL) {
		previousAcl->nextList = currentAcl->nextList;
	}
	
	// free the acl memory
	freeACL(currentAcl);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Loops through all the rules in an ACL and frees any memory
 * 	associated with them then frees the memory associated with ACL.
 * Input: acl - a pointer to an ACL to be freed
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
void freeACL(AccessControlList * acl) {
	ACLRule * rule = NULL, * nextRule = NULL;
	nextRule = acl->head;
	// loop through all the rules and free them
	while(nextRule!=NULL) {
		rule = nextRule;
		nextRule = nextRule->nextRule;
		free(rule);
	}
	// free the name then acl
	free(acl->name);
	free(acl);
}

/*----------------------------------------------------------------------------------------
 * Purpose: Remove all ACLs that have been created and added to the ACL linked
 * 	list (LoginSettings.rootAcl).
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int removeAllACLs() {
	AccessControlList * acl = NULL, * nextAcl = NULL;
	acl = LoginSettings.rootAcl;

	while(acl!=NULL) {
		nextAcl = acl->nextList;
		freeACL(acl);
		acl = nextAcl;
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Based on input paramters, will create a new ACLrule and add it
 * 	into an AccessControlList.
 * Input: list - the ACL where thew new rule will be created
 * 	targetIndex - the index where the rule should be added
 * 	addressString - a string representing the address
 * 	maskString - a string representing the mask
 * 	access - the type of access, PERMIT or DENY
 * Output: returns the pointer to the new ACL rule or NULL if no rule was
 * 	created.
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
ACLRule * addACLRule(AccessControlList * list, int targetIndex, char * addressString, char * maskString, ACCESS_RULE access) {
	ACLRule * currentRule = list->head;
	ACLRule * previousRule = NULL;
	int newIndex = 0;

	// find the index and reference to where the rule will be inserted
	if(currentRule==NULL)
		newIndex = 0;
	else {
		// loop until the targetIndex or the end of the list is found
		while(currentRule->nextRule!=NULL) {
			if(targetIndex==newIndex)
				break;

			newIndex++;
			previousRule = currentRule;
			currentRule = currentRule->nextRule;
		}

		if(targetIndex<0) {
			// add one so next index will be at the end of the list
			newIndex++;
		}
	}

	// create the rule
	ACLRule * newRule = NULL;
	if(addressString==NULL || maskString==NULL || strcmp(addressString, "any")==0 || strcmp(maskString, "any")==0)
		newRule = createACLRuleAny(newIndex, access);
	else
		newRule = createACLRule(newIndex, addressString, maskString, access);

	if(newRule==NULL) {
		// there was an error creating the new rule so return
		return NULL;
	}

	// insert at the beginning of the list 
	if(targetIndex==0) {
		// insert new rule into the chain
		newRule->nextRule = currentRule;
		list->head = newRule;
	}

	// insert in the middle of the list 
	if(targetIndex>0) {
		// insert new rule into the chain
		newRule->nextRule = currentRule;
		previousRule->nextRule = newRule;
	}

	// append to the end of the list
	if(targetIndex<0) {
		if(currentRule==NULL) {
			// add the first (and only) rule on onto the list
			list->head = newRule;
		}
		else {
			// add new rule onto the end of the chain
			currentRule->nextRule = newRule;
		}
	}

	// reindex the list
	reindexACL(list->head);

	return newRule;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Loops through a linked list of ACL rules and sets their index
 * Input: The head ACLRule * for a list of ACL rules
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
void reindexACL(ACLRule * rule) {
	int index = 0;

	if(rule!=NULL) {
		do {
			rule->index = index;
			index++;
			rule = rule->nextRule;
		} while(rule!=NULL);
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: This method will create a new ACLRule and return the pointer.  The caller
 * 	is responsible for cleaning up the structure when it's no longer needed. 
 * Input: index = the index of the rule in the list
 *	addressString = the string representation of the address
 *	maskString = the string representation of the mask (significant bits)
 *	access = enum indicating whether the rule is PERMIT or DENY
 * Output: ACLRule * - a pointer to the new rule
 * Note: This method assumes that the address is valid.  Callers can use checkAddress() 
 * 	in Util/address.h to determine the validity of an address.
 * Kevin Burnett @ October 23, 2008
 * -------------------------------------------------------------------------------------*/
ACLRule * createACLRule(int index, char * addressString, char * maskString, ACCESS_RULE access) {
	ACLRule * newRule = NULL;
	struct addrinfo * address = NULL, * mask = NULL;
	
	// create new ACL rule
	newRule = malloc(sizeof(ACLRule));
	if(newRule==NULL) {
		log_err("createACLRule: Unable to allocate memory for new ACL Rule.");
		return NULL;
	}
	memset(newRule, 0, sizeof(ACLRule));

	// set the rule's index
	newRule->index = index;

	// make sure the rule's doesn't allow any address
	newRule->allowAnyAddress = FALSE;

	// copy the addressString to the rule if applicable
	if(addressString!=NULL) {
		memcpy(newRule->addressString, addressString, ADDR_MAX_CHARS);

		// create the addrinfo for the address
		address = createAddrInfo(addressString, 0);	// ignore the port since we don't need it here
		newRule->address = address;
	}

	// copy the maskString to the rule
	if(maskString!=NULL) {
		memcpy(newRule->maskString, maskString, ADDR_MAX_CHARS);

		// create the addrinfo for the address
		mask = createAddrInfo(maskString, 0);	// ignore the port since we don't need it here
		newRule->mask = mask;
	}

	// set the type information and next rule
	newRule->access = access;
	newRule->nextRule = NULL;

	return newRule;
}

/*----------------------------------------------------------------------------------------
 * Purpose: This method will create a new ACLRule that accepts any address then return 
 * 	the pointer.  The caller is responsible for cleaning up the structure when it's 
 * 	no longer needed. 
 * Input: index = the index of the rule in the list
 *	access = enum indicating whether the rule is PERMIT or DENY
 * Output: ACLRule * - a pointer to the new rule
 * Kevin Burnett @ November 26, 2008
 * -------------------------------------------------------------------------------------*/
ACLRule * createACLRuleAny(int index, ACCESS_RULE access) {
	ACLRule * rule = createACLRule(index, NULL, NULL, access);
	rule->allowAnyAddress = TRUE;
	return rule;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Creates a default DENY ALL rule.  This rule is added to new ACLs.
 * Output: Returns the pointer to the ACLRule  
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
ACLRule * createDefaultACLRule() {
	// this creates a DENY all rule
	return createACLRuleAny(0, DENY);
}

/*----------------------------------------------------------------------------------------
 * Purpose: Based on the index of an ACL rule, this will remove the rule from
 * 	the AccessControlList.
 * Input: list - The AccessControlList
 * 	index - the index of the rule to remove
 * Output: 0 if the rule was removed and 1 if the rule was not removed
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
int removeACLRule(AccessControlList * list, int index) {
	ACLRule * currentRule = list->head;
	ACLRule * previousRule = NULL;

	// find a ACLRule with for the specified index
	do {
		if(currentRule->index==index)
			break;

		previousRule = currentRule;
		currentRule = currentRule->nextRule;
	} while(currentRule!=NULL);

	// no rule was found so return
	if(currentRule==NULL)
		return 1;
	
	
	// the only element
	if(previousRule==NULL && currentRule->nextRule==NULL) {
		list->head = NULL;
	}
	else {
		// remove the first element
		if(previousRule==NULL) {
			list->head = currentRule->nextRule;
		}

		// remove the last element
		if(currentRule->nextRule==NULL) {
			previousRule->nextRule = NULL;
		}
	}

	// remove a middle element
	if(previousRule!=NULL && currentRule->nextRule!=NULL) {
		previousRule->nextRule = currentRule->nextRule;
	}
	
	// reindex the list
	reindexACL(list->head);

	// free currentRule
	free(currentRule);

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Writes the ACL's display header into a buffer and returns it
 * Input: *acl - a pointer to the ACL
 * Output: Returns a pointer to a char * containing the display header
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
char * displayAccessControlList(AccessControlList * acl) {
	char * buffer = malloc(ACL_NAME_MAX_LEN);
	if(buffer==NULL) {
		log_err("displayAccessControlList: Unable to allocate memory.");
		return NULL;
	}
	memset(buffer, 0x00, ACL_NAME_MAX_LEN);
	if (sprintf(buffer, "%s\n", acl->name) > 0)
	{
		return buffer;
	}
	else
	{	
		free(buffer);	
		return NULL;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Writes the ACLRule header into a buffer and returns it
 * Output: a pointer to a string
 * Kevin Burnett @ August 30, 2008   
 * -------------------------------------------------------------------------------------*/
char * displayACLRuleHeader() {
	char * buffer = malloc(ACL_NAME_MAX_LEN);
	if(buffer==NULL) {
		log_err("displayACLRuleHeader: Unable to allocate memory.");
		return NULL;
	}
	strcpy(buffer, "index address               mask                  type \n");
	return buffer;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Writes the details of an ACL rule into a buffer and returns it
 * Input: a pointer to the ACL rule
 * Output: a pointer to a string
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
char * displayACLRule(ACLRule * rule) {
	char * buffer = malloc(6 + 22 + 22 + 9 + 1);	// malloc contains all the fragments that make up a complete string
	if(buffer==NULL) {
		log_err("displayACLRule: Unable to allocate memory.");
		return NULL;
	}
	int bytesWritten = 0;

	// print the rule index
	bytesWritten = sprintf(buffer, "%-6d", rule->index);

	// print the ip address
	if(rule->allowAnyAddress==TRUE)
		bytesWritten += sprintf(&buffer[bytesWritten], "%-22s", "any");
	else
		bytesWritten += sprintf(&buffer[bytesWritten], "%-22s", rule->addressString);

	// print the mask
	if(rule->allowAnyAddress==TRUE)
		bytesWritten += sprintf(&buffer[bytesWritten], "%-22s", "any");
	else
		bytesWritten += sprintf(&buffer[bytesWritten], "%-22s", rule->maskString);

	// print the rule type
	switch(rule->access) {
		case PERMIT:
			bytesWritten += sprintf(&buffer[bytesWritten], "permit  \n");
			break;
		case DENY:
			bytesWritten += sprintf(&buffer[bytesWritten], "deny    \n");
			break;
		case LABEL:
			bytesWritten += sprintf(&buffer[bytesWritten], "label   \n");
			break;
		case RIBONLY:
			bytesWritten += sprintf(&buffer[bytesWritten], "ribonly \n");
			break;
	}

	return buffer;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Initializes the AccessControlList module and creates default
 * 	permitall and denyall lists.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int initACLSettings() {
	AccessControlList * acl = NULL;
	ACLRule * rule = NULL;

	// create deny all list
	acl = createACL("denyall");

	// update mrt control to deny
	LoginSettings.mrtAcl = acl;

	// create permit all list
	acl = createACL("permitall");
	rule = addACLRule(acl, 0, NULL, NULL, PERMIT);
	if (rule == NULL)
	{
		log_err("initACLSettings: could not retreive ACL rule.");
		return 1;
	}
	removeACLRule(acl, 1);	// remove the default deny all rule

	// initialize the CLI and Clients to permit
	LoginSettings.cliAcl = acl;
	LoginSettings.clientUpdateAcl = acl;
	LoginSettings.clientRIBAcl = acl;

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Calls functions to read in all the settings for the ACL from the
 * 	configuration file.
 * Output: Returns 0 on success or 1 on failure.
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int readACLSettings() {
	int listCount = 0;
	AccessControlList * acl = NULL;
	LoginSettings.rootAcl = acl;
	char * name = NULL;

	// clear out all ACLs that are created in init
	removeAllACLs();
	
	// get the list count
	listCount = getConfigListCount(XML_ROOT_PATH XML_ACL_TAG "/" XML_ACL_LIST );

	int i = 0;
	for(i=0; i<listCount; i++) {
		// get the ACL name
		getConfigValueFromListAsString(&name, XML_ROOT_PATH XML_ACL_TAG "/" XML_ACL_LIST, XML_ACL_LIST_NAME, i, 255);
		// create the ACL
		acl = createACL(name);
		removeACLRule(acl, 0);	// remove the default rule

		// populate the list
		readACLList(i, acl);

		free(name);
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads in the rules for a ACL list
 * Input: index - the index of the ACL in the collection of nodes
 * 	acl - the list to read in
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int readACLList(int index, AccessControlList * acl) {
	ACLRule * rule = NULL;
	int ruleCount = 0, xpathLength = 0, i = 0;
	int r = 0;
	char * address = NULL, * mask = NULL, * allowed = NULL;
	ACCESS_RULE access = DENY;

	// build xpath query to specific ACL
	char xpath[255];
	memset(xpath, 0, 255);
	strcat(xpath, XML_ROOT_PATH);
	strcat(xpath, XML_ACL_TAG "/");
	strcat(xpath, XML_ACL_LIST "[");
	xpathLength = strlen(xpath);
	sprintf(&xpath[xpathLength], "%d", index+1);
	strcat(xpath, "]/" );
	strcat(xpath, XML_ACL_LIST_RULE );

	// get the number of rules
	ruleCount = getConfigListCount(xpath);

	// loop through each rule
	for(i=0; i<ruleCount; i++) {
		// build xpath query to specific rule
		memset(xpath, 0, 255);
		strcat(xpath, XML_ROOT_PATH);
		strcat(xpath, XML_ACL_TAG "/");
		strcat(xpath, XML_ACL_LIST "[");
		xpathLength = strlen(xpath);
		sprintf(&xpath[xpathLength], "%d", index + 1);
		strcat(xpath, "]/" XML_ACL_LIST_RULE); 

		// get the address
		r = getConfigValueFromListAsString( &address, xpath, XML_ACL_LIST_RULE_ADDRESS, i, 255);
 		if ( ( r == CONFIG_INVALID_ENTRY ) || ( r == CONFIG_NO_ENTRY ) )
                {
                	log_err("readACLList(): Invalid configuration of ACL rule ADDRESS");
			continue;
                }
		
		// get the mask 
		r = getConfigValueFromListAsString( &mask, xpath, XML_ACL_LIST_RULE_MASK, i, 255);
 		if ( ( r == CONFIG_INVALID_ENTRY ) || ( r == CONFIG_NO_ENTRY ) )
                {
                	log_err("readACLList(): Invalid configuration of ACL rule MASK");
			free(address);
			continue;
                }
		
		// get the access type 
		r = getConfigValueFromListAsString( &allowed, xpath, XML_ACL_LIST_RULE_ALLOWED, i, 255);
 		if ( ( r == CONFIG_INVALID_ENTRY ) || ( r == CONFIG_NO_ENTRY ) )
                {
                	log_err("readACLList(): Invalid configuration of ACL rule ACCESS TYPE");
			free(address);
			free(mask);
			continue;
                }

		if(strcmp(allowed, "permit")==0)
			access = PERMIT;
		if(strcmp(allowed, "deny")==0)
			access = DENY;
		if(strcmp(allowed, "label")==0)
			access = LABEL;
		if(strcmp(allowed, "ribonly")==0)
			access = RIBONLY;
		
		// finally add the rule
		rule = addACLRule(acl, -1, address, mask, access);
		if (rule == NULL)
		{
			log_err("readACLList(): could not add ACL rule");
		}

		free(address);
		free(mask);
		free(allowed);
	}

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Saves the ACL information 
 * Output: Returns 0 on success and 1 on failure
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int saveACLSettings() {
	AccessControlList * acl = NULL, * nextAcl = NULL;
	acl = LoginSettings.rootAcl;
	int err = 0;

	// open module element
	if (openConfigElement(XML_ACL_TAG)) {
		err = 1;
		log_err("failed to save ACL to Config file: %s", strerror(errno));
	}

	// loop through all lists
	while(acl!=NULL) {
		nextAcl = acl->nextList;
		saveACLList(acl);
		acl = nextAcl;
	}

	// close module element
	if(closeConfigElement()) {
		err = 1;
		log_err("failed to save Login to Config file: %s", strerror(errno));
	}

	return err;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Saves the ACLList information
 * Input: acl - the list to be saved
 * Output: returns 0 on success and 1 on failure
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int saveACLList(AccessControlList * acl) {
	ACLRule * rule = NULL, * nextRule = NULL;
	rule = acl->head;
	int err = 0;

	// open the base element
	if(openConfigElement(XML_ACL_LIST)) {
		err = 1;
		log_err("failed to save ACL to Config file: %s", strerror(errno));
	}

	// save the list name
	if(setConfigValueAsString(XML_ACL_LIST_NAME, acl->name)) {
		err = 1;
		log_err("failed to save ACL Name to Config file: %s", strerror(errno));
	}

	// make sure a rule exists in the list
	if(rule!=NULL) {
		// loop through all rules
		while(rule!=NULL) {
			nextRule = rule->nextRule;
			// save the rule
			saveACLListRule(rule);
			rule = nextRule;
		}
	}

	if(closeConfigElement(XML_ACL_LIST)) {
		err = 1;
		log_err("failed to save list to Config file: %s", strerror(errno));
	}

	return err;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Saves an ACL rule 
 * Input: rule - the rule to be saved
 * Output: returns 0 on success and 1 on failure
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int saveACLListRule(ACLRule * rule) {
	int err = 0;

	if(openConfigElement(XML_ACL_LIST_RULE)) {
		err = 1;
		log_warning("failed to save rule to Config file: %s", strerror(errno));
	}

        if(setConfigValueAsInt(XML_ACL_LIST_RULE_INDEX, rule->index)) {
		err = 1;
                log_warning("failed to save rule index to Config File: %s", strerror(errno));
        } 

	if(rule->allowAnyAddress==TRUE) {
		if(setConfigValueAsString(XML_ACL_LIST_RULE_ADDRESS, "any")) {
			err = 1;
			log_warning("failed to save rule address to Config file: %s", strerror(errno));
		}

		if(setConfigValueAsString(XML_ACL_LIST_RULE_MASK, "any")) {
			err = 1 ;
			log_warning("failed to save rule mask to Config file: %s", strerror(errno));
		}
	}
	else {
		if(setConfigValueAsString(XML_ACL_LIST_RULE_ADDRESS, rule->addressString)) {
			err = 1;
			log_warning("failed to save rule address to Config file: %s", strerror(errno));
		}

		if(setConfigValueAsString(XML_ACL_LIST_RULE_MASK, rule->maskString)) {
			err = 1 ;
			log_warning("failed to save rule mask to Config file: %s", strerror(errno));
		}
	}

	switch(rule->access) {
		case PERMIT:
			if(setConfigValueAsString(XML_ACL_LIST_RULE_ALLOWED, "permit")) {
				err = 1;
				log_warning("failed to save rule allowed to Config file: %s", strerror(errno));
			}
			break;
		case DENY:
			if(setConfigValueAsString(XML_ACL_LIST_RULE_ALLOWED, "deny")) {
				err = 1;
				log_warning("failed to save rule allowed to Config file: %s", strerror(errno));
			}
			break;
		case LABEL:
			if(setConfigValueAsString(XML_ACL_LIST_RULE_ALLOWED, "label")) {
				err = 1;
				log_warning("failed to save rule allowed to Config file: %s", strerror(errno));
			}
			break;
		case RIBONLY:
			if(setConfigValueAsString(XML_ACL_LIST_RULE_ALLOWED, "ribonly")) {
				err = 1;
				log_warning("failed to save rule allowed to Config file: %s", strerror(errno));
			}
			break;
	}

	if(closeConfigElement()) {
		err = 1;
		log_warning("failed to save ACL to Config file: %s", strerror(errno));
	}

	return err;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Frees all memory used by the ACL module
 * Output: Returns 0 on success and 1 on failure
 * Kevin Burnett @ July 30, 2009
 * -------------------------------------------------------------------------------------*/
int freeACLModule() {
	AccessControlList * list = NULL;
	list = LoginSettings.rootAcl;

	while( list!=NULL ) {
		removeAllACLs();
		list = list->nextList;
	}

	return 0;
}
