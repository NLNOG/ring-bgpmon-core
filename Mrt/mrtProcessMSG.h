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
 *  File: mrtinstance.c
 * 	Authors: He Yan, Dan Massey, Mikhail, Cathie Olschanowsky
 *
 *  Date: Oct 7, 2008 
 */
#ifndef MRTPROCESSMSG_H_
#define MRTPROCESSMSG_H_
 
/* externally visible structures and functions for mrts */
#include "mrt.h"
#include "mrtMessage.h"
#include "mrtinstance.h"
#include "mrtUtils.h"
#include "../Util/log.h"
#include "../Peering/peersession.h"
#include "../Peering/bgpstates.h"
#include "../Peering/bgpevents.h"

/*--------------------------------------------------------------------------------------
 * Purpose: read from the backlog and process messages
 * Input:  the control MRT structure for this instance (and therefore the backlog)
 * Output: NONE
 * Description: This function will continue to run until it is Killed by its parent
--------------------------------------------------------------------------------------*/
void* MRT_processMessages();

/*--------------------------------------------------------------------------------------
 * Purpose: this code processes MRT messages of type 16  BGP4MP subtype 1,4 UPDATE
 * Input:  raw message, desired asN length, header, message and bmf pointer
 * Output: 1 for success
 *         0 for failure
 *         the bmf object is populated
 * Description: This function will process a single MRT message and then return
--------------------------------------------------------------------------------------*/
int MRT_processType16SubtypeMessage(uint8_t *rawMessage,int asNumLen,MRTheader *mrtHeader,MRTmessage *mrtMessage,BMF *bmf);

/*--------------------------------------------------------------------------------------
 * Purpose: this code attaches a session id to the BMF and submits it to the Q.
 * Input:  mrtHeader,bmf object
 * Output: 0 for success
 *         1 for success, but queue now full
 *        -1 for failure
 * Description: 
--------------------------------------------------------------------------------------*/
int submitBMF(MrtNode *cn, MRTheader *mrtHeader, MRTmessage *mrtMessage, BMF bmf);

#endif
