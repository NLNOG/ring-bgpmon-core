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
 *  File: periodic_commands.c
 * 	Authors: Kevin Burnett
 *
 *  Date: November 10, 2008
 */

// Needed for atoi() 
#include <stdlib.h>
// Needed for memset,strncmp
#include <string.h>
// Needed for strerror(errno)
#include <errno.h>

// Needed for the function definitions
#include "commands.h"
// Provides the definition for clientThreadArguments struct
#include "login.h"
// Provides the definition for commandArgument and commandNode structs
#include "commandprompt.h"
// Provides defaults used and Queue names
#include "../site_defaults.h"
// needed for TRUE/FALSE definitions
#include "../Util/bgpmon_defaults.h"
// Provides the definition for the ChainStruct
#include "../PeriodicEvents/periodic.h"

/*----------------------------------------------------------------------------------------
 * Purpose: Sets the route-refresh interval for the periodic module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ August 4, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdPeriodicRouteRefresh(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int temp = 0;
	temp = atoi(ca->commandArgument);
	setPeriodicRouteRefreshInterval(temp);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Sets the status message interval for the periodic module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ August 4, 2008   
 * -------------------------------------------------------------------------------------*/
int cmdPeriodicStatusMessage(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int temp = 0;
	temp = atoi(ca->commandArgument);
	setPeriodicStatusMessageInterval(temp);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Gets the route-refresh interval for the periodic module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ January 15, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowPeriodicRouteRefresh(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int temp = 0;
	temp = getPeriodicRouteRefreshInterval();
	sendMessage(client->socket, "%d\n", temp);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Returns the status message interval for the periodic module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ January 15, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowPeriodicStatusMessage(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int temp = 0;
	temp = getPeriodicStatusMessageInterval();
	sendMessage(client->socket, "%d\n", temp);
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Enables or Disables a route refresh for the periodic module
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ June 18, 2009
 * -------------------------------------------------------------------------------------*/
int cmdPeriodicRouteRefreshEnableDisable(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	if(listContainsCommand(cn, "enable")) {
		enablePeriodicRouteRefresh();
	}
	else {
		disablePeriodicRouteRefresh();
	}
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: 
 * Input: commandArgument - A linked list that provides all the parameters the users typed 
 * 		in. This list is in the same order as they were typed.
 * 	clientThreadArguments - A struct providing the basic address information for the 
 * 		current connection.
 * 	commandNode - A pointer to the current node in the command tree structure.
 * Output:  0 for success or 1 for failure
 * Kevin Burnett @ June 18, 2009
 * -------------------------------------------------------------------------------------*/
int cmdShowPeriodicRouteRefreshStatus(commandArgument * ca, clientThreadArguments * client, commandNode * cn) {
	int status = 0;
	status = getPeriodicRouteRefreshEnableStatus();

	if(status==TRUE)
		sendMessage(client->socket, "route refresh is enabled\n");
	else 
		sendMessage(client->socket, "route refresh is disabled\n");

	return 0;
}

