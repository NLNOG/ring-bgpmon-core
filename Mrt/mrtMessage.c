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
 *  File: mrtinstance.c
 * 	Authors: He Yan, Dan Massey, Mikhail, Cathie Olschanowsky
 *
 *  Date: Oct 7, 2008 
 *  Edited: April 2012, Cathie
 */

#include "mrtMessage.h" 

//#define DEBUG

/******************************************************************************
* MRT_header_invalid
* Purpose: validate the MRT header as best as possible
* Return: valid = 0
*         invalid type 1
*         invalid subtype 2
*         invalide length 3
******************************************************************************/
int
MRT_header_invalid(MRTheader *mrtHeader){

  // the timestamp can't be validated
  // the subtype must be one of the ones defined in mrt
  switch(mrtHeader->type){
    // there is no length validation done for this type
    case OSPFv2:
      switch(mrtHeader->subtype){
        case OSPF_STATE_CHANGE:
        case OSPF_LSA_UPDATE:
          return 0;
        default:
          return 2;
      }
    // there is no length validation for this type
    case TABLE_DUMP:
      switch(mrtHeader->subtype){
        case AFI_IPv4:
        case AFI_IPv6:
          return 0;
        default:
          return 2;
      }
    // there is no length validation for this type
    case TABLE_DUMP_V2:
      switch(mrtHeader->subtype){
        case PEER_INDEX_TABLE:
        case RIB_IPV4_UNICAST:
        case RIB_IPV4_MULTICAST:
        case RIB_IPV6_UNICAST:
        case RIB_IPV6_MULTICAST:
        case RIB_GENERIC:
          return 0;
        default:
          return 2;
      } 
    // a valid message will be less than (4096: BGP_MAX + 12 (header))
    // TODO: revisit, this isn't true for all of them
    case BGP4MP:
      switch(mrtHeader->subtype){
        case BGP4MP_STATE_CHANGE:
        case BGP4MP_MESSAGE:
        case BGP4MP_MESSAGE_AS4:
        case BGP4MP_STATE_CHANGE_AS4:
        case BGP4MP_MESSAGE_LOCAL:
        case BGP4MP_MESSAGE_AS4_LOCAL:
          if(mrtHeader->length > MAX_MRT_LENGTH){
            return 3;
          }
          return 0;
        default:
          return 2;
      }
    // draft 15 does not have info on this
    case BGP4MP_ET:
      switch(mrtHeader->subtype){
        default:
          return 2;
      }
    // this case does not have length validation 
    // and it does not define subtypes
    case ISIS:
    case ISIS_ET:
      return 0;

    // this case does not have length validation
    case OSPFv3:
    case OSPFv3_ET:
      switch(mrtHeader->subtype){
        case OSPF_STATE_CHANGE:
        case OSPF_LSA_UPDATE:
          return 0;
        default:
          return 2;
      }
  }
  // unable to match type information
  return 1;
}
