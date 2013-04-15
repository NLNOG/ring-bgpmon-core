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
#include "mrtinstance.h"
#include "../Peering/peersession.h"

/* a few global variables to play with across tests */
#define TEST_DIR "test/unit_test_input/mrt"

/*
TableBuffer *tableBuffer;
MrtNode cn;

 The suite initialization function.
  Returns zero on success, non-zero otherwise.
 
int
init_mrtinstance(void)
{
  mrtQueue = createQueue(copyBMF, sizeOfBMF, "test", strlen("test"), FALSE);
  return 0;
}

 The suite cleanup function.
 Returns zero on success, non-zero otherwise.

int
clean_mrtinstance(void)
{
  free(tableBuffer);
  return 0;
}

//MrtNode *cn,mrt_index *indexPtr,TableBuffer *tablebuffer,uint8_t *rawMessage, MRTheader *mrtHeader);
void
testMRT_createTableBufferFromType13Subtype1()
{
  MRTheader mrtHeader1;
  uint8_t rawMessage1[MAX_MRT_LENGTH]; 

  // read the header and the raw message
  char *dir = TEST_DIR;
  char filename[256];
  sprintf(filename,"%s/mrt.13.1",dir);

  // in order to verify the parsing match it to these manually
  // created arrays
  uint8_t tmp[4] = {0x4e,0x5e,0xcb,0x00};
  uint32_t timestamp;
  memmove(&timestamp,tmp,4);
  timestamp = ntohl(timestamp);
  tmp[0]=0x80;tmp[1]=0xdf;tmp[2]=0x33;tmp[3]=0x66;
  uint32_t c_bgpid;
  memmove(&c_bgpid,tmp,4);
  uint16_t prefix_count;
  tmp[0]=0x00;tmp[1]=0x2d;
  memmove(&prefix_count,tmp,2);
  prefix_count = ntohs(prefix_count);

  // open the file and read in the entire message to the buffer
  FILE *testfile = fopen(filename,"r");
  if(!testfile){
    CU_ASSERT(1 == 0);
    return;
  }
  int hdrSize = fread(&mrtHeader1,1,MRT_HEADER_LENGTH,testfile);
  CU_ASSERT(MRT_HEADER_LENGTH == hdrSize);
  mrtHeader1.timestamp = ntohl(mrtHeader1.timestamp); 
  CU_ASSERT(timestamp == mrtHeader1.timestamp);
  if(timestamp != mrtHeader1.timestamp){
    fprintf(stderr,"t is%x h.t is %x\n",timestamp,mrtHeader1.timestamp);
  }
  mrtHeader1.type = ntohs(mrtHeader1.type);
  CU_ASSERT(13 == mrtHeader1.type);
  mrtHeader1.subtype = ntohs(mrtHeader1.subtype);
  CU_ASSERT(1 == mrtHeader1.subtype);
  mrtHeader1.length = ntohl(mrtHeader1.length);
  CU_ASSERT( 593 == mrtHeader1.length);
  int msgSize = fread(rawMessage1,1,mrtHeader1.length,testfile);
  CU_ASSERT( msgSize == mrtHeader1.length);
  fclose(testfile);

  cn.labelAction = 1;
  mrt_index indexPtr;
  // upon calling this function mrt_index has been allocated but not initialized
  // table buffer has not been allocated
  // raw Message holds the bits for all of the mrt message except for the header
  // header holds the header
  CU_ASSERT(0 == MRT_createTableBufferFromType13Subtype1(&cn,&indexPtr,&tableBuffer,rawMessage1,&mrtHeader1));
  CU_ASSERT(c_bgpid == indexPtr.BGPSrcID);
  CU_ASSERT(prefix_count == indexPtr.PeerCount);
  CU_ASSERT(tableBuffer != NULL);
  int i;
  for(i=0;i<indexPtr.PeerCount;i++){
    Session_structp session = getSessionByID(tableBuffer[i].ID);
    CU_ASSERT(session !=NULL);
  }
  return;
}

void
testMRT_processType13SubtypeSpecific(){
  // this is the message being used for testing
  // Header:     4e5e cb00 000d 0002 0000 0080 
  // Seq Num.:   0000 0000  
  // Prefix Len: 00 
  // Entry Cnt:  0003
  // Index:      001e
  // Time:       4e5e 2757
  // Att. Length:0024
  // BGPAttr.   :flags:40
  //             code: 01
  //            length:01 0050  
  // 0200 0e02 0300 000b 5900 00ff f400 0040
  // fd40 0304 c407 6af5 8004 0400 0000 00
  // Index:      000b
  // Time:       4e4b 8edc
  // Att. Length:0024
  // BGPAttr:    40 0101 0050 0200 0e02 
  // 0300 007b 0c00 0073 d300 00ac cd40 0304
  // 5f8c 50fe 8004 0400 0000 00
  // Index:      000c
  // Time:       4e56 1327
  // AttLen:     0019
  //  40 0101 0050 0200 0a02 0200 002d
  //  a600 004a cf40 0304 6004 0037
  // in order to verify the parsing match it to these manually
  // created arrays
  int i;
  uint8_t tmp[4] = {0x4e,0x5e,0xcb,0x00};
  uint32_t timestamp;
  memmove(&timestamp,tmp,4);
  timestamp = ntohl(timestamp);

  MRTheader mrtHeader1;
  uint8_t rawMessage1[MAX_MRT_LENGTH]; 
  BGPMessage **bgp_arr;
  int bgp_count;
  int **peer_idxs;

  // read the header and the raw message
  char *dir = TEST_DIR;
  char filename[256];
  sprintf(filename,"%s/mrt.13.2",dir);

  // open the file and read in the entire message to the buffer
  FILE *testfile = fopen(filename,"r");
  if(!testfile){
    CU_ASSERT(1 == 0);
    return;
  }
  int hdrSize = fread(&mrtHeader1,1,MRT_HEADER_LENGTH,testfile);
  CU_ASSERT(hdrSize == MRT_HEADER_LENGTH);
  mrtHeader1.timestamp = ntohl(mrtHeader1.timestamp); 
  CU_ASSERT(timestamp == mrtHeader1.timestamp);
  if(timestamp != mrtHeader1.timestamp){
    fprintf(stderr,"t is%x h.t is %x\n",timestamp,mrtHeader1.timestamp);
  }
  mrtHeader1.type = ntohs(mrtHeader1.type);
  CU_ASSERT(13 == mrtHeader1.type);
  mrtHeader1.subtype = ntohs(mrtHeader1.subtype);
  CU_ASSERT(2 == mrtHeader1.subtype);
  mrtHeader1.length = ntohl(mrtHeader1.length);
  CU_ASSERT( 128 == mrtHeader1.length);
  int msgSize = fread(rawMessage1,1,mrtHeader1.length,testfile);
  CU_ASSERT(msgSize == mrtHeader1.length);
  fclose(testfile);
  CU_ASSERT(0 == MRT_processType13SubtypeSpecific(&mrtHeader1,rawMessage1,&bgp_arr,&peer_idxs,&bgp_count));
  CU_ASSERT(bgp_count==3);
  if(bgp_count != 3){
    fprintf(stderr,"bgp count is %d\n",bgp_count);
  }
  CU_ASSERT(NULL!=bgp_arr);
  
  for(i=0;i<bgp_count;i++){
    BGP_freeMessage(bgp_arr[i]);
  }
  free(bgp_arr);
  free(peer_idxs);
  
}

void
testMRT_processType16SubtypeMessage(){
//cn,asNumLen, mrtHeader,mrtMessage, bmf)
  MRTheader mrtHeader1;
  MRTmessage mrtMessage;
  BMF bmf;
  uint8_t rawMessage1[MAX_MRT_LENGTH]; 

  uint8_t tmp[4] = {0x4e,0x78,0x39,0xf0};
  uint32_t timestamp;
  memmove(&timestamp,tmp,4);
  timestamp = ntohl(timestamp);

  // read the header and the raw message
  char *dir = TEST_DIR;
  char filename[256];
  sprintf(filename,"%s/mrt.16.4",dir);

  // open the file and read in the entire message to the buffer
  FILE *testfile = fopen(filename,"r");
  if(!testfile){
    CU_ASSERT(1 == 0);
    return;
  }
  int hdrSize = fread(&mrtHeader1,1,MRT_HEADER_LENGTH,testfile);
  CU_ASSERT(hdrSize == MRT_HEADER_LENGTH);
  mrtHeader1.timestamp = ntohl(mrtHeader1.timestamp); 
  CU_ASSERT(timestamp == mrtHeader1.timestamp);
  if(timestamp != mrtHeader1.timestamp){
    fprintf(stderr,"t is%x h.t is %x\n",timestamp,mrtHeader1.timestamp);
  }
  mrtHeader1.type = ntohs(mrtHeader1.type);
  CU_ASSERT(16 == mrtHeader1.type);
  mrtHeader1.subtype = ntohs(mrtHeader1.subtype);
  CU_ASSERT(4 == mrtHeader1.subtype);
  mrtHeader1.length = ntohl(mrtHeader1.length);
  CU_ASSERT(63  == mrtHeader1.length);
  int msgSize = fread(rawMessage1,1,mrtHeader1.length,testfile);
  CU_ASSERT(msgSize == mrtHeader1.length);
  fclose(testfile);

  int asNumLen = 2;
  if(mrtHeader1.subtype == 4){
   asNumLen = 4;
  }
  CU_ASSERT(0 == MRT_processType16SubtypeMessage(rawMessage1,asNumLen,&mrtHeader1,&mrtMessage,&bmf));
  CU_ASSERT(NULL != bmf);
  free(bmf); 
}*/
