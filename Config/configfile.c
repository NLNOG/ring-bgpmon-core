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
 *  File: configfile.c
 * 	Authors: Kevin Burnett
 *
 *  Date: July 30, 2008 
 */
 
#include <libxml/parser.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "configfile.h"
#include "configdefaults.h"
#include "XMLUtils.h"
#include "../Login/login.h"
#include "../Queues/queue.h"
#include "../Util/bgpmon_defaults.h"
#include "../Util/log.h"
#include "../Peering/peers.h"
#include "../Peering/peergroup.h"
#include "../Clients/clients.h"
#include "../Mrt/mrt.h"
#include "../Chains/chains.h"
#include "../PeriodicEvents/periodic.h"
#include "../Util/acl.h"
#include "../Util/address.h"

 
// ptr to the actual xmlConfigFile
xmlDocPtr xmlConfigFilePtr;
xmlTextWriterPtr xmlConfigWriter;

/*------------------------------------------------------------------------------
 * Purpose: read the configuration file into memory
 * Input:   The config file name
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ July 24, 2008   
 * ---------------------------------------------------------------------------*/
int 
readConfigFile (char *configfile)
{
	xmlNodePtr cur = NULL;
	log_msg("The configuration file is %s", configfile);

	// create the default group before reading configuration file
	if( createDefaultPeerGroup() == -1 )
		log_fatal("readConfigFile: couldn't create default peer group!");
	else
		log_msg("readConfigFile: default peer group is created.");	

	//  attempt to load the configuration file to memory
	xmlConfigFilePtr = xmlReadFile(configfile, NULL, XML_PARSE_NOWARNING | XML_PARSE_NOERROR); 
	if (xmlConfigFilePtr== NULL) {
		log_warning("Unable to parse file %s.  No configuration file loaded.", configfile);
		return 0;	// return no error so bgpmon can run on initial settings
	}

	//  make sure the configuration file is not empty
	cur = xmlDocGetRootElement(xmlConfigFilePtr);
	if (cur == NULL) {
		log_warning("Empty file %s.  No configuration file loaded.", configfile);
		xmlFreeDoc(xmlConfigFilePtr);
		return 1;
	}

	//  expect the root to be <bgpmon> 
	if (xmlStrcmp(cur->name, BAD_CAST XML_BGPMON_TAG)) {
		log_warning("File %s has invalid root element.  No configuration file loaded.", configfile);
		xmlFreeDoc(xmlConfigFilePtr);
		return 1;
	}

	// parse the client information
	if (readClientsControlSettings()) {
		xmlFreeDoc(xmlConfigFilePtr);
		log_err("Invalid clients configuration in file %s.", configfile);
		return 1;
	}

	// parse the mrt information
	if (readMrtControlSettings()) {
		xmlFreeDoc(xmlConfigFilePtr);
		log_err("Invalid mrt configuration in file %s.", configfile);
		return 1;
	}

	//parse the chain information
	if (readChainsSettings()) {
		xmlFreeDoc(xmlConfigFilePtr);
		log_err("Invalid login configuration in file %s.", configfile);
		return 1;
	}

	// parse the peer groups information
	if (readPeerGroupsSettings()) {
		xmlFreeDoc(xmlConfigFilePtr);
		log_err("Invalid peer groups configuration in file %s.", configfile);
		return 1;
	}
	
	// parse the peers information
	if (readPeersSettings()) {
		xmlFreeDoc(xmlConfigFilePtr);
		log_err("Invalid peers configuration in file %s.", configfile);
		return 1;
	}

	// parse the Queue information
	if (readQueueSettings()) {
		xmlFreeDoc(xmlConfigFilePtr);
		log_err("Invalid queue configuration in file %s.", configfile);
		return 1;
	}

	// parse the Periodic information
	if (readPeriodicSettings()) {
		xmlFreeDoc(xmlConfigFilePtr);
		log_err("Invalid periodic configuration in file %s.", configfile);
		return 1;
	}

	// parse the acl information
	if (readACLSettings()) {
		xmlFreeDoc(xmlConfigFilePtr);
		log_err("Invalid ACL configuration in file %s.", configfile);
		return 1;
	}

	// parse the login information
	if (readLoginControlSettings()) {
		xmlFreeDoc(xmlConfigFilePtr);
		log_err("Invalid login configuration in file %s.", configfile);
		return 1;
	}

	xmlFreeDoc(xmlConfigFilePtr);

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Backs up the current configuration file then returns the new file name 
 * Input: The current configuration filename and the new configuration filename where
 * 	  it was backed up 
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ July 24, 2008   
 * -------------------------------------------------------------------------------------*/
int 
backupConfigFile(char * configFile, char * scratchDir) {
	// get time information
	time_t rawtime;
	struct tm* timeinfo;
        char backupFile[FILENAME_MAX_CHARS];
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	// construct backup config file name
        strcpy(backupFile, scratchDir);
        char *file = configFile;
        char *dirend =  strrchr(configFile,'/');
        if(dirend != NULL){
          file = dirend+1;
        }
        dirend = backupFile + strlen(backupFile);
        if(dirend != backupFile){
          dirend[0] = '/';
          dirend +=1;
        }
 
	int pch = 0;
	pch += sprintf(dirend, "%d", (timeinfo->tm_hour) + 1);
	pch += sprintf(&dirend[pch], "%d_", timeinfo->tm_min);
	pch += sprintf(&dirend[pch], "%d", (timeinfo->tm_mon) + 1);
	pch += sprintf(&dirend[pch], "%d", timeinfo->tm_mday);
	pch += sprintf(&dirend[pch], "%d_", (timeinfo->tm_year) + 1900);

	strcat(backupFile, file); 

	// open config file and backup config file
	FILE * configFilePtr;
	FILE * backupFilePtr;
	char buffer[100];

	// initialize file handles
	configFilePtr = fopen(configFile, "r");
	if(configFilePtr==NULL) {
		log_warning("Error opening config file for backup [%s]: %s", configFile, strerror(errno));
		return 1;
	}
	backupFilePtr = fopen(backupFile, "w");
        if(backupFilePtr == NULL){
          log_warning("Error opening backup config file [%s]: %s", backupFile, strerror(errno));
          return 1;
        }

	// write contents from config file to backup config file
	while(fgets(buffer, 100, configFilePtr)!=NULL){
		fprintf(backupFilePtr, "%s", buffer); 
        }

	// close file handles
	fclose(configFilePtr);
	fclose(backupFilePtr);

	return 0;
}

/*------------------------------------------------------------------------------
 * Purpose: Backs up the current configuration file then returns the new file
 *          name 
 * Input:   The current configuration filename and the new configuration 
 *          filename where it was backed up 
 * Output:  0 for success or 1 for failure
 * Note:        
 * Kevin Burnett @ July 24, 2008   
 * Cathie Olschanowsky @ June 2012
 * ---------------------------------------------------------------------------*/
int saveConfigFile(char * configFile) {
	int err = 0;

	xmlConfigWriter = xmlNewTextWriterFilename(configFile, 0);
	if(xmlConfigWriter==NULL) {
	  log_err("Unable to create config file: %s\n", configFile);
          return 1;
	}

	xmlTextWriterSetIndent(xmlConfigWriter, 1);
	xmlTextWriterSetIndentString (xmlConfigWriter, BAD_CAST "\t");

	// create the document
	if(xmlTextWriterStartDocument(xmlConfigWriter, "1.0", NULL, NULL)) {
		log_warning("Unable to create config file: %s\n", configFile);
		return 1;
	}

	// create the root element
	if(openConfigElement(XML_BGPMON_TAG)) {
		err = 1;
		log_warning("Unable to write root tag in file: %s\n", configFile);
	}

	// save the peer settings
	if(savePeersSettings()) {
		err = 1;
		log_warning("Unable to save peer settings in file %s.", configFile);
	}

	// save the peer group settings
	if(savePeerGroupsSettings()) {
		err = 1;
		log_warning("Unable to save peer group settings in file %s.", configFile);
	}	

	// save the Login settings
	if(saveLoginControlSettings()) {
		err = 1;
		log_warning("Unable to save Login settings in file %s.", configFile);
	}

	// save the acl settings
	if(saveACLSettings()) {
		err = 1;
		log_warning("Unable to save ACL settings in file %s.", configFile);
	}

	// save the Queue settings
	if(saveQueueSettings()) {
		err = 1;
		log_warning("Unable to save Queue settings in file %s.", configFile);
	}

	// save the Chain settings
	if(saveChainsSettings()) {
		err = 1;
		log_warning("Unable to save Chain settings in file %s.", configFile);
	}

	// save the Client settings
	if(saveClientsControlSettings()) {
		err = 1;
		log_warning("Unable to save Client control settings in file %s.", configFile);
	}

	// save the mrt settings
	if(saveMrtControlSettings()) {
		err = 1;
		log_warning("Unable to save mrt control settings in file %s.", configFile);
	}
	
	// save the Client settings
	if(savePeriodicSettings()) {
		err = 1;
		log_warning("Unable to save periodic module settings in file %s.", configFile);
	}

	// close the root element
	if(closeConfigElement()) {
		err = 1;
		log_warning("Unable to close root tag in file: %s\n", configFile);
	}

	// close the document
	if (xmlTextWriterEndDocument(xmlConfigWriter) == -1) {
		err = 1;
		log_warning("Unable to close config file: %s\n", configFile);
	}

	// free the writer object
	xmlFreeTextWriter(xmlConfigWriter);

	return err;
}

/*----------------------------------------------------------------------------------------
 *
 * Purpose: Reads a string value from the current configuration file
 * Input: value - a pointer to char*, used to return the value
 *		xpath - a x-path pointing towards the value needed
 * 		maxLength - the maximum length of a char* to be returned
 * Output: CONFIG_VALID_ENTRY means a valid config entry,
 *		  CONFIG_NO_ENTRY means not found config entry,
 *		  CONFIG_INVALID_ENTRY means a invalid config entry
 * Note:        
 * Kevin Burnett @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
int getConfigValueAsString( char **value, char *xpath, int maxLength ) {
	*value = NULL;
	if(xmlConfigFilePtr!=NULL) {
		*value = getValueAsString(xmlConfigFilePtr, xpath, maxLength);
		if (*value != NULL)
			return CONFIG_VALID_ENTRY;
		else
			return CONFIG_NO_ENTRY;
	}
	else {
		log_fatal("Configuration file not open.  Cannot read values from it.");
		return CONFIG_NO_ENTRY;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads an integer value from the current configuration file
 * Input: value - a pointer to int, used to return the value
 * 		xpath - a x-path pointing towards the value needed
 * 		min_value - the minimum value to be returned
 * 		max_value - the maximum value to be returned
 * Output:  CONFIG_VALID_ENTRY means a valid config entry,
 *		   CONFIG_NO_ENTRY means not found config entry,
 *		   CONFIG_INVALID_ENTRY means a invalid config entry
 * Note:        
 * Kevin Burnett @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
int getConfigValueAsInt( int *value, char * xpath, int min_value, int max_value) {
	if(xmlConfigFilePtr!=NULL) {
		return getValueAsInt( value, xmlConfigFilePtr, xpath, min_value, max_value);
	}
	else {
		log_fatal("Configuration file not open.  Cannot read values from it.");
		return CONFIG_NO_ENTRY;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads an double value from the current configuration file
 * Input: value - a pointer to float, used to return the value
 * 		xpath - a x-path pointing towards the value needed
 * 		min_value - the minimum value to be returned
 * 		max_value - the maximum value to be returned
 * Output:  CONFIG_VALID_ENTRY means a valid config entry,
 *		   CONFIG_NO_ENTRY means not found config entry,
 *		   CONFIG_INVALID_ENTRY means a invalid config entry     
 * Kevin Burnett @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
int  getConfigValueAsFloat( float *value, char * xpath, float min_value, float max_value) {
	if(xmlConfigFilePtr!=NULL) {
		return getValueAsFloat( value, xmlConfigFilePtr, xpath, min_value, max_value);
	}
	else {
		log_fatal("Configuration file not open.  Cannot read values from it.");
		return CONFIG_NO_ENTRY;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads address as a string from the current configuration file
 * 		    The address in the configuration file should one of them:
 *			1. IPv4 address (1.2.3.4)
 *			2. IPv6 address (fe80::211:11ff:fea1:6361%eth0)
 *			3. IPv4 any address (ipv4any)
 *			4. IPv6 any address (ipv6any)
 *			5. IPv4 loopback address (ipv4loopback) = 127.0.0.1
 *			6. IPv6 loopback address (ipv6loopback) = ::1
 * Input: addr - a pointer to char*,  used to return the value
 		xpath - a x-path pointing towards the value needed
 * 		 passive - ADDR_PASSIVE(1) means passive address which must be one of local addresses or addrany.
 *			   	   ADDR_ACTIVE(0) means active address which cannot be addrany.	 
 *
 * Output:  CONFIG_VALID_ENTRY means a valid config entry,
 *		   CONFIG_NO_ENTRY means not found config entry,
 *		   CONFIG_INVALID_ENTRY means a invalid config entry 
 * Note:        
 * Kevin Burnett @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
int getConfigValueAsAddr( char **addr, char * xpath, int passive ) {
	int result;
	*addr = NULL;
	if(xmlConfigFilePtr!=NULL) {
		result = getConfigValueAsString( addr, xpath, ADDR_MAX_CHARS);
		if(result != CONFIG_NO_ENTRY)
		{
			int addrResult = checkAddress( *addr, passive );
			if( addrResult == ADDR_VALID )
				return CONFIG_VALID_ENTRY;
			else
			{
				*addr = NULL;
				return CONFIG_INVALID_ENTRY;
			}
		}
		else
			return CONFIG_NO_ENTRY;
	}
	else {
		log_fatal("Configuration file not open.  Cannot read values from it.");
		return CONFIG_NO_ENTRY;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads a string value from a node in a node list
 * Input: value - a pointer to char*, used to return the value
 *		xpath - a x-path pointing towards the node list
 *		tag -  the node's tag	
 *		item - the index of the node	
 * 		maxLength - the maximum length of a char* to be returned
 * Output: CONFIG_VALID_ENTRY means a valid config entry,
 *		  CONFIG_NO_ENTRY means not found config entry,
 *		  CONFIG_INVALID_ENTRY means a invalid config entry
 * Note:        
 * Kevin Burnett @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
int getConfigValueFromListAsString(char **value, char *xpath, char *tag, int item, int maxLength) {
	*value = NULL;
	if(xmlConfigFilePtr!=NULL) {
		*value = getValueFromListAsString(xmlConfigFilePtr, xpath, tag, item, maxLength);
		if (*value != NULL)
			return CONFIG_VALID_ENTRY;
		else
			return CONFIG_NO_ENTRY;
	}
	else {
		log_fatal("Configuration file not open.  Cannot read values from it.");
		return CONFIG_NO_ENTRY;
	}	
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads a int value from a node in a node list
 * Input: value - a pointer to int, used to return the value
 *		xpath - a x-path pointing towards the node list
 *		tag -  the node's tag	
 *		item - the index of the node	
 * 		min_value - the minimum value to be returned
 * 		max_value - the maximum value to be returned
 * Output: CONFIG_VALID_ENTRY means a valid config entry,
 *		  CONFIG_NO_ENTRY means not found config entry,
 *		  CONFIG_INVALID_ENTRY means a invalid config entry
 * Note:        
 * Kevin Burnett @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
int getConfigValueFromListAsInt(int *value, char * xpath, char *tag, int item, int min_value, int max_value) {
	if(xmlConfigFilePtr!=NULL) {
		return getValueFromListAsInt(value, xmlConfigFilePtr, xpath, tag, item, min_value, max_value);
	}
	else {
		log_fatal("Configuration file not open.  Cannot read values from it.");
		return CONFIG_NO_ENTRY;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads a int value from a node in a node list
 * Input: value - a pointer to int, used to return the value
 *		xpath - a x-path pointing towards the node list
 *		tag -  the node's tag	
 *		item - the index of the node	
 * 		min_value - the minimum value to be returned
 * 		max_value - the maximum value to be returned
 * Output: CONFIG_VALID_ENTRY means a valid config entry,
 *		  CONFIG_NO_ENTRY means not found config entry,
 *		  CONFIG_INVALID_ENTRY means a invalid config entry
 * Note:        
 * Kevin Burnett @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
int getConfigValueFromListAsInt32(u_int32_t *value, char * xpath, char *tag, int item, u_int32_t min_value, u_int32_t max_value) {
	if(xmlConfigFilePtr!=NULL) {
		return getValueFromListAsInt32(value, xmlConfigFilePtr, xpath, tag, item, min_value, max_value);
	}
	else {
		log_fatal("Configuration file not open.  Cannot read values from it.");
		return CONFIG_NO_ENTRY;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads address as a string from node in a node list
 * 		    The address in the configuration file should one of them:
 *			1. IPv4 address (1.2.3.4)
 *			2. IPv6 address (fe80::211:11ff:fea1:6361%eth0)
 *			3. IPv4 any address (ipv4any)
 *			4. IPv6 any address (ipv6any)
 *			5. IPv4 loopback address (ipv4loopback) = 127.0.0.1
 *			6. IPv6 loopback address (ipv6loopback) = ::1
 * Input:  addr - a pointer to char*,  used to return the value
 *		 xpath - a x-path pointing towards the node list
 *		 tag -  the node's tag	
 *		 item - the index of the node			
 * 		 passive - ADDR_PASSIVE(1) means passive address which must be one of local addresses or addrany.
 *			   	   ADDR_ACTIVE(0) means active address which cannot be addrany.	 
 *
 * Output:  CONFIG_VALID_ENTRY means a valid config entry,
 *		   CONFIG_NO_ENTRY means not found config entry,
 *		   CONFIG_INVALID_ENTRY means a invalid config entry 
 * Note:        
 * Kevin Burnett @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
int getConfigValueFromListAsAddr( char **addr, char * xpath, char *tag, int item, int passive ) {
	int result;
	*addr = NULL;
	if(xmlConfigFilePtr!=NULL) {
		result = getConfigValueFromListAsString( addr, xpath, tag, item, ADDR_MAX_CHARS);
		if(result != CONFIG_NO_ENTRY)
		{
			int addrResult = checkAddress( *addr, passive );
			if( addrResult == ADDR_VALID )
				return CONFIG_VALID_ENTRY;
			else
			{
				*addr = NULL;
				return CONFIG_INVALID_ENTRY;
			}
		}
		else
			return CONFIG_NO_ENTRY;
	}
	else {
		log_fatal("Configuration file not open.  Cannot read values from it.");
		return CONFIG_NO_ENTRY;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Reads the count of elements which have the same xpath
 * Input: a x-path pointing towards the node list
 * Output: returns the count of the list
 * Note:        
 * Kevin Burnett @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
int getConfigListCount(char * xpath) {
	if(xmlConfigFilePtr!=NULL) {
		return getListCount(xmlConfigFilePtr, xpath);
	}
	else {
		log_fatal("Configuration file not open.  Cannot read values from it.");
		return -1;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: 
 * Input: 
 * Output: 
 * Note:        
 * Kevin Burnett @ September 11, 2008   
 * -------------------------------------------------------------------------------------*/
void getAddressString( BGPmonAddr * b_addr, char * output ) {

	char * s = NULL;
	struct sockaddr_in * v4 = NULL;

	//printf("ai_family: %d\n", b_addr->addr->ai_family);
	//switch(b_addr->addr->ai_family) {
	//	case AF_INET:
		      	v4 = (struct sockaddr_in*)b_addr->addr;
	      		s = inet_ntoa(v4->sin_addr);
	      		strcpy(output, s);
	//}

}

/*----------------------------------------------------------------------------------------
 * Purpose: Writes an string value to the configuration
 * Input: 
 * Output: 
 * Note:        
 * Kevin Burnett @ September 17, 2008   
 * -------------------------------------------------------------------------------------*/
int setConfigValueAsString( char * tag, char * value) {
	int rc = 0;
	rc = xmlTextWriterWriteElement(xmlConfigWriter, BAD_CAST tag, BAD_CAST value);
	return rc>0 ? 0 : -1;
}

/*----------------------------------------------------------------------------------------
 * Purpose: writes the an integer value into the node specified in the xpath
 * Input: 
 * Output: 
 * Note:        
 * Kevin Burnett @ September 17, 2008   
 * -------------------------------------------------------------------------------------*/
int setConfigValueAsInt(char * tag, int value) {
	int rc = 0;
	rc = xmlTextWriterWriteFormatElement(xmlConfigWriter, BAD_CAST tag, "%d", value);
	return rc>0 ? 0 : -1;
}

/*----------------------------------------------------------------------------------------
 * Purpose: writes the an float value into the node specified in the xpath
 * Input: 
 * Output: 
 * Note:        
 * Kevin Burnett @ September 17, 2008   
 * -------------------------------------------------------------------------------------*/
int setConfigValueAsFloat( char * tag, float value) {
	int rc = 0;
	rc = xmlTextWriterWriteFormatElement(xmlConfigWriter, BAD_CAST tag, "%f", value);
	return rc>0 ? 0 : -1;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Opens an XML tag in the configuration file
 * Input: 
 * Output: 
 * Note:        
 * Kevin Burnett @ September 17, 2008   
 * -------------------------------------------------------------------------------------*/
int openConfigElement(const char * tag) {
	int rc = 0;
	rc = xmlTextWriterStartElement(xmlConfigWriter, BAD_CAST tag);
	return rc>=0 ? 0 : -1;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Closes an XML tag in the configuration file
 * Input: 
 * Output: 
 * Note:        
 * Kevin Burnett @ September 17, 2008   
 * -------------------------------------------------------------------------------------*/
int closeConfigElement() {
	int rc = 0;
	rc = xmlTextWriterEndElement(xmlConfigWriter);
	return rc>=0 ? 0 : -1;
}


/*----------------------------------------------------------------------------------------
 * Purpose: Convert a hex string into an array of u_char 
 * Input:	hex string, its length
 * Output: Pointer to an array of u_char, note callerneeds to free it
 *		  NULL if the hex string is invalid.		
 * He Yan @ August 24, 2008   
 * ------------------------------------------------------------------------------------*/
u_char *hexStringToByteArray( char *hexString )
{
 	int len = strlen(hexString);
	if( len%2 == 1 )
	{
		return NULL;
	}

	// allocate memory for the output array
	u_char *array = malloc(len/2);

	int i;
	for( i=0; i < len/2; i++ )
	{
		char ch1 = hexString[i*2];
		char ch2 = hexString[i*2+1];
		int dig1, dig2;
		if( ch1>='0' && ch1<='9' ) 
			dig1 = ch1 - '0';
		else if(ch1>='A' && ch1<='F') 
			dig1 = ch1 - 'A' + 10;
		else if(ch1>='a' && ch1<='f') 
			dig1 = ch1 - 'a' + 10;
		else
		{
			free(array);
			return NULL;
		}
		if( ch2>='0' && ch2<='9' ) 
			dig2 = ch2 - '0';
		else if(ch2>='A' && ch2<='F') 
			dig2 = ch2 - 'A' + 10;
		else if(ch2>='a' && ch2<='f') 
			dig2 = ch2 - 'a' + 10;
		else
		{
			free(array);
			return NULL;
		}
		
		array[i] = dig1*16 + dig2;
	}
	return array;	
}


/*----------------------------------------------------------------------------------------
 * Purpose: Convert an array of u_char into a hex string
 * Input:	a pointer of u_char and len
 * Output: a hex string, note callerneeds to free it	
 * He Yan @ August 24, 2008   
 * ------------------------------------------------------------------------------------*/
char *byteArrayToHexString( u_char *data, int len )
{
	// allocate memory for the output array
	char *hexString = malloc(2*len + 1);
	int i;
	for (i=0; i<len; i++) 
	{
        sprintf(hexString+i*2, "%02X", data[i] & 0xff);
    }
	hexString[2*len + 1] = 0;
	return hexString;	
}
