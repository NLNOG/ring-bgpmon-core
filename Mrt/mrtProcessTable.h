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
#ifndef MRTPROCESSTABLE_H_
#define MRTPROCESSTABLE_H_
 
/* externally visible structures and functions for mrts */
#include "mrt.h"
#include "mrtMessage.h"
#include "mrtUtils.h"
#include "mrtinstance.h"
#include "../Util/bgp.h"
// needed for QueueWriter
#include "../Queues/queue.h"

/* required for logging functions */
#include "../Util/log.h"
#include "../Util/utils.h"

#define TABLE_TRANSFER_SLEEP 30

// MRT Peer_Index_Table,   subtype = 1
struct MRT_Index_Table
{
        u_int32_t BGPSrcID;
        u_int16_t ViewNameLen;
        u_int16_t ViewName;
        u_int16_t PeerCount;
        u_int8_t  PeerType;
        u_int32_t PeerBGPID;
        u_int8_t  PeerIP[16];
        char  PeerIPStr[ADDR_MAX_CHARS];
        u_int32_t PeerAS;
};
typedef struct MRT_Index_Table mrt_index;

// MRT RIB IP
struct MRT_RIB_Table
{
        u_int32_t SeqNumb;
        u_int8_t PrefixLen;
        char    Prefix[16];
        u_int16_t EntryCount;
        u_int16_t PeerIndex;
        u_int32_t OrigTime;
        u_int16_t AttrLen;
        u_char    BGPAttr[4096];
};
typedef struct MRT_RIB_Table mrt_uni;

// linked list of BGP messages
struct BGPTableStruct
{
        // includes WDLen=0, Attr Len, Attr, NLRI(Pref len, Pref), Could be MP_REACH
        BGPMessage *BGPmessage;
        // length of entire BGP message
        int length;
        // link to next structure
        struct BGPTableStruct *next;
} ;

struct BufferStructure
{
        // Session ID
        int ID;
        // flag which shows existence of linked list of BGP messages
        int table_exist;
        // linked list
        struct BGPTableStruct *start;
        struct BGPTableStruct *tail;
        uint32_t msg_count;
};
typedef struct BufferStructure  TableBuffer;



/*--------------------------------------------------------------------------------------
 * Purpose: check how many null and non null tables we have 
 * Input:  tablebuffer pointer,  number of peers in table
 * Output:  0 failure, 1 success (signal to exit while loop)
 * Mikhail Strizhov @ Oct 6, 2010
 * -------------------------------------------------------------------------------------*/
int checkBGPTableEmpty(TableBuffer  *tablebuffer, int NumberPeers);

/*--------------------------------------------------------------------------------------
 * Purpose: insert bmf messages into buffer
 * Input:  peers pointer to linked structure, tail of structure(makes addition faster), BMF message
 * Output: return linked list for every node, also updates tail by address
 * Mikhail Strizhov @ Oct 5, 2010
 * Input:  peers pointer to linked structure, tail of structure(makes addition faster), BMF message
 * Output: return linked list for every node, also updates tail by address
 * Mikhail Strizhov @ Oct 5, 2010
 * -------------------------------------------------------------------------------------*/
int insertBGPTable(TableBuffer *tbl, BGPMessage *bgp );

/*--------------------------------------------------------------------------------------
 * Purpose: write bmf messages to peer queue
 * Input: SessionID, linked list, Queue writer, convert flag (1 or 0)
 * Output: none
 * Mikhail Strizhov @ Oct 6, 2010
 * -------------------------------------------------------------------------------------*/
void writeBGPTableToQueue(int ID, struct BGPTableStruct **start, QueueWriter writerpointer, int asLen);

/*--------------------------------------------------------------------------------------
 * Purpose: free linked list of BGP messages
 * Input:  peers pointer to linked structure
 * Output: none
 * Mikhail Strizhov @ Oct 5, 2010
 * -------------------------------------------------------------------------------------*/
void freeBGPTable ( TableBuffer *tbl) ;

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
MRT_processType13(MrtNode *cn, uint8_t *rawMessage, int *rawMessage_length, MRTheader *mrtHeader);

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
                                        const uint8_t *rawMessage,int rawMessage_length, MRTheader *mrtHeader);

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
       |         Entry Count           |  RIB Entries (variable)
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

                    Figure 9: RIB_GENERIC Entry Header
---------------------------------------------------------------------------------------*/
int 
MRT_processType13SubtypeGeneric(MRTheader *mrtHeader,const uint8_t *rawMessage, BGPMessage **bgp_arr,int *peer_idxs,int *bgp_count) ;

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
                                 int *peer_indexes,int *bgp_count,int max_count) ;

#endif
