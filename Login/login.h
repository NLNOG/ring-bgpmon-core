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
 *  File: login.h
 * 	Authors: Kevin Burnett, Dan Massey
 *
 *  Date: Jun 18, 2008
 */

#ifndef LOGIN_H_
#define LOGIN_H_

#include <netdb.h>
// needed for pthread mutex
#include <pthread.h>
// needed for time structs
#include <time.h>

// needed for defaults
#include "../site_defaults.h"
// needed for ACL data types
#include "../Util/acl.h"
// needed for ADDR_MAX_CHARS
#include "../Util/bgpmon_defaults.h"
// needed for CliNode
#include "commandprompt.h"

// forward declaration of a commandNodeStruct;
struct commandNodeStruct;

struct LoginControls_struct_st {
	char 		configFile[FILENAME_MAX_CHARS];
	char 		scratchDir[FILENAME_MAX_CHARS];

	// Login/Command Line Interface Options 
        int    		recoveryMode;
	char		listenAddr[ADDR_MAX_CHARS];
	int		listenPort;
        char  		accessPassword[PASSWORD_MAX_CHARS];
        char  		enablePassword[PASSWORD_MAX_CHARS];

	int enableModeActive;
	int maxConnections;
	int activeConnections;
	int nextConnectionId;

	pthread_mutex_t loginLock;
	pthread_t loginThread;

	int 		shutdown;	// indicates whether to stop the thread
	time_t		lastAction;	// last time the thread was active

	AccessControlList * rootAcl;	// the first defined ACL

	// client acl's
	AccessControlList * clientUpdateAcl;
	AccessControlList * clientRIBAcl;

	// command line interface acl
	AccessControlList * cliAcl;

	// mrt acl
	AccessControlList * mrtAcl;

	// root node for the command line interface
	struct commandNodeStruct * rootCommandNode;		

	// root node to all the CLI connections
	struct CliStruct * firstNode;
};
typedef struct LoginControls_struct_st LoginControls_struct;

// The CLI structure holds connection settings for the Command Line Module
LoginControls_struct LoginSettings;

/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for the thread
 * Input:  none 
 * Output: none
 * Kevin Burnett @ June 24, 2009
 * -------------------------------------------------------------------------------------*/
time_t getLoginControlLastAction();

/*--------------------------------------------------------------------------------------
 * Purpose: the main function of clients control module
 * Input:  none 
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void *loginControlThread( void *arg );

/*--------------------------------------------------------------------------------------
 * Purpose: Start to listen on the configured addr+port.
 * Input:  none
 * Output: socket ID of the listener or -1 if listener create fails
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
int startLoginControlListener();

/*--------------------------------------------------------------------------------------
 * Purpose: Accept the new client and spawn a new thread for the client  
 *          if it passes the ACL check and the max client number is not reached.
 * Input:  the socket used for listening
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void startLogin( int listenSocket );

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default clients control configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
int initLoginControlSettings(char *config_file, char* scratchDir, int recoveryPort);

/*--------------------------------------------------------------------------------------
 * Purpose: Read the clients control settings from the config file.
 * Input: none
 * Output:  returns 0 on success, 1 on failure
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
int readLoginControlSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Save the  clients control  settings to the config file.
 * Input:  none
 * Output:  retuns 0 on success, 1 on failure
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
int saveLoginControlSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: launch clients control thread, called by main.c
 * Input:  none
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void launchLoginControlThread();

/*--------------------------------------------------------------------------------------
 * Purpose: checks to see if enable mode is active or not
 * Input:  none
 * Output: returns 0 if enable mode is inactive and 1 if enable mode is active 
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
int isEnableModeActive();

/*--------------------------------------------------------------------------------------
 * Purpose: sets enable mode to active
 * Input:  none
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void enableModeActive();

/*--------------------------------------------------------------------------------------
 * Purpose: sets enable mode to inactive
 * Input:  none
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void enableModeInactive();

/*--------------------------------------------------------------------------------------
 * Purpose: Sets the enabled password
 * Input: char * - password
 * Kevin Burnett @ January 24, 2009
 * -------------------------------------------------------------------------------------*/
void setEnablePassword(char *);

/*--------------------------------------------------------------------------------------
 * Purpose: Sets the login password
 * Input: char * - password
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
void setAccessPassword(char *);

/*--------------------------------------------------------------------------------------
 * Purpose: Sets address for the login listener
 * Input: char * - the address
 * Kevin Burnett @ January 26, 2009
 * -------------------------------------------------------------------------------------*/
int setListenAddr(char *);

/*--------------------------------------------------------------------------------------
 * Purpose: gets the address for the login listener
 * output: char * - the address
 * Note:  The caller is responsible for free the allocated memory
 * 	from this function
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
char * getListenAddr();

/*--------------------------------------------------------------------------------------
 * Purpose: sets the port for the login listener
 * Iutput: port - the address
 * Kevin Burnett @ January 26, 2009
 * -------------------------------------------------------------------------------------*/
int setListenPort(int);

/*--------------------------------------------------------------------------------------
 * Purpose: gets the port for the login listener
 * output: port - the address
 * Kevin Burnett @ January 26, 2009
 * -------------------------------------------------------------------------------------*/
int getListenPort();

/*--------------------------------------------------------------------------------------
 * Purpose: Intialize the shutdown process for the login module
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 1, 2009
 * -------------------------------------------------------------------------------------*/
void signalLoginShutdown();

/*--------------------------------------------------------------------------------------
 * Purpose: wait on all login pieces to finish closing before returning
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 1, 2009
 * -------------------------------------------------------------------------------------*/
void waitForLoginShutdown();

#endif // LOGIN_H_
