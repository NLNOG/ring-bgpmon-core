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
 *  File: mrtUtils_t.c
 *  Authors: Catherine Olschanowsky
 *  Date: Aug. 30, 2011
 */
#include <CUnit/Basic.h>
#include <stdlib.h>
#include <stdio.h>
#include "mrtUtils_t.h"

/* a few global variables to play with across tests */
#define TEST_DIR "test/unit_test_input/mrt"

/* The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
int
init_mrtUtils(void)
{
  return 0;
}

/* The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
int
clean_mrtUtils(void)
{
  return 0;
}

//
void
testMRT_backlog_init()
{
  MRT_backlog bl;
  CU_ASSERT(0 == MRT_backlog_init(&bl));
 
  // now check to be sure that everything we need in there has actually been allocated
  CU_ASSERT(NULL != bl.buffer);
  CU_ASSERT(0 == bl.start_pos);
  CU_ASSERT(0 == bl.end_pos);
  CU_ASSERT(MRT_START_BACKLOG_SIZE == bl.size);
  CU_ASSERT(MRT_START_BACKLOG_SIZE == bl.start_size);
  CU_ASSERT(0 == MRT_backlog_destroy(&bl));
  return;
}

// this version of the test is not using any locks, because the test driver 
// only has a single thread
void
testMRT_backlog_write()
{
  MRT_backlog bl;
  uint8_t rawMessage[MAX_MRT_LENGTH];
  MRTheader mrtHeader;
  help_load_test_MRT(&mrtHeader,rawMessage);

  CU_ASSERT(0== MRT_backlog_init(&bl));
  // now write that message to the backlog
  CU_ASSERT(0 == MRT_backlog_write(&bl,rawMessage,MRT_HEADER_LENGTH+mrtHeader.length));
  // make sure that the write actually happened
  CU_ASSERT(0 == bl.start_pos);
  CU_ASSERT(MRT_HEADER_LENGTH+mrtHeader.length == bl.end_pos);
  // a subsequent write shouldn't really be any different, but just to be sure...
  CU_ASSERT(0 == MRT_backlog_write(&bl,rawMessage,MRT_HEADER_LENGTH+mrtHeader.length));
  // make sure that the write actually happened
  CU_ASSERT(0 == bl.start_pos);
  CU_ASSERT((MRT_HEADER_LENGTH+mrtHeader.length)*2 == bl.end_pos);

  CU_ASSERT(0 == MRT_backlog_destroy(&bl)); 

  // next test a write on a very small buffer so that we can see it wrap 
  // around a grow
  // this is guaranteed to be too small ( no room for the header )
  CU_ASSERT(0 == MRT_backlog_init_size(&bl,mrtHeader.length));
  CU_ASSERT(0 == MRT_backlog_write(&bl,rawMessage,MRT_HEADER_LENGTH+mrtHeader.length));
  CU_ASSERT(0 == bl.start_pos);
  CU_ASSERT((mrtHeader.length*2) == bl.size);
  CU_ASSERT(MRT_HEADER_LENGTH+mrtHeader.length == bl.end_pos);
  CU_ASSERT(0 == MRT_backlog_destroy(&bl)); 

  // now test writing when we have to wrap around -- need enough space, but
  // move the start_pos and end_pos toward the end of the buffer
  CU_ASSERT(0 == MRT_backlog_init_size(&bl,3*(MRT_HEADER_LENGTH+mrtHeader.length)));
  // trick it into thinking it is not empty and put the pointer toward the end
  bl.start_pos = 2*(MRT_HEADER_LENGTH+mrtHeader.length) + MRT_HEADER_LENGTH;
  bl.end_pos = bl.start_pos+2;
  uint32_t message_size = (MRT_HEADER_LENGTH+mrtHeader.length);
  uint32_t prev_end= bl.end_pos;
  CU_ASSERT(0 == MRT_backlog_write(&bl,rawMessage,MRT_HEADER_LENGTH+mrtHeader.length));
  CU_ASSERT( (2*(MRT_HEADER_LENGTH+mrtHeader.length) + MRT_HEADER_LENGTH) == bl.start_pos);
  CU_ASSERT( (3*(MRT_HEADER_LENGTH+mrtHeader.length)) == bl.size);
  CU_ASSERT( (message_size-(bl.size-prev_end)) == bl.end_pos);
  CU_ASSERT( bl.start_pos > bl.end_pos);

  // now that wrapping has happened try to write one more and make sure it works
  prev_end= bl.end_pos;
  CU_ASSERT(0 == MRT_backlog_write(&bl,rawMessage,MRT_HEADER_LENGTH+mrtHeader.length));
  CU_ASSERT( (2*(MRT_HEADER_LENGTH+mrtHeader.length) + MRT_HEADER_LENGTH) == bl.start_pos);
  CU_ASSERT( (3*(MRT_HEADER_LENGTH+mrtHeader.length)) == bl.size);
  CU_ASSERT( (prev_end+message_size) == bl.end_pos);

  CU_ASSERT(0 == MRT_backlog_destroy(&bl)); 
  
}

void
testMRT_backlog_read()
{
  MRT_backlog bl;
  uint8_t rawMessage[MAX_MRT_LENGTH];
  uint8_t rawMessage_r[MAX_MRT_LENGTH];
  MRTheader mrtHeader,mrtHeader_r;

  CU_ASSERT(0== MRT_backlog_init(&bl));
  // try to read from an empty backlog
  CU_ASSERT(1 == MRT_backlog_read(&bl,&mrtHeader,rawMessage,MAX_MRT_LENGTH));

  // now write that message to the backlog
  help_load_test_MRT(&mrtHeader,rawMessage);
  CU_ASSERT(0 == MRT_backlog_write(&bl,rawMessage,MRT_HEADER_LENGTH+mrtHeader.length));

  // make sure that the write actually happened
  CU_ASSERT(0 == bl.start_pos);
  CU_ASSERT(MRT_HEADER_LENGTH+mrtHeader.length == bl.end_pos);

  // now read the message
  CU_ASSERT(0 == MRT_backlog_read(&bl,&mrtHeader_r,rawMessage_r,MAX_MRT_LENGTH));

  // check that the messages are correct
  CU_ASSERT(0 == memcmp(&mrtHeader,&mrtHeader_r,MRT_HEADER_LENGTH));
  CU_ASSERT(mrtHeader.timestamp == mrtHeader_r.timestamp);
  CU_ASSERT(mrtHeader.type == mrtHeader_r.type);
  CU_ASSERT(mrtHeader.subtype == mrtHeader_r.subtype);
  CU_ASSERT(mrtHeader.length == mrtHeader_r.length);
  CU_ASSERT(0 == memcmp(rawMessage+MRT_HEADER_LENGTH,rawMessage_r,mrtHeader.length));
 
  // check the status of the backlog
  CU_ASSERT(bl.end_pos == bl.start_pos);

  // clean up
  CU_ASSERT(0 == MRT_backlog_destroy(&bl)); 
}

void
testMRT_backlog_resize()
{

  MRT_backlog bl;
  uint32_t size;
  CU_ASSERT(0 == MRT_backlog_init(&bl));
  size = bl.size;
  CU_ASSERT(0 == MRT_backlog_expand(&bl)); 
  CU_ASSERT(size*2 == bl.size);
  CU_ASSERT(0 == bl.start_pos);
  CU_ASSERT(0 == bl.end_pos);
  CU_ASSERT(0 == MRT_backlog_shrink(&bl,size));
  CU_ASSERT(size == bl.size);
  CU_ASSERT(0 == bl.start_pos);
  CU_ASSERT(0 == bl.end_pos);
  CU_ASSERT(0 == MRT_backlog_destroy(&bl));

  // this time write a message, resize and then read it
  uint8_t rawMessage[MAX_MRT_LENGTH];
  uint8_t rawMessage_r[MAX_MRT_LENGTH];
  MRTheader mrtHeader,mrtHeader_r;
  CU_ASSERT(0 == MRT_backlog_init(&bl));
  size = bl.size;

  // now write that message to the backlog
  help_load_test_MRT(&mrtHeader,rawMessage);
  CU_ASSERT(0 == MRT_backlog_write(&bl,rawMessage,MRT_HEADER_LENGTH+mrtHeader.length));

  // make sure that the write actually happened
  CU_ASSERT(0 == bl.start_pos);
  CU_ASSERT(MRT_HEADER_LENGTH+mrtHeader.length == bl.end_pos);

  CU_ASSERT(0 == MRT_backlog_expand(&bl)); 
  CU_ASSERT(size*2 == bl.size);
  CU_ASSERT(0 == bl.start_pos);
  CU_ASSERT(MRT_HEADER_LENGTH+mrtHeader.length == bl.end_pos);

  // read the message and make sure the expand didn't mess it up
  CU_ASSERT(0 == MRT_backlog_read(&bl,&mrtHeader_r,rawMessage_r,MAX_MRT_LENGTH));
  // check that the messages are correct
  CU_ASSERT(0 == memcmp(&mrtHeader,&mrtHeader_r,MRT_HEADER_LENGTH));
  CU_ASSERT(mrtHeader.timestamp == mrtHeader_r.timestamp);
  CU_ASSERT(mrtHeader.type == mrtHeader_r.type);
  CU_ASSERT(mrtHeader.subtype == mrtHeader_r.subtype);
  CU_ASSERT(mrtHeader.length == mrtHeader_r.length);
  CU_ASSERT(0 == memcmp(rawMessage+MRT_HEADER_LENGTH,rawMessage_r,mrtHeader.length));
 
  // check the status of the backlog
  CU_ASSERT(bl.end_pos == bl.start_pos);
  CU_ASSERT(0 == MRT_backlog_destroy(&bl));
}


void
help_load_test_MRT(MRTheader *mrtHeader1,uint8_t *rawMessage1){

  // start out by reading an MRT message from a file
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
  int hdrSize = fread(mrtHeader1,1,MRT_HEADER_LENGTH,testfile);
  CU_ASSERT(hdrSize == MRT_HEADER_LENGTH);
  memcpy(rawMessage1,mrtHeader1,MRT_HEADER_LENGTH);
  mrtHeader1->timestamp = ntohl(mrtHeader1->timestamp); 
  CU_ASSERT(timestamp == mrtHeader1->timestamp);
  if(timestamp != mrtHeader1->timestamp){
    fprintf(stderr,"t is%x h.t is %x\n",timestamp,mrtHeader1->timestamp);
  }
  mrtHeader1->type = ntohs(mrtHeader1->type);
  CU_ASSERT(16 == mrtHeader1->type);
  mrtHeader1->subtype = ntohs(mrtHeader1->subtype);
  CU_ASSERT(4 == mrtHeader1->subtype);
  mrtHeader1->length = ntohl(mrtHeader1->length);
  CU_ASSERT(63  == mrtHeader1->length);
  int msgSize = fread((rawMessage1+MRT_HEADER_LENGTH),1,mrtHeader1->length,testfile);
  CU_ASSERT(msgSize == mrtHeader1->length);
  fclose(testfile);
}


