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
 *  File: mrt.h
 * 	Authors: He Yan, Dan Massey
 *  Date: Oct 8, 2008 
 */

#ifndef MRT_H_
#define MRT_H_

/* needed for system types such as time_t */
#include <sys/types.h>
#include "../Util/bgpmon_formats.h"

// functions related to accepting and managing mrt connections
// see mrtcontrol.c for corresponding functions

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default mrt control configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
initMrtControlSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Read the mrt control settings from the config file.
 * Input: none
 * Output:  returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
readMrtControlSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Save the  mrts control  settings to the config file.
 * Input:  none
 * Output:  retuns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
saveMrtControlSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: launch mrt control thread, called by main.c
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
launchMrtControlThread();

/*--------------------------------------------------------------------------------------
 * Purpose: Get the state of mrt control
 * Input:
 * Output: returns TRUE or FALSE
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
isMrtControlEnabled();

/*--------------------------------------------------------------------------------------
 * Purpose: enable the mrt control
 * Input:	
 * Output: 
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
enableMrtControl();


/*--------------------------------------------------------------------------------------
 * Purpose: disable the mrt control
 * Input:	
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
disableMrtControl();

/*--------------------------------------------------------------------------------------
 * Purpose: Get the listening port of mrt control
 * Input:	
 * Output: the listening port of mrt control module 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
getMrtControlListenPort();

/*--------------------------------------------------------------------------------------
 * Purpose: Set the listening port of mrt control
 * Input:	the port
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
setMrtControlListenPort( int port );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the listening address of mrt control
 * Input:	
 * Output: the listening address for mrt control
 *          or NULL if no address for mrt control
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * 
getMrtControlListenAddr();

/*--------------------------------------------------------------------------------------
 * Purpose: Set the listening address of mrt control
 * Input:	the addr in string format
 * Output: ADDR_VALID means address is valid and set successfully.
 *	   ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *	   ADDR_ERR_INVALID_PASSIVE means this is not a valid passive address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
setMrtControlListenAddr( char *addr );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for this thread
 * Input:	
 * Output: a timevalue indicating the last time the thread was active
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
time_t 
getMrtControlLastAction();

/*--------------------------------------------------------------------------------------
 * Purpose: Get the maximum number of Mrts allowed
 * Input:	
 * Output:  an integer indicating the max Mrts
 * Catherine Olschanowsky April, 2012
 * -------------------------------------------------------------------------------------*/ 
int
getMrtMaxConnections();

/*--------------------------------------------------------------------------------------
 * Purpose: Set the maximum number of Mrts allowed
 * Input:  int, the new maximum	
 * Output:  0 on success, 1 on failure
 * Catherine Olschanowsky April, 2012
 * -------------------------------------------------------------------------------------*/ 
int
setMrtMaxConnections(int newMax);

// functions related to a particular mrt connection
// see mrtinstance.c for corresponding functions

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of IDs of all active mrt connections
 * Input: a pointer to an unallocated array
 * Output: the length of the array or -1 on failure
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
getActiveMrtsIDs(long **mrtIDs);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the connected mrt's port
 * Input: ID of the mrt
 * Output: port of this mrt 
 *         or -1 if there is no mrt with this ID 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
getMrtPort(long ID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the connected mrt's address
 * Input: ID of the mrt
 * Output: address of this mrt in a char array. 
 *         or NULL if there is no mrt with this ID 
 * Note: 1. The caller doesn't need to allocate memory.
 *       2. The caller must free the string after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * 
getMrtAddress(long ID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the connected time of a mrt
 * Input: ID of the mrt
 * Output: connected time of this mrt
 * 		  or time = 0 if there is no mrt with this ID
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
time_t 
getMrtConnectedTime(long ID);

/*--------------------------------------------------------------------------------------
 * Purpose: Get the labelAction of a mrt
 * Input: ID of the mrt
 * Output: Label Action of this mrt
 * Mikhail Strizhov @ Jan 30, 2010
 * -------------------------------------------------------------------------------------*/ 
int 
getMrtLabelAction(long ID);


/*--------------------------------------------------------------------------------------
 * Purpose: Get the number of items written by this mrt connection
 * Input: ID of the mrt
 * Output: the number of items
 * 	   or -1 if there is no mrt with this ID
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------
 */ 
long
getMrtWriteItems(long ID);


/*--------------------------------------------------------------------------------------
 * Purpose: Get the last action time for this mrt connection
 * Input: ID of the mrt
 * Output: a timevalue indicating the last time the thread was active
 *         or time = 0 if there is no mrt with this ID
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t
getMrtLastAction(long ID);


/*--------------------------------------------------------------------------------------
 * Purpose:  destroy the socket and memory associated with a mrt ID
 * Input:  the mrt ID to destroy
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void 
destroyMrt(long id);

/*--------------------------------------------------------------------------------------
 * Purpose: kill a mrt connection, close the thread and free all associated memory 
 * Input: ID of the mrt
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
deleteMrt(long ID);

/*--------------------------------------------------------------------------------------
 * Purpose: Intialize the shutdown process for the mrt module
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void signalMrtShutdown();

/*--------------------------------------------------------------------------------------
 * Purpose: wait on all mrt pieces to finish closing before returning
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void waitForMrtShutdown();

#endif /*MRT_H_*/
