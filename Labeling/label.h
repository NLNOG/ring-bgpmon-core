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
 *	OF MERCHANTABILITY, FITNESS FOR A PARTIC`:ULAR PURPOSE AND
 *	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *	OTHER DEALINGS IN THE SOFTWARE.
 * 
 * 
 *  File: label.h
 * 	Authors: He Yan
 *  Date: Jun 22, 2008
 */

#ifndef LABEL_H_
#define LABEL_H_

// needed for refrence to labeling thread
#include <pthread.h>

#include "../Queues/queue.h"

/* label action */
enum labelAction {
	NoAction = 0,	// nothing needs to be done by labeling module
	Label,			// labeling module needs to store rib and label updates
	StoreRibOnly	// labeling module only needs to store rib
};

struct LabelControls_struct_st {
	time_t		lastAction;
	pthread_t 	labelThread;
	int		shutdown;
};
typedef struct LabelControls_struct_st LabelControls_struct;

LabelControls_struct LabelControls;

/* label types in Bgpmon */
#define BGPMON_LABEL_NULL					0
#define BGPMON_LABEL_WITHDRAW				1
#define BGPMON_LABEL_WITHDRAW_DUPLICATE		2		            
#define BGPMON_LABEL_ANNOUNCE_NEW			3
#define BGPMON_LABEL_ANNOUNCE_DUPLICATE		4
#define BGPMON_LABEL_ANNOUNCE_DPATH			5
#define BGPMON_LABEL_ANNOUNCE_SPATH			6

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default Labeling configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int initLabelingSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Read the labeling settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readLabelingSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: Save the labeling settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int saveLabelingSettings();

/*--------------------------------------------------------------------------------------
 * Purpose: launch labeling thread, called by main.c
 * Input:  none
 * Output: none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void launchLabelingThread();

/*--------------------------------------------------------------------------------------
 * Purpose: Clear the content of Rib table of a session
 * Input:  sessionID - ID of the session needs to clear Rib
 * Output: 0 means success, -1 means failure.
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int cleanRibTable(int sessionID);

/*--------------------------------------------------------------------------------------
 * Purpose:delete the entrie Rib table of a session
 * Input:  sessionID - ID of the session needs to delete Rib
 * Output: 0 means success, -1 means failure.
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int deleteRibTable(int sessionID);

/*----------------------------------------------------------------------------------------
 * Purpose: send out the rib table of a session
 * Input:	ID of a session
 * 		Queue rwiter
 * 		time for each session to tranfer data
 * Output: 0 means success, -1 means failure
 * He Yan @ Jun 22, 2008
 * Mikhail Strizhov @ July 23, 2010
 * -------------------------------------------------------------------------------------*/
int sendRibTable(int sessionID, QueueWriter labeledQueueWriter, int transfer_time);

/*--------------------------------------------------------------------------------------
 * Purpose: get the last action time of the lable thread
 * Input:  
 * Output: last action time
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
time_t getLabelThreadLastActionTime();

/*--------------------------------------------------------------------------------------
 * Purpose: Intialize the shutdown process for the label module
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 9, 2009
 * -------------------------------------------------------------------------------------*/
void signalLabelShutdown();

/*--------------------------------------------------------------------------------------
 * Purpose: wait on all label pieces to finish closing before returning
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 9, 2009
 * -------------------------------------------------------------------------------------*/
void waitForLabelShutdown();

#endif /*LABEL_H_*/
