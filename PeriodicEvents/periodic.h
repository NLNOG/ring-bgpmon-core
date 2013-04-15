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
 *  File: periodic.h
 * 	Authors:  He Yan
 *  Date: Jun 24, 2008 
 */

#ifndef PERIODIC_H_
#define PERIODIC_H_

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the periodic events settings.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
initPeriodicSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Read the periodic events settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
readPeriodicSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Save the periodic events settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure.
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int 
savePeriodicSettings();


/*--------------------------------------------------------------------------------------
 * Purpose: launch periodic route refresh and status message threads, called by main.c
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void LaunchPeriodicThreads();

/*--------------------------------------------------------------------------------------
 * Purpose: Set the interval of route refresh
 * Input:	the interval
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
setPeriodicRouteRefreshInterval( int interval );

/*--------------------------------------------------------------------------------------
 * Purpose: get the interval of route refresh
 * Input:	
 * Output: the interva
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
getPeriodicRouteRefreshInterval();

/*--------------------------------------------------------------------------------------
 * Purpose: Set the interval of sending status message
 * Input:	the interval
 * Output: 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void 
setPeriodicStatusMessageInterval( int interval );

/*--------------------------------------------------------------------------------------
 * Purpose: Get the interval of sending status message
 * Input:	
 * Output: the interval
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int
getPeriodicStatusMessageInterval();

/*--------------------------------------------------------------------------------------
 * Purpose: enable route refresh from peers
 * Input: the transfer type
 * Output: 
 * Kevin Burnett @ June 18, 2009
 * -------------------------------------------------------------------------------------*/ 
void 
enablePeriodicRouteRefresh();

/*--------------------------------------------------------------------------------------
 * Purpose: disables route refresh from peers
 * Input: the transfer type
 * Output: 
 * Kevin Burnett @ June 18, 2009
 * -------------------------------------------------------------------------------------*/ 
void 
disablePeriodicRouteRefresh();

/*--------------------------------------------------------------------------------------
 * Purpose: Get the transfer type
 * Input:	
 * Output: the type
 * Kevin Burnett @ June 18, 2009
 * -------------------------------------------------------------------------------------*/ 
int
getPeriodicRouteRefreshEnableStatus();

/*--------------------------------------------------------------------------------------
 * Purpose: get the last action time of the Periodic thread
 * Input:
 * Output: last action time
 * Mikhail Strizhov @ Jun 25, 2009
 * -------------------------------------------------------------------------------------*/
time_t 
getPeriodicRouteRefreshThreadLastActionTime();

/*--------------------------------------------------------------------------------------
 * Purpose: get the last action time of the Periodic thread
 * Input:
 * Output: last action time
 * Mikhail Strizhov @ Jun 25, 2009
 * -------------------------------------------------------------------------------------*/
time_t
getPeriodicStatusMessageThreadLastActionTime();

/*--------------------------------------------------------------------------------------
 * Purpose: Intialize the shutdown process for the periodic module
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void signalPeriodicShutdown();

/*--------------------------------------------------------------------------------------
 * Purpose: wait on all periodic pieces to finish closing before returning
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void waitForPeriodicShutdown();

#endif /*PERIODIC_H_*/

