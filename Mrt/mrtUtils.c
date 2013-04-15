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
 * 	Authors: Dan Massey, Cathie Olschanowsky
 *
 *  Date: April 2012
 *  Edited: June 2012
 */
 
/* externally visible structures and functions for mrts */
#include "mrtUtils.h"

//#define DEBUG

/******************************************************************************
* MRT_backlog_init
* Purpose: allocate the buffer and provide initial values
* Uses the default size defined by MRT_START_BACKLOG_SIZE
* Returns: 0 for success, 1 on failure
******************************************************************************/
int
MRT_backlog_init(MRT_backlog *backlog){

  if(MRT_backlog_init_size(backlog,MRT_START_BACKLOG_SIZE)){
    return -1;
  }
  // initialize the lock for the backlog
  if (pthread_mutex_init( &(backlog->lock), NULL ) ){
    log_err("Unable to init mutex lock for mrt thread\n");
    return -1;
  }
  return 0;
}

/******************************************************************************
* MRT_backlog_init
* Purpose: allocate the buffer and provide initial values
* Takes a pointer to the backlog as well as the size in bytes 
* Returns: 0 for success, 1 on failure
******************************************************************************/
int
MRT_backlog_init_size(MRT_backlog *backlog, uint32_t size){

  backlog->start_size = size;
  backlog->size = size;
  backlog->start_pos = 0;
  backlog->end_pos = 0;
  backlog->buffer = malloc(sizeof(uint8_t)*backlog->size);
  if(backlog->buffer == NULL){
    log_err("Unable to malloc space for MRT backlog\n");
    return -1;
  }

  return 0;
}

/******************************************************************************
* MRT_backlog_destroy
* Purpose: free allocated space, and destroy the lock
* Returns: 0 for success, 1 on failure
******************************************************************************/
int
MRT_backlog_destroy(MRT_backlog *backlog){
  free(backlog->buffer);
  return pthread_mutex_destroy(&(backlog->lock));
}

/******************************************************************************
* MRT_backlog_write
* Purpose: Write some number of bytes from a buffer to the backlog 
* Notes: This code assumes that the backlog lock has already been obtained!! 
*        If expansion of the buffer is needed, and not possible, messages
*        may be dropped.
* Returns: 0 for success, 1 on failure
******************************************************************************/
int 
MRT_backlog_write(MRT_backlog *backlog,uint8_t *buffer, uint32_t bytes){

  // if the backlog is empty, start it over at the beginning
  if(backlog->end_pos == backlog->start_pos){
    backlog->end_pos = backlog->start_pos = 0;
#ifdef DEBUG
    log_msg("write: end_pos: %d start:pos %d\n",backlog->end_pos,
                                                backlog->start_pos);
#endif
  }

  // the first thing we do here is to check if the buffer is full
  // if it is full then we can attempt to extend the buffer
  // if that fails we will have to start losing data

  uint32_t space =  backlog->size - backlog->end_pos + backlog->start_pos;
  if(backlog->start_pos > backlog->end_pos){
    space = backlog->start_pos - backlog->end_pos;
  }
  if( space <= bytes ){
    if(MRT_backlog_expand(backlog)){
      // extending the backlog failed -- this may be due to being out of space, or hitting the max
      // this is where we go ahead and miss some messages
      log_err("MRT: Skipping messages in buffer due to lack of space");
      while(space <= bytes){
        uint8_t rawMessage[MAX_MRT_LENGTH];
        MRTheader mrtHeader;
        if(MRT_backlog_read(backlog,&mrtHeader,rawMessage,MAX_MRT_LENGTH)){
          // if the read has failed -- then the write has really failed
          return 1;
        }
        space =  backlog->size - backlog->end_pos + backlog->start_pos;
        if(backlog->start_pos > backlog->end_pos){
          space = backlog->start_pos - backlog->end_pos;
        }
      } 
    }
    return MRT_backlog_write(backlog,buffer,bytes);
  }  

  // at this point we can be sure that we have enough room to write
  // there are 3 cases
  // 1. no wrapping has happened and it is not needed
  // 2. no wrapping has happened, but we will start it here
  // 3. wrapping has happened

  // check to see that there has not been any wrapping
  if(backlog->start_pos <= backlog->end_pos){  // no wrapping
    // check to see that there is room between the end of the current data
    // and the end of the backlog
    if((backlog->size-backlog->end_pos) > bytes){
      // case 1: no wrapping in past, none needed now
#ifdef DEBUG
log_msg("write no wrapping\n");
#endif
      memcpy(&(backlog->buffer[backlog->end_pos]),buffer,bytes);
      backlog->end_pos += bytes;
#ifdef DEBUG
    log_msg("write: end_pos: %d start:pos %d\n",backlog->end_pos,
                                                backlog->start_pos);
#endif
    }else{
#ifdef DEBUG
log_msg("write wrapping\n");
#endif
      // case 2: no wrapping in past, but we need it here
      // copy enough to fill up the end of the buffer
      memcpy(&(backlog->buffer[backlog->end_pos]),
             buffer,
             (backlog->size-backlog->end_pos));
      // copy the rest to the beginning of the buffer
      memcpy(&(backlog->buffer[0]),
             buffer+(backlog->size-backlog->end_pos),
             (bytes -(backlog->size-backlog->end_pos)));
      backlog->end_pos = (bytes -(backlog->size-backlog->end_pos));
#ifdef DEBUG
    log_msg("write: end_pos: %d start:pos %d\n",backlog->end_pos,
                                                backlog->start_pos);
#endif
    }
  // case 3: there has been wrapping, but we still have a continuous section of 
  //         memory to copy to
  }else{ 
#ifdef DEBUG
log_msg("wrote after wrapping\n");
#endif
      memcpy(&(backlog->buffer[backlog->end_pos]),buffer,bytes);
      backlog->end_pos += bytes;
#ifdef DEBUG
    log_msg("write: end_pos: %d start:pos %d\n",backlog->end_pos,
                                                backlog->start_pos);
#endif
  }
  return 0;
}

/******************************************************************************
* MRT_backlog_expand
* Purpose: Attempt to expand the buffer to be twice the size it was
* Notes:  Assumes the backlog lock has been aquired
*        
*        
* Returns: 0 for success, other on failure
******************************************************************************/
int
MRT_backlog_expand(MRT_backlog *backlog){

  uint32_t new_size = 2*backlog->size;
  uint8_t *new_log;
  
  new_log = malloc(new_size*sizeof(uint8_t));
  if(new_log == NULL){
    log_err("Unable to further expand the backlog");
    return -1;
  }
  // we have the larger space... the copying from the old buffer will differ
  // depending on the state of wrapping
  if(backlog->start_pos <= backlog->end_pos){
    memcpy(new_log,&(backlog->buffer[backlog->start_pos]),(backlog->end_pos-backlog->start_pos));
    free(backlog->buffer);
    backlog->buffer = new_log;
    backlog->end_pos = backlog->end_pos-backlog->start_pos;
    backlog->start_pos = 0;
    backlog->size = new_size;
    return 0;
  }

  // if we are here there was wrapping and we need to do 2 memcpys
  memcpy(new_log,&(backlog->buffer[backlog->start_pos]),(backlog->size-backlog->start_pos));
  memcpy(&(new_log[(backlog->size-backlog->start_pos)]),backlog->buffer,(backlog->end_pos));
  backlog->end_pos = (backlog->size-backlog->start_pos) + backlog->end_pos;
  backlog->start_pos = 0;
  backlog->size = new_size;
  free(backlog->buffer);
  backlog->buffer = new_log;
  return 0;
}


/******************************************************************************
* MRT_backlog_shrink
* Purpose: Attempt to shrink the buffer back down to the default starting
*          size
* Notes: Assumes the lock has already been obtained
*        Assumes that the buffer is empty
*        
* Returns: 0 for success, other on failure
******************************************************************************/
int 
MRT_backlog_shrink(MRT_backlog *backlog,uint32_t new_size){

  uint8_t *new_log;

  // only shrink an empty buffer
  if(backlog->start_pos != backlog->end_pos){
    return -1;
  }
  
  new_log = malloc(new_size*sizeof(uint8_t));
  if(new_log == NULL){
    log_err("Unable to further expand the backlog");
    return -1;
  }
  free(backlog->buffer);

  backlog->buffer = new_log;
  backlog->start_pos = 0;
  backlog->end_pos = 0;
  backlog->size = new_size;

  return 0;
  
}
/******************************************************************************
* MRT_backlog_read
* This function reads a single mrt message from the backlog
* If the message that is next to be read will not fit in the space
* provided the length of that message is returned.
* Otherwise, 0 on success, 1 on empty, -1 on corrupt, > 1 if the buffer is not
* large enough (the buffer is obviously larger than 1 byte and so thats a 
* resonable restriction.
* The function can be called again with a larger buffer
******************************************************************************/
int 
MRT_backlog_read(MRT_backlog *backlog,MRTheader *mrtHeader,uint8_t *rawMessage, uint16_t bytes){

  uint32_t message_index;
#ifdef DEBUG
    log_msg("read: end_pos: %d start:pos %d\n",backlog->end_pos,
                                                backlog->start_pos);
#endif

  // check to see if the backlog is empty 
  if(backlog->start_pos == backlog->end_pos){
    return 1;
  } 

  // start by scanning for the header
  // there are several cases here
  // 1 - there was no wrapping and there is not a full message
  // 2 - there was no wrapping and there is a full header
  // 3 - there was wrapping and the header is not full
  // 4 - there was wrapping and the header is wrapped
  // 5 - there was wrapping and the header is not wrapped
  if(backlog->start_pos < backlog->end_pos){
    // there has not been any wrapping
    if((backlog->end_pos - backlog->start_pos) < MRT_HEADER_LENGTH){
      // there is not a full message in the buffer as of now
      return 1;
    }
#ifdef DEBUG
log_msg("read: no wrapping, copy header\n");
#endif
    memcpy(mrtHeader,&(backlog->buffer[backlog->start_pos]),
         MRT_HEADER_LENGTH);
    message_index = backlog->start_pos + MRT_HEADER_LENGTH;

  }else if(((backlog->size-backlog->start_pos)+backlog->end_pos) < MRT_HEADER_LENGTH){
    // there has been wrapping and there is not a full message
    return 1; 

  }else if((backlog->size-backlog->start_pos) < MRT_HEADER_LENGTH){
    // the header is in the buffer, but it is wrapped
    memcpy(mrtHeader,&(backlog->buffer[backlog->start_pos]),
           (backlog->size-backlog->start_pos));
    memcpy((mrtHeader+(backlog->size-backlog->start_pos)),backlog->buffer,
           (MRT_HEADER_LENGTH-(backlog->size-backlog->start_pos)));
    message_index = MRT_HEADER_LENGTH-(backlog->size-backlog->start_pos); 
#ifdef DEBUG
log_msg("read: wrapped header\n");
#endif

  }else{
    // there has been wrapping, but the header is not wrapped
    memcpy(mrtHeader,&(backlog->buffer[backlog->start_pos]),
         MRT_HEADER_LENGTH);
    message_index = backlog->start_pos + MRT_HEADER_LENGTH;
#ifdef DEBUG
log_msg("read: wrapped, but not header\n");
#endif
    
  }

  // the header has been read -- network ordering
  mrtHeader->timestamp = ntohl(mrtHeader->timestamp);
  mrtHeader->type = ntohs(mrtHeader->type);
  mrtHeader->subtype = ntohs(mrtHeader->subtype);
  mrtHeader->length = ntohl(mrtHeader->length);

  // validate the header
  int err_code = MRT_header_invalid(mrtHeader);
  if(err_code){
#ifdef DEBUG
 log_err("invalid header: type %d subtype  %d\n",mrtHeader->type,
                                                 mrtHeader->subtype);
#endif
    // we don't have a valid header... 
    return (-1*err_code); 
  } 

  // check to see if there is a full message in the buffer
  if(backlog->start_pos < backlog->end_pos){
    // there has not been any wrapping
    if((backlog->end_pos - backlog->start_pos) < 
       (MRT_HEADER_LENGTH + mrtHeader->length)){
      // there is not a full message in the buffer as of now
      return 1;
    }
  }else if(((backlog->size-backlog->start_pos)+backlog->end_pos) < 
           (MRT_HEADER_LENGTH + mrtHeader->length)){
    return 1;
  }
  
  // check to see if the length of the message will fit in the buffer
  if(mrtHeader->length > bytes){
    return mrtHeader->length;
  }

  // Now we can go ahead and read the message.
  // No validation is done on the message itself, that is left to the
  // caller.
  // We have the same cases as above
  // 1 - there has been no wrapping, but the full message is not there
  // 2 - no wrapping, message is there
  // 3 - wrapping, message is not there
  // 4 - wrapping, message is wrapped
  // 5 - wrapping, message is not wrapped
  if(message_index < backlog->end_pos){
    // there has not been any wrapping
    if((backlog->end_pos - message_index) < mrtHeader->length){
      // there is not a full message in the buffer as of now
      return 1;
    }
#ifdef DEBUG
log_msg("read: no message wrapping\n");
#endif
    memcpy(rawMessage,&(backlog->buffer[message_index]),
         mrtHeader->length);
    message_index = message_index + mrtHeader->length;

  }else if(((backlog->size-message_index)+backlog->end_pos)<mrtHeader->length){
    // there has been wrapping and there is not a full message
    return 1; 

  }else if((backlog->size-message_index) < mrtHeader->length){
    // the message is in the buffer, but it is wrapped
#ifdef DEBUG
log_msg("read: message wrapped\n");
#endif
    memcpy(rawMessage,&(backlog->buffer[message_index]),
           (backlog->size-message_index));
    memcpy((rawMessage+(backlog->size-message_index)),backlog->buffer,
           (mrtHeader->length-(backlog->size-message_index)));
    message_index = mrtHeader->length-(backlog->size-message_index); 

  }else{
#ifdef DEBUG
log_msg("read: message not wrapped\n");
#endif
    // there has been wrapping, but the message is not wrapped
    memcpy(rawMessage,&(backlog->buffer[message_index]),
         mrtHeader->length);
    message_index = message_index + mrtHeader->length;
  }

  // move the message index to the end of the read message
  backlog->start_pos = message_index;
#ifdef DEBUG
    log_msg("read: end_pos: %d start:pos %d\n",backlog->end_pos,
                                                backlog->start_pos);
#endif

  return 0;
}

/******************************************************************************
* MRT_backlog_fastforward_BGP
* This function attempts to find the next BGP header and then backup to the
* start of the MRT header before the BGP header
* Assumptions: The buffer is full of MRT data
*              The lock has already been acquired
* Returns: 0 if a fast forward took place, 1 if a fast forward was no possible
******************************************************************************/
int
MRT_backlog_fastforward_BGP(MRT_backlog *backlog)
{

  uint32_t curr_pos;
  uint8_t hdr_cnt = 0;
  // As long as curr_pos is less than the end of the buffer
  // and not equal to the end_pos we should keep going
  // start at start_pos;
  curr_pos = backlog->start_pos;
  while(curr_pos != backlog->end_pos && hdr_cnt < 16){

    // if we have all ones increment out count
    if(backlog->buffer[curr_pos] == 255){
      hdr_cnt++;
    }else{
      // start over at 0
      hdr_cnt = 0;
    }

    curr_pos++;
    if(curr_pos >= backlog->size){
      curr_pos = 0;
    }
  }

  // check to see if we found the header
  if(hdr_cnt == 16){
    // now we need to step forward until we reach
    // the end of this message
    uint8_t bgp_length = ntohs((uint8_t)backlog->buffer[curr_pos]);
    bgp_length -= BGP_HEADER_LEN;
    // attempt to fast forward to the end of the length (this should be the
    // next message)
    if(curr_pos >= backlog->start_pos && curr_pos <= backlog->end_pos){
      if(curr_pos+bgp_length > backlog->end_pos){
        return 1; 
      } 
      backlog->start_pos = (curr_pos+bgp_length);
      return 0;
    }else if(curr_pos >= backlog->start_pos && (backlog->end_pos <
                                                backlog->start_pos)){
      if((bgp_length - (backlog->size-curr_pos)) > backlog->end_pos){
        return 1;
      }
      backlog->start_pos = (bgp_length - (backlog->size-curr_pos));
      return 0;
    }else if(curr_pos < backlog->start_pos && curr_pos <= backlog->end_pos){
      if((curr_pos + bgp_length) <= backlog->end_pos){
        backlog->start_pos = (curr_pos + bgp_length);
        return 0;
      }
    }
  }
  backlog->start_pos = backlog->end_pos = 0;
  // if we get here then we have emptied the buffer
  // and not found a valid header
  // don't change the state - just indicate the fast forwarding failed
  return 1;
}
  
/******************************************************************************
 * MRT_readMessage
 * Input: socket, pointer to the MRT header to populate, pointer to space
 *        for the raw message
 * Return: -1 on read error
 *          n on success; n= the number of messages fast forwarded over
 *                        to get to the next valid header
 *          0 on total sucess > 0 with some failures, but can proceed
 *****************************************************************************/
int
MRT_readMessage(int socket,MRTheader *mrtHeader,uint8_t *rawMessage){

  int forward = 0;

  while(MRT_parseHeader(socket,mrtHeader)){
    log_warning("mrtThread, header parsing failed\n");
    if(MRT_fastForwardToBGPHeader(socket)){
      return -1;
    }
    forward++;
  }
  if(forward > 0){
    log_err("mrtThread (readMessage): had to fast forward %d times to recover, new length is %lu\n",forward,mrtHeader->length);
  }

  int read_len = readn(socket,rawMessage,mrtHeader->length);
  if(read_len != mrtHeader->length){
    log_err("mrtThread, message reading failed\n");
    return -1;
  }
  return forward;
}

/******************************************************************************
 * MRT_readMessageNoHeader
 * Input: socket, pointer to the MRT header to populate, pointer to space
 *        for the raw message
 * Return: -1 on read error
 *          n on success; n= the number of messages fast forwarded over
 *                        to get to the next valid header
 *          0 on total sucess > 0 with some failures, but can proceed
 *****************************************************************************/
int
MRT_readMessageNoHeader(int socket,MRTheader mrtHeader,uint8_t *rawMessage){

  int read_len = readn(socket,rawMessage,mrtHeader.length);
  if(read_len != mrtHeader.length){
    log_err("mrtThread, message reading failed\n");
    return -1;
  }
  return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: read in the mrt header
 * Input:  the socket to read from, a pointer the MRT header struct to populate
 * Output: 1 for success
 *         0 for failure --> should result in closing the MRT session
 * Cathie Olschanowsky @ 8/2011
 * first read the MRT common header http://tools.ietf.org/search/draft-ietf-grow-mrt-15#section-2
 * expected format
 *0                   1                   2                   3
 *0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|                           Timestamp                           |
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|             Type              |            Subtype            |
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|                             Length                            |
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|                      Message... (variable)
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
--------------------------------------------------------------------------------------*/
int 
MRT_parseHeader(int socket,MRTheader* mrtHeader)
{
  int n = readn( socket, mrtHeader, sizeof(MRTheader));
  if( n != sizeof(MRTheader) )
  {
    log_err("mrtThread, read MRT header: EOF may have been reached: attempted to read %u bytes and read %d",sizeof(MRTheader),n);
    return -1;
  }
  mrtHeader->timestamp = ntohl(mrtHeader->timestamp);
  mrtHeader->type = ntohs(mrtHeader->type);
  mrtHeader->subtype = ntohs(mrtHeader->subtype);
  mrtHeader->length = ntohl(mrtHeader->length);

  if(mrtHeader->length > MAX_MRT_LENGTH){
    log_warning("mrtThread, read MRT header: invalid length %lu > %lu (this may be valid for table dump)\n",mrtHeader->length,MAX_MRT_LENGTH);
  }
  if(mrtHeader->length < MRT_MRT_MSG_MIN_LENGTH){
    log_err("mrtThread, read MRT header: invalid length %lu < %lu\n",mrtHeader->length,MRT_MRT_MSG_MIN_LENGTH);
    return -1;
  }

  return 0;
}
/*--------------------------------------------------------------------------------------
 * Purpose: write an MRT header to a buffer
 * Input:  the buffer to write to and the mrt header
 * Output: 1 for success
 *         0 for failure --> should result in closing the MRT session
 * Cathie Olschanowsky @ march 2012
 * first read the MRT common header http://tools.ietf.org/search/draft-ietf-grow-mrt-15#section-2
 * expected format
 *0                   1                   2                   3
 *0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|                           Timestamp                           |
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|             Type              |            Subtype            |
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|                             Length                            |
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|                      Message... (variable)
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
--------------------------------------------------------------------------------------*/
int
MRT_writeHeader(uint8_t *dest,MRTheader* mrtHeader){

  int time_pos = 0;
  int type_pos = 4; 
  int subtype_pos = 6;
  int length_pos = 8;

  dest[time_pos]   = htonl(mrtHeader->timestamp);
  dest[type_pos]   = htons(mrtHeader->type);
  dest[subtype_pos]= htons(mrtHeader->subtype);
  dest[length_pos] = htons(mrtHeader->length);

  mrtHeader->timestamp = ntohl(mrtHeader->timestamp);
  mrtHeader->type = ntohs(mrtHeader->type);
  mrtHeader->subtype = ntohs(mrtHeader->subtype);
  mrtHeader->length = ntohl(mrtHeader->length);

  return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: fast forward reading from the socket, the given number of bytes
 * Input:  the socket to read from, the number of bytes to skip
 * Output: 1 for success
 *         0 for failure
--------------------------------------------------------------------------------------*/
int 
fastForward(int socket, int length)
{

  int n = 0;
  u_int8_t *skip = malloc(length);
  if(skip == NULL)
  {
    log_err("mrtThread, unable to malloc %u for skipping\n",length);
    return -1;
  }else{
    n = readn( socket, skip, length );
    if( n != length )
    {
      log_err("mrtThread, unable to read skipped message");
      free(skip);
      return -1;
    }
  }
  free(skip);
  return 0;
}
/*--------------------------------------------------------------------------------------
 * Purpose: fast forward reading from the socket to the next valid BGP header
 * Input:  the socket to read from
 * Output: 1 for success
 *         0 for failure
 * we are searching for 16 255s in a row here.
 * the magic 18 is because 16 bytes + 2 bytes for length = 18 bytes
 * the lenght is inclusive in the BGP header
--------------------------------------------------------------------------------------*/
int 
MRT_fastForwardToBGPHeader(int socket)
{

  u_int8_t nextByte = 0;
  int i,n;

  while(1)
  {
    n = readn( socket, &nextByte, 1);
    if(n != 1){
      log_err("mrtThread, unable to read from socket\n");
      return -1;
    }
    if(nextByte == 255)
    {	
      for(i=0; i<15; i++)
      {
        n = readn( socket, &nextByte, 1);
        if(n != 1){
          log_err("mrtThread, unable to read from socket\n");
          return -1;
        }
        if( nextByte != 255 )
        {
          break;
        }
      }
      if( i == 15 )
      {
        u_int16_t len;
        u_int8_t *tmpBuf = NULL;
        n = readn( socket, &len, 2);
        if(n != 2){
          log_err("mrtThread, unable to read from socket\n");
          return -1;
        }
        len = ntohs(len);
        tmpBuf = malloc(len-18);
        if(tmpBuf == NULL){ 
          log_err("mrtThread, malloc error\n");
          return -1;
        }
        n = readn( socket, tmpBuf, len-18);
        if(n != (len-18)){
          log_err("mrtThread, unable to read from socket\n");
          free(tmpBuf);
          return -1;
        }
        log_msg("mrtThread, found next BGP message with len %d", len);
        free(tmpBuf);
        break;
      }
    }
  }
  return 0;
}
