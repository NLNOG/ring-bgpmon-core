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
 */
 
#include "mrtProcessTable.h"

/* required for logging functions */
#include "../Util/log.h"
#include "../Util/utils.h"

/* needed for address management  */
#include "../Util/address.h"

/* needed for writen function  */
#include "../Util/unp.h"
#include "../Util/bgp.h"

/* needed for function getXMLMessageLen */
#include "../XML/xml.h"

/* needed for session related function */
#include "../Peering/peersession.h"

/* needed for Mrt Table Dump v2 and state of connection */
#include "../Peering/bgppacket.h"
#include "../Peering/bgpmessagetypes.h"

/* needed for deleteRib table if socket connection lost */
#include "../Labeling/label.h"

/* needed for session State */
#include "../Peering/bgpstates.h"
#include "../Peering/bgpevents.h"


/* needed for malloc and free */
#include <stdlib.h>
/* needed for strncpy */
#include <string.h>
/* needed for system error codes */
#include <errno.h>
/* needed for addrinfo struct */
#include <netdb.h>
/* needed for system types such as time_t */
#include <sys/types.h>
/* needed for time function */
#include <time.h>
/* needed for socket operations */
#include <sys/socket.h>
/* needed for pthread related functions */
#include <pthread.h>

#include <stdio.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <netinet/in.h>

//#define DEBUG


/*--------------------------------------------------------------------------------------
 * Purpose: check how many null and non null tables we have 
 * Input:  tablebuffer pointer,  number of peers in table
 * Output:  0 failure, 1 success (signal to exit while loop)
 * Mikhail Strizhov @ Oct 6, 2010
 * -------------------------------------------------------------------------------------*/
int checkBGPTableEmpty(TableBuffer  *tablebuffer, int NumberPeers)
{	
	int retval = 0;
	int i=0;
	int nulltable = 0;
	int nonnulltable = 0;
	for (i=0;i<NumberPeers;i++)
	{
		if (tablebuffer[i].start == NULL)
		{
			nulltable++;
		}
		else
		{
			nonnulltable++;	
		}
	}

	log_msg("%s, BGP Table still maintains %d peer tables",__FUNCTION__, nonnulltable);
	// check if numberof nulltables is eq to NumberPeers and nonnull is 0
	if ((nulltable == NumberPeers) && (nonnulltable == 0))
	{
		retval = 1;
	}

	return retval;
}

/*--------------------------------------------------------------------------------------
 * Purpose: insert bmf messages into buffer
 * Input:  peers pointer to linked structure, tail of structure(makes addition faster), BMF message
 * Output: return linked list for every node, also updates tail by address
 * Mikhail Strizhov @ Oct 5, 2010
 * -------------------------------------------------------------------------------------*/
int insertBGPTable(TableBuffer *tbl, BGPMessage *bgp )
{
  if(tbl == NULL){
    log_err("Unable to add to non-existant table\n");
    return -1;
  }
  tbl->table_exist = 1;

  if(tbl->start==NULL){
    tbl->start=calloc(1,sizeof(struct BGPTableStruct));
    if(tbl->start == NULL){
      log_err("Malloc Error\n");
      return -1;
    }
    tbl->msg_count = 1;
    tbl->tail = tbl->start;
  } else {
    // get tail
    tbl->tail->next = calloc(1,sizeof(struct BGPTableStruct));
    if(tbl->tail->next == NULL){
      log_err("Malloc Error\n");
      return -1;
    }
    // update tail
    tbl->tail = tbl->tail->next;
  }
	
  tbl->msg_count += 1;
  tbl->tail->BGPmessage = bgp;
  tbl->tail->next  = NULL;
  return 0;

}

/*--------------------------------------------------------------------------------------
 * Purpose: write bmf messages to peer queue
 * Input: SessionID, linked list, Queue writer, convert flag (1 or 0)
 * Output: none
 * Mikhail Strizhov @ Oct 6, 2010
 * -------------------------------------------------------------------------------------*/
void
writeBGPTableToQueue(int ID, struct BGPTableStruct **start, QueueWriter writerpointer, int asLen)
{

  char bgpSerialized[BGP_MAX_MSG_LEN];
  // check if its already free
  if( *start == NULL ) return;   
	
  struct BGPTableStruct *ptr = NULL;

  // this is a temporary hack while we rework the queue system
  // I am breaking the messages into chunks -- filling the queue 1/2 of the way
  // for each group... and then sleeping for up to 1 seconds. 
  int chunkSize = QUEUE_MAX_ITEMS/4; 
  uint32_t messageCount=0;
  for (ptr = *start; ptr != NULL; ptr = ptr->next){
    BMF m = createBMF(ID,BMF_TYPE_TABLE_TRANSFER);
    int res =BGP_serialize((uint8_t*)bgpSerialized,ptr->BGPmessage,asLen);
    if(res){
      log_err("%s: Unable to serialize BGP message",__FUNCTION__);
      return;
    }
    bgpmonMessageAppend(m,bgpSerialized,ptr->BGPmessage->length);
    writeQueue(writerpointer, m );
    incrementSessionMsgCount(ID);
    messageCount++;
    if(messageCount%chunkSize==0){
      sleep(0);
    }
  } 
  log_msg("%s: Table dump required %u messages",__FUNCTION__,messageCount);
  return;
}

/*--------------------------------------------------------------------------------------
 * Purpose: free linked list of BGP messages
 * Input:  peers pointer to linked structure
 * Output: none
 * Mikhail Strizhov @ Oct 5, 2010
 * -------------------------------------------------------------------------------------*/
void freeBGPTable ( TableBuffer *tbl) 
{
	struct BGPTableStruct *ptr1,*ptr2;

	ptr1=tbl->start;
        while( ptr1!=NULL ) 
	{
                ptr2=ptr1->next; 
                BGP_freeMessage(ptr1->BGPmessage);
		free(ptr1);
                ptr1 = ptr2;
	}
        tbl->start = tbl->tail = NULL;
}

/*--------------------------------------------------------------------------------------
 * Purpose: this code processes MRT messages of type 13   TABLE_DUMP_V2
 * Input:  the socket to read from, the mrtheader object that put us here,
 * Output: 1 for success
 *         0 for failure
 * We are making the assumption that messages of type 13 will not be found in 
 * the same conversation as other types. Once we step into this subroutine
 * we will only process type 13s until disconnect.
 * TODO: this function needs more refactoring --> it is too long The Jira Issue is 
 * BGPMON-29
--------------------------------------------------------------------------------------*/
int 
MRT_processType13(MrtNode *cn, uint8_t *rawMessage, int *rawMessage_length, MRTheader *mrtHeader)
{

  mrt_index indexPtr; 	
  short eof = 0;
  TableBuffer *tablebuffer;
  BGPMessage **bgp_arr;
  int *peer_idxs;
  int bgp_count = 0;
  int i;

  if(mrtHeader->subtype != 1){
    log_err("mrtThread, TABLE_DUMP_V2 initiated with subtype %d rather than 1\n",mrtHeader->subtype);
    return -1;
  }

  // the first message is used to create a temporary table of sessions.
  // if this fails we should drop the connection -- return -1
  if(MRT_createTableBufferFromType13Subtype1(cn,&indexPtr,&tablebuffer,rawMessage,*rawMessage_length,mrtHeader)){
    log_err("mrtThread, TABLE_DUMP_V2 the first message was not processed\n");
    return -1;  
  }

  // create the space to be used for processing the messages
  bgp_arr = calloc(indexPtr.PeerCount,sizeof(BGPMessage*));
  peer_idxs = calloc(indexPtr.PeerCount,sizeof(int));

  // The Message 13 subtype 1 message is complete and now we recieve other messages
  // at this point we are in a conversation with the MRT collector and are 
  // expecting a series of messages of type 13 --> no other type
  // should be seen and we should not see any more of subtype 1.
#ifdef DEBUG
int debug_count = 0;
#endif
  uint32_t msg_count = 0;
  while ( cn->deleteMrt==FALSE && !eof )
  {
#ifdef DEBUG
debug_count++;
#endif
    // update the last action time
    cn->lastAction = time(NULL);

    if(MRT_parseHeader(cn->socket,mrtHeader)){
      eof = 1;
      continue;
    }
    if(mrtHeader->length > *rawMessage_length){
      free(rawMessage);
      rawMessage = calloc(mrtHeader->length,sizeof(uint8_t));
      *rawMessage_length = mrtHeader->length;
    }
    if(rawMessage == NULL){
      log_err("%s, Malloc failure, unable to allocate message buffer",__FUNCTION__);
      return -1;
    }
    if(MRT_readMessageNoHeader(cn->socket,*mrtHeader,rawMessage)){
      eof = 1;
      continue;
    }

    // Only one type of message should be coming through on this port
    if(mrtHeader->type != TABLE_DUMP_V2){
      log_err("mrtThread, an MRT message of type %d was recieved by the thread handling only 13 (TABLE) messages\n",mrtHeader->type);
      free(tablebuffer);
      return -1;
    }

    switch (mrtHeader->subtype){
      case PEER_INDEX_TABLE:
        log_err("mrtThread, only one type 13 subtype 1 message is expected by this thread\n");
        free(tablebuffer);
        return -1;
      case RIB_IPV4_MULTICAST:
      case RIB_IPV6_MULTICAST:
        log_warning("mrtThread, unsupported table dump subtype RIB_IPV(4|6)_MULTICAST\n");
        break;
      case RIB_IPV4_UNICAST:
      case RIB_IPV6_UNICAST:
        if(MRT_processType13SubtypeSpecific(tablebuffer,mrtHeader,rawMessage,bgp_arr,peer_idxs,&bgp_count,indexPtr.PeerCount)){
          eof = 1;
        }
        msg_count+=bgp_count;
        break;
      case RIB_GENERIC:
        if(MRT_processType13SubtypeGeneric(mrtHeader,rawMessage,bgp_arr,peer_idxs,&bgp_count)){
          eof = 1;
        }
        break;
      default:
        log_err("mrtThread, Invalid subtype for type 13 MRT message\n", mrtHeader->subtype);
        free(tablebuffer);
        return -1;
    }

  } // end while
  log_msg("%s: A total of %u messages created",__FUNCTION__,msg_count);

  free(bgp_arr);
  free(peer_idxs);
  // the loop has ended --> this means that the MRT session is being closed through a shutdown or through an eof.
  close( cn->socket );
  log_msg("MRT table dump thread: preparing to exit");

  int tableloopflag=0;
  // 6 * 30 = 3 minutes to wait for update message. If no update message - erase all.
  // there is no need to do this if we are shutting down bgpmon.... in that case just proceed to cleanup
  while (tableloopflag<6 && cn->deleteMrt==FALSE ){

    cn->lastAction = time(NULL);
    // check if all pointers are null. if yes, than all tables are sent, thus exit thread
    if (checkBGPTableEmpty(tablebuffer, indexPtr.PeerCount) == 1)
    {	
      log_msg("mrtThread, Table Transfer is empty, exit the MRT thread");
      break;
    }
    // update the last action time
    // go through the peer list
    int i;
    for( i=0; i<indexPtr.PeerCount && cn->deleteMrt==FALSE; i++ ){	
      // check the state of ID:  if update message came, it will be changed to stateMrtEstablished
      if ( getSessionState(tablebuffer[i].ID) == stateMrtEstablished ) 
      {
        log_msg("%s: found active session associated to table dump (session:%d)",__FUNCTION__,tablebuffer[i].ID);
        // check if session  ASN has changed to 2 byte
        if ( (tablebuffer[i].table_exist == 1) && (tablebuffer[i].start != NULL)   ) {
          cn->lastAction = time(NULL);
          log_msg("mrtThread, Session %d change ASN to %d, send entire table to MRTQueue",
                   tablebuffer[i].ID,getSessionASNumberLength(tablebuffer[i].ID));
          // write all messages for this peer
          log_msg("%s: this table has %u messages",__FUNCTION__,tablebuffer[i].msg_count);
          writeBGPTableToQueue(tablebuffer[i].ID, &(tablebuffer[i].start), cn->qWriter, getSessionASNumberLength(tablebuffer[i].ID) );
          // free linked list
          log_msg("mrtThread, Table for Session %d sent", tablebuffer[i].ID);
          freeBGPTable(&tablebuffer[i]);
          tablebuffer[i].start = NULL;
          tablebuffer[i].tail = NULL;
        }else if (tablebuffer[i].table_exist == 0){
          // case when received table for a peer is null (tablebuffer[i].start == NULL)
          // and table_exist flag was not set to 1
          // delete entire table from our RIB-IN
          cn->lastAction = time(NULL);
          log_msg("No BGP messages received from MRT TABLE_DUMP_V2, I will delete RIB-IN table for Session %d", tablebuffer[i].ID);
          // change state to stateError
          setSessionState( getSessionByID(tablebuffer[i].ID), stateError, eventManualStop);
          // delete prefix table and attributes
          if (cleanRibTable( tablebuffer[i].ID)  < 0)
          {
            log_warning("Could not clear table for Session ID %d", tablebuffer[i].ID);
          }
          // skip deletion next loop
          tablebuffer[i].table_exist = 1;
          // free nothing
          freeBGPTable(&tablebuffer[i]);
          tablebuffer[i].start = NULL;
          tablebuffer[i].tail = NULL;
        }
      }
    }
    tableloopflag++;
    // sleep
    sleep(TABLE_TRANSFER_SLEEP);
  }

  log_msg("MRT table dump thread(%d) freeing memory.",cn->id);
  cn->lastAction = time(NULL);
  // free peer table index
  for (i=0; i < indexPtr.PeerCount;i++)
  {
    // free linked list
    freeBGPTable(&tablebuffer[i]);
    tablebuffer[i].start = NULL;
    tablebuffer[i].tail = NULL;
  }
  indexPtr.PeerCount = 0;
  free(tablebuffer);
  log_msg("MRT table dump thread(%d) exiting.",cn->id);
  return 1;
}	

/******************************************************************************
 * Name: MRT_createTableBufferFromType13Subtype1
 * Input: MrtNode, mrtindex, tablebuffer (to be allocated), rawMessage mrtHeader
 * Outout: 0 on sucess, -1 on failure
 * Description:
 * This function reads the first message in a rib table conversation
 * and creates the infrastructure for saving all of the other data to be 
 * receieved.
******************************************************************************/
int
MRT_createTableBufferFromType13Subtype1(MrtNode *cn, mrt_index *indexPtr,TableBuffer **tablebuffer,
                                        const uint8_t *rawMessage,int rawMessage_length, MRTheader *mrtHeader)
{

  int idx = 0;
  char SrcIPString[ADDR_MAX_CHARS];
  // This is an MRT message type 13 subtype 1
  // It is a PEER_INDEX_TABLE

  // now we start reading the header
  // http://tools.ietf.org/search/draft-ietf-grow-mrt-17#section-4
  //         0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                      Collector BGP ID                         |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |       View Name Length        |     View Name (variable)      |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |          Peer Count           |    Peer Entries (variable)
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  //                Figure 5: PEER_INDEX_TABLE Subtype

  // collector BGP ID
  memmove(&indexPtr->BGPSrcID,&rawMessage[idx],sizeof(indexPtr->BGPSrcID)); 
  idx += sizeof(indexPtr->BGPSrcID);
  if( inet_ntop(AF_INET, &indexPtr->BGPSrcID, SrcIPString, ADDR_MAX_CHARS) == NULL ){
     log_err("mrtThread, TABLE DUMP v2 message: indexPtr->BGPSrcID ipv4 convert error!");
     return -1;
  }
  if(idx >= rawMessage_length){
    log_err("%s, reading over the end of the raw Message: Parsing error detected after BGPSrcID",__FUNCTION__);
    return -1;
  }

  // View Name Length
  MRT_READ_2BYTES(indexPtr->ViewNameLen,rawMessage[idx]);
  idx += 2;
  if (indexPtr->ViewNameLen != 0)
  {
    // this assumes that the view name lenght is expresses in bytes
    idx += indexPtr->ViewNameLen;
  }
  if(idx >= rawMessage_length){
    log_err("%s, reading over the end of the raw Message: Parsing error detected after ViewNameLen",__FUNCTION__);
    return -1;
  }

  // Peer Count
  MRT_READ_2BYTES(indexPtr->PeerCount,rawMessage[idx]);
  idx += 2;
  if(idx >= rawMessage_length){
    log_err("%s, reading over the end of the raw Message: Parsing error detected after PeerCount",__FUNCTION__);
    return -1;
  }

  // allocate array of linked structure for each peer
  (*tablebuffer) = calloc( indexPtr->PeerCount,sizeof(TableBuffer));
  if (*tablebuffer == NULL)
  {
    log_err("Calloc failed");
    return -1;
  }

  // Now that the header has been read -- handle each entry
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |   Peer Type   |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                         Peer BGP ID                           |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                   Peer IP address (variable)                  |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                        Peer AS (variable)                     |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  //                      Figure 6: Peer Entries
  int i;
  for (i = 0; i < (indexPtr->PeerCount); i++)
  {
    // Peer Type
    // the peer type will tell us we we have ipv4 or ipv6 IP addresses
    //The Peer Type field is a bit field which encodes the type of the AS
    //and IP address as identified by the A and I bits, respectively,
    //below.
    //
    // 0 1 2 3 4 5 6 7
    //+-+-+-+-+-+-+-+-+
    //| | | | | | |A|I|
    //+-+-+-+-+-+-+-+-+
    //
    //Bit 6: Peer AS number size:  0 = 16 bits, 1 = 32 bits
    //Bit 7: Peer IP Address family:  0 = IPv4,  1 = IPv6
    //
    //                 Figure 7: Peer Type Field
    indexPtr->PeerType = rawMessage[idx];
    idx++;
    if(idx >= rawMessage_length){
      log_err("%s, reading over the end of the raw Message: Parsing error detected after PeerType",__FUNCTION__);
      return -1;
    }
			
    // Peer BGP ID 
    memmove(&indexPtr->PeerBGPID,&rawMessage[idx],sizeof (indexPtr->PeerBGPID));
    idx+=4;
    if(idx >= rawMessage_length){
      log_err("%s, reading over the end of the raw Message: Parsing error detected after PeerBGPID",__FUNCTION__);
      return -1;
    }

    // Peer IP address: the size depends on if it is IPV4 or 6 (see note above)
    if ( (indexPtr->PeerType & 0x01) == 0) 
    {
      memmove(&indexPtr->PeerIP,&rawMessage[idx],4);
      idx+=4;
    if(idx >= rawMessage_length){
      log_err("%s, reading over the end of the raw Message: Parsing error detected after PeerIP",__FUNCTION__);
      return -1;
    }
      if( inet_ntop(AF_INET, &indexPtr->PeerIP, indexPtr->PeerIPStr, ADDR_MAX_CHARS) == NULL ){
        perror("INET_NTOP ERROR:");
        log_err("mrtThread, TABLE DUMP v2 message: ipv4 source address convert error!");
        free(*tablebuffer);
        *tablebuffer = NULL;
        return -1;
      }
    }else{ // indexPtr.PeerType & 0x01) == 1
      memmove(&indexPtr->PeerIP,&rawMessage[idx],16);
      idx+=16;
      if(idx >= rawMessage_length){
        log_err("%s, reading over the end of the raw Message: Parsing error detected after PeerIP(IPV6)",__FUNCTION__);
        return -1;
      }
      if( inet_ntop(AF_INET6, &indexPtr->PeerIP, indexPtr->PeerIPStr, ADDR_MAX_CHARS) == NULL ){
        perror("INET_NTOP ERROR:");
        log_err("mrtThread, TABLE DUMP v2 message: ipv6 source address convert error!");
        free(*tablebuffer);
        tablebuffer = NULL;
        return -1;
      }				
    }

    // check 2nd bit - ASN2 or ASN4
    int ASNumLen = 4;
    if ( (indexPtr->PeerType & 0x02) == 0)
    {
      // set AS Num Len to 2 byte
      ASNumLen = 2;
    }
    // PeerAS
    memmove(&indexPtr->PeerAS,&rawMessage[idx],ASNumLen);
    idx+=ASNumLen;
    if(idx >= rawMessage_length){
      log_err("%s, reading over the end of the raw Message: Parsing error detected after ASNumLen",__FUNCTION__);
      return -1;
    }
    if(ASNumLen == 2){
      indexPtr->PeerAS = ntohs (indexPtr->PeerAS);
    }else{
      indexPtr->PeerAS = ntohl (indexPtr->PeerAS);
    }

    // at this point the index table entry has been read. 
    // the next step is to look up the associated session.
    // in the case that a session does not exist, a temporary session is created.
    // A temporary session is required, until we recieve an update message with the associated
    // session that indicates if we should be using 2 byte or 4 byte ASNs in the table.
    // the MRT collector promotes ASNs to 4 bytes and so at this point we cannot know which to use.
    int sessionID = findOrCreateMRTSessionStruct(indexPtr->PeerAS,indexPtr->PeerIPStr,cn->addr,
                                                 cn->labelAction,UNKASNLEN,stateError,eventManualStop);
    if( sessionID < 0)
    {
      log_err("mrtThread (13), fail to create a new session for, peer AS %u, peer IP %s, collector IP %s",  
                indexPtr->PeerAS, indexPtr->PeerIPStr, cn->addr );
      free(*tablebuffer);
      *tablebuffer = NULL;
      return -1;
    }else{
      (*tablebuffer)[i].ID = sessionID;
#ifdef DEBUG
      log_msg("mrtThread (13), found or created session for peer AS %u, peer IP %s, collector IP %s",  
                indexPtr->PeerAS, indexPtr->PeerIPStr, cn->addr );
#endif
    }
  } // end of Peer Entries Loop

  log_msg("%s:successful completion of 1st message\n",__FUNCTION__);
  return 0;
}

/*-------------------------------------------------------------------------------------
 * Purpose: Process the Generic subtype from ribs
 * Input: NOT YET IMPLEMENTED
 * Output: NOT YET IMPLEMENTED
 * Info:
 * 4.3.3. RIB_GENERIC Subtype


   The RIB_GENERIC header is shown below.  It is used to cover RIB
   entries which do not fall under the common case entries defined
   above.  It consists of an AFI, Subsequent AFI (SAFI) and a single
   NLRI entry.  The NLRI information is specific to the AFI and SAFI
   values.  An implementation which does not recognize particular AFI
   and SAFI values SHOULD discard the remainder of the MRT record.

        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         Sequence number                       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |    Address Family Identifier  |Subsequent AFI |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |     Network Layer Reachability Information (variable)         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |         Entry Count           |  RIB Entries (variable)
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

                    Figure 9: RIB_GENERIC Entry Header
---------------------------------------------------------------------------------------*/
int 
MRT_processType13SubtypeGeneric(MRTheader *mrtHeader,const uint8_t *rawMessage, BGPMessage **bgp_arr,int *peer_idxs,int *bgp_count) 
{
  log_warning("mrtThread, generic table table dump message ignored\n");
  return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: this code processes MRT messages of type 13 subtype (2-5) 
 * Input:  pointer to table buffer, ptr to mrt header, raw mesage, a pointer to an 
           array of messages, an array for peer indexes, an array for counts and
           the max count expected
 * Output: 0 for success
 *         -1 for failure, this may be a read error or a format error
 *           therefore, it should not cause the read loop to exit
 *
 * Description: The main job of this function is to take the rawMessage, which is 
 * in MRT format, change it to BGP format, and then wrap it with a BMF header.
 * There will be one bmf message for each rib entry in the MRT message
--------------------------------------------------------------------------------------*/
int 
MRT_processType13SubtypeSpecific(TableBuffer *tablebuffer,MRTheader *mrtHeader,const uint8_t *rawMessage, BGPMessage **bgp_arr,
                                 int *peer_indexes,int *bgp_count,int max_count) 
{

  int idx =0; // this variable keeps track of our progress walking through rawMessage
  uint16_t afi;
  uint8_t safi;

  // max prefix length in bytes (IPV6 = 16, IPV4 = 4)
  // we can get this from the subtype
  int max_prefix_len = 0;
  switch (mrtHeader->subtype ){
    case RIB_IPV4_UNICAST:
      max_prefix_len = 4;
      afi = 1;
      safi = 1;
      break;
    case RIB_IPV4_MULTICAST:
      max_prefix_len = 4;
      afi = 1;
      safi = 2;
      break;
    case RIB_IPV6_UNICAST:
      max_prefix_len = 16;
      afi = 2;
      safi = 1;
      break;
    case RIB_IPV6_MULTICAST:
      max_prefix_len = 16;
      afi = 2;
      safi = 2;
      break;
    default:
      log_err("mrtThread, invalid subtype\n");
      return -1;
  } 

  // space to save the message data
  mrt_uni uni; 

  // 4.3.2. AFI/SAFI specific RIB Subtypes
  //
  //
  //   The AFI/SAFI specific RIB Subtypes consist of the RIB_IPV4_UNICAST,
  //   RIB_IPV4_MULTICAST, RIB_IPV6_UNICAST, and RIB_IPV6_MULTICAST
  //   Subtypes.  These specific RIB table entries are given their own MRT
  //   TABLE_DUMP_V2 subtypes as they are the most common type of RIB table
  //   instances and providing specific MRT subtypes for them permits more
  //   compact encodings.  These subtypes permit a single MRT record to
  //   encode multiple RIB table entries for a single prefix.  The Prefix
  //   Length and Prefix fields are encoded in the same manner as the BGP
  //   NLRI encoding for IPV4 and IPV6 prefixes.  Namely, the Prefix field
  //   contains address prefixes followed by enough trailing bits to make
  //   the end of the field fall on an octet boundary.  The value of
  //   trailing bits is irrelevant.
  //
  //        0                   1                   2                   3
  //        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //       |                         Sequence number                       |
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //       | Prefix Length |
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //       |                        Prefix (variable)                      |
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //       |         Entry Count           |  RIB Entries (variable)
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  //                        Figure 8: RIB Entry Header
		
  // Sequence number
  MRT_READ_4BYTES(uni.SeqNumb,rawMessage[idx]);
  idx+=4;

  // Prefix Length
  uni.PrefixLen = rawMessage[idx];
  idx+=1;

  // validate the prefix length
  if(uni.PrefixLen < 0 || uni.PrefixLen > max_prefix_len*8){
    log_err("mrtThread, the prefix length is not valid for this type \n");
    return 0;
  }

  // Prefix
  int i;
  for(i=0; i<uni.PrefixLen/8;i++){
    uni.Prefix[i] = rawMessage[idx];
    idx++;
  }
  if(uni.PrefixLen%8){
    uni.Prefix[i] = rawMessage[idx];
    idx++;
  }

  // Entry Count
  MRT_READ_2BYTES(uni.EntryCount,rawMessage[idx]);
  idx +=2;

  if(uni.EntryCount > max_count){
    log_err("The mrt table dump message contains more entries than the number of peers\n");
    return -1;
  }
 
  // each entry will result in a bgp message being added to the array
  (*bgp_count) = 0;

  // RIB Entries
  // 4.3.4. RIB Entries
  //
  //
  //   The RIB Entries are repeated Entry Count times.  These entries share
  //   a common format as shown below.  They include a Peer Index from the
  //   PEER_INDEX_TABLE MRT record, an originated time for the RIB Entry,
  //   and the BGP path attribute length and attributes.  All AS numbers in
  //   the AS_PATH attribute MUST be encoded as 4-Byte AS numbers.
  //
  //        0                   1                   2                   3
  //        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //       |         Peer Index            |
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //       |                         Originated Time                       |
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //       |      Attribute Length         |
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //       |                    BGP Attributes... (variable)
  //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  //                          Figure 10: RIB Entries
  //
  //   There is one exception to the encoding of BGP attributes for the BGP
  //   MP_REACH_NLRI attribute (BGP Type Code 14) RFC 4760 [RFC4760].  Since
  //   the AFI, SAFI, and NLRI information is already encoded in the
  //   MULTIPROTOCOL header, only the Next Hop Address Length and Next Hop
  //   Address fields are included.  The Reserved field is omitted.  The
  //   attribute length is also adjusted to reflect only the length of the
  //   Next Hop Address Length and Next Hop Address fields.
#ifdef DEBUG
log_msg("MRT type specific message: %d entry count\n",uni.EntryCount);
#endif
  int j;
  for (j = 0; j < uni.EntryCount; j++)
  {
    // each of these will result in an BGP message being created
    // each of them will be an update message
    BGPMessage *bgpMessage = BGP_createMessage(BGP_UPDATE);

    // Peer Index
    MRT_READ_2BYTES(uni.PeerIndex,rawMessage[idx]);
    idx+=2;
 
    // Originating Time 
    MRT_READ_4BYTES(uni.OrigTime,rawMessage[idx]);
    idx+=4;

    // Attribute Length	
    MRT_READ_2BYTES(uni.AttrLen,rawMessage[idx]);
    idx+=2;
    int attr_start_pos = idx; // keep track of where the attributes start
                              // so we know when they are finished
  
    // BGP Attributes
    //                 0                   1
    //          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    //         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //         |  Attr. Flags  |Attr. Type Code|
    //         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    while(idx-attr_start_pos < uni.AttrLen){
      // type is handled special because it is actually a 2 octet data type and not 
      // a value to be combined using ntohs.
      uint8_t flags = rawMessage[idx]; 
      uint8_t code  = rawMessage[idx+1]; 
      idx+=2;

      // If the Extended Length bit of the Attribute Flags octet is set
      // to 0, the third octet of the Path Attribute contains the length
      // of the attribute data in octets.
      //
      // If the Extended Length bit of the Attribute Flags octet is set
      // to 1, the third and fourth octets of the path attribute contain
      // the length of the attribute data in octets.
      int len = BGP_PAL_LEN(flags);
      uint16_t length;
      if(len == 1){
        length = rawMessage[idx];
        idx++;
      }else{
        MRT_READ_2BYTES(length,rawMessage[idx]);
        idx+=2;
      }
    
      // this is a special case ==> MP_REACH_NLRI
      // for this one we need to do some editing to the attributes
      // There is one exception to the encoding of BGP attributes for the BGP
      // MP_REACH_NLRI attribute (BGP Type Code 14) RFC 4760 [RFC4760].  Since
      // the AFI, SAFI, and NLRI information is already encoded in the
      // MULTIPROTOCOL header, only the Next Hop Address Length and Next Hop
      // Address fields are included.  The Reserved field is omitted.  The
      // attribute length is also adjusted to reflect only the length of the
      // Next Hop Address Length and Next Hop Address fields.

      // TODO: remove the 14 and put in #define
      if(code == 14){
#ifdef DEBUG
  log_msg("MRT TD Processing code type 14\n");
#endif
        // this is what we want
        //+---------------------------------------------------------+
        //| Address Family Identifier (2 octets)                    |
        //+---------------------------------------------------------+
        //| Subsequent Address Family Identifier (1 octet)          |
        //+---------------------------------------------------------+
        //| Length of Next Hop Network Address (1 octet)            |
        //+---------------------------------------------------------+
        //| Network Address of Next Hop (variable)                  |
        //+---------------------------------------------------------+
        //| Reserved (1 octet)                                      |
        //+---------------------------------------------------------+
        //| Network Layer Reachability Information (variable)       |
        //+---------------------------------------------------------+

        // this is what we have
        //+---------------------------------------------------------+
        //| Length of Next Hop Network Address (1 octet)            |
        //+---------------------------------------------------------+
        //| Network Address of Next Hop (variable)                  |
        //+---------------------------------------------------------+

        // TODO: magic number removal
        uint8_t newAtt[135];
        int i =0;
        // AFI and SAFI are determined by the type and subtype
        // look at the switch statement above to see them set
        BGP_ASSIGN_2BYTES(&newAtt[i],afi);
        i+=2;
        newAtt[i]=safi;
        i+=1;

        // Length of next hop and next hop
        int j;
        for(j=0;j<length;j++){
          newAtt[i+j]=rawMessage[idx+j];
        }
        i+=j;
        idx+=j;

        // Reserved
        i+=1;

        // NLRI -- there is only one here
        newAtt[i] = uni.PrefixLen;
        i+=1;
        for(j=0;j<uni.PrefixLen/8;j++){
          newAtt[i] = uni.Prefix[j];
          i+=1;
        } 
        if(uni.PrefixLen%8){
          newAtt[i] = uni.Prefix[j];
          i+=1;
        }
        //idx += length;
        // this is where the path attribute is added to the object 
        if(uni.AttrLen != (idx-attr_start_pos)){
          log_err("%s: Within special type attr length and current position don't match\n");
        }
        BGP_addPathAttributeToUpdate(bgpMessage,flags,code,i,newAtt);
      }else{ 
        // this is where the path attribute is added to the object 
        BGP_addPathAttributeToUpdate(bgpMessage,flags,code,length,&rawMessage[idx]);
        idx+=length; 
      }
    }
    if(idx-attr_start_pos != uni.AttrLen){
      log_err("Attribute length does not match position: attrLen: %d pos: %d\n",uni.AttrLen,idx-attr_start_pos);
    }
    if(uni.PeerIndex >= max_count){
      log_err("PeerIndex is out of range: %d\n",uni.PeerIndex);
      return -1;
    }
    (*bgp_count)++;
    // At this point this BGP message is complete add it to the table buffer
    if(insertBGPTable(&(tablebuffer[uni.PeerIndex]),bgpMessage)){
      log_err("%s: Unable to insert bgp message to table buffer",__FUNCTION__);
      return -1;
    }
  } // end for

  return 0;
}
