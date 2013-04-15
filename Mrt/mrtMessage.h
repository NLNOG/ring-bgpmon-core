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
 *  File: mrtMessage.h
 * 	Authors: He Yan, Dan Massey, Cathie Olschanowsky
 *  Date: Oct 7, 2008 
 */

#ifndef MRTMESSAGE_H_
#define MRTMESSAGE_H_

// needed for ADDR_MAX_CHARS
#include "../Util/bgpmon_defaults.h"
#include <inttypes.h>

#define UNKASNLEN -1
#define MAX_MRT_LENGTH  4108 // 4096 + 12
#define MRT_MRT_MSG_MIN_LENGTH  35 // BGP header (19)  MESSAGE header (16)
#define MRT_HEADER_LENGTH 12

enum mrt_type{
  OSPFv2 = 11,
  TABLE_DUMP = 12,
  TABLE_DUMP_V2 = 13,
  BGP4MP = 16,
  BGP4MP_ET = 17,
  ISIS = 32,
  ISIS_ET = 33,
  OSPFv3 = 48,
  OSPFv3_ET = 49,
};

enum BGP4MP_subtype{
  BGP4MP_STATE_CHANGE = 0,
  BGP4MP_MESSAGE = 1,
  BGP4MP_MESSAGE_AS4 = 4,
  BGP4MP_STATE_CHANGE_AS4 = 5,
  BGP4MP_MESSAGE_LOCAL = 6,
  BGP4MP_MESSAGE_AS4_LOCAL = 7
};

enum TABLE_DUMP_V2_subtype{
  PEER_INDEX_TABLE = 1,
  RIB_IPV4_UNICAST = 2,
  RIB_IPV4_MULTICAST = 3,
  RIB_IPV6_UNICAST = 4,
  RIB_IPV6_MULTICAST = 5,
  RIB_GENERIC = 6
};

enum OSPFv2_subtype{
  OSPF_STATE_CHANGE = 0,
  OSPF_LSA_UPDATE = 1
};

enum TABLE_DUMP_subtype{
  AFI_IPv4 = 1,
  AFI_IPv6 = 2
};

struct MRTStruct
{
        uint32_t               timestamp;
        uint16_t               type;
        uint16_t               subtype;
        uint32_t               length;
} ;
typedef struct MRTStruct MRTheader;

struct MRTMessageStruct
{
  uint32_t peerAs;
  uint32_t localAs;
  uint16_t interfaceIndex;
  uint16_t addressFamily;
  uint8_t  peerIPAddress[16];
  uint8_t  localIPAddress[16];
  char  peerIPAddressString[ADDR_MAX_CHARS];
  char  localIPAddressString[ADDR_MAX_CHARS];
};
typedef struct MRTMessageStruct MRTmessage;

/******************************************************************************
* MRT_header_invalid
* Purpose: validate the MRT header as best as possible
* Return: valid = 0
*         invalid type 1
*         invalid subtype 2
*         invalide length 3
******************************************************************************/
int MRT_header_invalid(MRTheader *mrtHeader);

#endif
