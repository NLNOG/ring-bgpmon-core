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
 *  File: bgp_t.c
 *  Authors: Catherine Olschanowsky
 *  Date: Aug. 30, 2011
 */
#include <CUnit/Basic.h>
#include <stdlib.h>
#include <stdio.h>
#include "bgp.h"

/* a few global variables to play with across tests */
BGPMessage *bgpMessage;

/* TEST: BGP_createMessage
 */
void 
testBGP_createMessage(void)
{
  bgpMessage = BGP_createMessage(BGP_UPDATE);
  CU_ASSERT(NULL != bgpMessage);
  if(bgpMessage == NULL){
    return;
  }
  CU_ASSERT(bgpMessage->type == BGP_UPDATE);
  CU_ASSERT(bgpMessage->length == 0);
  CU_ASSERT(bgpMessage->typeSpecific != NULL);
  if(bgpMessage->typeSpecific == NULL){
    return;
  }
  CU_ASSERT(((BGPUpdate*)(bgpMessage->typeSpecific))->withdrawnRoutesCount == 0);
  CU_ASSERT(((BGPUpdate*)(bgpMessage->typeSpecific))->totalPathAttsCount == 0);
}

/* TEST: int BGP_addWithdrawnRouteToUpdate(BGPMessage* msg, uint16_t length, const uint8_t *prefix);
 *
 */
void 
testBGP_addWithdrawnRouteToUpdate()
{
  uint8_t prefix = 0xFE;
  uint16_t len = 7;

  BGPUpdate *update = (BGPUpdate*)bgpMessage->typeSpecific;
  CU_ASSERT(0 == BGP_addWithdrawnRouteToUpdate(bgpMessage,0,NULL));
  CU_ASSERT(-1 == BGP_addWithdrawnRouteToUpdate(bgpMessage,1,NULL));
  CU_ASSERT(0 == BGP_addWithdrawnRouteToUpdate(bgpMessage,len,&prefix));
  CU_ASSERT(1 == update->withdrawnRoutesCount);
  CU_ASSERT(NULL != update->withdrawnRoutes);
  if(NULL == update->withdrawnRoutes){
    return;
  }
  CU_ASSERT(len == update->withdrawnRoutes[0]->length);
  CU_ASSERT(prefix == *(update->withdrawnRoutes[0]->prefix));

  CU_ASSERT(0 == BGP_addWithdrawnRouteToUpdate(bgpMessage,len,&prefix));
  CU_ASSERT(2 == update->withdrawnRoutesCount);
  CU_ASSERT(NULL != update->withdrawnRoutes);
  if(NULL == update->withdrawnRoutes){
    return;
  }
  CU_ASSERT(len == update->withdrawnRoutes[0]->length);
  CU_ASSERT(prefix == *(update->withdrawnRoutes[0]->prefix));
  CU_ASSERT(len == update->withdrawnRoutes[1]->length);
  CU_ASSERT(prefix == *(update->withdrawnRoutes[1]->prefix));
}

/* TEST: int BGP_addPathAttributeToUpdate(BGPMessage* msg, uint16_t type, uint16_t length, const uint8_t *value);
 *
*/
void 
testBGP_addPathAttributeToUpdate()
{
  uint8_t flags1  = 0x40;
  uint8_t code1   = 0xFF;
  uint8_t flags2  = 0x50;
  uint8_t code2   = 0xF0;
  uint16_t length = 1;
  uint8_t value = 0xFF;

  BGPUpdate *update = (BGPUpdate*)bgpMessage->typeSpecific;
  CU_ASSERT(0 == BGP_addPathAttributeToUpdate(bgpMessage,flags1,code1,0,NULL));
  CU_ASSERT(-1 == BGP_addPathAttributeToUpdate(bgpMessage,flags1,code1,length,NULL));
  CU_ASSERT(0 == BGP_addPathAttributeToUpdate(bgpMessage,flags1,code1,length,&value));
  CU_ASSERT(NULL != update->pathAtts);
  if(NULL == update->pathAtts){
    return;
  }
  CU_ASSERT(1 == update->totalPathAttsCount);
  CU_ASSERT(0x40 == update->pathAtts[0]->flags);
  CU_ASSERT(0xFF == update->pathAtts[0]->code);
  CU_ASSERT(1 == update->pathAtts[0]->length);
  CU_ASSERT(value == update->pathAtts[0]->value[0]);
  CU_ASSERT(1 == (BGP_PAL_LEN(update->pathAtts[0]->flags)));

  CU_ASSERT(0 == BGP_addPathAttributeToUpdate(bgpMessage,flags2,code2,length,&value));
  CU_ASSERT(2 == update->totalPathAttsCount);
  CU_ASSERT(0x50 == update->pathAtts[0]->flags);
  CU_ASSERT(0xF0 == update->pathAtts[0]->code);
  CU_ASSERT(1 == update->pathAtts[0]->length);
  CU_ASSERT(value == update->pathAtts[0]->value[0]);
  CU_ASSERT(2 == (BGP_PAL_LEN(update->pathAtts[0]->flags)));

  CU_ASSERT(0x40 == update->pathAtts[1]->flags);
  CU_ASSERT(0xFF == update->pathAtts[1]->code);
  CU_ASSERT(1 == update->pathAtts[1]->length);
  CU_ASSERT(value == update->pathAtts[1]->value[0]);
  CU_ASSERT(1 == (BGP_PAL_LEN(update->pathAtts[1]->flags)));

  return;
}

/* TEST: BGP_addNRLIToUpdate(BGPMessage* msg, uint8_t length, const uint8_t *value);
 *
*/
void 
testBGP_addNLRIToUpdate()
{
  uint8_t length = 1;
  uint8_t value = 0xFF;

  BGPUpdate *update = (BGPUpdate*)bgpMessage->typeSpecific;
  CU_ASSERT(-1 == BGP_addNLRIToUpdate(NULL, 0, NULL));
  CU_ASSERT(0 == BGP_addNLRIToUpdate(bgpMessage, 0, NULL));
  CU_ASSERT(-1 == BGP_addNLRIToUpdate(bgpMessage, length, NULL));
  CU_ASSERT(0 == BGP_addNLRIToUpdate(bgpMessage, length, &value));
  CU_ASSERT(NULL != update->nlris);
  CU_ASSERT(1 == update->nlriCount);

}

/* TEST: BGP_calculateLength(BGPMessage* msg);
 *
*/
void 
testBGP_calculateLength()
{
  CU_ASSERT(0<BGP_calculateLength(bgpMessage,0));
  // this test will fail if any of the above tests are changed
  // this is really not a good way to do this test
  CU_ASSERT((38) ==BGP_calculateLength(bgpMessage,0));
  if((38) != BGP_calculateLength(bgpMessage,0)){
    fprintf(stderr,"calculated len is %d\n",BGP_calculateLength(bgpMessage,0));
  }
  CU_ASSERT((38) ==BGP_calculateLength(bgpMessage,0));
  if((38) != BGP_calculateLength(bgpMessage,0)){
    fprintf(stderr,"calculated len is %d\n",BGP_calculateLength(bgpMessage,0));
  }
  CU_ASSERT(bgpMessage->length == (BGP_calculateLength(bgpMessage,0)));
}

/*void BGP_serialize(BGPMessage* msg);
 *
*/
void 
testBGP_serialize()
{
  uint8_t testMsg[38] = {0xFF,0xFF,0xFF,0xFF, // marker
                         0xFF,0xFF,0xFF,0xFF,
                         0xFF,0xFF,0xFF,0xFF,
                         0xFF,0xFF,0xFF,0xFF,
                         0x00,0x26,0x02,      // length and type
                         0x00,0x04,           // length of withdrawn routes
                         0x07,0xFE,0x07,0xFE, // withdrawn routes
                         0x00,0x09,           // length of PAs
                         0x50,0xF0,0x00,0x01, // PA2
                         0xFF,
                         0x40,0xFF,0x01,0xFF, // PA1
                         0x01,0xFF};          // NLRI
                          
  uint8_t binMsg[1024];;
  int retval = BGP_serialize(binMsg,bgpMessage,0);
  CU_ASSERT(retval == 0);
  CU_ASSERT(binMsg != NULL);
  CU_ASSERT(bgpMessage->length > 0);
  CU_ASSERT(bgpMessage->length <= 4096);
  int i;
  for(i=0;i<bgpMessage->length;i++){
    CU_ASSERT(testMsg[i] == binMsg[i]);
    if(testMsg[i] != binMsg[i]){
      fprintf(stderr,"position: %d,%x:%x\n",i,testMsg[i],binMsg[i]);
    }
  }
}



/* The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
int init_bgp(void)
{
  return 0;
}

/* The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
int clean_bgp(void)
{
  BGP_freeMessage(bgpMessage);
  return 0;
}
