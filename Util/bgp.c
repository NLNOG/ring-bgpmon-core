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
 *  File: bgpmon_formats.c
 * 	Authors: He Yan
 *  Date: May 6, 2008
 */

/*
 *  Utility functions that work for our internal struct BMF
 */

#include "bgp.h"
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "log.h"

/******************************************************************************
Name: BGP_createMessage
Input: Message Type (only Update is supported)
Output: A pointer to the allocated space
Author: Catherine Olschanowsky
******************************************************************************/
BGPMessage*
BGP_createMessage(enum BGPType type)
{
  BGPMessage *msg = calloc(1,sizeof(BGPMessage));
  if(msg == NULL){
    return NULL;
  }
  msg->type = type;

  // if the type is an update allocate an update struct
  switch ( type ){
    case BGP_UPDATE:
      {
        BGPUpdate *upd = calloc(1,sizeof(BGPUpdate));
        if(upd == NULL){
          free(msg);
          return NULL;
        }
        msg->typeSpecific = upd;
      }
      break;
    default:
      free(msg);
      msg = NULL;
  }
  return msg;
}

/******************************************************************************
Name: BGP_addWithdrawnRouteToUpdate
Input: A pointer to the BGPMessate, the length of the prefix, the prefix
Output: 0 = success, -1 = failure
Author: Catherine Olschanowsky
******************************************************************************/
int
BGP_addWithdrawnRouteToUpdate(BGPMessage* msg, uint16_t length, const uint8_t *prefix)
{
  // standard error checks 
  if(msg == NULL){ return -1; }
  if(msg->type != BGP_UPDATE){ return -1; }
  if(msg->typeSpecific == NULL){ return -1; } 
  if(length == 0) { return 0; }
  if(prefix == NULL) { return -1; }
  if(length > BGP_MAX_PREFIX_LEN_BYTES_IPV4*8){ return -1; };


  BGPUpdate *update = (BGPUpdate*)msg->typeSpecific;
 
  // add the information
  update->withdrawnRoutesCount += 1;
  BGPWithdrawnRoute *wRoute = calloc(1,sizeof(BGPWithdrawnRoute));
  if(wRoute == NULL){
    return -1;
  }
  wRoute->length = length;
  int length_bytes = length/8;
  if(length%8 != 0){
    length_bytes += 1;
  }
  memmove(wRoute->prefix,prefix,length_bytes);
  BGPWithdrawnRoute **tmp = update->withdrawnRoutes;
  update->withdrawnRoutes = calloc(update->withdrawnRoutesCount,sizeof(BGPWithdrawnRoute*));
  if(NULL == update->withdrawnRoutes){
    free(wRoute);
    update->withdrawnRoutes = tmp;
    return -1;
  }
  int i;
  for(i=0; i<(update->withdrawnRoutesCount-1); i++){
    update->withdrawnRoutes[i] = tmp[i];
  }
  free(tmp);

  update->withdrawnRoutes[update->withdrawnRoutesCount-1] = wRoute;
  return 0; 
}

/******************************************************************************
Name: BGP_addPathAttributeToUpdate
Input: A pointer to the BGPMessage, the type length and value
Output: 0 = success, -1 = failure
Author: Catherine Olschanowsky
******************************************************************************/
int
BGP_addPathAttributeToUpdate(BGPMessage* msg,uint8_t flags, uint8_t code, uint16_t length, const uint8_t *value)
{
  // standard error checks 
  if(msg == NULL){return -1; }
  if(msg->type != BGP_UPDATE){ return -1; }
  if(msg->typeSpecific == NULL){ return -1;}
  if(length == 0){ return 0;}
  if(value == NULL){ return -1; } 

  BGPUpdate *update = (BGPUpdate*)msg->typeSpecific;

  // create some space for the data
  BGPPathAttribute *bgpPA = calloc(1,sizeof(BGPPathAttribute));
  if(bgpPA == NULL){
    return -1;
  }
  bgpPA->code = code;
  bgpPA->flags = flags;
  bgpPA->length = length;
  bgpPA->value = calloc(length,1);
  if(bgpPA->value == NULL){
    free(bgpPA);
    return -1;
  }
  memmove(bgpPA->value,value,length);

  // the new attribute must be added in order based on the value
  // in the code field
  // only one attribute of each type is allowed.
  // if we see a duplicate -- perform a replacement
  BGPPathAttribute **tmp = update->pathAtts;
  update->pathAtts = calloc(update->totalPathAttsCount+1,sizeof(BGPPathAttribute*));
  int i;
  for(i=0; i<update->totalPathAttsCount && tmp[i]->code < bgpPA->code;i++){
      update->pathAtts[i] = tmp[i];
  }
  // either reason we exited the loop -- this is where the attribute belongs
  update->pathAtts[i] = bgpPA;
  // if we exited because we are at the end there isn't anything left to do
  if(i != update->totalPathAttsCount){
    // if we exited because we found a code greater than the one we are inserting
    // we need to insert and then continue with the loop
    if(tmp[i]->code > bgpPA->code){
      for( ;i<update->totalPathAttsCount;i++){
        update->pathAtts[i+1] = tmp[i];
      }
      update->totalPathAttsCount++;
    // we found an exact match -- in this case we need to continue the loop, but
    // ignore(free) the entry that was a duplicate
    }else{
      free(tmp[i]->value);
      free(tmp[i]);
      i++;
      for( ;i<update->totalPathAttsCount;i++){
        update->pathAtts[i] = tmp[i];
      }
    }
  }else{
    update->totalPathAttsCount++;
  }
  free(tmp);
  return 0;
}

/******************************************************************************
Name: BGP_addNLRIToUpdate
Input: A pointer to the BGPMessage,the lenght of the prefix (bits), value
Output: 0 = success, -1 = failure
Author: Catherine Olschanowsky
******************************************************************************/
int
BGP_addNLRIToUpdate(BGPMessage* msg, uint8_t length, const uint8_t *prefix)
{
  // standard error checks 
  if(msg == NULL){ return -1; }
  if(msg->type != BGP_UPDATE){ return -1; }
  if(msg->typeSpecific == NULL){ return -1; } 
  if(length == 0) { return 0; }
  if(prefix == NULL) { return -1; }
  if(length > BGP_MAX_PREFIX_LEN_BYTES_IPV4*8){ return -1; };

  BGPUpdate *update = (BGPUpdate*)msg->typeSpecific;

  // add the information
  update->nlriCount += 1;
  BGPNLRI *nlri = calloc(1,sizeof(BGPNLRI));
  if(nlri == NULL){
    return -1;
  }
  nlri->length = length;
  int length_bytes = length/8;
  if((length%8) != 0){
    length_bytes += 1;
  }
  memmove(nlri->prefix,prefix,length_bytes);

  BGPNLRI **tmp = update->nlris;
  update->nlris = calloc(update->nlriCount,sizeof(BGPNLRI*));
  if(NULL == update->nlris){
    free(nlri);
    update->nlris = tmp;
    return -1;
  }
  int i;
  for(i=0; i<(update->nlriCount-1); i++){
    update->nlris[i] = tmp[i];
  }
  free(tmp);

  update->nlris[update->nlriCount-1] = nlri;
  return 0; 
}

/******************************************************************************
Name: BGP_serialize
Input: A pointer to the BGPMessage, 1 to translate asns to 2 bytes, 0 to leave 
       them as 4 byte ASNs
Output: A pointer to the binary message (note that msg->length is changed
        to reflect the number of bytes. (not including the header)
Author: Catherine Olschanowsky
******************************************************************************/
int    
BGP_serialize(uint8_t* bits,BGPMessage* msg, int asnLen)
{
  // currently only support update
  if(msg->type != BGP_UPDATE){
    return -1;
  }
  BGPUpdate *update = (BGPUpdate*)msg->typeSpecific;


  // i will keep our place in the message as we populate
  int i;
  // l will be used to hold the place of a length field 
  // this way we can add the bits for that section
  // before we know the length and then go back
  int lm,l; // lm is the main length for the whole message
  // idx is for loops that don't move us along inthe msg
  int idx,idy;

  // add in the BGP header
  for(i=0;i<16;i++){
    bits[i] = 0xFF;
  }
  lm = i;
  i+=2;
  bits[i]=msg->type;
  i+=1;

  // Add the withdrawn routes
  // skip the length for now
  l=i;
  i+=2;
  for(idx=0;idx<update->withdrawnRoutesCount;idx++){
    bits[i]=update->withdrawnRoutes[idx]->length;
    i+=1;
    for(idy=0;idy<update->withdrawnRoutes[idx]->length/8;idy++){
      bits[i]=update->withdrawnRoutes[idx]->prefix[idy];
      i+=1;      
    } 
    if(update->withdrawnRoutes[idx]->length%8 != 0){
      bits[i]=update->withdrawnRoutes[idx]->prefix[idy];
      i+=1;      
    }
  }
  // Add the length back in: the 2 is here because of the length of the length field
  BGP_ASSIGN_2BYTES(&bits[l],(uint16_t)(i-l-2));
 
  // Add in the path attributes
  l = i; // l marks the spot for the length
  i+=2;  // i is incremented to leave the length spot alone
  for(idx=0;idx<update->totalPathAttsCount;idx++){
    bits[i] = update->pathAtts[idx]->flags;
    i+=1;
    bits[i] = update->pathAtts[idx]->code;
    i+=1;

    // this is the base case.
    // For all attributes other than AS_PATH we don't mess with them
    // we are looking at
    // <type, length, value>
    // type is flags+code which we took care of above
    // this logic is to determine if we have a length of one or two
    if(update->pathAtts[idx]->code != BGP_PATTR_AS_PATH){
      // Path Attribute Length (PAL) length is 1
      if(1 == (BGP_PAL_LEN(update->pathAtts[idx]->flags))){
        bits[i] = (uint8_t)update->pathAtts[idx]->length;
        i+=1;
      }else{
        // Path Attribute Length (PAL) length is 2
        BGP_ASSIGN_2BYTES(&bits[i],update->pathAtts[idx]->length);
        i+=2;
      }
      // now that we have the length we just copy the value over
      // byte at a time
      for(idy=0;idy<update->pathAtts[idx]->length;idy++){
        bits[i] = update->pathAtts[idx]->value[idy];
        i+=1;
      }
    }else{
      // AS_PATH is a well-known mandatory attribute that is composed
      // of a sequence of AS path segments.  Each AS path segment is
      // represented by a triple <path segment type, path segment
      // length, path segment value>.
      // The path segment type is a 1-octet length field with the
      // following values defined:
         // Value      Segment Type
         // 1         AS_SET: unordered set of ASes a route in the
                      // UPDATE message has traversed
         // 2         AS_SEQUENCE: ordered set of ASes a route in
                      // the UPDATE message has traversed
      // The path segment length is a 1-octet length field,
      // containing the number of ASes (not the number of octets) in
      // the path segment value field.
      // The path segment value field contains one or more AS
      // numbers, each encoded as a 2-octet length field.

      // the above text is taken from RFC 4271, however, this code must handle
      // AS_PATHs created by MRT and therefore, the ASes might be 4 bytes
      // this code is used to calculate the length of an AS_PATH assuming that
      // it is to be truncated from 4 bytes to 2 bytes -- I believe this is
      // only useful from the MRT module

      // http://tools.ietf.org/html/rfc4893 describes how to properly handle 
      // 4 byte ASNs that cannot be reduced to 2 bytes, however, this is not
      // really needed as we have a simplified case. Either, we have 2 byte
      // ASNs that have been promoted to 4 byte ASNs within MRT (and can therefore,
      // be truncated) or we have a router that can handle 4 byte ASNs in which 
      // case we leave them be

      // we can calculate the length by looking at the path segment length
      // multiplying that value by 2 and subtracting it from the length in the
      // path attribute header
      
      // <type, length, value>
      // type is flags+code which we took care of above
      // this logic is to determine if we have a length of one or two
      // if the Path attribute length (PAL) length is 1
      int l2 = i; // hold the place for the length (l is already in use)
      if((BGP_PAL_LEN(update->pathAtts[idx]->flags)) == 1){
        i+=1;
      }else{
        i+=2;
      }
      // now we look into creating the value
      // <path segment type, path segment length, path segment value>
      for(idy = 0; idy < update->pathAtts[idx]->length; ){
        bits[i] = update->pathAtts[idx]->value[idy]; // path segment type
        i++; idy++;
        int psl = bits[i] = update->pathAtts[idx]->value[idy]; // path segment length
        i++; idy++;
        // need to translate to 2 byte asns
        if(asnLen == 2){
          int idz;
          for(idz = 0; (idz/4) < psl; ){
            uint32_t tmp;
            BGP_READ_4BYTES(tmp,update->pathAtts[idx]->value[idy+idz]);
            BGP_ASSIGN_2BYTES(&bits[i],(uint16_t)tmp);
            i+=2;
            idz+=4;
          }
          idy+=idz;
        // can keep asns at 4 bytes (just copy)
        }else{
          int idz;
          for(idz = 0; idz < psl; idz++){
            bits[i] = update->pathAtts[idx]->value[idy+idz];
            i+=1;
          }
          idy+=idz;
        }
      }
      // go back and add in the length of this path attribute
      if((BGP_PAL_LEN(update->pathAtts[idx]->flags)) == 1){
        bits[l2] = (uint8_t)(i-l2-1);
      }else{
        BGP_ASSIGN_2BYTES(&bits[l2],(uint16_t)(i-l2-2));
      }
    }
  }
  // Add the length back in: the 2 is here because of the length of the length field
  BGP_ASSIGN_2BYTES(&bits[l],(uint16_t)(i-l-2));

  // Add in the NLRI information
  for(idx=0; idx<update->nlriCount;idx++){
    bits[i] = update->nlris[idx]->length;
    i+=1;
    for(idy=0;idy<(update->nlris[idx]->length/8);idy++){
      bits[i] = update->nlris[idx]->prefix[idy];
      i+=1;
    }
    if(update->nlris[idx]->length%8 > 0){
      bits[i] = update->nlris[idx]->prefix[idy];
      i+=1;
    }
  }

  BGP_ASSIGN_2BYTES(&bits[lm],i);
  msg->length = i;
   
  return 0; 
  
}

/******************************************************************************
Name: BGP_calculateLength
Input: A pointer to the BGPMessage
Output: The number of bytes anticipated in the message
Author: Catherine Olschanowsky
******************************************************************************/
int
BGP_calculateLength(BGPMessage* msg, int asnLen)
{
  int i;
  // the first task should be too allocate enough space to hold the message,
  // but we don't know what that is.
  // so, the first thing we need to do is calculate how long the message
  // is going to turn out to be.
  int length_in_bytes = BGP_HEADER_LENGTH;
  if(msg->type == BGP_UPDATE){
    BGPUpdate *update = (BGPUpdate*)msg->typeSpecific;
    // withdrawn routes
    length_in_bytes += 2; // space for the length
    for(i=0;i<update->withdrawnRoutesCount;i++){
      length_in_bytes += 1; // for the length of this prefix
      length_in_bytes += update->withdrawnRoutes[i]->length/8;
      if(update->withdrawnRoutes[i]->length%8 != 0){
        length_in_bytes++;
      }
    }
    // path attributes
    length_in_bytes += 2; // space for the length
    for(i=0;i<update->totalPathAttsCount;i++){
      length_in_bytes += 2; // type
      length_in_bytes += BGP_PAL_LEN(update->pathAtts[i]->flags);
      length_in_bytes += update->pathAtts[i]->length; // value
      // one special case due to ASN4 versus ASN2
      if(update->pathAtts[i]->code == BGP_PATTR_AS_PATH){
        // AS_PATH is a well-known mandatory attribute that is composed
        // of a sequence of AS path segments.  Each AS path segment is
        // represented by a triple <path segment type, path segment
        // length, path segment value>.
        // The path segment type is a 1-octet length field with the
        // following values defined:
           // Value      Segment Type
           // 1         AS_SET: unordered set of ASes a route in the
                        // UPDATE message has traversed
           // 2         AS_SEQUENCE: ordered set of ASes a route in
                        // the UPDATE message has traversed
        // The path segment length is a 1-octet length field,
        // containing the number of ASes (not the number of octets) in
        // the path segment value field.
        // The path segment value field contains one or more AS
        // numbers, each encoded as a 2-octet length field.

        // the above text is taken from RFC 4271, however, this code must handle
        // AS_PATHs created by MRT and therefore, the ASes might be 4 bytes
        // this code is used to calculate the length of an AS_PATH assuming that
        // it is to be truncated from 4 bytes to 2 bytes -- I believe this is
        // only useful from the MRT module

        // we can calculate the length by looking at the path segment length
        // multiplying that value by 2 and subtracting it from the length in the
        // path attribute header
        if(asnLen == 2){
          // this has to be done for each segment
          int idz;
          for(idz = 1; idz < update->pathAtts[i]->length; ){
            length_in_bytes -= (update->pathAtts[i]->value[idz]*2);
            idz += update->pathAtts[i]->value[idz]*4 + 1;
          }
        }
      }
    }
    // nlri
    for(i=0;i<update->nlriCount;i++){
      length_in_bytes += 1;
      length_in_bytes += update->nlris[i]->length/8;
      if((update->nlris[i]->length%8) != 0){
        length_in_bytes += 1;
      }
    }
    msg->length = length_in_bytes;
  }else{
    // only updates are supported at this point
    return 0;
  }
  return length_in_bytes;
}

/******************************************************************************
Name: BGP_freeMessage
Input: A pointer to the BGPMessage
Output: 0 = success, -1 = failure
Author: Catherine Olschanowsky
******************************************************************************/
int         
BGP_freeMessage(BGPMessage* msg)
{
  int i;
  if(msg->type == BGP_UPDATE){
    BGPUpdate *update = (BGPUpdate*)msg->typeSpecific;
    // withdrawn routes
    for(i=0;i<update->withdrawnRoutesCount;i++){
      free(update->withdrawnRoutes[i]);
    }
    free(update->withdrawnRoutes);
    // attributes
    for(i=0;i<update->totalPathAttsCount;i++){
      free(update->pathAtts[i]->value);
      free(update->pathAtts[i]);
    }
    free(update->pathAtts);
    // nlri
    for(i=0;i<update->nlriCount;i++){
      free(update->nlris[i]);
    }
    free(update->nlris);
  }
  free(msg->typeSpecific);
  free(msg);
  return 0;
}
