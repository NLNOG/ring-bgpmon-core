/*
 * 
 * Copyright (c) 2010 Colorado State University
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
 *  File: XMLUtils.h
 * Author: He Yan
 */

#ifndef XMLUTILS_H_
#define XMLUTILS_H_

#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/xinclude.h>
#include <libxml2/libxml/xmlIO.h>
#include <libxml2/libxml/xpath.h>
#include <libxml2/libxml/xpathInternals.h>
#include <libxml2/libxml/tree.h>

#include <stdio.h>

int getListCount( xmlDocPtr document, char *xpath );
char *getValueAsString( xmlDocPtr document, char *xpath, int maxLength );
int getValueAsInt( int *value, xmlDocPtr document, char * xpath, int min_value, int max_value);
int  getValueAsFloat( float *value, xmlDocPtr document, char * xpath, float min_value, float max_value);
char *getAttributeAsString( xmlDocPtr document, char *xpath, char *attribute );
int getNumOfNodes( xmlDocPtr document, char *xpath );
char *getValueFromListAsString( xmlDocPtr document, char *xpath, char *tag, int item, int maxLength);
int getValueFromListAsInt( int *value, xmlDocPtr document, char * xpath, char *tag, int item, int min_value, int max_value);
int getValueFromListAsInt32( u_int32_t *value, xmlDocPtr document, char * xpath, char *tag, int item, u_int32_t min_value, u_int32_t max_value);


#endif /*XMLUTILS_H_*/
