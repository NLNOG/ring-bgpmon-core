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
 *  File: bgpmon_formats.h
 * 	Authors: He Yan
 *  Date: Jun 20, 2008
 */


#ifndef BGPMON_FORMATS_H_
#define BGPMON_FORMATS_H_

#include <sys/types.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/timeb.h>


/*
 *  The BGPmon internal message format (BMF) exchanged between BGPmon modules.
 */

#define BMF_MAX_MSG_LEN 		8192
#define BMF_HEADER_LEN 			16

/* BGP header length: Marker(16) + Length(2) + Type(1) */
#define BGP_HEADER_LEN 			19

struct BGPmonInternalMessageFormatStruct 
{
	u_int32_t		timestamp;
	u_int32_t		precisiontime;
	u_int16_t	        sessionID;
	u_int16_t		type;
	u_int32_t		length;
	u_char			message[BMF_MAX_MSG_LEN];
};
typedef struct BGPmonInternalMessageFormatStruct *BMF;

/* bgpmon internal message format types */
#define BMF_TYPE_RESERVED		256//0
#define BMF_TYPE_MSG_TO_PEER		257//1
#define BMF_TYPE_MSG_FROM_PEER		258//2
#define BMF_TYPE_MSG_LABELED		259//3
#define BMF_TYPE_TABLE_TRANSFER		260//4
#define BMF_TYPE_SESSION_STATUS		261//5
#define BMF_TYPE_QUEUES_STATUS		262//6
#define BMF_TYPE_CHAINS_STATUS		263//7
#define BMF_TYPE_FSM_STATE_CHANGE	264//8
#define BMF_TYPE_BGPMON_START		265//9
#define BMF_TYPE_BGPMON_STOP		266//10
#define BMF_TYPE_MRT_STATUS			277
#define BMF_TYPE_TABLE_START		267
#define BMF_TYPE_TABLE_STOP		268

/* Create a BMF instance by allocating memory and setting time */
/* time is set to the current time and is the main purpose of this function */  
/* sessionID, and type are specified as parameters,  length is 0 */
BMF createBMF( u_int16_t sessionID, u_int16_t type);

/* Append additional data to an existing BMF instance  */
/* to append data, specify the length of the data to add and the data   */
int bgpmonMessageAppend(BMF m, const void *message, u_int32_t len);

/* Destroy a BMF instance  */
void destroyBMF( BMF bmf );

#endif

