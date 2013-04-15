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
 *  Date: March 2012
 */

#ifndef MRTUTILS_H_
#define MRTUTILS_H_

#include "mrt.h"
#include "mrtMessage.h"
#include "../Util/log.h"
#include "../Util/unp.h"
#include <stdlib.h>
#include <pthread.h>

#define MRT_START_BACKLOG_SIZE 81920 // the default starting size (bytes) for 
                                     // backlog
#define MRT_READ_2BYTES(a,b) a=ntohs(*((uint16_t*)&b)) // read and convert
#define MRT_READ_4BYTES(a,b) a=ntohl(*((uint32_t*)&b)) // read and convert

struct backlog_struct{
  uint8_t         *buffer;        // a pointer to the backlog
  uint32_t        size;           // starts out as 0 (in bytes)
  uint32_t        start_size;      // starts out as 0 (in bytes)
  uint32_t        start_pos; 
  uint32_t        end_pos;
  pthread_mutex_t lock;

};
typedef struct backlog_struct MRT_backlog;

/******************************************************************************
* MRT_backlog_init
* Purpose: allocate the buffer and provide initial values
* Uses the default size defined by MRT_START_BACKLOG_SIZE
* Returns: 0 for success, 1 on failure
******************************************************************************/
int MRT_backlog_init(MRT_backlog *backlog); 

/******************************************************************************
* MRT_backlog_init
* Purpose: allocate the buffer and provide initial values
* Takes a pointer to the backlog as well as the size in bytes 
* Returns: 0 for success, 1 on failure
******************************************************************************/
int MRT_backlog_init_size(MRT_backlog *backlog,uint32_t size); 

/******************************************************************************
* MRT_backlog_destroy
* Purpose: free allocated space, and destroy the lock
* Returns: 0 for success, 1 on failure
******************************************************************************/
int MRT_backlog_destroy(MRT_backlog *backlog); 

/******************************************************************************
* MRT_backlog_write
* Purpose: Write some number of bytes from a buffer to the backlog 
* Notes: This code assumes that the backlog lock has already been obtained!! 
*        If expansion of the buffer is needed, and not possible, messages
*        may be dropped.
* Returns: 0 for success, 1 on failure
******************************************************************************/
int MRT_backlog_write(MRT_backlog *backlog,uint8_t *buffer, uint32_t bytes);

/******************************************************************************
* MRT_backlog_expand
* Purpose: Attempt to expand the buffer to be twice the size it was
* Notes:  Assumes the backlog lock has been aquired
*        
*        
* Returns: 0 for success, other on failure
******************************************************************************/
int MRT_backlog_expand(MRT_backlog *backlog);

/******************************************************************************
* MRT_backlog_shrink
* Purpose: Attempt to shrink the buffer back down to the default starting
*          size
* Notes: Assumes the lock has already been obtained
*        Assumes that the buffer is empty
*        
* Returns: 0 for success, other on failure
******************************************************************************/
int MRT_backlog_shrink(MRT_backlog *backlog,uint32_t new_size);

/******************************************************************************
* MRT_backlog_read
* This function reads a single mrt message from the backlog
* If the message that is next to be read will not fit in the space
* provided the lenght of that message is returned.
* Otherwise, 0 on success, -1 on failure
* The function can be called again with a larger buffer
******************************************************************************/
int MRT_backlog_read(MRT_backlog *backlog,MRTheader *mrtHeader,uint8_t *rawMessage, uint16_t bytes);

/******************************************************************************
* MRT_backlog_fastforward_BGP
* This function attempts to find the next BGP header and then backup to the
* start of the MRT header before the BGP header
* Assumptions: The buffer is full of MRT data
*              The lock has already been acquired
* Returns: 0 if a fast forward took place, 1 if a fast forward was no possible
******************************************************************************/
int MRT_backlog_fastforward_BGP(MRT_backlog *backlog);

/******************************************************************************
 * MRT_readMessage
 * Input: socket, pointer to the MRT header to populate, pointer to space
 *        for the raw message
 * Return: -1 on read error
 *          n on success; n= the number of messages fast forwarded over
 *                        to get to the next valid header
 *          0 on total sucess > 0 with some failures, but can proceed
 *****************************************************************************/
int MRT_readMessage(int socket,MRTheader *mrtHeader,uint8_t *rawMessage);

/******************************************************************************
 * MRT_readMessageNoHeader
 * Input: socket, pointer to the MRT header to populate, pointer to space
 *        for the raw message
 * Return: -1 on read error
 *          n on success; n= the number of messages fast forwarded over
 *                        to get to the next valid header
 *          0 on total sucess > 0 with some failures, but can proceed
 *****************************************************************************/
int MRT_readMessageNoHeader(int socket,MRTheader mrtHeader,uint8_t *rawMessage);

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
int MRT_parseHeader(int socket,MRTheader* mrtHeader);

/*--------------------------------------------------------------------------------------
 * Purpose: fast forward reading from the socket, the given number of bytes
 * Input:  the socket to read from, the number of bytes to skip
 * Output: 1 for success
 *         0 for failure
--------------------------------------------------------------------------------------*/
int MRT_fastForward(int socket, int length);

/*--------------------------------------------------------------------------------------
 * Purpose: fast forward reading from the socket to the next valid BGP header
 * Input:  the socket to read from
 * Output: 1 for success
 *         0 for failure
 * we are searching for 16 255s in a row here.
 * the magic 18 is because 16 bytes + 2 bytes for length = 18 bytes
 * the lenght is inclusive in the BGP header
--------------------------------------------------------------------------------------*/
int MRT_fastForwardToBGPHeader(int socket);

#endif
