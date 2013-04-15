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
 *  File: bgpmon_formats.c
 * 	Authors: He Yan
 *  Date: May 6, 2008
 */

/*
 *  Utility functions that work for our internal struct BMF
 */

#include "bgpmon_formats.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/timeb.h>


BMF
createBMF( u_int16_t sessionID, u_int16_t type )
{	
	struct timeb tp;
	BMF m = malloc( sizeof (struct BGPmonInternalMessageFormatStruct) );
	if ( m )
	{
		ftime(&tp);
		m->timestamp =  tp.time;
		m->precisiontime = tp.millitm;
		m->sessionID= sessionID;
		m->type = type;
		m->length = 0;
	}
	else
		log_fatal( "CreateBgpmonMessage: malloc failed" );
	return m;
}

int 
bgpmonMessageAppend(BMF m, const void *message, u_int32_t len)
{
	/* appends to message buffer
	 */
	if ( message != NULL && len > 0 && m->length + len <= BMF_MAX_MSG_LEN )
	{
		memcpy(&m->message[m->length], message, len);
		m->length += len;
	}
	else{
		if(len >= BMF_MAX_MSG_LEN){
			log_err( "BgpmonMessageAppend: length error. Length is %lu, BMF_MAX_MSG_LEN is %lu", len, BMF_MAX_MSG_LEN);
                        return -1;
		} else{
                         log_err("bgpmonMessageAppend: Invalid BMF or message supplied!");
                         return -1;
                }
	}
        return 0;
}

void 
destroyBMF( BMF bmf )
{	
	if( bmf )
		free(bmf);
}

