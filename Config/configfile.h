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
 *  File: configfile.h
 * 	Authors: Kevin Burnett, Dan Massey
 *
 *  Date: July 30, 2008
 */

#ifndef CONFIGFILE_H_
#define CONFIGFILE_H_

#include <sys/socket.h>
#include "../Util/utils.h"
// needed for u_char data type
#include <sys/types.h>


enum configErrCode { 
	CONFIG_VALID_ENTRY = 0,
	CONFIG_NO_ENTRY = -1,
	CONFIG_INVALID_ENTRY = -2
};


// backs up the configuration file and returns the backup file name 
int backupConfigFile(char * config_file, char * backup_config_file);

// read the configuration file into memory
int readConfigFile (char * configfile);

// save the configuration file out to disk
int saveConfigFile(char * configFile);

// gets an string value out of the configuration
int getConfigValueAsString( char **value, char *xpath, int maxLength );

// gets an integer value out of the configuration
int getConfigValueAsInt( int *value, char * xpath, int min_value, int max_value);

// gets an float value out of the configuration
int  getConfigValueAsFloat( float *value, char * xpath, float min_value, float max_value);

// gets an address as a string from configuration
int getConfigValueAsAddr( char **addr, char * xpath, int passive );

// writes the a string value into the node specified in the xpath
int setConfigValueAsString( char * xpath, char * value);

// writes the an integer value into the node specified in the xpath
int setConfigValueAsInt( char * xpath, int value);

// writes the an float value into the node specified in the xpath
int setConfigValueAsFloat( char * xpath, float value);

// gets an address as a string in a list from configuration
int getConfigValueFromListAsAddr( char **addr, char * xpath, char *tag, int item, int passive );

// gets a string from within a set
int getConfigValueFromListAsString(char **value, char *xpath, char *tag, int item, int maxLength);

// gets an integer value from within a set
int getConfigValueFromListAsInt(int *value, char * xpath, char *tag, int item, int min_value, int max_value);
int getConfigValueFromListAsInt32(u_int32_t *value, char * xpath, char *tag, int item, u_int32_t min_value, u_int32_t max_value); 

// get the count of a list
int getConfigListCount(char * xpath);

void getAddressString( BGPmonAddr * b_addr, char * output );

int openConfigElement(const char * tag);
int closeConfigElement();

/*----------------------------------------------------------------------------------------
 * Purpose: Convert a hex string into an array of u_char 
 * Input:	hex string, its length
 * Output: Pointer to an array of u_char, note callerneeds to free it
 *		  NULL if the hex string is invalid.		
 * He Yan @ Agu 24, 2008   
 * ------------------------------------------------------------------------------------*/
u_char *hexStringToByteArray( char *hexString );

/*----------------------------------------------------------------------------------------
 * Purpose: Convert an array of u_char into a hex string
 * Input:	a pointer of u_char and len
 * Output: a hex string, note callerneeds to free it	
 * He Yan @ August 24, 2008   
 * ------------------------------------------------------------------------------------*/
char *byteArrayToHexString( u_char *data, int len );

#endif	// CONFIGFILE_H_
