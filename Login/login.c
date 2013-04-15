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
 *  File: login.c
 * 	Authors: Kevin Burnett
 *
 *  Date: June 9, 2008 
 */

#include <stdio.h>
// needed for malloc and free
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
// needed for system types such as time_t
#include <sys/types.h>
// needed for time functions
#include <time.h>
#include <arpa/inet.h> 
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>

#include "login.h"
#include "commandprompt.h"
#include "../Util/log.h"
#include "../Util/acl.h"
// needed for TRUE/FALSE definitions
#include "../Util/bgpmon_defaults.h"
#include "../Config/configfile.h"
#include "../Config/configdefaults.h"
// needed for address management
#include "../Util/address.h"
// needed to set the default acl
#include "../Util/acl.h"

//#define DEBUG

/*------------------------------------------------------------------------------
 * Purpose: Initialize the default login control configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * Kevin Burnett @ November 18, 2008
 * ---------------------------------------------------------------------------*/
int 
initLoginControlSettings(char *config_file, char* scratchDir, int recoveryPort) { 
	int err = 0;

	// zero out all LoginSettings values
	memset(&LoginSettings, 0x00, sizeof(LoginSettings));
	
	// set the configuration file location
	memcpy(LoginSettings.configFile, config_file, FILENAME_MAX_CHARS); 

	// set the configuration file location
	memcpy(LoginSettings.scratchDir, scratchDir, FILENAME_MAX_CHARS); 

	// address used to listen for cli connections
	int result = checkAddress(DEFAULT_LOGIN_LISTEN_ADDR, ADDR_PASSIVE);
	if(result != ADDR_VALID)
	{
		err = 1;
		strncpy(LoginSettings.listenAddr, IPv4_LOOPBACK, ADDR_MAX_CHARS);
	}
	else
		strncpy(LoginSettings.listenAddr, DEFAULT_LOGIN_LISTEN_ADDR, ADDR_MAX_CHARS);

	// set the port for Login
	if(recoveryPort>0) {
		// set to recovery mode if port is greater than 0
		LoginSettings.recoveryMode = TRUE;
		LoginSettings.listenPort = recoveryPort;
	}
	else {
		// set port
		LoginSettings.recoveryMode = FALSE;
		LoginSettings.listenPort = DEFAULT_LOGIN_PORT;
	}

	// make sure port is valid otherwise default to 50000
	if ( (LoginSettings.listenPort < 1) ||
             (LoginSettings.listenPort > 65536) ) {
                err = 1;
                log_warning("Invalid site default for login listen port.");
		LoginSettings.listenPort = 50000;
        }
	
	// Maximum number of cli connections allowed
	if (MAX_CLI_IDS < 0)  {
           err = 1;
           log_warning("Invalid site default for max allowed cli connections.");
	   LoginSettings.maxConnections = 1;
        } else {
          LoginSettings.maxConnections = MAX_CLI_IDS;
        }

	// initial bookkeeping figures
	LoginSettings.activeConnections = 0;
	LoginSettings.enableModeActive = FALSE;
	LoginSettings.lastAction = time(NULL);
	LoginSettings.firstNode = NULL;

	// initialize the passwords 
	memcpy(LoginSettings.accessPassword, DEFAULT_ACCESS_PASSWORD,
               sizeof(DEFAULT_ACCESS_PASSWORD));
	memcpy(LoginSettings.enablePassword, DEFAULT_ENABLE_PASSWORD,
               sizeof(DEFAULT_ENABLE_PASSWORD));

        // create a lock for the login list
        if (pthread_mutex_init(&LoginSettings.loginLock, NULL) )
                log_fatal("unable to init mutex lock for login");

	// setup the command tree structure
	LoginSettings.rootCommandNode = malloc(sizeof(commandNode));
	if(LoginSettings.rootCommandNode==NULL) {
		log_err("Failed to create command tree structure.");
		free(LoginSettings.rootCommandNode);
		return 1;
	}
	memset(LoginSettings.rootCommandNode, 0, sizeof(commandNode));

	// initialize the commands in the tree
	initCommandTree(LoginSettings.rootCommandNode);

	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Read the login control settings from the config file.
 * Input: none
 * Output:  returns 0 on success, 1 on failure
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
int readLoginControlSettings() {	
	int err = 0;
	int result = 0;
	int num = 0;
	char * addr = NULL;
	char * temp = NULL;

  // get listen addr and port only if we are not in recovery mode
  if(!LoginSettings.recoveryMode){
    result = getConfigValueAsAddr(&addr, XML_LOGIN_ADDRESS_PATH, ADDR_PASSIVE);
    if (result == CONFIG_VALID_ENTRY){
      result = checkAddress(addr, ADDR_PASSIVE);
      if(result != ADDR_VALID){
        err = 1;
        log_warning("Invalid configuration of login listen address.");
      } else {
        strncpy(LoginSettings.listenAddr, addr, ADDR_MAX_CHARS);
      }
      free(addr);
    }else if ( result == CONFIG_INVALID_ENTRY ){
      err = 1;
      log_warning("Invalid configuration of login listen address.");
    }else{
      log_msg("No configuration of login listen address, using default.");
    }
#ifdef DEBUG
    debug(__FUNCTION__, "Login Control Addr:%s", LoginSettings.listenAddr);
#endif


    // get listen port
    result = getConfigValueAsInt(&num, XML_LOGIN_PORT_PATH, 1, 65536);
    if (result == CONFIG_VALID_ENTRY) {
      LoginSettings.listenPort = num;
    }else if( result == CONFIG_INVALID_ENTRY ){
      err = 1;
      log_warning("Invalid configuration of login listen port.");
    } else{
      log_msg("No configuration of login listen port, using default.");
    }
    #ifdef DEBUG
    debug(__FUNCTION__, "Login listen port set to %d", LoginSettings.listenPort);
    #endif
  }
	
	// get ACCESS_PASSWORD value
	if(getConfigValueAsString(&temp, XML_LOGIN_ACCESS_PASSWORD_PATH, PASSWORD_MAX_CHARS )) {
		log_warning("Invalid configuration of Login access password, default value used.");
	}
	else {
		memcpy(LoginSettings.accessPassword, temp, strlen(temp) + 1);
		free(temp);
	}
	
	// get ENABLE_PASSWORD value
	if(getConfigValueAsString(&temp, XML_LOGIN_ENABLE_PASSWORD_PATH, PASSWORD_MAX_CHARS)) {
		log_warning("Invalid configuration of Login enable password, default value used.");
	}
	else {
		memcpy(LoginSettings.enablePassword, temp, strlen(temp) + 1);
		free(temp);
	}

	// get the cli ACL
	AccessControlList * acl = NULL;
#ifdef DEBUG
	char * aclName = NULL;
#endif
	result = getConfigValueAsString(&temp, XML_LOGIN_CLI_ACL_PATH, PASSWORD_MAX_CHARS);
	if(result==0) {
		acl = getACL(temp);
		if(acl!=NULL) {
			LoginSettings.cliAcl = acl;
#ifdef DEBUG
			aclName = acl->name;
#endif
		}
		free(temp);
	}
#ifdef DEBUG
	debug(__FUNCTION__, "Command Line Interface Access Control List set to [%s]", aclName);
#endif
	
	// get the client update ACL
	acl = NULL;
#ifdef DEBUG
	aclName = NULL;
#endif
	result = getConfigValueAsString(&temp, XML_LOGIN_CLIENT_UPDATE_ACL_PATH, PASSWORD_MAX_CHARS);
	if(result==0) {
		acl = getACL(temp);
		if(acl!=NULL) {
			LoginSettings.clientUpdateAcl = acl;
#ifdef DEBUG
			aclName = acl->name;
#endif
		}
		free(temp);
	}
#ifdef DEBUG
	debug(__FUNCTION__, "Client Update's Access Control List set to [%s]", aclName);
#endif

	// get the client rib ACL
	acl = NULL;
#ifdef DEBUG
	aclName = NULL;
#endif
	result = getConfigValueAsString(&temp, XML_LOGIN_CLIENT_RIB_ACL_PATH, PASSWORD_MAX_CHARS);
	if(result==0) {
		acl = getACL(temp);
		if(acl!=NULL) {
			LoginSettings.clientRIBAcl = acl;
#ifdef DEBUG
			aclName = acl->name;
#endif
		}
		free(temp);
	}
#ifdef DEBUG
	debug(__FUNCTION__, "Client RIB's Access Control List set to [%s]", aclName);
#endif
	
	// get the mrt ACL
	acl = NULL;
#ifdef DEBUG
	aclName = NULL;
#endif
	result = getConfigValueAsString(&temp, XML_LOGIN_MRT_ACL_PATH, PASSWORD_MAX_CHARS);
	if(result==0) {
		acl = getACL(temp);
		if(acl!=NULL) {
			LoginSettings.mrtAcl = acl;
#ifdef DEBUG
			aclName = acl->name;
#endif
		}
		free(temp);
	}
#ifdef DEBUG
	debug(__FUNCTION__, "Mrt's Access Control List set to [%s]", aclName);
#endif

	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Save the login control settings to the config file.
 * Input:  none
 * Output:  retuns 0 on success, 1 on failure
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
int saveLoginControlSettings() {
	int err = 0;

	// save login tag
	if (openConfigElement(XML_LOGIN_TAG)) {
		err = 1;
		log_warning("failed to save Login to Config file: %s", strerror(errno));
	}

	// save listen addr
        if (setConfigValueAsString(XML_LOGIN_ADDRESS_TAG, LoginSettings.listenAddr)) {
		err = 1;
                log_warning("failed to save Login Address to Config File: %s", strerror(errno));
        } 

	// save listen port 
        if (setConfigValueAsInt(XML_LOGIN_PORT_TAG, LoginSettings.listenPort)) {
		err = 0;
                log_warning("failed to save Login Port to Config File: %s", strerror(errno));
        } 

	// save access password
        if (setConfigValueAsString(XML_LOGIN_ACCESS_PASSWORD_TAG, LoginSettings.accessPassword)) {
		err = 1;
                log_warning("failed to save Access Password to Config File: %s", strerror(errno));
        } 

	// save enable password
        if (setConfigValueAsString(XML_LOGIN_ENABLE_PASSWORD_TAG, LoginSettings.enablePassword)) {
		err = 1;
                log_warning("failed to save Enable Password to Config File: %s", strerror(errno));
        } 

	// make sure the acl exits then get the name to save
	char * cliName = NULL;
	if(LoginSettings.cliAcl!=NULL)
		cliName = LoginSettings.cliAcl->name;
        if (setConfigValueAsString(XML_LOGIN_CLI_ACL_TAG, cliName)) {
		err = 1;
                log_warning("failed to save ACL to Config File: %s", strerror(errno));
        } 

	// make sure the client update acl exists then get the name
	char * clientUpdateName = NULL;
	if(LoginSettings.clientUpdateAcl!=NULL)
		clientUpdateName = LoginSettings.clientUpdateAcl->name;
        if (setConfigValueAsString(XML_LOGIN_CLIENT_UPDATE_ACL_TAG, clientUpdateName)) {
		err = 1;
                log_warning("failed to save ACL to Config File: %s", strerror(errno));
        } 

	// make sure the client rib acl exists then get the name
	char * clientRIBName = NULL;
	if(LoginSettings.clientRIBAcl!=NULL)
		clientRIBName = LoginSettings.clientRIBAcl->name;
        if (setConfigValueAsString(XML_LOGIN_CLIENT_RIB_ACL_TAG, clientRIBName)) {
		err = 1;
                log_warning("failed to save ACL to Config File: %s", strerror(errno));
        } 
	
	// make sure the acl exits then get the name
	char * mrtName = NULL;
	if(LoginSettings.mrtAcl!=NULL)
		mrtName = LoginSettings.mrtAcl->name;
        if (setConfigValueAsString(XML_LOGIN_MRT_ACL_TAG, mrtName)) {
		err = 1;
                log_warning("failed to save ACL to Config File: %s", strerror(errno));
        } 

	// save closing login tag
	if(closeConfigElement()) {
		err = 1;
		log_warning("failed to save Login to Config file: %s", strerror(errno));
	}
	
	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: launch login control thread, called by main.c
 * Input:  none
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void launchLoginControlThread() {
	int error;
	
	pthread_t loginThreadID;

	// create login listener thread
	if ((error = pthread_create(&loginThreadID, NULL, loginControlThread, NULL)) > 0 )
		log_fatal("Failed to create login thread: %s\n", strerror(error));

	// keep login listener thread reference
	LoginSettings.loginThread = loginThreadID;

#ifdef DEBUG
	debug(__FUNCTION__, "Created login thread!");
#endif
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for this thread
 * Input:	
 * Output: a timevalue indicating the last time the thread was active
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/ 
time_t getLoginControlLastAction() {
	return LoginSettings.lastAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Signals to the Login Module that BGPmon is shutting down
 * Input:  none
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void signalLoginShutdown() {
	// both the login listener and all cli connections check this variable
	// to determine when to shutdown
	LoginSettings.shutdown = TRUE;

#ifdef DEBUG
	debug(__FUNCTION, "Login module signaled for shutdown.");
#endif
}

/*--------------------------------------------------------------------------------------
 * Purpose: waits for the login module to finish shutting down
 * Input:  none
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void waitForLoginShutdown() {
	void * status = NULL;

	// wait for login control thread exit
	pthread_join(LoginSettings.loginThread, status);

	// wait for each cli connection thread to exit
	CliNode * cn = LoginSettings.firstNode;
	while(cn!=NULL) {
		status = NULL;
		pthread_join(cn->cliThread, status);
		cn = cn->next;
	}

	// free memory used by command tree
	freeCommandNode(LoginSettings.rootCommandNode);
}

/*--------------------------------------------------------------------------------------
 * Purpose: the main function of login control module
 *  listens for cli connections and starts new thread for each connection 
 * Input:  none
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void * loginControlThread( void *arg) {
	fd_set read_fds; 	// file descriptor list for select()
	int fdmax = 0;		// maximum file descriptor number
	int listenSocket = -1;	// socket to listen for connections

	// timer to periodically check thread status
	struct timeval timeout; 
	timeout.tv_usec = 0;
	timeout.tv_sec = THREAD_CHECK_INTERVAL; 

	log_msg( "Login control thread started." );
	
	// listen for connections and start other client threads as needed.
  	while ( LoginSettings.shutdown == FALSE ) 
	{
		// update the last active time for this thread
		LoginSettings.lastAction = time(NULL);

		// if socket is down, reopen
		if (listenSocket == - 1) 
		{
			listenSocket = startLoginControlListener();
			// if listen succeeded, setup FD values
			// otherwise we will try next loop time
			if (listenSocket != - 1) 
			{
				FD_ZERO( &read_fds );
				FD_SET(listenSocket, &read_fds);
				fdmax = listenSocket+1;
#ifdef DEBUG
				debug( __FUNCTION__, "Opened the login listening socket(%d)!! ", listenSocket );
#endif
			}
		}
		else
		{
			FD_ZERO( &read_fds );
			FD_SET(listenSocket, &read_fds);
			fdmax = listenSocket+1;
		}
#ifdef DEBUG
		debug( __FUNCTION__, "login control thread is enabled" );
#endif

		if( select(fdmax, &read_fds, NULL, NULL, &timeout) == -1 )
		{
			log_err("login control thread select error:%s", strerror(errno));
			continue;
		}
		timeout.tv_usec = 0;
		timeout.tv_sec = THREAD_CHECK_INTERVAL; 


		if( listenSocket >= 0)
		{
			if( FD_ISSET(listenSocket, &read_fds) )//new client
			{
#ifdef DEBUG
				debug( __FUNCTION__, "new client attempting to start." );
#endif
				startLogin( listenSocket );
			}
		}
	}
	
	log_warning( "Login control thread exiting" );	 
	
	// close socket if open
	if( listenSocket != -1) 
	{
		close(listenSocket);
		FD_ZERO( &read_fds );
		fdmax = 0;
		listenSocket = -1;			
#ifdef DEBUG
		debug( __FUNCTION__, "Close the login listening socket(%d)!! ", listenSocket );
#endif
	}
	
	return NULL;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Start to listen on the configured addr+port.
 * Input:  none
 * Output: socket ID of the listener or -1 if listener create fails
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
int startLoginControlListener() {	 
	// socket to listen for incoming connections
	int listenSocket = 0;

	// create addrinfo struct for the listener
	struct addrinfo *res = createAddrInfo(LoginSettings.listenAddr, LoginSettings.listenPort);
	if( res == NULL ) 
	{
		log_err( "login control thread createAddrInfo error!" );
		freeaddrinfo(res);
		listenSocket = -1;
		return listenSocket;
	}

	// open the listen socket
	listenSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if ( listenSocket == -1 )
	{
		log_err( "fail to create login listener socket %s", strerror(errno) );
		freeaddrinfo(res);
		return listenSocket;
	}	

#ifdef DEBUG
	// allow rapid reuse of socket if in debug mode
	int yes=1;
	if(setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,&yes, sizeof(int)) < 0)
		log_err("setsockopt error");
#endif

	// bind to configured address and port
	if (bind(listenSocket, res->ai_addr, res->ai_addrlen) < 0)
	{
		log_err( "login listener unable to bind %s", strerror(errno) );
		close(listenSocket);
		freeaddrinfo(res);
		listenSocket = -1;
		return listenSocket;
	}	

	//start listening
	if (listen(listenSocket, 0) < 0) {
		log_err( "login listener unable to listen" );
		freeaddrinfo(res);
		close(listenSocket);
		listenSocket = -1;
		return listenSocket;
	}
	freeaddrinfo(res);

	return(listenSocket);	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Accept the new cli and spawn a new thread for the client 
 *          if more clients are allowed and it passes the ACL check.
 * Input:  the socket used for listening
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void startLogin( int listenSocket ) {
	// structure to store the client addrress from accept
	struct sockaddr_storage loginaddr;
	socklen_t addrlen = sizeof (loginaddr);
	
	// accept connection
	int loginSocket = accept( listenSocket, (struct sockaddr *) &loginaddr, &addrlen);
	if( loginSocket == -1 ) {
		log_err( "Failed to accept new client" );
		return;
	}

	// convert address into address string and port
	char *addr;
	int port;
	if( getAddressFromSockAddr((struct sockaddr *)&loginaddr, &addr, &port) )
	{
		log_warning( "Unable to get address and port for new connection." );
		return;
	}
#ifdef DEBUG 	
	else
		debug(__FUNCTION__, "connection request from: %s, port: %d ", addr, port);
#endif

	// too many clients, close this one
	if ( LoginSettings.activeConnections == LoginSettings.maxConnections )
	{
		log_warning( "At maximum number of connected clients: connection from %s port %d rejected.", addr, port );
		close(loginSocket);
		free(addr);
		return;
	}

	//check the new client against ACL
	if ( checkACL((struct sockaddr *) &loginaddr, CLI_ACL) == FALSE )
	{
		log_msg("cli connection from %s port %d rejected by access control list",addr, port);
		close(loginSocket);
		free(addr);
		return;
	}
	
	// lock the client list
	if ( pthread_mutex_lock( &(LoginSettings.loginLock) ) )
		log_fatal( "lock login list failed" );

		// create the argument structure for the new client thread
		CliNode * cn = malloc(sizeof (CliNode));
		if(cn==NULL) {
			log_warning("Failed to create cli structure.  Closing connection from %s port %d.", addr, port);
			close(loginSocket);
			free(addr);
			return;
		}
		memset(cn, 0, sizeof(CliNode));
		cn->socket = loginSocket;
		cn->port = port;
		cn->localASNumber = 0;
		cn->id = LoginSettings.nextConnectionId;
		cn->lastAction = time(NULL);
		memcpy(&cn->addr, addr, strlen(addr));

		//increment the number of active clients
		LoginSettings.activeConnections++;
		LoginSettings.nextConnectionId++;

		// update list of cli connections
		cn->next = LoginSettings.firstNode;
		LoginSettings.firstNode = cn;

		// spawn a new thread for this client, detach thread as early as possible to avoid memory leakage
		pthread_t cliThreadID;
		pthread_attr_t cliThreadAttr;
		int error;
		error = pthread_attr_init(&cliThreadAttr);
		error = pthread_attr_setdetachstate(&cliThreadAttr,PTHREAD_CREATE_DETACHED);
		error = pthread_create(&cliThreadID, &cliThreadAttr, &cliThread, cn);
		if (error > 0) {
			log_warning("Failed to create login thread: %s", strerror(error));
		}

		// thread reference
		cn->cliThread = cliThreadID;

	// unlock the client list
	if ( pthread_mutex_unlock( &(LoginSettings.loginLock) ) )
		log_fatal( "unlock login list failed");

#ifdef DEBUG
	debug(__FUNCTION__, "login accepted from: %s, port: %d ", addr, port);
#endif
	free(addr);
}

/*--------------------------------------------------------------------------------------
 * Purpose: checks to see if enable mode is active or not
 * Input:  none
 * Output: returns TRUE if enable mode is inactive and FALSE if enable mode is active 
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
int isEnableModeActive() {
	return LoginSettings.enableModeActive == TRUE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: sets enable mode to active
 * Input:  none
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void enableModeActive() {
	LoginSettings.enableModeActive = TRUE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: sets enable mode to inactive
 * Input:  none
 * Output: none
 * Kevin Burnett @ November 18, 2008
 * -------------------------------------------------------------------------------------*/
void enableModeInactive() {
	LoginSettings.enableModeActive = FALSE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Sets the enabled password
 * Input:  char * - the new password
 * Kevin Burnett @ January 24, 2009
 * -------------------------------------------------------------------------------------*/
void setEnablePassword(char * pwd) {
	strncpy(LoginSettings.enablePassword, pwd, (size_t)PASSWORD_MAX_CHARS);
}

/*--------------------------------------------------------------------------------------
 * Purpose: Sets the access password
 * Input:  char * - the new password
 * Kevin Burnett @ January 24, 2009
 * -------------------------------------------------------------------------------------*/
void setAccessPassword(char * password) {
	strncpy(LoginSettings.accessPassword, password, (size_t)PASSWORD_MAX_CHARS);
}

/*--------------------------------------------------------------------------------------
 * Purpose: Sets address for the login listener
 * Input: char * - the address
 * Kevin Burnett @ January 26, 2009
 * -------------------------------------------------------------------------------------*/
int setListenAddr(char * addr) {
	if(checkAddress(addr, ADDR_PASSIVE)) {
		return 1;
	}
	else {
		strncpy(LoginSettings.listenAddr, addr, (size_t)ADDR_MAX_CHARS);
		return 0;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: gets the address for the login listener
 * output: char * - the address
 * Note:  The caller is responsible for free the allocated memory
 * 	from this function
 * Kevin Burnett @ February 5, 2009
 * -------------------------------------------------------------------------------------*/
char * getListenAddr() {
	char * addr = (char*)malloc(ADDR_MAX_CHARS);
	if(addr!=NULL) {
		strncpy(addr, LoginSettings.listenAddr, (size_t)ADDR_MAX_CHARS);
	}
	else {
		log_warning("Unable to allocate memory for login-listener address.\n");
		return NULL;
	}

	return addr;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Sets port for the login listener
 * Input: char * - the address
 * Kevin Burnett @ January 26, 2009
 * -------------------------------------------------------------------------------------*/
int setListenPort(int port) {
	if(checkPort(port)) {
		return 1;
	}
	else {
		LoginSettings.listenPort = port;
		return 0;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: gets the port for the login listener
 * output: port - the address
 * Kevin Burnett @ January 26, 2009
 * -------------------------------------------------------------------------------------*/
int getListenPort() {
	int port = 0;
	port = LoginSettings.listenPort;
	return port;
}
