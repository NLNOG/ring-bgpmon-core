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
 *  File: bgp.h
 * 	Authors: Catherine Olschanowsky
 *  Date: Aug. 30, 2011
 */

#ifndef BGP_H_
#define BGP_H_

#include <stdint.h>

#define BGP_MAX_MSG_LEN 		4096
#define BGP_MARKER_LEN 			16
#define BGP_HEADER_LENGTH               BGP_MARKER_LEN+3 
#define BGP_MAX_PREFIX_LEN_BYTES_IPV4   4           
#define BGP_MAX_PREFIX_LEN_BYTES_IPV6   16          

// performs a mask of a one byte field to determine
// the length of the Path attributes length field (1 or 2 bytes)
#define BGP_PAL_LEN(a) ((a&0x10) ? 2 : 1)
// this is essentially a cheap way to do a memcopy or memmove
// the pointer is cast the right sized pointer which enables
// a direct copy
#define BGP_ASSIGN_2BYTES(a,b) *((uint16_t*)a)=htons(b)
#define BGP_ASSIGN_4BYTES(a,b) *((uint32_t*)a)=htonl(b)
#define BGP_READ_2BYTES(a,b) a=ntohs(*((uint16_t*)&b))
#define BGP_READ_4BYTES(a,b) a=ntohl(*((uint32_t*)&b))

/*     ************** NOTE to programmers *******************
   below are each of the structs that are used to 
   represent and build bgp messages.
   they should not be accessed, directly, but 
   the functions associated with them should be
   used instead.
******************************************************************/

/***********************************************************************
4.1. Message Header Format


   Each message has a fixed-size header.  There may or may not be a data
   portion following the header, depending on the message type.  The
   layout of these fields is shown below:

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      +                                                               +
      |                                                               |
      +                                                               +
      |                           Marker                              |
      +                                                               +
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |          Length               |      Type     |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

      Marker:

         This 16-octet field is included for compatibility; it MUST be
         set to all ones.

      Length:

         This 2-octet unsigned integer indicates the total length of the
         message, including the header in octets.  Thus, it allows one
         to locate the (Marker field of the) next message in the TCP
         stream.  The value of the Length field MUST always be at least
         19 and no greater than 4096, and MAY be further constrained,
         depending on the message type.  "padding" of extra data after
         the message is not allowed.  Therefore, the Length field MUST
         have the smallest value required, given the rest of the
         message.

      Type:

         This 1-octet unsigned integer indicates the type code of the
         message.  This document defines the following type codes:

                              1 - OPEN
                              2 - UPDATE
                              3 - NOTIFICATION
                              4 - KEEPALIVE

         [RFC2918] defines one more type code.
****************************************************************************/
enum BGPType
{
  BGP_OPEN         = 1,
  BGP_UPDATE       = 2,
  BGP_NOTIFICATION = 3,
  BGP_KEEPALIVE    = 4
} BGPTypes;

enum BGP_PATTR_Type
{
  BGP_PATTR_ORIGIN  = 1,
  BGP_PATTR_AS_PATH = 2,
  BGP_PATTR_NEXT_HOP = 3,
  BGP_PATTR_MULTI_EXITDISC = 4,
  BGP_PATTR_LOCAL_PREF = 5,
  BGP_PATTR_ATOMIC_AGGREGATE = 6,
  BGP_PATTR_AGGREGATOR = 7
} BGP_PATTR_Types;

struct BGPMessageStruct
{
  uint16_t    length;
  enum BGPType type;
  void         *typeSpecific;
};
typedef struct BGPMessageStruct BGPMessage;

/***********************************************************************
         Reachability information is encoded as one or more 2-tuples of
         the form <length, prefix>, whose fields are described below:

                  +---------------------------+
                  |   Length (1 octet)        |
                  +---------------------------+
                  |   Prefix (variable)       |
                  +---------------------------+
************************************************************************/
struct BGPNLRIStruct
{
  uint8_t length;
  uint8_t prefix[BGP_MAX_PREFIX_LEN_BYTES_IPV4];
};
typedef struct BGPNLRIStruct BGPNLRI;

/***********************************************************************
      Withdrawn Routes:

         This is a variable-length field that contains a list of IP
         address prefixes for the routes that are being withdrawn from
         service.  Each IP address prefix is encoded as a 2-tuple of the
         form <length, prefix>, whose fields are described below:

                  +---------------------------+
                  |   Length (1 octet)        |
                  +---------------------------+
                  |   Prefix (variable)       |
                  +---------------------------+
***********************************************************************/
struct BGPWithdrawnRouteStruct
{
  uint8_t length;
  uint8_t prefix[BGP_MAX_PREFIX_LEN_BYTES_IPV4];
};
typedef struct BGPWithdrawnRouteStruct BGPWithdrawnRoute;

/************************************************************************
 A variable-length sequence of path attributes is present in
         every UPDATE message, except for an UPDATE message that carries
         only the withdrawn routes.  Each path attribute is a triple
         <attribute type, attribute length, attribute value> of variable
         length.

         Attribute Type is a two-octet field that consists of the
         Attribute Flags octet, followed by the Attribute Type Code
         octet.

               0                   1
               0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
               |  Attr. Flags  |Attr. Type Code|
               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

************************************************************************/
struct BGPPathAttributeStruct
{
  uint8_t  flags;
  uint8_t  code;
  uint16_t length;
  uint8_t  *value;
  
};
typedef struct BGPPathAttributeStruct BGPPathAttribute;

/**************************************************************************
4.3. UPDATE Message Format

      +-----------------------------------------------------+
      |   Withdrawn Routes Length (2 octets)                |
      +-----------------------------------------------------+
      |   Withdrawn Routes (variable)                       |
      +-----------------------------------------------------+
      |   Total Path Attribute Length (2 octets)            |
      +-----------------------------------------------------+
      |   Path Attributes (variable)                        |
      +-----------------------------------------------------+
      |   Network Layer Reachability Information (variable) |
      +-----------------------------------------------------+

**************************************************************************/
struct BGPUpdStruct
{
  uint16_t withdrawnRoutesCount;
  BGPWithdrawnRoute **withdrawnRoutes;
  uint16_t totalPathAttsCount;
  BGPPathAttribute  **pathAtts;
  uint16_t nlriCount;
  BGPNLRI **nlris;
};
typedef struct BGPUpdStruct BGPUpdate;

/************************************************************************
                Public Function Definitions
************************************************************************/
BGPMessage* BGP_createMessage(enum BGPType);
int         BGP_addWithdrawnRouteToUpdate(BGPMessage* msg, uint16_t length, const uint8_t *prefix);
int         BGP_addPathAttributeToUpdate(BGPMessage* msg, uint8_t flags,uint8_t code, uint16_t length, const uint8_t *value);
int         BGP_addNLRIToUpdate(BGPMessage* msg, uint8_t length, const uint8_t *value);
int         BGP_serialize(uint8_t* bits,BGPMessage* msg,int asn4to2translation);
int         BGP_freeMessage(BGPMessage*);
int         BGP_calculateLength(BGPMessage* msg,int asn4to2translation);

#endif
