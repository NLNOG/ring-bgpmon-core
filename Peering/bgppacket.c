/* 
 * 	Copyright (c) 2010 Colorado State University
 * 
 * 	Permission is hereby granted, free of charge, to any person
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
 *  File: bgppacket.c
 * 	Authors: He Yan
 *  Data: Apr 22, 2008 
 */

/*  Must be compiled with packed structures.
 * 
 * SSL support for all readn/writen call, either call an 
 * ssl specific routine or pass a flag to the existing
 * routines.
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

 
#include "bgppacket.h"
#include "../Util/unp.h"
#include "../Util/log.h"
#include "../Util/bgpmon_defaults.h"

//#define DEBUG

/*--------------------------------------------------------------------------------------
 * Purpose: functions to manipulate BGP header
 * He Yan @ June 15, 2008   
 * -------------------------------------------------------------------------------------*/
int 
maskOK( PBgpHeader h )
{
	int i;
	for ( i = 0; i < sizeof( h->mask ); i++)
		if ( h->mask[i] != 0xff )
			return(FALSE);
	return(TRUE);
}

PBgpHeader 
createBGPHeader( int type )
{
	PBgpHeader h = NULL;
	h = malloc( sizeof( struct BGPHeaderStruct ) );
	if ( h != NULL )
	{
		//memset(h, 0, sizeof(struct BGPHeaderStruct));
		memset(h->mask, 0xff, sizeof(h->mask));
		h->length = htons(19);
		h->type = type;		
	}
	else
		log_fatal("createBGPHeader: malloc failed");
	
	return( h );
}
 
void 
destroyBGPHeader( PBgpHeader h )
{
	if ( h != NULL ) 
		free( h );
} 

PBgpHeader 
readBGPHeader( int socket, int to )
{
	PBgpHeader h = createBGPHeader( 0 );
	memset(h, 0, sizeof(struct BGPHeaderStruct));
	if ( h != NULL )
	{
		int hl = sizeof(struct BGPHeaderStruct);
		int n = readn( socket, (char *) h, hl);		
		if ( n != hl || !maskOK(h) ) // check mask, length for possible errors 
		{
			log_err("readBGPHeader:%s: %x","invalid header",h);
#ifdef DEBUG			
			hexdump(LOG_ERR, h, hl);
#endif			
			destroyBGPHeader(h);
			h = NULL;
		}
		else
		{
#ifdef DEBUG
			debug("readbgpheader:", "");
			hexdump(LOG_DEBUG, h, hl);
#endif
		}
	}
	else
		log_fatal("readBGPHeader: malloc failed");
		
	return(h);
}

ssize_t 
writeBGPHeader( PBgpHeader h, int socket )
{
	int hl = sizeof(struct BGPHeaderStruct);
	int n = writen(socket, h, hl);
	if ( n != hl )
	{
#ifdef DEBUG
		debug("writeBGPHeader", "write failed");
		hexdump(LOG_DEBUG, h, hl);
#endif
		n = -1;
	}
	else
	{
#ifdef DEBUG
		debug("writeBGPHeader", "");
		hexdump(LOG_DEBUG, h, hl);
#endif
	}
	return(n);
}

void 
setBGPHeaderType( PBgpHeader h, u_int8_t type )
{
	h->type = type;
}

void 
setBGPHeaderLength( PBgpHeader h, u_int16_t length )
{
	h->length = htons( length + sizeof(struct BGPHeaderStruct) );
}

u_int8_t 	
getBGPHeaderType( PBgpHeader h )
{
	return( h->type );
}

u_int16_t 
getBGPHeaderLength( PBgpHeader h )
{
	return( ntohs(h->length) );
}


/*--------------------------------------------------------------------------------------
 * Purpose: functions to manipulate BGP open message
 * He Yan @ June 15, 2008   
 * -------------------------------------------------------------------------------------*/
PBgpOpen 
createBGPOpen( u_int8_t version, u_int32_t as, u_int16_t holdTime, u_int32_t identifier, PBgpOpenParameters openParameters )
{
	PBgpOpen opn = malloc( sizeof( struct BGPOpenStruct )+ openParameters->length);
	if ( opn != NULL )
	{
		// fill in the structure
		opn->version = version;
		if (as <= 65535 )
		{
		        opn->as = ( htonl(as) >> 16);
		}
		else
		{
			log_warning("Including AS_TRANS AS in BGP open message!");
			opn->as = htons(AS_TRANS);
		}
		opn->holdTime = htons(holdTime);
		opn->identifier = identifier;
		opn->optionalParameterLength = openParameters->length;		
		memcpy(opn->optionalParameter, openParameters->list, openParameters->length);
#ifdef DEBUG
		debug( "createBGPOpen", "");
		hexdump(LOG_DEBUG, opn, sizeof( struct BGPOpenStruct ) + openParameters->length );
#endif
	}
	else
		log_fatal("createBGPOpen: malloc failed");
		
	return( opn ); 
}

void
destroyBGPOpen( PBgpOpen o )
{
	if ( o != NULL )
		free( o );
}

PBgpOpen 
readBGPOpen( int socket )
{
	PBgpOpen opn1 = NULL;
	PBgpOpen opn = malloc( sizeof( struct BGPOpenStruct ));
	if ( opn != NULL )
	{
		int opnl = sizeof( struct BGPOpenStruct );
		int l = readn( socket, opn, opnl );
		if ( l != opnl )
		{
			log_err("readBGPOpen", " invalid length");
#ifdef DEBUG			
			hexdump(LOG_ERR, opn, l);
#endif			
			free( opn );
			opn1 = NULL;
		}
		else
		{
			opn1 = malloc( sizeof( struct BGPOpenStruct ) + opn->optionalParameterLength);
			if ( opn1 != NULL )
			{
				opn1->as = opn->as;
				opn1->holdTime = opn->holdTime;
				opn1->identifier= opn->identifier;
				opn1->optionalParameterLength = opn->optionalParameterLength;
				opn1->version = opn->version;
				free(opn);
				//read the open parameters
				int m = readn( socket, opn1->optionalParameter, opn1->optionalParameterLength );
				if ( m != opn1->optionalParameterLength )
				{
					log_err("readBGPOpen", " invalid open parameters length");
#ifdef DEBUG					
					hexdump(LOG_ERR, opn1->optionalParameter, m);
#endif					
					free( opn1 );
					opn1 = NULL;
				}
				else
				{
#ifdef DEBUG
					debug("readBGPOpen", "");
					hexdump(LOG_DEBUG, opn1, l+m);
#endif
				}
			}
			else
				log_fatal( "readBGPOpen1: malloc failed");
		}		
	}
	else
		log_fatal( "readBGPOpen: malloc failed");
	
	return( opn1 );
}

ssize_t 
writeBGPOpen( PBgpOpen opn, int socket)
{
	int opnl = sizeof( struct BGPOpenStruct ) + opn->optionalParameterLength;
	int l = writen(socket, opn, opnl );
	if ( l != opnl )
	{
		log_err("writeBGPOpen: write failed");
#ifdef DEBUG		
		hexdump(LOG_ERR, opn, opnl);
#endif		
		l = -1;
	}
	else
	{
#ifdef DEBUG
		debug("writeBGPOpen", "");
		log_msg("opnl: %d", opnl);
		hexdump(LOG_DEBUG, opn, opnl);
#endif
	}
		
	return( l );
}

int 
lengthBGPOpen( PBgpOpen o )
{
	return sizeof( struct BGPOpenStruct )+ o->optionalParameterLength;
}

void 
setBGPOpenAutonomousSystem( PBgpOpen o, u_int16_t autonomousSystem )
{
	o->as = ntohs(autonomousSystem);
}

void 
setBGPOpenHoldTime( PBgpOpen o, u_int16_t holdTime )
{
	o->holdTime = htons(holdTime);
}

void 
setBGPOpenIdentifier( PBgpOpen o, u_int32_t identifier )
{
	o->identifier = htonl(identifier);
}
void 
setBGPOpenOptionalParameterLength( PBgpOpen o, u_int8_t parameterLength )
{
	o->optionalParameterLength = parameterLength;
}

void
setBGPOpenVersion( PBgpOpen o, u_int8_t version)
{
	o->version = version;
}

u_int16_t 
getBGPOpenAutonomousSystem( PBgpOpen o )
{
	return( ntohs( o->as ));
}

u_int16_t 
getBGPOpenHoldTime( PBgpOpen o )
{
	return( ntohs( o->holdTime ));
}

u_int32_t 
getBGPOpenIdentifier( PBgpOpen o )
{
	return( ntohl(o->identifier));
}

u_int8_t	
getBGPOpenOptionalParameterLength( PBgpOpen o )
{
	return( o->optionalParameterLength );
}

u_int8_t
getBGPOpenVersion( PBgpOpen o)
{
	return( o->version );	
}

/*--------------------------------------------------------------------------------------
 * Purpose: functions to manipulate BGP open parameters
 * He Yan @ June 15, 2008   
 * -------------------------------------------------------------------------------------*/
PBgpOpenParameters 
createBGPOpenParameters()
{
	PBgpOpenParameters prm = malloc( sizeof( struct BGPOpenParametersStruct ));
	if ( prm != NULL )
	{
		memset( prm->list, 0, bufferLength );
		prm->length = 0;
#ifdef DEBUG
		debug( "createBGPOpenParameters", "");
		hexdump(LOG_DEBUG, prm, 0);
#endif
	}
	else
		log_fatal( "createBGPOpenParameters: malloc failed");
		
	return( prm );
}

void 
destroyBGPOpenParameters( PBgpOpenParameters p )
{
	if ( p != NULL )
		free( p );
}

int 
lengthBGPOpenParameters( PBgpOpenParameters prm )
{
	return(prm->length);
}


PBgpOpenParameters extractBGPOpenParameters(PBgpOpen open)
{
	PBgpOpenParameters prm = malloc( sizeof( struct BGPOpenParametersStruct ));
	if ( prm != NULL )
	{	
		memset( prm, 0, sizeof( struct BGPOpenParametersStruct ) );
		prm->length = open->optionalParameterLength;
		memcpy(prm->list, open->optionalParameter, prm->length);
	}
	else
		log_fatal( "extractBGPOpenParameters: malloc failed");
	
	return prm;
}

/*
 * Extract all the capabilities from received open parameters
 */
PBgpCapabilities extractBgpCapabilities (PBgpOpenParameters prm)
{
	PBgpCapabilities pbcs = malloc( sizeof( struct BGPCapabilitiesStruct ));
	if ( pbcs != NULL )
	{		
		int i = 0;
		memset( pbcs, 0, sizeof( struct BGPCapabilitiesStruct ) );
		while ( i < prm->length )
		{
			if(prm->list[i] == capabilityParameter)
			{
				log_msg( "Capability No%d: %d %d",pbcs->count, prm->list[i], prm->list[i+2]);
				pbcs->capalities[pbcs->count].code = prm->list[i+2];
				pbcs->capalities[pbcs->count].length= prm->list[i+3];
				if(pbcs->capalities[pbcs->count].length != 0)
				{
					memcpy(pbcs->capalities[pbcs->count].value, &prm->list[i+4], pbcs->capalities[pbcs->count].length);
#ifdef DEBUG					
					hexdump(LOG_INFO, pbcs->capalities[pbcs->count].value, pbcs->capalities[pbcs->count].length);
#endif					
				}
				pbcs->count++;
			}
			i = i + 2 + prm->list[i+1];			
		}
	}
	else
		log_fatal( "extractBgpCapabilities: malloc failed");
	
	return pbcs;
}

/*
 * Check if one paticular capability is existing with all the received capabilities.
 * If the checkValueLen != 0, also check the capability value with expected value
 * Return Value:
 * 1 means check fails
 * 0 means check passes
 */
int checkOneBgpCapability(PBgpCapabilities pbcs, u_char code, int checkValueLen, u_char *checkValue)
{
	int i;
	for(i=0; i<pbcs->count; i++)
	{
		if(pbcs->capalities[i].code == code)
		{
			if(checkValueLen == 0)
				return 0;
			else
			{
				if( memcmp(checkValue, pbcs->capalities[i].value, checkValueLen) == 0 )
					return 0;
			}
		}		
	}
	return 1;
}


/*--------------------------------------------------------------------------------------
 * Purpose: check BGP open message for 4-byte ASN capability
 * Input:  capabilities, as number from Open message
 * Output:  AS number from cpability of AS number from Open message
 * Mikhail Strizhov @ Oct 21, 2011
 * -------------------------------------------------------------------------------------*/
u_int32_t getASfromBgpCapability(PBgpCapabilities pbcs, u_int16_t as)
{
  int i = 0;
  for (i=0; i<pbcs->count; i++)
  {
    // fourbytesASnumber defined in bgppacket.h
    if(pbcs->capalities[i].code == fourbytesASnumber )
    {
      return ( ntohl ( (*(u_int32_t *) (&pbcs->capalities[i].value))) );
    }		
  }
  log_warning("Reveiced BGP open message does not include 4-byte AS capability!");
  return (ntohs(as));
}


/*
 *  * Check if one paticular capability is existing in announced capabilities.
 *   * If the checkValueLen != 0, also check the capability value with expected value
 *    * Return Value:
 *     * 1 means check fails
 *      * 0 means check passes
 *       */
int checkOneBgpAnnCapability(PBgpCapability  *announceCaps, int numOfAnnCaps, u_char code, int checkValueLen, u_char *checkValue)
{
        int i;
        for(i=0; i<numOfAnnCaps; i++)
        {   
                if(announceCaps[i]->code == code)
                {   
                        if(checkValueLen == 0)
                                return 0;
                        else
                        {   
                                if( memcmp(checkValue, announceCaps[i]->value, checkValueLen) == 0 ) 
                                        return 0;
                        }   
                }    
        }   
        return 1;
}



/*
 * Return the number of one paticular capability with all the received capabilities.
 * Return Value:
 * Numer of Requesting Capability
 */
int numOfOneBgpCapability(PBgpCapabilities pbcs, u_char code)
{
	int i;
	int n = 0;
	for(i=0; i<pbcs->count; i++)
	{
		if(pbcs->capalities[i].code == code)
			n++;		
	}
	return n;
}	

/*
 * Return the ith afi/safi  if there are multiple MP capabilities 
 * Return Value: afi&safi
 */
u_int32_t afisafiBGPOpenParameter(PBgpCapabilities pbcs, int index)
{
	int i;
	int n = 0;
	u_int32_t afisafi=0;
	for(i=0; i<pbcs->count; i++)
	{
		if(pbcs->capalities[i].code == multiprotocolCapability)
		{
			if(index == n)
			{
				memcpy( &afisafi, pbcs->capalities[i].value, 4);
				break;
			}
			n++;		
		}	
	}
	return afisafi;
}	
 
/*
 * Create a generic BGP Capability
 * Return Value: The created capability
 */
PBgpCapability createGenericBGPCapability(u_char code, u_char length, u_char *value)
{
	PBgpCapability pbc = malloc( sizeof( struct BGPCapabilityStruct ));
	if ( pbc != NULL )
	{	
		pbc->code = code;
		pbc->length = length;
		if(length != 0)
			memcpy(pbc->value, value, length);
	}
	else
		log_fatal( "createGenericBGPCapability: malloc failed");
	return pbc;
}

/*
 * Add a new capability to the BGP open parameters
 * Return Value: NULL
 */

void addOneBGPNewCapability(PBgpOpenParameters prm, PBgpCapability pbc)	
{
	if ( prm->length + 8 < bufferLength )
	{
		prm->list[prm->length+0] = capabilityParameter; 
		prm->list[prm->length+1] = 2 + pbc->length; // parameter length
		prm->list[prm->length+2] = pbc->code; 
		prm->list[prm->length+3] = pbc->length; // capability length
		memcpy(&prm->list[prm->length+4], pbc->value, pbc->length);
		prm->length = prm->length + 4 + pbc->length; 
	}
	else
		log_err("addOneCapability: invalid capability length");
	
}

/*
 * create a new MP capability
 * Return Value: NULL
 */
PBgpCapability createMultiProtocolCapability( int afi, int safi )
{
	u_char afisafi[4];
	afisafi[0] = afi / unsignedCharMax;
	afisafi[1] = afi % unsignedCharMax;
	afisafi[2] = 0;
	afisafi[3] = safi % unsignedCharMax;
	
	PBgpCapability pbc = createGenericBGPCapability(multiprotocolCapability, 4, afisafi);
	return pbc;
}

void 
addMultiProtocolCapability( PBgpOpenParameters prm, int afi, int safi )
{
	/* 
	 */
	if ( prm->length + 8 < bufferLength )
	{
		prm->list[prm->length+0] = capabilityParameter; 
		prm->list[prm->length+1] = 6; // parameter length
		prm->list[prm->length+2] = multiprotocolCapability; 
		prm->list[prm->length+3] = 4; // capability length
		prm->list[prm->length+4] = afi / unsignedCharMax;
		prm->list[prm->length+5] = afi % unsignedCharMax;
		prm->list[prm->length+6] = 0;
		prm->list[prm->length+7] = safi % unsignedCharMax;
		prm->length = prm->length + 8; 
#ifdef DEBUG
		debug("addMultiprotcolCapability", "");
		hexdump(LOG_DEBUG, prm, prm->length);
#endif
	}
	else
		log_err("writeBGPOpenParameters: invalid length");
}

void destroyBGPCapabilities( PBgpCapabilities pbcs )
{
	if ( pbcs != NULL ) 
		free( pbcs );
} 

/*--------------------------------------------------------------------------------------
 * Purpose: functions to manipulate BGP update message
 * He Yan @ June 15, 2008   
 * -------------------------------------------------------------------------------------*/
PBgpUpdate 
createBGPUpdate()
{	
	PBgpUpdate update = malloc( sizeof( struct BGPUpdateStruct ) );
	if ( update != NULL )
	{
		memset( update, 0, sizeof( struct BGPUpdateStruct ) );
	}
	else
		log_fatal( "createBGPUpdate: malloc failed");
	return( update ); 
}

void 
destroyBGPUpdate( PBgpUpdate u )
{
	if ( u )
		free(u);
}


PBgpUpdate 
readBGPUpdate( int socket, int len )
{
	PBgpUpdate update = createBGPUpdate( );
	if ( update )
	{
		int l = readn ( socket, update, len );
		update->datalen = len;
		if ( l != len )
		{
			log_err( "readBGPUpdate"," invalid length");
#ifdef DEBUG			
			hexdump(LOG_ERR, update, l);
#endif			
			free( update );
			update = NULL;
		}
		else
		{
#ifdef DEBUG
			debug("readBGPUpdate", "");
			hexdump(LOG_DEBUG, update, l);
#endif
		}
	}
	else
		log_fatal("readBGPUpdate: malloc failed");
	return(update);	
}

ssize_t 
writeBGPUpdate( PBgpUpdate u, int socket)
{
	/* NOTE: not implemented
	 */
	log_fatal( "writeBGPUpdate: not implemented");
	return(0);
}

/*--------------------------------------------------------------------------------------
 * Purpose: functions to manipulate BGP refresh message
 * He Yan @ June 15, 2008   
 * -------------------------------------------------------------------------------------*/
int
lengthBGPRefresh( PBgpRefresh r )
{
	return( sizeof ( r ) );
}

PBgpRefresh 
createBGPRefresh( u_int32_t afisafi)
{
	PBgpRefresh refresh = malloc( sizeof( struct BGPRefreshStruct ) );
	if ( refresh != NULL )
	{
		// fill in the structure
		memcpy( refresh, &afisafi, 4);				
#ifdef DEBUG
		debug("createBGPRefresh", "");
		hexdump(LOG_DEBUG, refresh, sizeof( struct BGPRefreshStruct ) );
#endif
	}
	else
		log_fatal( "createBGPRefresh: malloc failed");
		
	return( refresh ); 
}

void 
destroyBGPRefresh( PBgpRefresh r )
{
	if ( r != NULL )
		free( r );
}

PBgpRefresh 
readBGPRefresh( int socket )
{
	PBgpRefresh refresh = createBGPRefresh( 0 ); // dummy AFI/SAFI
	if ( refresh )
	{
		int len = sizeof ( struct BGPRefreshStruct );
		int l = readn( socket, refresh, len );
		if ( l != len )
		{
			log_err("readBGPRefresh"," invalid length");
#ifdef DEBUG			
			hexdump( LOG_ERR, refresh, l );
#endif			
			free( refresh );
			refresh = NULL;
		}
		else
		{
#ifdef DEBUG
			debug("readBGPRefresh", "");
			hexdump(LOG_DEBUG, refresh, l);
#endif
		}
	}	
	else
		log_fatal( "readBGPRefresh: malloc failed");	
	return(refresh);
}

ssize_t 
writeBGPRefresh( PBgpRefresh ref, int socket )
{
	int refl = sizeof( struct BGPRefreshStruct );
	int l = writen(socket, ref, refl );
	if ( l != refl )
	{
		log_err("writeBGPRefresh: write failed");
#ifdef DEBUG		
		hexdump(LOG_ERR, ref, l );
#endif		
		l = -1;
	}
	else
	{
#ifdef DEBUG
		debug("writeBGPRefresh", "");
		hexdump(LOG_DEBUG, ref, refl);
#endif
	} 
	return( l );
}

/*--------------------------------------------------------------------------------------
 * Purpose: functions to manipulate BGP notification message
 * He Yan @ June 15, 2008   
 * -------------------------------------------------------------------------------------*/
int 
lengthBGPNotification( PBgpNotification notify )
{
	return( 2 + notify->datalen );
}

PBgpNotification 
createBGPNotification(int code, int subcode, char *message, int len)
{
	PBgpNotification notify = malloc( sizeof( struct BGPNotificationStruct ) );
	if ( notify != NULL)
	{
		memset( notify, 0, sizeof( notify ) );
		notify->errorCode = code;
		notify->errorSubcode = subcode;
		notify->datalen = len;
		if ( len > 0 )
			memcpy( &notify->data, message, len );				
#ifdef DEBUG
		debug( "createBGPNotification, code=%i, subcode=%i","",code,subcode);
		hexdump(LOG_DEBUG, notify, 2 + notify->datalen);
#endif
	}
	else
		log_fatal("createBGPNotification: malloc failed");
	return( notify ); 
}

void 
destroyBGPNotification( PBgpNotification notify )
{
	if ( notify != NULL )
		free( notify );
}

PBgpNotification 
readBGPNotification( int socket, int len )
{
	PBgpNotification notify = createBGPNotification( 0, 0, NULL, 0 );
	if ( notify != NULL)
	{
		int l = readn( socket, notify, len );
		notify->datalen = len-2;
		if ( l != len )
		{
			log_err( "readBGPNotification invalid length");
#ifdef DEBUG			
			hexdump(LOG_ERR, notify, l);
#endif			
			free( notify );
			notify = NULL;
		}
		else
		{
#ifdef DEBUG
			debug( "readBGPNotification", "");
			hexdump(LOG_DEBUG, notify, l);
#endif
		}
	}	
	else
		log_fatal( "readBGPNotification: malloc failed");
	return(notify);
}

ssize_t 
writeBGPNotification( PBgpNotification ntf, int socket )
{
	int ntfl = lengthBGPNotification( ntf );
	int l = writen(socket, ntf, ntfl );
	if ( l != ntfl )
	{
		log_err("writeBGPNotification: write failed, %x",ntf);
#ifdef DEBUG		
		hexdump(LOG_ERR, ntf, l);
#endif		
		l = -1;
	}
	else
	{
#ifdef DEBUG
		debug("writeBGPNotification %x","",ntf);
		hexdump(LOG_DEBUG, ntf, ntfl);
#endif
	} 
	return( l );
}
