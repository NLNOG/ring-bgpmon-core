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
 *  File: labelutils.c
 * 	Authors: He Yan 
 *  Date: May 31, 2008 
 */
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
	 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "labelutils.h"

/*----------------------------------------------------------------------------------------
 * Purpose: Mstream functions. 
 * He Yan @ May 7, 2008
 * -------------------------------------------------------------------------------------*/
void mstream_init(MSTREAM *s, u_char *data, u_int32_t len) {
   s->start = data;
   s->position = 0;
   s->len=len;
}

int mstream_getc(MSTREAM *s, u_char *d) {
   if (d==NULL)
      return 1;
   return mstream_get(s, d, sizeof(u_char));
}

int mstream_getw(MSTREAM *s, u_int16_t *d) {
   if (d==NULL)
      return 1;
   if (mstream_get(s, d, sizeof(u_int16_t)))
      return 1;
   *d = ntohs (*d);
   return 0;
}

int mstream_getl(MSTREAM *s, u_int32_t *d) {
   if (d==NULL)
      return 1;
   if (mstream_get(s, d, sizeof(u_int32_t)))
      return 1;
   *d = ntohl(*d);
   return 0;
}

int64_t 
mstream_can_read(MSTREAM *s) {
   return ((int64_t)s->len-s->position);
}

u_char * mstream_current_address (MSTREAM *s) {
   return s->start + s->position;
}

int mstream_get (MSTREAM *s, void *d, u_int32_t len) {
   int64_t room = mstream_can_read(s);

   if (room < len || d == NULL)
      return 1;   
   memcpy(d, s->start + s->position, len);
   s->position += len;
   return 0;
}

int mstream_add (MSTREAM *s, void *data, u_int16_t len) {   
   if (s->position + len > s->len)
      return 1;
   if (len != 0 && data != NULL) {
      memcpy(s->start+s->position, data, len);
      s->position += len;
   }
   return 0;
}


