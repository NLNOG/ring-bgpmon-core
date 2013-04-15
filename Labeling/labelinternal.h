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
 *  File: labelinternal.h
 * 	Authors: He Yan
 *  Date: Jun 22, 2008
 */

#ifndef LABELINTERNAL_H_
#define LABELINTERNAL_H_
#include "rtable.h"
/* needed for copying BGPmon Internal Format messages */
#include "../Util/bgpmon_formats.h"
/* needed for session structure*/
#include "../Peering/peersession.h"
/* needed for MAX_PEER_IDS  */
#include "../site_defaults.h"

/*----------------------------------------------------------------------------------------
 * Purpose: Process one BMF message 
 * Input: BMF message
 * Output:  0 for success or 1 for failure
 * Note: 1. Apply this BGP update message to the correspondong rib table based on the peerID.
 	 2. Label this BGP update message based on the correspondong rib.  
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int processBMF (BMF bmf);

/*--------------------------------------------------------------------------------------
 * Purpose: Apply a BGP Update message to a rib table
  * Input:  originatedTS - the timestamp
 *		  parsedUpdateMsg -  Parsed BGP update messages
 *		  session - the corresponding session structure
 *		  bmf - BMF message if labeling is enabled and the labels of prefixes will be appened to the BMF message 
 * Output: 0 for success or -1 for failure
 * He Yan @ July 4th, 2008
 * -------------------------------------------------------------------------------------*/ 
int applyBGPUpdate (time_t originatedTS, ParsedBGPUpdate *parsedUpdateMsg, Session_structp session, BMF bmf);

/*----------------------------------------------------------------------------------------
 * Purpose: Entry function of rib/label thread
 * Input:
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void * labelingThread( void *arg ) ;

/*--------------------------------------------------------------------------------------
 * Purpose: Destory a prefix table
 * Input:	 prefixTable - the pointer to a prefix table
 *		 session - the corresponding session structure which includes the prefix table
 * Output: 0 means success, -1 means failure.
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int destroyPrefixTable ( PrefixTable *prefixTable, Session_structp session );


/*--------------------------------------------------------------------------------------
 * Purpose: Destory a attribute table
 * Input:	 attrTable - the pointer to a attribute table
 *		 session - the corresponding session structure which includes the attribute table
 * Output: 0 means success, -1 means failure.
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
int destroyAttrTable ( AttrTable *attrTable, Session_structp session );


/*--------------------------------------------------------------------------------------
 * Purpose: Print a attribute table
 * Input:	 session - the corresponding session structure which includes the attribute table
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
void printAttrTable( Session_structp session );


/*--------------------------------------------------------------------------------------
 * Purpose: Print a prefix table
 * Input: session - the corresponding session structure which includes the prefix table
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
void printPrefixTable(Session_structp session);

#endif /*LABELINTERNAL_H_*/
