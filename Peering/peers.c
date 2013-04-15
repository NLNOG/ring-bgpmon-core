/* 
 * 	Copyright (c) 2010 Colorado State University
 * 
 *	Permission is hereby granted, free of charge, to any person
 *  	obtaining a copy of this software and associated documentation
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
 *  	File: peers.c
 *  	Authors:  He Yan, Dan Massey
 *  	Date: Oct 13, 2008 
 */

#pragma mark -
/* externally visible structures and functions for peers */
#include "peers.h"

/* required for logging functions */
#include "../Util/log.h"
/* needed for reading and saving configuration */
#include "../Config/configdefaults.h"
#include "../Config/configfile.h"

/* needed for ADDR_MAX_CHARS and MAX_PEER_IDS */ 
#include "../site_defaults.h"

/* required for TRUE/FALSE defines  */
#include "../Util/bgpmon_defaults.h"

/* needed for address management  */
#include "../Util/address.h"

/* needed for session function */
#include "peersession.h"

/* needed for malloc and free */
#include <stdlib.h>
/* needed for strncpy */
#include <string.h>
/* needed for system error codes */
#include <errno.h>
/* needed for system types such as time_t */
#include <sys/types.h>
/* needed for time function */
#include <time.h>
/* needed for pthread related functions */
#include <pthread.h>
/* needed for generatePeeringString */
#include "../XML/xml.h"
/* needed for label action*/
#include "../Labeling/label.h"
/* needed for bgp states*/
#include "bgpstates.h"
/* needed for peer group's function*/
#include "peergroup.h"
/* needed for LOG_INFO*/
#include <syslog.h>
#pragma mark -

//#define DEBUG

/*--------------------------------------------------------------------------------------
 * Purpose: Initialize the default peers configuration.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int initPeersSettings()
{
	// initialize the list of peers
	int i;
	for( i = 0; i < MAX_PEER_IDS; i++)
		Peers[i] = NULL;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Read the peers settings from the config file.
 * Input: none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readPeersSettings()
{	
  debug(__FUNCTION__, "readPeersSettings" );

  int 	err = 0;	// indicates whether read was successful
  int 	valid;		// is the current peer configuration valid
	
  // get the count of peers
  int peersCount = getConfigListCount(XML_PEERS_PATH);
  #ifdef DEBUG
  debug(__FUNCTION__, "found %d configured peers", peersCount);
  #endif
	
  // check if the count of peers exceeds MAX_PEER_IDS. If so, trim it.
  if(peersCount > MAX_PEER_IDS){
    peersCount = MAX_PEER_IDS;
    log_warning("Configured peers (%d) exceeds maximum allowable number of peers (%d).", peersCount, MAX_PEER_IDS);
    log_warning("using only first %d configured peers", MAX_PEER_IDS);	
  }

  // read each peer's configuration and create the peer structure
  int i;
  for(i = 0; i < peersCount; i++){
    PConfiguration peerConf;
    valid = readPeerConf( &peerConf, XML_PEERS_PATH, i );
    
    // if peer configuration is valid, create peer struct
    if (valid == TRUE){
      //create the peer structure
      int newPeerID = createPeerStruct(peerConf);
      if (newPeerID == -1) { 
					err = 1;
					log_warning("Unable to create structure for peer %d", i);
      }else{
				
        log_msg("created new peer with id (%d)", newPeerID);
        #ifdef DEBUG
        printPeerSettings(newPeerID);
        #endif
      }
    }else{
				err = 1;
    }
  }
  return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Read the peers settings from the config file.
 * Input: a double poniter to the returned peer configuration structure
 *		xpath to a peer or peer group
 *		the index of the element to read
 * Output: TRUE means success, FALSE means failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int readPeerConf( PConfiguration *conf, char * xpath, int i)
{
	int 	result; 	// indicates whether XML read was successful
	char	*addr;		// used to store addresses from XML config
	char	*md5Passwd;		// used to store MD5 password from XML config
	char	*groupName;		// used to store peer's group name
	int 	port;		// used to store port from XML config
	int 	version;	// used to store version from XML config
	u_int32_t AS;			// used to store AS number from XML config
	int 	holdtime;	// used to store BGP hold time from XML config
	char 	*BGPID;		// used to store BGP identification from XML config
	int 	labelAction; // used to store label action from XML config
	int 	rrAction; // used to store route refresh action from XML config
	int 	enabled;	// used to store enabled status from XML config
	int 	valid;		// is the current peer configuration valid
	
	PConfiguration peerConf = createPeerConf( NULL, 0 );
	if( peerConf == NULL ){
		return -1;
	}
	
	// initially, peer configuration is valid
	valid = TRUE;

	/****************************************
	 * read local Settings
	 ****************************************/
	// get local addr
	addr = NULL;
	result = getConfigValueFromListAsAddr(&addr, xpath, XML_MONITOR_ADDR, i, ADDR_PASSIVE);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d local address.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration peer configuration's local address.", i);
	else
		strncpy(peerConf->localSettings.addr, addr, ADDR_MAX_CHARS);

	// free adress if defined and get the next peer
	if (addr != NULL) 
		free(addr);	
	

	// get local port
	result = getConfigValueFromListAsInt(&port, xpath, XML_MONITOR_PORT, i, 1, 65536);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d local port.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's local port.", i);
	else
		peerConf->localSettings.port = port;

	// get local BGP version
	result = getConfigValueFromListAsInt(&version, xpath, XML_MONITOR_VERSION, i, 1, 65536);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d local BGP version.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's local BGP version.", i);
	else
		peerConf->localSettings.BGPVer = version;

	// get local BGP AS number
	result = getConfigValueFromListAsInt32(&AS, xpath, XML_MONITOR_AS, i, 1, 4294967295U);
	if ( result == CONFIG_INVALID_ENTRY )	
	{
		log_warning("Invalid configuration of peer %d local BGP AS number.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's localBGP AS number.", i);			
	else
		peerConf->localSettings.AS2 = AS;

	// get announced BGP hold time
	result = getConfigValueFromListAsInt(&holdtime, xpath, XML_ANNOUNCE_HOLDTIME, i, 1, 65536);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d local BGP hold time.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's local BGP hold time.", i);
	else
		peerConf->localSettings.holdtime = holdtime;

	// get local BGP ID
	BGPID = NULL;
	result = getConfigValueFromListAsAddr(&BGPID, xpath, XML_MONITOR_ID, i, ADDR_ACTIVE);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d local BGP ID.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's local BGP ID.", i);
	else
		peerConf->localSettings.BGPID = inet_addr(BGPID);

	// free BGPID if defined and get the next peer
	if (BGPID != NULL) 
		free(BGPID); 	

	/****************************************
	 *Read Remote Settings
	 ****************************************/
	if( strcmp(xpath,  XML_PEERS_PATH) == 0 )
	{
		// get remote addr
		addr = NULL;
		result = getConfigValueFromListAsAddr(&addr, xpath, XML_PEER_ADDR, i, ADDR_ACTIVE);
		if ( ( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) ) 
		{
			log_warning("Invalid configuration of peer %d remote address.", i);
			valid = FALSE;
		}
		else
			strncpy(peerConf->remoteSettings.addr, addr, ADDR_MAX_CHARS);

		// free adress if defined and get the next peer
		if (addr != NULL) 
			free(addr); 	
		

		// get remote port
		result = getConfigValueFromListAsInt(&port, xpath, XML_PEER_PORT, i, 1, 65536);
		if ( ( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) ) 
		{
			log_warning("Invalid configuration of peer %d remote port.", i);
			valid = FALSE;
		}
		else
			peerConf->remoteSettings.port = port;
	}

	// get remote BGP version
	result = getConfigValueFromListAsInt(&version, xpath, XML_PEER_VERSION, i, 1, 65536);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d remote BGP version.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for for peer configuraion's remote BGP version.", i);
	else
		peerConf->remoteSettings.BGPVer = version;

	// get peer BGP AS number
	result = getConfigValueFromListAsInt32(&AS, xpath, XML_PEER_AS, i, 1, 4294967295U);
	if ( result == CONFIG_INVALID_ENTRY )	
	{
		log_warning("Invalid configuration of peer %d remote BGP AS number.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's remote BGP AS number.", i);
	else
		peerConf->remoteSettings.AS2 = AS;

	// get min required peer's BGP hold time
	result = getConfigValueFromListAsInt(&holdtime, xpath, XML_MIN_HOLDTIME, i, 1, 65536);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d min hold time.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's min hold time.", i);
	else
		peerConf->remoteSettings.MinHoldtime = holdtime;
	
	// get peer's BGP ID
	BGPID = NULL;
	result = getConfigValueFromListAsAddr(&BGPID, xpath, XML_PEER_ID, i, ADDR_ACTIVE);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d router BGP ID.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's remote BGP ID.", i);
	else
		peerConf->remoteSettings.BGPID = inet_addr(BGPID);		

	// free BGPID if defined and get the next peer
	if (BGPID != NULL) 
		free(BGPID);	

	// get peer's MD5 passwd
	md5Passwd = NULL;
	result = getConfigValueFromListAsString(&md5Passwd, xpath, XML_PEER_MD5_PASS, i, PASSWORD_MAX_CHARS);
	if( result == CONFIG_VALID_ENTRY ) 
		strncpy(peerConf->remoteSettings.md5Passwd, md5Passwd, PASSWORD_MAX_CHARS);
	// free md5Passwd if defined and get the next peer
	if (md5Passwd != NULL) 
		free(md5Passwd); 			


	/****************************************
	 * Read Label Related Settings
	 ****************************************/
	// get label action
	result = getConfigValueFromListAsInt(&labelAction, xpath, XML_LABEL_ACTION, i, 0, 2);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d label action.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's label action.", i);
	else
		peerConf->labelAction = labelAction;

	/****************************************
	 * Read Router Refresh Related Settings
	 ****************************************/
	// get router refresh action
	rrAction = 0;
	result = getConfigValueFromListAsInt(&rrAction, xpath, XML_RR_ACTION, i, 0, 1);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d route refresh action.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's route refresh action", i);		
	else
		peerConf->routeRefreshAction = rrAction;

	/****************************************
	 * Announced Capabilities
	 ****************************************/
	char capXpath[255];
	int	capXpathLen;
	memset(capXpath, 0, 255);
	strcat(capXpath, xpath);
	strcat(capXpath, "[");
	capXpathLen = strlen(capXpath);
	sprintf(&capXpath[capXpathLen], "%d", i + 1);
	strcat(capXpath, "]/"XML_ANNOUNCE_CAP_LIST"/"XML_CAPABILITY);  

	// get the number of announced capabiilities
	int annCapsCount = getConfigListCount(capXpath);

	// loop through each cap
	int j;
	for( j=0; j<annCapsCount; j++ )
	{
		int capValid = TRUE;
		int code;
		result = getConfigValueFromListAsInt(&code, capXpath, XML_CAPABILITY_CODE, j, 1, 255);
		if ( ( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) )	
		{
			log_warning("Invalid configuration of %d announce capability code of peer %d.", j, i);
			capValid = FALSE;
		}

		int length;
		char *hexString = NULL;
		u_char *value = NULL;
		result = getConfigValueFromListAsInt(&length, capXpath, XML_CAPABILITY_LENGTH, j, 1, 255);
		if ( result == CONFIG_INVALID_ENTRY )  
		{
			log_warning("Invalid configuration of %d announce capability length of peer %d.", j, i);
			capValid = FALSE;
		}
		else if( result == CONFIG_NO_ENTRY )
		{
			length = 0;
			value = NULL;
		}	
		else
		{
			result = getConfigValueFromListAsString(&hexString, capXpath, XML_CAPABILITY_VALUE, j, 255);
			if ( ( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) )	
			{
				log_warning("Invalid configuration of %d announce capability value of peer %d.", j, i);
				capValid = FALSE;
			}

			// check the len of hex string
			if( strlen(hexString)/2 != length || strlen(hexString)%2 == 1 )
			{
				log_warning("Invalid configuration of %d announce capability value of peer %d.", j, i);
				capValid = FALSE;
			}	
								
			// convert the hex string to a byte array
			value = hexStringToByteArray(hexString);
			if( value == NULL )
			{
				log_warning("Invalid configuration of %d announce capability value of peer %d.", j, i);
				capValid = FALSE;
			}		
					
			if (hexString != NULL) 
				free(hexString); 					
		}	

		if( capValid == TRUE )
		{
#ifdef DEBUG
			debug(__FUNCTION__, "Peer %d's %d annouce capability: code %d, len %d", 
					i, j, code, length);
			hexdump(LOG_INFO, value, length);
#endif					
			peerConf->announceCaps[peerConf->numOfAnnCaps] = createGenericBGPCapability(code, length, value);
			peerConf->numOfAnnCaps++;
			if (value != NULL) 
				free(value);
		}
		else
			valid = FALSE;
	}


	/****************************************
	 * Receive Capabilitie Requirement List
	 ****************************************/
	memset(capXpath, 0, 255);
	strcat(capXpath, xpath );
	strcat(capXpath, "[");
	capXpathLen = strlen(capXpath);
	sprintf(&capXpath[capXpathLen], "%d", i + 1);
	strcat(capXpath, "]/"XML_RECEIVE_CAP_REQ_LIST"/"XML_CAPABILITY);	

	// get the number of announced capabiilities
	int revCapsCount = getConfigListCount(capXpath);

	// loop through each cap
	for( j=0; j<revCapsCount; j++ )
	{
		int capValid = TRUE;
		int code;
		result = getConfigValueFromListAsInt(&code, capXpath, XML_CAPABILITY_CODE, j, 1, 255);
		if ( ( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) )	
		{
			log_warning("Invalid configuration of %d receive capability code of peer %d.", j, i);
			capValid = FALSE;
		}

		int action;
		result = getConfigValueFromListAsInt(&action, capXpath, XML_CAPABILITY_ACTION, j, 0, 2);
		if ( ( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) )	
		{
			log_warning("Invalid configuration of %d receive capability action of peer %d.", j, i);
			capValid = FALSE;
		}

		int length;
		char *hexString = NULL;
		u_char *value = NULL;
		result = getConfigValueFromListAsInt(&length, capXpath, XML_CAPABILITY_LENGTH, j, 0, 255);
		if ( result == CONFIG_INVALID_ENTRY )  
		{
			log_warning("Invalid configuration of %d receive capability length of peer %d.", j, i);
			capValid = FALSE;
		}
		else if( result == CONFIG_NO_ENTRY )
		{
			length = -1;
			value = NULL;
		}	
		else
		{	
			if( length > 0 )
			{
				result = getConfigValueFromListAsString(&hexString, capXpath, XML_CAPABILITY_VALUE, j, 255);
				if ( ( result == CONFIG_INVALID_ENTRY ) || ( result == CONFIG_NO_ENTRY ) )	
				{
					log_warning("Invalid configuration of %d receive capability value of peer %d.", j, i);
					capValid = FALSE;
				}

				// check the len of hex string
				if( strlen(hexString)/2 != length || strlen(hexString)%2 == 1 )
				{
					log_warning("Invalid configuration of %d receive capability value of peer %d.", j, i);
					capValid = FALSE;
				}	
									
				// convert the hex string to a byte array
				value = hexStringToByteArray(hexString);
				if( value == NULL )
				{
					log_warning("Invalid configuration of %d receive capability value of peer %d.", j, i);
					capValid = FALSE;
				}		
						
				if (hexString != NULL) 
					free(hexString);	
			}
			else
				value = NULL;
		}	

		if( capValid == TRUE )
		{
#ifdef DEBUG
			debug(__FUNCTION__, "Peer %d's %d receive capability requirement: code %d, len %d, action %d", 
					i, j, code, length, action);
			hexdump(LOG_INFO, value, length);
#endif					
			peerConf->receiveCapRequirements[peerConf->numOfReceiveCaps].code = code;
			peerConf->receiveCapRequirements[peerConf->numOfReceiveCaps].action = action;
			peerConf->receiveCapRequirements[peerConf->numOfReceiveCaps].checkValueLen = length;
			if(length > 0)
				memcpy(peerConf->receiveCapRequirements[peerConf->numOfReceiveCaps].checkValue, value, length);
			peerConf->receiveCapRequirements[peerConf->numOfReceiveCaps].flag = FALSE;
			peerConf->numOfReceiveCaps++;
			if (value != NULL) 
				free(value);
		}
		else
			valid = FALSE;		
	}

	// get enabled flag
	result = getConfigValueFromListAsInt(&enabled, xpath, XML_PEER_ENABLED, i, 0, 1);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d enabled flag.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration for peer configuraion's enable flag.", i);	
	else
		peerConf->enabled = enabled;

	// get group name
	groupName = NULL;
	result = getConfigValueFromListAsString(&groupName, xpath, XML_GROUP_NAME_VALUE, i, PEER_GROUP_MAX_CHARS);
	if ( result == CONFIG_INVALID_ENTRY ) 
	{
		log_warning("Invalid configuration of peer %d group name.", i);
		valid = FALSE;
	}
	else if( result == CONFIG_NO_ENTRY )
		log_msg("No peer configuration peer configuraion's group name.", i);
	else
	{
		strncpy(peerConf->groupName, groupName, PEER_GROUP_MAX_CHARS);
		peerConf->groupID = findPeerGroupID(groupName);
	}

	*conf = peerConf;
	return valid;
}
#pragma mark -
/*--------------------------------------------------------------------------------------
 * Purpose: print the peer settings
 * Input:  the peer ID
 * Output: NULL
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void printPeerSettings(int peerID)
{	
	debug(__FUNCTION__, "Peer %d: Local Addr: %s", peerID, getPeerLocalAddress(peerID));
	debug(__FUNCTION__, "Peer %d: Local port set to %d", peerID, getPeerLocalPort(peerID));
	debug(__FUNCTION__, "Peer %d: Local BGP version set to %d", peerID, getPeerLocalBGPVersion(peerID));
	debug(__FUNCTION__, "Peer %d: Local BGP AS number set to %d", peerID, getPeerLocalASNum(peerID));
	debug(__FUNCTION__, "Peer %d: Local BGP hold time set to %d", peerID, getPeerLocalHoldTime(peerID));
	debug(__FUNCTION__, "Peer %d: Local BGP ID set to %d", peerID, getPeerLocalBGPID(peerID));
	debug(__FUNCTION__, "Peer %d: remote Addr: %s", peerID, getPeerRemoteAddress(peerID));
	debug(__FUNCTION__, "Peer %d: remote port set to %d", peerID, getPeerRemotePort(peerID));
	debug(__FUNCTION__, "Peer %d: remote BGP version set to %d", peerID, getPeerRemoteBGPVersion(peerID));
	debug(__FUNCTION__, "Peer %d: remote BGP AS number set to %d", peerID, getPeerRemoteASNum(peerID));
	debug(__FUNCTION__, "Peer %d: remote min hold time set to %d", peerID, getPeerRemoteHoldTime(peerID));
	debug(__FUNCTION__, "Peer %d: remote BGP ID set to %d", peerID, getPeerRemoteBGPID(peerID));
	debug(__FUNCTION__, "Peer %d: MD5 passwd: %s", peerID, getPeerMD5Passwd(peerID) );
	debug(__FUNCTION__, "Peer %d: label action set to %d", peerID, getPeerLabelAction(peerID));
	debug(__FUNCTION__, "Peer %d: route refresh action set to %d", peerID, getPeerRouteRefreshAction(peerID));
	debug(__FUNCTION__, "Peer %d: group name set to %s", peerID, getPeer_GroupName(peerID));
	debug(__FUNCTION__, "Peer %d: group ID set to %d", peerID, findPeerGroupID(getPeer_GroupName(peerID)));
	if(getPeerEnabledFlag(peerID) == TRUE) 
		debug(__FUNCTION__, "Peer %d is enabled", peerID);
	else
		debug(__FUNCTION__, "Peer %d is disabled", peerID);
}

/*--------------------------------------------------------------------------------------
 * Purpose: Save one peer settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int saveOnePeerSetting(PConfiguration conf)
{
	int err = 0;
	// save local addr
	if(strlen(conf->localSettings.addr)!= 0)
	{
		if ( setConfigValueAsString(XML_MONITOR_ADDR, conf->localSettings.addr) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's local address to config file.");
		}
	}

	// save local port
	if(conf->localSettings.port != -1)
	{	
		if ( setConfigValueAsInt(XML_MONITOR_PORT, conf->localSettings.port) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's local port to config file.");
		}	
	}

	// save local BGP version
	if(conf->localSettings.BGPVer != -1)
	{
		if ( setConfigValueAsInt(XML_MONITOR_VERSION, conf->localSettings.BGPVer) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's local bgp version to config file.");
		}
	}

	// save local AS
	if(conf->localSettings.AS2 != -1)
	{	
		if ( setConfigValueAsInt(XML_MONITOR_AS, conf->localSettings.AS2) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's local AS to config file.");
		}		
	}

	// save announce holdtime
	if(conf->localSettings.holdtime != -1)
	{
		if ( setConfigValueAsInt(XML_ANNOUNCE_HOLDTIME, conf->localSettings.holdtime) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's announce holdtime to config file.");
		}		
	}

	// save local BGP ID
	if(conf->localSettings.BGPID != -1)
	{	
		struct in_addr INAddr;
		INAddr.s_addr = conf->localSettings.BGPID;
		if ( setConfigValueAsString(XML_MONITOR_ID, inet_ntoa(INAddr) ) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's local BGP ID to config file.");
		}
	}


	// save remote addr
	if(strlen(conf->remoteSettings.addr)!= 0)
	{	
		if ( setConfigValueAsString(XML_PEER_ADDR, conf->remoteSettings.addr) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's remote address to config file.");
		}
	}
	
	// save remote port
	if(conf->remoteSettings.port != -1)
	{		
		if ( setConfigValueAsInt(XML_PEER_PORT, conf->remoteSettings.port) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's remote port to config file.");
		}	
	}

	// save remote BGP version
	if(conf->remoteSettings.BGPVer != -1)
	{	
		if ( setConfigValueAsInt(XML_PEER_VERSION, conf->remoteSettings.BGPVer) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's remote bgp version to config file.");
		}
	}

	// save remote AS
	if(conf->remoteSettings.AS2 != -1)
	{		
		if ( setConfigValueAsInt(XML_PEER_AS, conf->remoteSettings.AS2) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's remote AS to config file.");
		}	
	}

	// save announce holdtime
	if(conf->remoteSettings.MinHoldtime != -1)
	{	
		if ( setConfigValueAsInt(XML_MIN_HOLDTIME, conf->remoteSettings.MinHoldtime) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's min holdtime to config file.");
		}		
	}
	
	// save remote BGP ID
	if(conf->remoteSettings.BGPID != -1)
	{		
		struct in_addr INAddr;
		INAddr.s_addr = conf->remoteSettings.BGPID;
		if ( setConfigValueAsString(XML_PEER_ID, inet_ntoa(INAddr) ))
		{
			err = 1;
			log_warning("Failed to save peer configuraion's remote BGP ID to config file.");
		}		
	}

	// save MD5 password
	if(strlen(conf->remoteSettings.md5Passwd)!= 0)
	{	
		if ( setConfigValueAsString(XML_PEER_MD5_PASS, conf->remoteSettings.md5Passwd) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's MD5 password to config file.");
		}
	}

	// save enabled flag
	if(conf->enabled != -1)
	{		
		if ( setConfigValueAsInt(XML_PEER_ENABLED, conf->enabled) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's enabled flag to config file.");
		}		
	}

	// save route refresh action
	if(conf->routeRefreshAction != -1)
	{		
		if ( setConfigValueAsInt(XML_RR_ACTION, conf->routeRefreshAction) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's route refresh action to config file.");
		}		
	}

	// save label action
	if(conf->labelAction != -1)
	{		
		if ( setConfigValueAsInt(XML_LABEL_ACTION, conf->labelAction) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's label action to config file.");
		}		
	}

	// save announcing capabilities
	if(conf->numOfAnnCaps> 0)
	{
		// open announcing cap list
		if (openConfigElement(XML_ANNOUNCE_CAP_LIST)) 
		{
			err = 1;
			log_warning("Failed to save peer configuraion's announcing cap list to config file.");
		}

		// loop through each cap
		int i;
		for( i=0; i<conf->numOfAnnCaps; i++ )
		{
			// open a capability
			if (openConfigElement(XML_CAPABILITY)) 
			{
				err = 1;
				log_warning("Failed to save peer configuraion's announcing cap list to config file.");
			}

			// save the cap code
			if ( setConfigValueAsInt(XML_CAPABILITY_CODE, conf->announceCaps[i]->code) )
			{
				err = 1;
				log_warning("Failed to save peer configuraion's %d cap to config file.", i);
			}

			// save the cap len and value
			if ( conf->announceCaps[i]->length != 0 )
			{
				char *tmp = byteArrayToHexString(conf->announceCaps[i]->value, 
					conf->announceCaps[i]->length);
				
				if ( setConfigValueAsInt(XML_CAPABILITY_LENGTH, conf->announceCaps[i]->length) )
				{
					err = 1;
					log_warning("Failed to save peer configuraion's %d cap to config file.", i);
				}

				if ( setConfigValueAsString(XML_CAPABILITY_VALUE, tmp) )
				{
					err = 1;
					log_warning("Failed to save peer configuraion's %d cap to config file.", i);
				}				
				free(tmp);
			}

			// close a capability
			if (closeConfigElement(XML_CAPABILITY)) 
			{
				err = 1;
				log_warning("Failed to save peer configuraion's announcing cap list to config file.");
			}			
		}
		
		// close announcing cap list
		if (closeConfigElement(XML_ANNOUNCE_CAP_LIST)) 
		{
			err = 1;
			log_warning("Failed to save peer configuraion's announcing cap list to config file.");
		}	
	}

	// save requirements of receiving capabilities
	if(conf->numOfReceiveCaps> 0)
	{
		// open receiving cap list
		if (openConfigElement(XML_RECEIVE_CAP_REQ_LIST)) 
		{
			err = 1;
			log_warning("Failed to save peer configuraion's receiving cap list to config file.");
		}	
		
		// loop through each cap
		int i;
		for( i=0; i<conf->numOfReceiveCaps; i++ )
		{
			// open a capability
			if (openConfigElement(XML_CAPABILITY)) 
			{
				err = 1;
				log_warning("Failed to save peer configuraion's receiving cap list to config file.");
			}

			// save the cap code
			if ( setConfigValueAsInt(XML_CAPABILITY_CODE, conf->receiveCapRequirements[i].code) )
			{
				err = 1;
				log_warning("Failed to save peer configuraion's %d cap to config file.", i);
			}

			// save the cap action
			if ( setConfigValueAsInt(XML_CAPABILITY_ACTION, conf->receiveCapRequirements[i].action) )
			{
				err = 1;
				log_warning("Failed to save peer configuraion's %d cap to config file.", i);
			}

			// save the cap len and value
			if ( conf->receiveCapRequirements[i].checkValueLen != -1 )
			{
				if ( setConfigValueAsInt(XML_CAPABILITY_LENGTH, conf->receiveCapRequirements[i].checkValueLen) )
				{
					err = 1;
					log_warning("Failed to save peer configuraion's %d cap to config file.", i);
				}

				if ( conf->receiveCapRequirements[i].checkValueLen > 0 )
				{
					char *tmp = byteArrayToHexString(conf->receiveCapRequirements[i].checkValue, 
						conf->receiveCapRequirements[i].checkValueLen);

					if ( setConfigValueAsString(XML_CAPABILITY_VALUE, tmp) )
					{
						err = 1;
						log_warning("Failed to save peer configuraion's %d cap to config file.", i);
					}				
					free(tmp);
				}
			}

			// close a capability
			if (closeConfigElement(XML_CAPABILITY)) 
			{
				err = 1;
				log_warning("Failed to save peer configuraion's receiving cap list to config file.");
			}			
		}

		// close receiving cap list
		if (closeConfigElement(XML_RECEIVE_CAP_REQ_LIST)) 
		{
			err = 1;
			log_warning("Failed to save peer configuraion's receiving cap list to config file.");
		}			
	}

	// write peer group name
	if(strcmp(conf->groupName, DEFAULT_PEER_GROUP_NAME)!= 0)
	{
		if ( setConfigValueAsString(XML_GROUP_NAME_VALUE, conf->groupName) )
		{
			err = 1;
			log_warning("Failed to save peer configuraion's group name to config file.");
		}
	}	
	return err;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Save the peers  settings to the config file.
 * Input:  none
 * Output: returns 0 on success, 1 on failure
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int savePeersSettings()
{	
	int err = 0;
	int i;

	// open the peers list
	if (openConfigElement(XML_PEER_LIST_TAG)) 
	{
		err = 1;
		log_warning("Failed to save peers list tag to config file.");
	}

	for(i = 0; i < MAX_PEER_IDS; i++)
	{
		if( Peers[i] != NULL )
		{
			// open a peer 
			if (openConfigElement(XML_PEER_TAG)) 
			{
				err = 1;
				log_warning("Failed to save peer tag to config file.");
			}

			// write a peer
			err = saveOnePeerSetting(Peers[i]->configuration);

			// close a peer
			if (closeConfigElement(XML_PEER_TAG)) 
			{
				err = 1;
				log_warning("Failed to save peer tag to config file.");
			}
		}
	}

	// close the peers list
	if (closeConfigElement(XML_PEER_LIST_TAG)) 
	{
		err = 1;
		log_warning("Failed to save peers list tag to config file.");
	}
	return err;
}
#pragma mark -
/*--------------------------------------------------------------------------------------
 * Purpose: create the configuration structure
 * Input:  the peer 's address and port
 * Output: pointer to configuration or NULL if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
PConfiguration createPeerConf( char *peerAddr, int port )
{
	PConfiguration peerConf = malloc( sizeof( struct ConfigurationStruct ));
	if( peerConf == NULL ) 
	{
		log_err( "Creation of peer configuration failed" );
		return NULL;
	}
	memset( peerConf, 0, sizeof( struct ConfigurationStruct ));

	if( peerAddr !=NULL ){
		strcpy( peerConf->remoteSettings.addr, peerAddr );
	}
	peerConf->remoteSettings.port = port;
	
	peerConf->remoteSettings.AS2 = -1;
	peerConf->remoteSettings.BGPID = -1;
	peerConf->remoteSettings.BGPVer = -1;
	peerConf->remoteSettings.MinHoldtime = -1;
	
	peerConf->localSettings.port = -1;
	peerConf->localSettings.AS2 = -1;
	peerConf->localSettings.BGPID = -1;
	peerConf->localSettings.BGPVer = -1;
	peerConf->localSettings.holdtime= -1;

	peerConf->enabled = -1;
	peerConf->routeRefreshAction = -1;
	peerConf->labelAction= -1;
	peerConf->numOfReceiveCaps = 0;
	peerConf->numOfAnnCaps = 0;	
	
	strcpy(peerConf->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
	peerConf->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
	if( peerConf->groupID == -1 ){
		log_err( "Couldn't find default peer group -> Creation of peer configuration failed" );
		free(peerConf);
		return NULL;
	}else{
		return peerConf;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: find the ID of a peer given addr and port
 * Input: addr in string and port number
 * Output: ID of the peer
 * Note: If the peer with specified addr and port is not exsiting, return -1. 
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findPeerID(char *addr, int port)
{
	int i;
	for(i = 0; i < MAX_PEER_IDS; i++)
	{
		if( Peers[i] != NULL && !strcmp(Peers[i]->configuration->remoteSettings.addr, addr) 
			&& Peers[i]->configuration->remoteSettings.port == port)
			return Peers[i]->peerID;
	}
	return -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Create a peer structure
 * Input:  the peer configuration struture
 * Output: id of the new peer group or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeerStruct( PConfiguration config )
{

	// find a empty slot for this new peer
	int i;
	for(i = 0; i < MAX_PEER_IDS; i++)
	{
		if(Peers[i] == NULL)
			break;
	}

	// if the number of peer exceeds MAX_PEER_IDS, return -1
	if( i == MAX_PEER_IDS )
	{
	 	log_err("Unable to create new peer, max peer exceeded");
		return -1;
	}
	
	// Returns a new  or NULL if not enough memory.
	PPeer p = malloc( sizeof( struct PeerStruct ) );
	if( p == NULL )
	{
		log_err("createPeerStruct: malloc failed");
		return -1;
	}

	memset(p, 0, sizeof( struct PeerStruct) );
	
	// peer ID
	p->peerID = i;
	p->sessionID = -1;
	p->configuration = config;

	Peers[i] = p;
 	return i;
	
}

/*--------------------------------------------------------------------------------------
 * Purpose: create a peer, called by CLI.
 * Input:  peerAddress - could be a peer's address
 *		 port - the peer's port	
 * Output: id of the new peer or -1 if creation failed
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
int createPeer( char *peerAddr, int port )
{
	PConfiguration peerConf = createPeerConf( peerAddr, port );
	if( peerConf != NULL)
	{
		int peerID = createPeerStruct( peerConf );
		if( peerID == -1 ) 
		{
			log_err( "Creation of peer structure failed" );
			return -1;
		}
		return peerID;
	}
	else
		return -1;	
}
/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of IDs of all active peers
 * Input: a pointer to an array
 * Output: the length of the array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getAllPeersIDs(int **peerIDs)
{
	int i;
	// get a count of the current peers
	int peerCount = 0;
	for(i = 0; i < MAX_PEER_IDS; i++)
	{
		if( Peers[i] != NULL )
			peerCount++;
	}
	
	// allocate memory for the resulting ids
	int *IDs = malloc(sizeof(int)*peerCount);
	if (IDs == NULL) 
	{
		log_warning("getAllPeersIDs: Unable to allocate memory");
		return 0; 
	}

	// note is possible some peers were added since we counted above
	int actualPeers = 0;
	for(i = 0; i < MAX_PEER_IDS; i++)
	{
		if( (Peers[i] != NULL ) && (actualPeers <= peerCount ) )
		{
			IDs[actualPeers] = i; 
			actualPeers++;
		}
	}
	*peerIDs = IDs; 
	return actualPeers;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Print Peers Array
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
void printAllPeers()
{
	int i;
	for(i = 0; i < MAX_PEER_IDS; i++)
	{
		if( Peers[i] != NULL )
		{
			log_msg("Peer %d --> Session %d", 
				Peers[i]->peerID, Peers[i]->sessionID);
		}
	}
}
#pragma mark -
/*--------------------------------------------------------------------------------------
 * Purpose: Set the session ID of a peer
 * Input: peer ID and session ID
 * Output:  0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerSessionID(int peerID, int sessionID)
{
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerSessionID: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL )
	{	
		log_err("setPeerSessionID: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	Peers[peerID]->sessionID = sessionID;
	return 0;
}	


/*--------------------------------------------------------------------------------------
 * Purpose: Get the session ID of a peer
 * Input: peer ID
 * Output: the corresponding session ID
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerSessionID(int peerID)
{
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerSessionID: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL )
	{	
		log_err("getPeerSessionID: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	return Peers[peerID]->sessionID;	
}	

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's group name
 * Input:	the peer's ID
 * Output: the group name of the peer
 *          or NULL 
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeer_GroupName(int peerID)
{	
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerGroupName: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return NULL;
	}
	if( Peers[peerID] == NULL )
	{	
		log_err("getPeerGroupName: couldn't find a peer with ID:%d", peerID);
		return NULL;
	}
	return Peers[peerID]->configuration->groupName;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's group name
 * Input:	the peer's ID and the group name in string
 * Output:  0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeer_GroupName( int peerID, char *groupName )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerGroupName: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}

	// check if the peer is defined
	if( Peers[peerID] == NULL )
	{
		log_err("setPeerGroupName: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the supplied group name is different from current
	if( strcmp(Peers[peerID]->configuration->groupName, groupName) == 0 )
		return 0;

	// check if the peer group exists
	int gID = findPeerGroupID(groupName);
	if( gID == -1 )
	{
		log_err("setPeerGroupName: couldn't find a peer group with name:%s", groupName);
		return -1;		
	}
		
	strncpy( Peers[peerID]->configuration->groupName, groupName, ADDR_MAX_CHARS);
	Peers[peerID]->configuration->groupID = gID;

	return ADDR_VALID;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's MD5 password
 * Input:	the peer's ID
 * Output: the lMD5 password
 *             NULL if there is no peer with this ID or the peer has no MD5 password
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerMD5Passwd(int peerID)
{	
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerMD5Passwd: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return NULL;
	}
	if( Peers[peerID] == NULL )
	{	
		log_err("getPeerMD5Passwd: couldn't find a peer with ID:%d", peerID);
		return NULL;
	}

	if( strlen(Peers[peerID]->configuration->remoteSettings.md5Passwd) == 0 )
	{
		char *tmp = NULL;
		tmp = getPeerGroupMD5Passwd(Peers[peerID]->configuration->groupID);
		if( tmp == NULL )
		{
			log_err("getPeerMD5Passwd: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupMD5Passwd(Peers[peerID]->configuration->groupID);
		}
		else
			return tmp;
	}
	else
	{
		return Peers[peerID]->configuration->remoteSettings.md5Passwd;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's MD5 password
 * Input:	the peer's ID and the MD5 password in string
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerMD5Passwd( int peerID, char *passwd )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerMD5Passwd: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}

	// check if the peer is defined
	if( Peers[peerID] == NULL )
	{
		log_err("setPeerMD5Passwd: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the supplied password is different from current
	if( strcmp(Peers[peerID]->configuration->remoteSettings.md5Passwd, passwd) == 0 )
		return 0;

	// if different, set it
	strncpy( Peers[peerID]->configuration->remoteSettings.md5Passwd, passwd, ADDR_MAX_CHARS);
	return 0;
}
#pragma mark -
/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's address
 * Input:	the peer's ID
 * Output: the address of the peer
 *          or NULL if no address for this peer 
 * Note: The caller doesn't need to free memory.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerRemoteAddress(int peerID)
{	
	if (peerID >= MAX_PEER_IDS){
		log_err("getPeerRemoteAddress: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return NULL;
	}
	if( Peers[peerID] == NULL ){	
		log_err("getPeerRemoteAddress: couldn't find a peer with ID:%d", peerID);
		return NULL;
	}
	return Peers[peerID]->configuration->remoteSettings.addr;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's remote address
 * Input:	the peer's ID and the local addr in string
 * Output:  ADDR_VALID means address is valid and set successfully.
 *	    ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *          ADDR_ERR_INVALID_ACTIVE means this is not a valid active address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
setPeerRemoteAddress( int peerID, char *addr ){

  // check the peer ID is valid
  if (peerID >= MAX_PEER_IDS){
    log_err("setPeerRemoteAddress: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
    return ADDR_ERR_INVALID_FORMAT;
  }

  // check if the peer is defined
  if( Peers[peerID] == NULL ) {
    log_err("setPeerRemoteAddress: couldn't find a peer with ID:%d", peerID);
    return ADDR_ERR_INVALID_FORMAT;
  }

  // check if the supplied address is valid
  int result = checkAddress(addr, ADDR_PASSIVE);
  if(result != ADDR_VALID){
    return result;
  }

  // check if the supplied address is different from current
  if( strcmp(Peers[peerID]->configuration->remoteSettings.addr, addr) == 0 ){
    return ADDR_VALID;
  }

  // if different, set it
  strncpy( Peers[peerID]->configuration->remoteSettings.addr, addr, ADDR_MAX_CHARS);
  return ADDR_VALID;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the port of the peer
 * Input:   the peer's ID
 * Output: the peer's port
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerRemotePort(int peerID)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerRemotePort: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerRemotePort: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	return Peers[peerID]->configuration->remoteSettings.port;
}
/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's remote port number
 * Input:       the peer's ID and the port
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int
setPeerRemotePort( int peerID, int port ) {

  // check the peer ID is valid
  if (peerID >= MAX_PEER_IDS){
    log_err("setPeerRemotePort: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
    return -1;
  }
  // check if the peer is configured
  if( Peers[peerID] == NULL ){
          log_err("setPeerRemotePort: couldn't find a peer with ID:%d", peerID);
          return -1;
  }
  //  check if the port is valid
  if ( (port < 0) || (port > 65536) ){
        log_err("setPeerRemotePort: invalid  port number of %d", port);
        return -1;
  }

  // check if the port changes
  if( Peers[peerID]->configuration->remoteSettings.port == port ){
    return 0;
  }

  // if change, set it
  Peers[peerID]->configuration->remoteSettings.port = port;

  return 0;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote BGP ID of the peer
 * Input:   the peer's ID
 * Output: the peer's remote BGP ID
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getPeerRemoteBGPID(int peerID)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerRemoteBGPID: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerRemoteBGPID: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	if( Peers[peerID]->configuration->remoteSettings.BGPID == -1 )
	{
		long long BGPID;
		BGPID = getPeerGroupRemoteBGPID(Peers[peerID]->configuration->groupID);
		if( BGPID == -1 )
		{
			log_err("getPeerRemoteBGPID: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupRemoteBGPID(Peers[peerID]->configuration->groupID);
		}
		else
			return BGPID;
	}
	else
		return Peers[peerID]->configuration->remoteSettings.BGPID;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's remote BGP ID
 * Input:	the peer's ID and remote BGP ID
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRemoteBGPID( int peerID, long long BGPID )
{	
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerRemoteBGPID: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerRemoteBGPID: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the remote BGP ID changed
	if( Peers[peerID]->configuration->remoteSettings.BGPID == BGPID )
		return 0;
	
	// if changed, set it
	Peers[peerID]->configuration->remoteSettings.BGPID = BGPID;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the remote AS number of the peer
 * Input:   the peer's ID
 * Output: the peer's remote AS number
 *         or 0 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getPeerRemoteASNum( int peerID )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerRemoteASNum: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return 0;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerRemoteASNum: couldn't find a peer with ID:%d", peerID);
		return 0;
	}

	if( Peers[peerID]->configuration->remoteSettings.AS2 == -1 )
	{
		u_int32_t remoteAS;
		remoteAS = getPeerGroupRemoteASNum(Peers[peerID]->configuration->groupID);
		if( remoteAS == 0 )
		{
			log_err("getPeerRemoteBGPID: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupRemoteASNum(Peers[peerID]->configuration->groupID);
		}
		else
			return remoteAS;
	}
	else
		return Peers[peerID]->configuration->remoteSettings.AS2;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's remote AS number
 * Input:	the peer's ID and remote AS number
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRemoteASNum( int peerID, u_int32_t AS )
{	
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerRemoteASNum: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerRemoteASNum: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the remote AS number changed
	if( Peers[peerID]->configuration->remoteSettings.AS2 == AS )
		return 0;
	
	// if changed, set it
	Peers[peerID]->configuration->remoteSettings.AS2 = AS;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer 's remote BGP version
 * Input:	the peer 's ID
 * Output: the remote BGP Versionn
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerRemoteBGPVersion( int peerID )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerRemoteBGPVersion: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerRemoteBGPVersion: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	if( Peers[peerID]->configuration->remoteSettings.BGPVer == -1 )
	{
		int version;
		version = getPeerGroupRemoteBGPVersion(Peers[peerID]->configuration->groupID);
		if( version == -1 )
		{
			log_err("getPeerRemoteBGPVersion: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupRemoteBGPVersion(Peers[peerID]->configuration->groupID);
		}
		else
			return version;
	}
	else
		return Peers[peerID]->configuration->remoteSettings.BGPVer;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's remote BGP version
 * Input:	the peer's ID and and remote BGP version
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRemoteBGPVersion( int peerID, int version )
{	
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerRemoteBGPVersion: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerRemoteBGPVersion: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the remote BGP version changed
	if( Peers[peerID]->configuration->remoteSettings.BGPVer == version )
		return 0;
	
	// if changed, set it
	Peers[peerID]->configuration->remoteSettings.BGPVer = version;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer 's required min hold time
 * Input:	the peer 's ID
 * Output: the required min hold time
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerRemoteHoldTime( int peerID )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerRemoteHoldTime: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerRemoteHoldTime: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	if( Peers[peerID]->configuration->remoteSettings.MinHoldtime== -1 )
	{
		int holdtime;
		holdtime = getPeerGroupRemoteHoldTime(Peers[peerID]->configuration->groupID);
		if( holdtime == -1 )
		{
			log_err("getPeerRemoteHoldTime: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupRemoteHoldTime(Peers[peerID]->configuration->groupID);
		}
		else
			return holdtime;
	}
	else
		return Peers[peerID]->configuration->remoteSettings.MinHoldtime;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's required min hold time
 * Input:	the peer's ID and and the required min hold time
 * Output: 0 means success, -1 means failure
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRemoteHoldTime( int peerID, int holdtime )
{	
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerRemoteHoldTime: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerRemoteHoldTime: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the required min hold time changed
	if( Peers[peerID]->configuration->remoteSettings.MinHoldtime == holdtime )
		return 0;
	
	// if changed, set it
	Peers[peerID]->configuration->remoteSettings.MinHoldtime = holdtime;
	return 0;
}


#pragma mark -
/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local address
 * Input:	the peer's ID
 * Output: the local address
 *            NULL if there is no peer with this ID the peer has no local address
 * Note: The caller doesn't need to free memory
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
char * getPeerLocalAddress(int peerID)
{	
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerLocalAddress: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return NULL;
	}
	if( Peers[peerID] == NULL )
	{	
		log_err("getPeerLocalAddress: couldn't find a peer with ID:%d", peerID);
		return NULL;
	}

	if( strlen(Peers[peerID]->configuration->localSettings.addr) == 0)
	{
		char *tmp = NULL;
		tmp = getPeerGroupLocalAddress(Peers[peerID]->configuration->groupID);
		if( tmp == NULL )
		{
			log_err("getPeerLocalAddress: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupLocalAddress(Peers[peerID]->configuration->groupID);
		}
		else
			return tmp;
	}
	else
	{
		return Peers[peerID]->configuration->localSettings.addr;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local address
 * Input:	the peer's ID and the local addr in string
 * Output:  ADDR_VALID means address is valid and set successfully.
 *	    ADDR_ERR_INVALID_FORMAT means this is a invalid-format address
 *          ADDR_ERR_INVALID_ACTIVE means this is not a valid active address
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int setPeerLocalAddress( int peerID, char *addr )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerLocalAddress: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return ADDR_ERR_INVALID_FORMAT;
	}

	// check if the peer is defined
	if( Peers[peerID] == NULL )
	{
		log_err("setPeerLocalAddress: couldn't find a peer with ID:%d", peerID);
		return ADDR_ERR_INVALID_FORMAT;
	}

	// check if the supplied address is valid
	int result = checkAddress(addr, ADDR_PASSIVE);
	if(result != ADDR_VALID)
		return result;

	// check if the supplied address is different from current
	if( strcmp(Peers[peerID]->configuration->localSettings.addr, addr) == 0 )
		return ADDR_VALID;

	// if different, set it
	strncpy( Peers[peerID]->configuration->localSettings.addr, addr, ADDR_MAX_CHARS);
	return ADDR_VALID;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local port number
 * Input:   the peer's ID
 * Output: the local port
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerLocalPort(int peerID)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerLocalPort: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerLocalPort: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	
	if( Peers[peerID]->configuration->localSettings.port == -1 )
	{
		int localPort;
		localPort = getPeerGroupLocalPort(Peers[peerID]->configuration->groupID);
		if( localPort == -1 )
		{
			log_err("getPeerLocalPort: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupLocalPort(Peers[peerID]->configuration->groupID);
		}
		else
			return localPort;
	}
	else
		return Peers[peerID]->configuration->localSettings.port;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local port number
 * Input:	the peer's ID and the port
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalPort( int peerID, int port )
{	
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerLocalPort: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerLocalPort: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	//  check if the port is valid
	if ( (port < 0) || (port > 65536) ) 
	{
		log_err("setPeerLocalPort: invalid  port number of %d", port);
		return -1;
	}
	
	// check if the port changes
	if( Peers[peerID]->configuration->localSettings.port == port )
		return 0;
	
	// if change, set it
	Peers[peerID]->configuration->localSettings.port = port;

	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get peer's local BGP ID
 * Input:   the peer's ID
 * Output: the monitor side BGP ID
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
long long getPeerLocalBGPID(int peerID)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerLocalBGPID: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerLocalBGPID: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	if( Peers[peerID]->configuration->localSettings.BGPID == -1 )
	{
		int localBGPID;
		localBGPID = getPeerGroupLocalBGPID(Peers[peerID]->configuration->groupID);
		if( localBGPID == -1 )
		{
			log_err("getPeerLocalBGPID: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupLocalBGPID(Peers[peerID]->configuration->groupID);
		}
		else
			return localBGPID;
	}
	else
		return Peers[peerID]->configuration->localSettings.BGPID;		
}

/*--------------------------------------------------------------------------------------
 * Purpose:Set peer's local BGP ID
 * Input:	the peer's ID and the local BGP ID
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalBGPID( int peerID, long long BGPID )
{	
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerLocalBGPID: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerLocalBGPID: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the BGPID has changed
	if( Peers[peerID]->configuration->localSettings.BGPID == BGPID )
		return 0;
	
	// if changed, set it
	Peers[peerID]->configuration->localSettings.BGPID = BGPID;

	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local AS number
 * Input:   the peer's ID
 * Output: the local AS number
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
u_int32_t getPeerLocalASNum( int peerID )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerLocalASNum: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return 0;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerLocalASNum: couldn't find a peer with ID:%d", peerID);
		return 0;
	}
	if( Peers[peerID]->configuration->localSettings.AS2 == -1 )
	{
		u_int32_t localAS;
		localAS = getPeerGroupLocalASNum(Peers[peerID]->configuration->groupID);
		if( localAS == 0 )
		{
			log_err("getPeerLocalASNum: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupLocalASNum(Peers[peerID]->configuration->groupID);
		}
		else
			return localAS;
	}
	else
		return Peers[peerID]->configuration->localSettings.AS2;		
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local AS number
 * Input:	the peer's ID and local AS number
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalASNum( int peerID, u_int32_t AS )
{	
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerLocalASNum: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerLocalASNum: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the AS number has changed
	if( Peers[peerID]->configuration->localSettings.AS2 == AS )
		return 0;
	
	// if changed, set it
	Peers[peerID]->configuration->localSettings.AS2 = AS;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local BGP version
 * Input:   the peer's ID
 * Output: the local BGP version
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerLocalBGPVersion( int peerID )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerLocalBGPVersion: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerLocalBGPVersion: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	if( Peers[peerID]->configuration->localSettings.BGPVer == -1 )
	{
		int version;
		version = getPeerGroupLocalBGPVersion(Peers[peerID]->configuration->groupID);
		if( version == -1 )
		{
			log_err("getPeerLocalBGPVersion: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupLocalBGPVersion(Peers[peerID]->configuration->groupID);
		}
		else
			return version;
	}
	else
		return Peers[peerID]->configuration->localSettings.BGPVer;		
}


/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local BGP version
 * Input:	the peer's ID and local BGP version
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalBGPVersion( int peerID, int version )
{	
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerLocalBGPVersion: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerLocalBGPVersion: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the BGP version has changed 
	if( Peers[peerID]->configuration->localSettings.BGPVer == version )
		return 0;
	
	// if changed, set it
	Peers[peerID]->configuration->localSettings.BGPVer = version;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the peer's local holdtime
 * Input:   the peer's ID
 * Output: the local hold time
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerLocalHoldTime( int peerID )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerLocalHoldTime: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerLocalHoldTime: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	if( Peers[peerID]->configuration->localSettings.holdtime == -1 )
	{
		int holdtime;
		holdtime = getPeerGroupLocalHoldTime(Peers[peerID]->configuration->groupID);
		if( holdtime == -1 )
		{
			log_err("getPeerLocalBGPVersion: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupLocalHoldTime(Peers[peerID]->configuration->groupID);
		}
		else
			return holdtime;
	}
	else
		return Peers[peerID]->configuration->localSettings.holdtime;		
}


/*--------------------------------------------------------------------------------------
 * Purpose: Set the peer's local hold time
 * Input:	the peer's ID and local hold time
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLocalHoldTime( int peerID, int holdtime )
{	
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerLocalHoldTime: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerLocalHoldTime: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if the BGP version has changed 
	if( Peers[peerID]->configuration->localSettings.holdtime == holdtime )
		return 0;
	
	// if changed, set it
	Peers[peerID]->configuration->localSettings.holdtime = holdtime;
	return 0;
}
#pragma mark -
/*--------------------------------------------------------------------------------------
 * Purpose: Get peer's the label action
 * Input:   the peer's ID
 * Output: the label action
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerLabelAction( int peerID )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerLabelAction: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerLabelAction: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	if( Peers[peerID]->configuration->labelAction == -1 )
	{
		int action;
		action = getPeerGroupLabelAction(Peers[peerID]->configuration->groupID);
		if( action == -1 )
		{
			log_err("getPeerLabelAction: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupLabelAction(Peers[peerID]->configuration->groupID);
		}
		else
			return action;
	}
	else
		return Peers[peerID]->configuration->labelAction;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set peer's the label action
 * Input:	the peer's ID and label action
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerLabelAction( int peerID, int labelAction )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerLabelAction: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerLabelAction: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	// check if label action changes
	if( Peers[peerID]->configuration->labelAction == labelAction )
		return 0;
	
	// if changed ...
	log_msg("setPeerLabelAction: from %d to %d", Peers[peerID]->configuration->labelAction, labelAction);
	if( Peers[peerID]->configuration->labelAction == Label || Peers[peerID]->configuration->labelAction == StoreRibOnly )
	{
		setSessionLabelAction(Peers[peerID]->sessionID, labelAction);
	}
	Peers[peerID]->configuration->labelAction = labelAction;
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get peer's the route refresh action
 * Input:   the peer's ID
 * Output: the route refresh action
 *         or -1 if there is no peer with this ID
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int getPeerRouteRefreshAction( int peerID )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerRouteRefreshAction: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerRouteRefreshAction: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	if( Peers[peerID]->configuration->routeRefreshAction == -1 )
	{
		int action;
		action = getPeerGroupRouteRefreshAction(Peers[peerID]->configuration->groupID);
		if( action == -1 )
		{
			log_err("getPeerRouteRefreshAction: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupRouteRefreshAction(Peers[peerID]->configuration->groupID);
		}
		else
			return action;
	}
	else
		return Peers[peerID]->configuration->routeRefreshAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set peer's the route refresh action
 * Input:	the peer's ID and route refresh action
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerRouteRefreshAction( int peerID, int rrAction )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerRouteRefreshAction: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerRouteRefreshAction: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	Peers[peerID]->configuration->routeRefreshAction = rrAction;
	setSessionRouteRefreshAction(Peers[peerID]->sessionID, rrAction);
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get peer's the enabled flag
 * Input:	the peer's ID
 * Output: TRUE or FALSE
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerEnabledFlag(int peerID)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerEnabledFlag: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return FALSE;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerEnabledFlag: couldn't find a peer with ID:%d", peerID);
		return FALSE;
	}
	if( Peers[peerID]->configuration->enabled == -1 )
	{
		int enabled;
		enabled = getPeerGroupEnabledFlag(Peers[peerID]->configuration->groupID);
		if( enabled == -1 )
		{
			log_err("getPeerEnabledFlag: couldn't find a peer group with ID:%d", Peers[peerID]->configuration->groupID);
			strcpy(Peers[peerID]->configuration->groupName, DEFAULT_PEER_GROUP_NAME); //refer to the default group
			Peers[peerID]->configuration->groupID = findPeerGroupID(DEFAULT_PEER_GROUP_NAME);
			return getPeerGroupEnabledFlag(Peers[peerID]->configuration->groupID);
		}
		else
			return enabled;
	}
	else
		return Peers[peerID]->configuration->enabled;	
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set peer's the enabled flag
 * Input:	the peer's ID and the enabled flag
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int setPeerEnabledFlag( int peerID, int enabled )
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("setPeerEnabledFlag: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("setPeerEnabledFlag: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	Peers[peerID]->configuration->enabled = enabled;
	
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: check if the peer's configuration is complete to launch the thread
 * Input:	the peer's ID
 * Output: FALSE means Ok, TRUE means incomplete configuration
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int checkPeerConfiguration(int peerID)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("checkPeerConfiguratio: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("checkPeerConfiguratio: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	
	char *tmp = getPeerRemoteAddress(peerID);
	if( tmp == NULL )
		return -1;	
	if( getPeerRemoteASNum(peerID) > 0 && getPeerLocalASNum(peerID) > 0 && strlen(tmp) != 0 )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: launch all peers
 * Input:	
 * Output:
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
void launchAllPeers()
{
	int i;
	for(i = 0; i < MAX_PEER_IDS; i++)
	{
		if( Peers[i] != NULL )
		{
			//launch the peer thread
			if( getPeerEnabledFlag(Peers[i]->peerID)== TRUE && Peers[i]->sessionID == -1 )
			{
				// check the configuration of this peer
				if( checkPeerConfiguration(i) == TRUE)
				{					
					int error;
					// launch the peer thread
					debug(__FUNCTION__, "Creating peer thread %d...", i);

					pthread_t peerThreadID;
					if ((error = pthread_create(&peerThreadID, NULL, peerThreadFunction, &(Peers[i]->peerID))) > 0 )
						log_fatal("Failed to create peer thread: %s\n", strerror(error));
					Peers[i]->peerThread = peerThreadID;

					debug(__FUNCTION__, "Created peer thread %d!", i);
				}
				else
					debug(__FUNCTION__, "Couldn't create peer thread %d as the configuration is invalid!", i);
			}		
		}
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Signals all peer threads to shutdown
 * Input:	
 * Output:
 * Kevin Burnett @ July 9, 2009
 * -------------------------------------------------------------------------------------*/ 
void signalPeersShutdown()
{
	int i;
	for(i = 0; i < MAX_PEER_IDS; i++)
	{
		if( Peers[i] != NULL )
		{
			setPeerEnabledFlag(Peers[i]->peerID, FALSE);
		}
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: Waits until all peer threads have been stopped before returning
 * Input:	
 * Output:
 * Kevin Burnett @ July 9, 2009
 * -------------------------------------------------------------------------------------*/ 
void waitForPeersShutdown()
{
	void * status = NULL;
	int i;
	for(i = 0; i < MAX_PEER_IDS; i++)
	{
		if( Peers[i] != NULL )
		{
			status = NULL;
			pthread_join(Peers[i]->peerThread, status);
		}
	}
}

/*--------------------------------------------------------------------------------------
 * Purpose: reconnect a peer, called by CLI to make new configuraion applied 
 * Input:	the peer's ID
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int reconnectPeer(int peerID)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("resetPeer: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("resetPeer: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	// set the reset flag
	if( getPeerEnabledFlag(peerID)== TRUE && Peers[peerID]->sessionID >= 0 )
	{
		Sessions[Peers[peerID]->sessionID]->reconnectFlag = TRUE;
	}
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: delete a peer
 * Input:	the peer's ID
 * Output: 0 means success, -1 means failure
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
deletePeer(int peerID)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("deletePeer: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("deletePeer: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	int i;
	for( i=0; i<Peers[peerID]->configuration->numOfAnnCaps; i++ )
	{
		if( Peers[peerID]->configuration->announceCaps[i] != NULL )
			free( Peers[peerID]->configuration->announceCaps[i] );
	}
	
	free( Peers[peerID]->configuration );
	free( Peers[peerID] );
	Peers[peerID] = NULL;

	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: get peer's session ID by a peer ID
 * Input:	the peer's sesssion ID
 * Output:
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/ 
int 
getSessionIDByPeerID(int peerID)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getSessionIDByPeerID: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}

	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("getSessionIDByPeerID: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	
	return Peers[peerID]->sessionID;
}
#pragma mark -
/*--------------------------------------------------------------------------------------
 * Purpose: find an announce capabilty
 * Input:	the configuration structure, cap code, cap value len and cap value.
 * Output: the index of the found capability or -1 means not found
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findAnnounceCapability(PConfiguration conf, int code, int len, u_char *value)
{
	int i;
	for( i=0; i<conf->numOfAnnCaps; i++ )
	{	
		if( conf->announceCaps[i]->code == code && conf->announceCaps[i]->length == len )
		{
			if( conf->announceCaps[i]->length == 0 )
				return i;
			else if( memcmp(conf->announceCaps[i]->value, value, len) == 0)
				return i;
		}
			
	}
	return -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: add an announce capabilty
 * Input:	the configuration structure, cap code, cap value len and cap value.
 * Output: 0 means success, -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int addAnnounceCapability(PConfiguration conf, int code, int len, u_char *value)
{
	// check if the capability is existing
	if( findAnnounceCapability(conf, code, len, value) < 0 )
	{
		conf->announceCaps[conf->numOfAnnCaps]
			= createGenericBGPCapability( code, len, value);
		conf->numOfAnnCaps++;
		return 0;
	}
	else
		return -1;

}

/*--------------------------------------------------------------------------------------
 * Purpose: remove an annouce capabilty
 * Input:	the configuration structure, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 mean failure because the capabilty doesn't exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int removeAnnounceCapability(PConfiguration conf, int code, int len, u_char *value)
{
	// check if the capability is existing
	int i = findAnnounceCapability(conf, code, len, value);
	if( i >= 0 )
	{
		int j;
		free(conf->announceCaps[i]);
		for(j=i; j<conf->numOfAnnCaps-1; j++)
		{
			conf->announceCaps[j] = conf->announceCaps[j+1];
		}
		conf->announceCaps[j] = NULL;
		conf->numOfAnnCaps--;
	}
	else
		return -1;
	
	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: add an annouce capabilty to a peer, called by CLI
 * Input:	the peer's ID, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerAddAnnounceCapability(int peerID, int code, int len, u_char *value)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("peerAddAnnounceCapability: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("peerAddAnnounceCapability: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	return addAnnounceCapability(Peers[peerID]->configuration, code, len, value);
}

/*--------------------------------------------------------------------------------------
 * Purpose: remove an annouce capabilty from a peer, called by CLI
 * Input:	the peer's ID, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 mean failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerRemoveAnnounceCapability(int peerID, int code, int len, u_char *value)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("peerRemoveAnnounceCapability: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("peerRemoveAnnounceCapability: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	
	return removeAnnounceCapability(Peers[peerID]->configuration, code, len, value);
}

/*--------------------------------------------------------------------------------------
 * Purpose: find a receive capabilty
 * Input:	the configuration structure, action, cap code, cap value len and cap value.
 * Output: the index of the found capability or -1 means not found
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findReceiveCapability(PConfiguration conf, int action, int code, int len, u_char *value)
{
	int i;
	for( i=0; i<conf->numOfReceiveCaps; i++ )
	{	
		if( conf->receiveCapRequirements[i].action == action
			&& conf->receiveCapRequirements[i].code == code 
			&& conf->receiveCapRequirements[i].checkValueLen == len )
		{
			if( conf->receiveCapRequirements[i].checkValueLen <= 0 )
				return i;
			else if( memcmp(conf->receiveCapRequirements[i].checkValue, value, len) == 0)
				return i;
		}			
	}
	return -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: add a receive capabilty
 * Input:	the configuration structure, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int addReceiveCapability(PConfiguration conf, int action, int code, int len, u_char *value)
{
	// check if the capability is existing
	if( findReceiveCapability(conf, action, code, len, value) < 0 )
	{
		conf->receiveCapRequirements[conf->numOfReceiveCaps].code = code;
		conf->receiveCapRequirements[conf->numOfReceiveCaps].action = action;
		conf->receiveCapRequirements[conf->numOfReceiveCaps].checkValueLen = len;
		if(len > 0)
			memcpy(conf->receiveCapRequirements[conf->numOfReceiveCaps].checkValue, value, len);
		conf->receiveCapRequirements[conf->numOfReceiveCaps].flag = 0;
		conf->numOfReceiveCaps++;
		return 0;
	}
	else
		return -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: remove a receive capabilty
 * Input:	the configuration structure, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty does't exist
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int removeReceiveCapability(PConfiguration conf, int action, int code, int len, u_char *value)
{
	// check if the capability is existing
	int i = findReceiveCapability(conf, action, code, len, value);
	if( i >= 0 )
	{
		int j;
		for(j=i; j<conf->numOfReceiveCaps-1; j++)
		{
			conf->receiveCapRequirements[j] = conf->receiveCapRequirements[j+1];
		}
		memset( &conf->receiveCapRequirements[j], 0, sizeof(PeerCapabilityRequirement));
		conf->numOfReceiveCaps--;
		return 0;
	}
	else
		return -1;

}

/*--------------------------------------------------------------------------------------
 * Purpose: add an receive capabilty to a peer, called by CLI
 * Input:	the peer's ID, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty already exists
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerAddReceiveCapability(int peerID, int action, int code, int len, u_char *value)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("addReceiveCapability: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("addReceiveCapability: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	return addReceiveCapability(Peers[peerID]->configuration, action, code, len, value);
}

/*--------------------------------------------------------------------------------------
 * Purpose: remove a receive capabilty from a peer, called by CLI
 * Input:	the peer's ID, action, cap code, cap value len and cap value.
 * Output: 0 means ok. -1 means failure because the capabilty does't exist
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int peerRemoveReceiveCapability(int peerID, int action, int code, int len, u_char *value)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("removeReceiveCapability: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("removeReceiveCapability: couldn't find a peer with ID:%d", peerID);
		return -1;
	}

	return removeReceiveCapability(Peers[peerID]->configuration, action, code, len, value);
}

/*--------------------------------------------------------------------------------------
 * Purpose: find a receive capabilty without looking at action field
 * Input:	cap code, cap value len and cap value.
 * Output: the index of the found capability or -1 means not found
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findReceiveCapabilityWithoutAction( int numOfCaps, PeerCapabilityRequirement *caps, int code, int len, u_char *value)
{
	int i;
	for( i=0; i<numOfCaps; i++ )
	{	
		if( caps[i].code == code 
			&& caps[i].checkValueLen == len )
		{
			if( caps[i].checkValueLen <= 0 )
				return i;
			else if( memcmp(caps[i].checkValue, value, len) == 0)
				return i;
		}			
	}
	return -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of rules how to check received capabilites, 
 		    these rules are from peer, peer group and default peer group
 * Input: the peer's ID and a pointer to the return array
 * Output: the length of the return array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerReceiveCapabities(int peerID, PeerCapabilityRequirement **rcvCaps)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerReceiveCapabities: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerReceiveCapabities: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	
	// first allocate memory for the peer's receive capabilites
	int len = 0;
	PeerCapabilityRequirement *caps = NULL;

	// put all the peer receive capabilites in the list at first
	int i = 0;
	for(i = 0; i < Peers[peerID]->configuration->numOfReceiveCaps; i++)
	{	
		PeerCapabilityRequirement tmp = Peers[peerID]->configuration->receiveCapRequirements[i];
		if( findReceiveCapabilityWithoutAction(len, caps, tmp.code, tmp.checkValueLen, tmp.checkValue) < 0 )
		{
			len++;
			caps = (PeerCapabilityRequirement *)realloc (caps, len * sizeof(PeerCapabilityRequirement));
			if (caps == NULL) 
			{
				log_warning("getPeerReceiveCapabities: Unable to allocate memory");
				return -1; 
			}			
			caps[len-1] = tmp;
		}
	}
	
	// then put all the receive capabilites from peer group
	if(Peers[peerID]->configuration->groupID >= 0)
	{
		for(i = 0; i < PeerGroups[Peers[peerID]->configuration->groupID]->configuration->numOfReceiveCaps; i++)
		{
			PeerCapabilityRequirement tmp = PeerGroups[Peers[peerID]->configuration->groupID]->configuration->receiveCapRequirements[i];
			if( findReceiveCapabilityWithoutAction(len, caps, tmp.code, tmp.checkValueLen, tmp.checkValue) < 0 )
			{
				len++;
				caps = (PeerCapabilityRequirement *) realloc (caps, len * sizeof(PeerCapabilityRequirement));
				if (caps == NULL) 
				{
					log_warning("getPeerReceiveCapabities: Unable to allocate memory");
					return -1; 
				}					
				caps[len-1] = tmp;
			}
		}
	}

	// then put all the receive capabilites from default peer group
	if(PeerGroups[Peers[peerID]->configuration->groupID]->configuration->groupID >= 0)
	{
		for(i = 0; i < PeerGroups[PeerGroups[Peers[peerID]->configuration->groupID]->configuration->groupID]->configuration->numOfReceiveCaps; i++)
		{
			PeerCapabilityRequirement tmp = PeerGroups[PeerGroups[Peers[peerID]->configuration->groupID]->configuration->groupID]->configuration->receiveCapRequirements[i];
			if( findReceiveCapabilityWithoutAction(len, caps, tmp.code, tmp.checkValueLen, tmp.checkValue) < 0 )
			{
				len++;
				caps = (PeerCapabilityRequirement *) realloc (caps, len * sizeof(PeerCapabilityRequirement));
				if (caps == NULL) 
				{
					log_warning("getPeerReceiveCapabities: Unable to allocate memory");
					return -1; 
				}					
				caps[len-1] = tmp;
			}
		}	
	}
	
#ifdef DEBUG
	for(i = 0; i < len; i++)
	{	
		debug(__FUNCTION__, "Peer %d: Capability Code %d, len %d, action %d", peerID, caps[i].code, caps[i].checkValueLen, caps[i].action);
	}
#endif
		
	*rcvCaps = caps; 
	return len;
}

/*--------------------------------------------------------------------------------------
 * Purpose: find an announce capabilty
 * Input:	cap code, cap value len and cap value.
 * Output: the index of the found capability or -1 means not found
 *
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int findAnnounceCapabilityInArray(int numOfCaps, PBgpCapability *caps, int code, int len, u_char *value)
{
	int i;
	for( i=0; i<numOfCaps; i++ )
	{	
		if( caps[i]->code == code && caps[i]->length == len )
		{
			if( caps[i]->length == 0 )
				return i;
			else if( memcmp(caps[i]->value, value, len) == 0)
				return i;
		}
			
	}
	return -1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get an array of announced capabilities which are from peer, peer group and default group
 * Input: the peer's ID and a pointer to the return array
 * Output: the length of the return array
 * Note: 1. The caller doesn't need to allocate memory.
 *	 2. The caller must free the array after using it.
 * He Yan @ Sep 22, 2008
 * -------------------------------------------------------------------------------------*/
int getPeerAnnounceCapabities(int peerID, PBgpCapability **annCaps)
{
	// check the peer ID is valid
	if (peerID >= MAX_PEER_IDS)
	{
		log_err("getPeerAnnounceCapabities: peer ID %d exceeds max %d", peerID, MAX_PEER_IDS);
		return -1;
	}
	// check if the peer is configured
	if( Peers[peerID] == NULL ) 
	{
		log_err("getPeerAnnounceCapabities: couldn't find a peer with ID:%d", peerID);
		return -1;
	}
	
	int len = 0;
	PBgpCapability *caps = NULL;

	// put all the peer announce capabilites in the list at first
	int i = 0;
	for(i = 0; i < Peers[peerID]->configuration->numOfAnnCaps; i++)
	{	
		PBgpCapability tmp = Peers[peerID]->configuration->announceCaps[i];
		if( findAnnounceCapabilityInArray(len, caps, tmp->code, tmp->length, tmp->value) < 0 )
		{
			len++;
			caps = (PBgpCapability *)realloc (caps, len * sizeof(PBgpCapability));
			if (caps == NULL) 
			{
				log_warning("getPeerAnnounceCapabities: Unable to allocate memory");
				return -1; 
			}			
			caps[len-1] = createGenericBGPCapability( tmp->code, tmp->length, tmp->value);
		}
	}


	// then put all the receive capabilites from peer group
	if(Peers[peerID]->configuration->groupID >= 0)
	{
		for(i = 0; i < PeerGroups[Peers[peerID]->configuration->groupID]->configuration->numOfAnnCaps; i++)
		{
			PBgpCapability tmp = PeerGroups[Peers[peerID]->configuration->groupID]->configuration->announceCaps[i];
			if( findAnnounceCapabilityInArray(len, caps, tmp->code, tmp->length, tmp->value) < 0 )
			{
				len++;
				caps = (PBgpCapability *) realloc (caps, len * sizeof(PBgpCapability));
				if (caps == NULL) 
				{
					log_warning("getPeerAnnounceCapabities: Unable to allocate memory");
					return -1; 
				}					
				caps[len-1] = createGenericBGPCapability( tmp->code, tmp->length, tmp->value);
			}
		}
	}

	// then put all the receive capabilites from default peer group
	if(PeerGroups[Peers[peerID]->configuration->groupID]->configuration->groupID >= 0)
	{
		for(i = 0; i < PeerGroups[PeerGroups[Peers[peerID]->configuration->groupID]->configuration->groupID]->configuration->numOfAnnCaps; i++)
		{
			PBgpCapability tmp = PeerGroups[PeerGroups[Peers[peerID]->configuration->groupID]->configuration->groupID]->configuration->announceCaps[i];
			if( findAnnounceCapabilityInArray(len, caps, tmp->code, tmp->length, tmp->value) < 0 )
			{
				len++;
				caps = (PBgpCapability *) realloc (caps, len * sizeof(PBgpCapability));
				if (caps == NULL) 
				{
					log_warning("getPeerAnnounceCapabities: Unable to allocate memory");
					return -1; 
				}					
				caps[len-1] = createGenericBGPCapability( tmp->code, tmp->length, tmp->value);
			}
		}	
	}
	
#ifdef DEBUG
	for(i = 0; i < len; i++)
	{	
		debug(__FUNCTION__, "Peer %d: Annouce Capability Code %d, len %d", peerID, caps[i]->code, caps[i]->length);
	}
#endif
	*annCaps = caps; 
	return len;
}

