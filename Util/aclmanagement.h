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
 *  File: aclmanagement.h
 * 	Authors: Kevin Burnett
 *
 *  Date: Jun 23, 2008
 */

#ifndef ACLMANAGEMENT_H_
#define ACLMANAGEMENT_H_

#include "bgpmon_defaults.h"

#define ACL_RULE_ANY_MASK "255.255.255.255"
#define ACL_RULE_ANY_IP "0.0.0.0"

// the maximum length of an ACL name
#define ACL_NAME_MAX_LEN 200

/*
 * the types of rules in the access list
 */
enum ACCESS_RULE_ENUM {DENY = 0, PERMIT, LABEL, RIBONLY};

/*
 * A single rule in an Access Control List (Linked List)
 */
struct ACLRule_struct {
	int index;
	int allowAnyAddress;
	struct addrinfo * address;
	char addressString[ADDR_MAX_CHARS];
	struct addrinfo * mask;
	char maskString[ADDR_MAX_CHARS];
	ACCESS_RULE access; 

	// reference to the next rule in the list
	struct ACLRule_struct * nextRule;
};


/*----------------------------------------------------------------------------------------
 * Purpose: Creates a new ACL and adds it into the linked list containing all
 * 	ACLS (LoginSettings.rootAcl).
 * Input: ACLName - the name for the new ACL
 * Output: A pointer to the AccessControlList structure
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
AccessControlList* createACL(char * ACLName);

/*----------------------------------------------------------------------------------------
 * Purpose: Removes an ACL from the ACL Linked List based on a name
 * Input: aclName - the name of the acl to be removed
 * Output: 0 for success or 1 for failure
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
int removeACL(char * aclName);

/*----------------------------------------------------------------------------------------
 * Purpose: Loops through all the rules in an ACL and frees any memory
 * 	associated with them then frees the memory associated with ACL.
 * Input: acl - a pointer to an ACL to be freed
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
void freeACL(AccessControlList * acl);

/*----------------------------------------------------------------------------------------
 * Purpose: Based on input paramters, will create a new ACLrule and add it
 * 	into an AccessControlList.
 * Input: list - the ACL where thew new rule will be created
 * 	targetIndex - the index where the rule should be added
 * 	addressString - a string
 * Output: returns the pointer to the new ACL rule or NULL if no rule was
 * 	created.
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
ACLRule * addACLRule(AccessControlList * list, int targetIndex, char * addressString, char * maskString, ACCESS_RULE access);

/*----------------------------------------------------------------------------------------
 * Purpose: Based on the index of an ACL rule, this will remove the rule from
 * 	the AccessControlList.
 * Input: list - The AccessControlList
 * 	index - the index of the rule to remove
 * Output: 0 if the rule was removed and 1 if the rule was not removed
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
int removeACLRule(AccessControlList * list, int index);

/*----------------------------------------------------------------------------------------
 * Purpose: Writes the ACL's display header into a buffer and returns it
 * Input: *acl - a pointer to the ACL
 * Output: Returns a pointer to a char * containing the display header
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
char * displayAccessControlList(AccessControlList * acl);

/*----------------------------------------------------------------------------------------
 * Purpose: Writes the ACLRule header into a buffer and returns it
 * Output: a pointer to a string
 * Kevin Burnett @ August 30, 2008   
 * -------------------------------------------------------------------------------------*/
char * displayACLRuleHeader();

/*----------------------------------------------------------------------------------------
 * Purpose: Writes the details of an ACL rule into a buffer and returns it
 * Input: a pointer to the ACL rule
 * Output: a pointer to a string
 * Kevin Burnett @ July 29, 2008   
 * -------------------------------------------------------------------------------------*/
char * displayACLRule(ACLRule * rule);

#endif	// ACL_H_
