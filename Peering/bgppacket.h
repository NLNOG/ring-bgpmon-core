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
 *  File: bgppacket.h
 * 	Authors: He Yan
 *  Data: Apr 22, 2008  
 */

#ifndef BGPPACKET_H_
#define BGPPACKET_H_

/*
 * Defines routines to implement BGP message formats from RFC 4271, 2918. 
 * 
 * Includes functions for both reading and writing.
 * 
 * Packets are handled in pieces via separate types and routine specific
 * to each type.
 * 
 * Dynamically allocates the memory for the types when a pointer is returned.
 * The caller is responsible for freeing the memory at the appropriate time. 
 * 
 * The routines constitute a partial implementation of packet handling 
 * for the monitor.  
 *   
 */

#include <sys/types.h>
#include <arpa/inet.h>

/* 
 * BGP header shared by all message formats.
 *  
 * A header should be read or written first.  When read, the message
 * type determines which routines to use next and the length will
 * indicate how much additional information is included.
 */
#define maskLength 16
	 
struct BGPHeaderStruct
{
	u_int8_t	mask[maskLength];
	u_int16_t	length;
	u_int8_t	type;
}__attribute__((__packed__));

typedef struct 		BGPHeaderStruct 		*PBgpHeader;

PBgpHeader 	createBGPHeader			( int ); 
PBgpHeader 	readBGPHeader				( int , int );

ssize_t 		writeBGPHeader			( PBgpHeader , int );
void 			setBGPHeaderType		( PBgpHeader , u_int8_t );
void 			setBGPHeaderLength	( PBgpHeader , u_int16_t );
u_int8_t 	getBGPHeaderType		( PBgpHeader );
u_int16_t 	getBGPHeaderLength	( PBgpHeader );
void 			destroyBGPHeader		( PBgpHeader ); 

/*
 * BGP open message
 * 
 * Processed after the BGP header.  Optional open paraemters are
 * processed separately afterwards.  
 * 
 * Note that on writing the header length must be determined and set 
 * after the other components of the message are created, 
 * then the portions of the message can be sent out in order.
 */
#define BGPVersion 4
	
struct BGPOpenStruct
{
	u_int8_t			version;
	u_int16_t			as;
	u_int16_t			holdTime;
	u_int32_t			identifier;
	u_int8_t			optionalParameterLength;
	u_int8_t			optionalParameter[];
}__attribute__((__packed__));

typedef struct 		BGPOpenStruct				 							*PBgpOpen;


/*BGP Open Parameters*/
#define bufferLength 4096
struct BGPOpenParametersStruct 
{
	u_char		list[bufferLength]; 
	int 		length; 
};
typedef struct 						BGPOpenParametersStruct 			*PBgpOpenParameters;


PBgpOpen 		createBGPOpen				(  u_int8_t, u_int32_t, u_int16_t, u_int32_t, PBgpOpenParameters );
PBgpOpen 		readBGPOpen												( int  );

ssize_t 		writeBGPOpen											( PBgpOpen, int );
int 				lengthBGPOpen											( PBgpOpen );
void 			setBGPOpenVersion									( PBgpOpen, u_int8_t );
void 			setBGPOpenAutonomousSystem				( PBgpOpen, u_int16_t );
void 			setBGPOpenHoldTime								( PBgpOpen, u_int16_t );
void 			setBGPOpenIdentifier							( PBgpOpen, u_int32_t );
void 			setBGPOpenOptionalParameterLength	( PBgpOpen, u_int8_t );
u_int8_t 	getBGPOpenVersion									( PBgpOpen );
u_int16_t 	getBGPOpenAutonomousSystem				( PBgpOpen );
u_int16_t 	getBGPOpenHoldTime								( PBgpOpen );
u_int32_t 	getBGPOpenIdentifier							( PBgpOpen );
u_int8_t		getBGPOpenOptionalParameterLength	( PBgpOpen );
void 			destroyBGPOpen										( PBgpOpen );

/* 
 * BGP Capabilities and BGP Open Parameters 
 *
 * capability advertisements (RFC 3392),
 * multiprotocol capabilities (RFC 2858),
 * route refresh capability (RFC 2918).
 */


#define capabilityParameter 2
#define multiprotocolCapability 1
#define routeRefreshCapability 2
#define standardRouteRefresh 5
#define fourbytesASnumber 65
#define ciscoRefreshCapability 0x80
#define ciscoRouteRefresh 0x80
#define unsignedCharMax 256
#define maxCapabilityValueLen 256
#define maxNumOfCapabilities 256
#define AS_TRANS 23456



/*BGP Capability*/
struct BGPCapabilityStruct 
{
	u_char code;
  	u_char length;
	u_char value[maxCapabilityValueLen]; 
}__attribute__((__packed__));


struct BGPCapabilitiesStruct 
{
	int count;
	struct BGPCapabilityStruct capalities[maxNumOfCapabilities];
}__attribute__((__packed__));


typedef struct 						BGPCapabilityStruct 			*PBgpCapability;
typedef struct 						BGPCapabilitiesStruct 			*PBgpCapabilities;


PBgpOpenParameters	createBGPOpenParameters			();
ssize_t				writeBGPOpenParameters				( PBgpOpenParameters, int );
int 					lengthBGPOpenParameters			( PBgpOpenParameters );
PBgpOpenParameters	extractBGPOpenParameters			( PBgpOpen );
PBgpCapabilities	extractBgpCapabilities 					(PBgpOpenParameters);
int 				checkOneBgpCapability				(PBgpCapabilities, u_char, int, u_char *);
int checkOneBgpAnnCapability(PBgpCapability  *announceCaps, int numOfAnnCaps, u_char code, int checkValueLen, u_char *checkValue);
int 				numOfOneBgpCapability				(PBgpCapabilities, u_char);
PBgpCapability 		createGenericBGPCapability			(u_char, u_char, u_char *);
void					addOneBGPNewCapability( PBgpOpenParameters, PBgpCapability ); 
u_int32_t 			afisafiBGPOpenParameter				( PBgpCapabilities, int );
PBgpCapability createMultiProtocolCapability( int afi, int safi );
void 	addMultiProtocolCapability( PBgpOpenParameters prm, int afi, int safi );
void 				destroyBGPOpenParameters			( PBgpOpenParameters );
void 				destroyBGPCapabilities				( PBgpCapabilities );


/*
 * BGP update message.
 * 
 * The contents of the message are ignored by the monitor, the information is 
 * simply passed along to other portions of the system for processing.
 */
 
struct BGPUpdateStruct 
{
	u_char		data[bufferLength];
	int				datalen;
}__attribute__((__packed__));

typedef struct 		BGPUpdateStruct 	*PBgpUpdate;

PBgpUpdate 	createBGPUpdate		();
PBgpUpdate 	readBGPUpdate			( int, int );

ssize_t 		writeBGPUpdate		( PBgpUpdate, int  );
void 			destroyBGPUpdate	( PBgpUpdate );

/* 
 * BGP Notification Message
 */
struct BGPNotificationStruct
{
	u_int8_t				errorCode;
	u_int8_t				errorSubcode;
	u_char					data[bufferLength];
	int							datalen;
}__attribute__((__packed__)); 
typedef struct 					BGPNotificationStruct 		*PBgpNotification;

PBgpNotification 	createBGPNotification			();
PBgpNotification 	readBGPNotification				( int, int );

ssize_t 					writeBGPNotification			( PBgpNotification, int );
int 							lengthBGPNotification			( PBgpNotification );
void 						destroyBGPNotification		( PBgpNotification );

/*
 * BGP Route Refresh message
 */
struct BGPRefreshStruct
{
	u_int16_t		afi;
	u_int8_t		reserved;
	u_int8_t		safi;
}__attribute__((__packed__));

typedef	struct 			BGPRefreshStruct 	*PBgpRefresh;

PBgpRefresh 	createBGPRefresh	( u_int32_t );
PBgpRefresh 	readBGPRefresh		( int );

ssize_t 			writeBGPRefresh		( PBgpRefresh,  int  );
int 					lengthBGPRefresh	( PBgpRefresh );
void 				destroyBGPRefresh	( PBgpRefresh );

/*--------------------------------------------------------------------------------------
 * Purpose: check BGP open message for 4-byte ASN capability
 * Input:  capabilities, as number from Open message
 * Output:  AS number from cpability of AS number from Open message
 * Mikhail Strizhov @ Oct 21, 2011
 * -------------------------------------------------------------------------------------*/
u_int32_t getASfromBgpCapability(PBgpCapabilities pbcs, u_int16_t as);

#endif /*BGPPACKET_H_*/
