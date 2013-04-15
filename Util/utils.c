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
 *  File: utils.c
 * 	Authors:  Kevin Burnett, Dan Massey
 *  Date: July 21, 2008 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// needed for function inet_pton
#include <arpa/inet.h>

#include "utils.h"
#include "log.h"
#include "bgpmon_defaults.h"

/* a simple wrapper to make sure that pointers are not used after a free */
void bgpmon_free(void **space){
  free(*space);
  (*space) = NULL;
}

/* write an Address to the configuration file */
int saveBGPmonAddr(FILE * config_fd, char *tag, BGPmonAddr address) {
	return 0;
}

/* write a number to the configuration file */
int saveInteger(FILE * config_fd, char *tag, int num, int tabbing) {
	int failureOccured = FALSE;
	int i=0;
	for(i=0; i<tabbing; i++) {
		if(fprintf(config_fd, "\t")==0) {
			failureOccured = TRUE;
			break;
		}
	}

	if(failureOccured==FALSE && fprintf(config_fd, "<%s>%d</%s>\n", tag, num, tag)>=0) {
		// function succeeded
		return 0;
	}

	// function failed
	return 1;
}

/* write a string to the configuration file */
int saveString(FILE * config_fd, char *tag, char *string, int maxlen, int tabbing) {
	int failureOccured = FALSE;
	int i=0;
	for(i=0; i<tabbing; i++) {
		if(fprintf(config_fd, "\t")==0) {
			failureOccured = TRUE;
			break;
		}
	}

	char * temp = malloc(maxlen);
	memset(temp, 0, maxlen);
	memcpy(temp, string, maxlen);

	if(failureOccured==FALSE && fprintf(config_fd, "<%s>%s</%s>\n", tag, temp, tag)>=0) {
		// function succeeded
		free(temp);
		return 0;
	}

	// function failed
	free(temp);
	return 1;
}

int saveMainTagStart(FILE * config_fd, char * tag, int tabbing) {
	int i=0;
	for(i=0; i<tabbing; i++) {
		if(fprintf(config_fd, "\t")==0) {
			break;
		}
	}
	
	if(fprintf(config_fd, "<%s>\n", tag)<0) {
		log_fatal("Error saving tag: %s", tag);
		return 1;
	}
	return 0;
}

int saveMainTagEnd(FILE * config_fd, char * tag, int tabbing) {
	int i=0;
	for(i=0; i<tabbing; i++) {
		if(fprintf(config_fd, "\t")==0) {
			break;
		}
	}
	
	if(fprintf(config_fd, "</%s>\n", tag)<0) {
		log_fatal("Error saving tag: %s", tag);
		return 1;
	}
	return 0;
}
