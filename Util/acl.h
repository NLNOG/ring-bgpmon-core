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
 *  File: acl.h
 * 	Authors: Kevin Burnett
 *
 *  Date: Jun 23, 2008
 */

#ifndef ACL_H_
#define ACL_H_

#include <sys/types.h>
#include <sys/socket.h>

#include "../Util/log.h"
#include "../site_defaults.h"

#define CLIENT_UPDATE_ACL 1
#define CLIENT_RIB_ACL 2
#define CLI_ACL 3
#define MRT_ACL 4

/*
 * the types of rules in the access list
 */
typedef enum ACCESS_RULE_ENUM ACCESS_RULE;

/*
 * A single rule in an Access Control List (Linked List)
 */
typedef struct ACLRule_struct ACLRule;

/*
 * An access control list (Linked List)
 */
struct AccessControlList_struct {
	char * name;
	
	//First rule in the list
	struct ACLRule_struct * head; 

	// reference to the next ACL list
	struct AccessControlList_struct * nextList;
};

/*
 * An access control list (Linked List)
 */
typedef struct AccessControlList_struct AccessControlList;

/*----------------------------------------------------------------------------------------
 * Purpose: Based on a sockaddr, this will step through all the rules in a ACL list and
 * 	to determine if an address should be allowed to connect.
 * Input: sa - the sockaddr of the connecting client.
 * Output: Returns TRUE or FALSE (defined in Util/bgpmon_defaults.h) indicating whether
 * 	the address is accepted or not
 * Kevin Burnett @ October 24, 2008   
 * -------------------------------------------------------------------------------------*/
int checkACL( struct sockaddr *sa, int type);

/*----------------------------------------------------------------------------------------
 * Purpose: Calls functions to read in all the settings for the ACL from the
 * 	configuration file.
 * Output: Returns 0 on success or 1 on failure.
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int readACLSettings(); 

/*----------------------------------------------------------------------------------------
 * Purpose: Initializes the AccessControlList module and creates default
 * 	permitall and denyall lists.
 * Output: Returns 0 on success or 1 on failure
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int initACLSettings();

/*----------------------------------------------------------------------------------------
 * Purpose: Saves the ACL information for every ACL and ACL rule
 * Output: Returns 0 on success and 1 on failure
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int saveACLSettings();

/*----------------------------------------------------------------------------------------
 * Purpose: Frees all memory used by the ACL module
 * Output: Returns 0 on success and 1 on failure
 * Kevin Burnett @ September 12, 2008   
 * -------------------------------------------------------------------------------------*/
int freeACLModule();

/*----------------------------------------------------------------------------------------
 * Purpose: Based on an ACL name, this will find the corresponding ACL and return a
 * 	pointer to it.
 * Input: ACLName - The name of the ACL
 * Output: A pointer to the corresponding ACL is returned, if its found, otherwise NULL is
 * 	returned.
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
AccessControlList* getACL(char * ACLName);

#endif	// ACL_H_
