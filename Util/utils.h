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
 *  File: utils.h
 * 	Authors:  Kevin Burnett, Dan Massey
 *  Date: July 21, 2008 
 */

#ifndef UTILS_H_
#define UTILS_H_

// needed for addrinfo struct
#include <netdb.h>

// needed for FILE handle
#include <stdio.h>

struct BGPmonAddr_Struct {
   struct addrinfo *addr;
   char *addrstring;
   int isLocal;
   int isValid;
};

typedef struct BGPmonAddr_Struct BGPmonAddr; 

/* write an Address to the configuration file */
int saveBGPmonAddr(FILE *config_fd, char *tag, BGPmonAddr address);

/* write a number to the configuration file */
int saveInteger(FILE *config_fd, char *tag, int num, int tabbing);

/* write a string to the configuration file */
int saveString(FILE *config_fd, char *tag, char *string, int maxlen, int tabbing);

/* write an opening xml tag to the configuration file */
int saveMainTagStart(FILE * config_fd, char * tag, int tabbing);

/* write an closing xml tag to the configuration file */
int saveMainTagEnd(FILE * config_fd, char * tag, int tabbing);

/* simple wrapper to ensure pointers are not used after free */
void bgpmon_free(void **space);

#endif /*UTILS_H_*/
