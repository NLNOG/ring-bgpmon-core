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
 *  File: rtable.h
 * 	Authors: He Yan
 *  Data: Jun 22, 2008
 */

#ifndef RTABLE_H_
#define RTABLE_H_

#include <pthread.h>
#include "myhash.h"
#include "../Util/bgpmon_formats.h"
#include "labelutils.h"

#include "../Queues/queue.h"
#define MAX_BGP_MESSAGE_LEN	4096
#define MAX_PREFIX_LEN  512
#define PREFIX_SIZE(x) ((x/8)*8 == x)?x/8:x/8+1
#define MAXV(x, y) (x>y)?x:y

/*----------------------------------------------------------------------------------------
 * AS path Structure
 * -------------------------------------------------------------------------------------*/
typedef struct BGPASPathStruct {
   u_char		*data;      
   u_int32_t	len;
} BGPASPath;

/*----------------------------------------------------------------------------------------
 * Attribute Table Related Structures
 * -------------------------------------------------------------------------------------*/
typedef struct PrefixNodeStruct PrefixNode;

typedef struct PrefixRefNodeStruct {
   struct PrefixRefNodeStruct	*next;
   PrefixNode					*prefixNode;
} PrefixRefNode;

typedef struct ASPathStruct {
	BGPASPath 	asPathData;
	u_int16_t	asPathID;
	u_int32_t	refCount;
} ASPath;

typedef struct AttrNodeStruct {
   struct AttrNodeStruct	*next;
   u_int16_t				refCount;
   PrefixRefNode			*prefixRefNode;
   pthread_rwlock_t			lock;
   ASPath					*asPath;
   INDEX					bucketIndex;
   u_int16_t				basicAttrLen;
   u_int16_t				totalAttrLen;
   u_char					attr[0];
} AttrNode;

typedef struct AttrEntryStruct {
   struct AttrNodeStruct	*node;
   pthread_rwlock_t          lock;
   u_int16_t				nodeCount;
} AttrEntry;

typedef struct AttrTableStruct {
   u_int32_t                  attrCount;
   u_int32_t                  tableSize;
   u_int32_t                  ocupiedSize;
   u_int32_t                  maxNodeCount;
   u_int16_t                  maxCollision; 
   AttrEntry                 *attrEntries;
} AttrTable;



/*----------------------------------------------------------------------------------------
 * Prefix Table Related Structures
 * -------------------------------------------------------------------------------------*/
typedef struct PAddressStruct {
   u_int8_t       p_len;
   u_char         paddr[0];
   //u_char         *paddr;
} PAddress;

typedef struct PrefixStruct {
   u_int16_t      afi;
   u_int8_t       safi;
   PAddress       addr;
} Prefix;

struct PrefixNodeStruct {
   struct PrefixNodeStruct  *next;
   AttrNode                  *dataAttr;
   u_int32_t                  originatedTS;      
   Prefix                     keyPrefix;
};

typedef struct PrefixEntryStruct {
   struct PrefixNodeStruct  *node;
   u_int16_t                  nodeCount;
} PrefixEntry;

typedef struct PrefixTableStruct {
   u_int32_t                  prefixCount;
   u_int32_t                  tableSize;
   u_int32_t                  ocupiedSize;
   u_int16_t                  maxNodeCount;
   u_int16_t                  maxCollision;   
   PrefixEntry               *prefixEntries;
} PrefixTable;


/*----------------------------------------------------------------------------------------
 * Parsed BGP Update Structures
 * -------------------------------------------------------------------------------------*/
#define MAX_MP_NUM 16
//Helper structute
typedef struct HelperStruct {
	u_int8_t	flag;		/*0:mp reach, 1:mp unreach*/
	u_char		*data;
	u_int16_t	dataLength;
} Helper;

// BGP NLRI structure
typedef struct BGPNlriStruct {
	u_int16_t	  nlriLen;	/* length does not include afi and safi */
	u_int16_t	  afi;			
	u_int8_t 	  safi;
	u_char		  *nlri;	/* contains multiple prefixes */
} BGPNlri;

// BGPM mp NLRI structure
typedef struct BGPMPNlriStruct {
	u_int8_t	flag;		/*0:mp reachm, 1:mp unreach*/
	u_int16_t	nlriLen;	/* length does not include afi and safi */
	u_int16_t	afi;			
	u_int8_t	safi;
	u_char		*nlri;	/* contains multiple prefixes */
} BGPMPNlri;

/* BGP attributes: basic attributes + mpreach - mpreach nlri */
typedef struct BGPAttributeStruct {
	u_int16_t	basicAttrlen;	/*length for basic attributes*/
	u_int16_t	totalLen;		/* length including basic attr */
	u_char		*data;			
} BGPAttribute;

typedef struct ParsedBGPUpdateStruct {
	BGPNlri		unreachNlri;	/*unfeasible routes*/
	BGPNlri		reachNlri;		/*announced nlri */
	BGPMPNlri	mpNlri[MAX_MP_NUM];	/*nlri in mpreach/mpunreach attributes, we assume threre might be multiple mpreach/mpunreach attributes*/
	u_int8_t	numOfMpNlri;
	BGPASPath		asPath;		/*as path*/
	BGPAttribute	 attr;	   /*other attributes*/ 
} ParsedBGPUpdate;


/*----------------------------------------------------------------------------------------
 * Functions Declaraion
 * -------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------
 * Purpose: Create a prefix table for a session
 * Input: sessionID -  the ID of the session
 *		prefixTableSize - size(#buckets) of prefix table
 *		maxCollision -	max number of hash collisions 
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
void createPrefixTable(int sessionID, u_int32_t prefixTableSize, u_int16_t	maxCollision);


/*--------------------------------------------------------------------------------------
 * Purpose: Create a attribute table for a session
 * Input: sessionID -  the ID of the session
 *		attributeTableSize - size(#buckets) of attribute table
 *		maxCollision -  max number of hash collisions 
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
void createAttributeTable(int sessionID, u_int32_t attributeTableSize, u_int16_t  maxCollision);


/*--------------------------------------------------------------------------------------
 * Purpose: Parse a BGP Update message into reach nlri, unreach nlri, mpreach
 * 		    nlri, mpunreach nlri and attribute whcih is basic attr + mpreach - mpreach nlri.
 * Input: rawBGPUpdate - Raw BGP update message
 *		len	- length of rw BGP update
 *		parsedBGPUpdate -  parsed result
 * Output:  0 for success or -1 for failure
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
int parseBGPUpdate (void *rawBGPUpdate, u_int32_t length, ParsedBGPUpdate *parsedBGPUpdate);

/*--------------------------------------------------------------------------------------
 * Purpose: Print an AS path, for debugging
 * Input: asPath - pointer to the buffer of AS path
 * Output:
 * He Yan @ July 4th, 2008
 * -------------------------------------------------------------------------------------*/ 
char *printASPath(u_char *asPath, int ASNumLen);

/*------------------------------------------------------------------------------
 * Purpose: prefixesEqual
 * Input: 
 * Output: 
 * Catherine Olschanowsky June 2012
 * ---------------------------------------------------------------------------*/
int prefixesEqual(const PAddress* prefix1, const PAddress* prefix2);

/*------------------------------------------------------------------------------
 * Purpose: stringToPrefix
 * Input: 
 * Output: 
 * Catherine Olschanowsky June 2012
 * ---------------------------------------------------------------------------*/
PAddress* stringToPrefix(const char* prefixIn);

/*------------------------------------------------------------------------------
 * Purpose: Print prefix 
 * Input: lenght of prefix which is stored in prefix structure
 * Output: prefix string like 1.2.3.4/24
 * Mikhail Strizhov @ Feb 10, 2010
 * ---------------------------------------------------------------------------*/
char * printPrefix(const Prefix *prefix);

/*--------------------------------------------------------------------------------------
 * Purpose: Create and send table transfer BMF messages (type BMF_TYPE_TABLE_TRANSFER) from a specific
 *			attribute node in attribute hash table.
 *			Note all the prefix nodes(in prefix hash table) related 
 			to this this attribute node will be included in the created BMF message.
			If all prefixes wont fit in a single BMF message, multiple BMF messages will be sent.
 * Input: attrNode -  the attribute node used to create a BMF
 *		sessionID -  the ID of the session	
 *	  labeledQueueWriter - name of queue for sending BMF messages	
 * Output: 0 for success or -1 for failure
 * He Yan @ July 4th, 2008
 * Mikhail Strizhov @ July 21st, 2010
 * -------------------------------------------------------------------------------------*/ 
int sendBMFFromAttrNode(AttrNode *attrNode, int sessionID, QueueWriter labeledQueueWriter);


/*--------------------------------------------------------------------------------------
 * Purpose: create and send BMF message from Attributes
 * Input: attrNode -  the attribute node used to create a BMF
 * 	  nlri - NLRI structure
 *	  labeledQueueWriter - name of queue for sending BMF messages	
 * Output: 0 for success or -1 for failure
 * Mikhail Strizhov @ July 21st, 2010
 * -------------------------------------------------------------------------------------*/ 
int createAndSendBMFFromAttr(int sessionID,AttrNode *attrNode, MSTREAM mpAttr, MSTREAM nlri,  QueueWriter labeledQueueWriter); 

#endif /*RTABLE_H_*/
