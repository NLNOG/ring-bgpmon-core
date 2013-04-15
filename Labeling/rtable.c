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
 *  File: rtable.c
 * 	Authors: He Yan
 *  Data: Jun 22, 2008 
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <assert.h>
#include <syslog.h>
#include <pthread.h>

#include "rtable.h"
#include "label.h"
#include "labelutils.h"
#include "../Util/log.h"
#include "../Peering/bgpmessagetypes.h"
#include "../Peering/bgppacket.h"
#include "../Peering/peersession.h"

// needed for xml queue writer/reader
#include "../Queues/queue.h"

#include "myhash.h"
/* needed for copying BGPmon Internal Format messages */
#include "../Util/bgpmon_formats.h"

//#define DEBUG

static u_char buffer[MAX_BGP_MESSAGE_LEN];
static u_char buffer1[MAX_BGP_MESSAGE_LEN];


/*--------------------------------------------------------------------------------------
 * Purpose: Create a prefix table for a session
 * Input: sessionID -  the ID of the session
 *		prefixTableSize - size(#buckets) of prefix table
 *		maxCollision -  max number of hash collisions 
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
void createPrefixTable(int sessionID, u_int32_t prefixTableSize, u_int16_t  maxCollision) 
{
	u_int32_t i;

	Session_structp session = Sessions[sessionID];
	//assert(session->prefixTable == NULL);
	
	/*Allocation Memory*/
	session->prefixTable = malloc(sizeof(struct PrefixTableStruct));

	if( session->prefixTable )
	{
		/* Initialize prefix table */
		session->prefixTable->tableSize = prefixTableSize; 
		session->prefixTable->prefixCount = 0;
		session->prefixTable->ocupiedSize = 0;
		session->prefixTable->maxNodeCount = 0;
		session->prefixTable->maxCollision = maxCollision;
		session->prefixTable->prefixEntries = calloc (prefixTableSize, sizeof(PrefixEntry));
	
		if (session->prefixTable->prefixEntries == NULL) 
	  		log_fatal( "createPrefixTable: session %d calloc failed", sessionID );
		
		for (i=0; i<session->prefixTable->tableSize; i++) {
		  session->prefixTable->prefixEntries[i].nodeCount = 0;
		  session->prefixTable->prefixEntries[i].node = NULL;
		}   
		session->stats.memoryUsed += sizeof(PrefixTable) + prefixTableSize*sizeof(PrefixEntry);
		log_msg( "createPrefixTable: session %d successfully", session->sessionID);
	}
	else
		log_fatal( "createPrefixTable: session %d malloc failed", session->sessionID );
}




/*--------------------------------------------------------------------------------------
 * Purpose: Print a prefix table
 * Input: session - the corresponding session structure which includes the prefix table
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
void printPrefixTable(Session_structp session)
{
	log_msg("prefix table size: %d", session->prefixTable->tableSize);
	log_msg("prefix table attrCount: %d", session->prefixTable->prefixCount);
	log_msg("prefix table occupied size: %d", session->prefixTable->ocupiedSize);
	log_msg("prefix table max nodeCount: %d", session->prefixTable->maxNodeCount);
	log_msg("prefix table max collision: %d", session->prefixTable->maxCollision);
}

/*--------------------------------------------------------------------------------------
 * Purpose: Destory a prefix table
 * Input:	 prefixTable - the pointer to a prefix table
 *		 session - the corresponding session structure which includes the prefix table
 * Output: 0 means success, -1 means failure.
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int destroyPrefixTable ( PrefixTable *prefixTable, Session_structp session )
{
	u_int32_t      i, prefixCount = 0;
	PrefixNode    *prefixNode;
	PrefixNode    *nextPrefixNode;

	if( prefixTable == NULL)
		return -1;
	
	for( i=0; i< prefixTable->tableSize; i++ ) 
	{
		if (prefixTable->prefixEntries[i].node != NULL) 
		{
			prefixNode = prefixTable->prefixEntries[i].node;
			while (prefixNode != NULL) 
			{
				nextPrefixNode = prefixNode->next;
				session->stats.memoryUsed -= ( sizeof(PrefixNode) + (PREFIX_SIZE(prefixNode->keyPrefix.addr.p_len)) ); 
				free(prefixNode);
				prefixCount++;
				prefixNode = nextPrefixNode;
			}
	  	}
	    prefixTable->prefixEntries[i].nodeCount= 0;
	    prefixTable->prefixEntries[i].node = NULL;
	}

	if( prefixTable->prefixCount != prefixCount)
	{
		log_err("prefixTable's prefix count(%d) != actual prefix count(%d)", prefixTable->prefixCount, prefixCount);
		return -1;
	}
	prefixTable->prefixCount= 0;
	prefixTable->ocupiedSize= 0;
	prefixTable->maxNodeCount= 0;

	return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Create a attribute table for a session
 * Input: sessionID -  the ID of the session
 *		attributeTableSize - size(#buckets) of attribute table
 *		maxCollision -  max number of hash collisions 
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
void createAttributeTable(int sessionID, u_int32_t attributeTableSize, u_int16_t  maxCollision) 
{
	u_int32_t i;
	int error;

	Session_structp session = Sessions[sessionID];
	assert(session->attributeTable== NULL);
	
	/*Allocation Memory*/
	session->attributeTable = malloc(sizeof(struct AttrTableStruct));

	if( session->attributeTable )
	{
		session->attributeTable->tableSize = attributeTableSize;
		session->attributeTable->attrCount = 0;
		session->attributeTable->ocupiedSize = 0;
		session->attributeTable->maxNodeCount = 0;  
		session->attributeTable->maxCollision = maxCollision;
		session->attributeTable->attrEntries = calloc(attributeTableSize, sizeof(AttrEntry));
		if (session->attributeTable->attrEntries == NULL) 
		  log_fatal( "createAttributeTable: session %d calloc failed", session->sessionID);
		
		for (i=0; i<session->attributeTable->tableSize; i++)
		{
			session->attributeTable->attrEntries[i].nodeCount = 0;
			session->attributeTable->attrEntries[i].node = NULL;
 			if ((error = pthread_rwlock_init(&(session->attributeTable->attrEntries[i].lock), NULL)) > 0)       
		     	log_fatal("createAttributeTable: session %d failed to init rwlock: %s\n", session->sessionID, strerror(error));    			
		}
		
		session->stats.memoryUsed += sizeof(AttrTable) + attributeTableSize*sizeof(AttrEntry);
		log_msg( "createAttributeTable: session %d successfully", session->sessionID );
	}
	else
		log_fatal( "createAttributeTable: session %d malloc failed", session->sessionID );
}

/*--------------------------------------------------------------------------------------
 * Purpose: Print a attribute table
 * Input:	 session - the corresponding session structure which includes the attribute table
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
void printAttrTable( Session_structp session )
{
	int i,j;
	log_msg("attribute table size: %d", session->attributeTable->tableSize);
	log_msg("attribute table attrCount: %d", session->attributeTable->attrCount);
	log_msg("attribute table occupied size: %d", session->attributeTable->ocupiedSize);
	log_msg("attribute table max nodeCount: %d", session->attributeTable->maxNodeCount);
	log_msg("attribute table max collision: %d", session->attributeTable->maxCollision);
	log_msg("withdraw updates: %d", session->stats.withRcvd);
	log_msg("duplicate withdraw updates: %d", session->stats.duwiRcvd);
	log_msg("new updates: %d", session->stats.nannRcvd);
	log_msg("duplicate updates: %d", session->stats.dannRcvd);
	log_msg("dpath updates: %d", session->stats.dpathRcvd);
	log_msg("spath updates: %d", session->stats.spathRcvd);

	int asPathCount = 0;
	for(i=0; i<session->attributeTable->tableSize; i++)
	{
		if( session->attributeTable->attrEntries[i].nodeCount > 0 )
		{
			int *tmpArray = malloc(sizeof(int)*session->attributeTable->attrEntries[i].nodeCount);
			memset(tmpArray, 0, session->attributeTable->attrEntries[i].nodeCount);
			
			for(j=0; j<session->attributeTable->attrEntries[i].nodeCount; j++)
			{	
				struct AttrNodeStruct	*attrNode = session->attributeTable->attrEntries[i].node;
				tmpArray[attrNode->asPath->asPathID] = 1;
				attrNode = attrNode->next;			
			}

			for(j=0; j<session->attributeTable->attrEntries[i].nodeCount; j++)
			{
				if(tmpArray[j] == 1)
					asPathCount++;
			}
			free(tmpArray);
                        tmpArray = NULL;
		}
	}
	log_msg("attribute table AS Path Count: %d", asPathCount);
}

/*--------------------------------------------------------------------------------------
 * Purpose: Destory a attribute node
 * Input:	attrNode - pointer to the attribute node needed to be deleted
 *		session - the corresponding session structure which includes the attribute table
 * Output:
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
void destroyAttrNode ( AttrNode *attrNode, Session_structp session )
{
	int error;
  	if( (error = pthread_rwlock_wrlock (&(attrNode->lock))) > 0 ) 
	{
    	log_fatal ("Failed to wrlock an entry in the rib table: %s", strerror(error));
	}	
	PrefixRefNode *prefixRefNode = NULL;
	PrefixRefNode *nextPrefixRefNode = NULL;
	prefixRefNode = attrNode->prefixRefNode;	
	while( prefixRefNode != NULL )
	{
		nextPrefixRefNode = prefixRefNode->next;
	 	free(prefixRefNode);
	 	session->stats.memoryUsed -= sizeof(PrefixRefNode);
	 	prefixRefNode = nextPrefixRefNode;	 	
	}
	pthread_rwlock_unlock(&(attrNode->lock));
	if( (error = pthread_rwlock_destroy(&(attrNode->lock))) > 0 )       
    	log_fatal("Failed to destroy rwlock: %s\n", strerror(error));  		
	session->stats.memoryUsed -= (sizeof(AttrNode) + attrNode->totalAttrLen);
	attrNode->asPath->refCount--;
	if( attrNode->asPath->refCount == 0 )
	{
		free(attrNode->asPath->asPathData.data);
                attrNode->asPath->asPathData.data = NULL;
		free(attrNode->asPath);
                attrNode->asPath = NULL;
	}
    free(attrNode);   
}


/*--------------------------------------------------------------------------------------
 * Purpose: Destory a attribute table
 * Input:	 attrTable - the pointer to a attribute table
 *		 session - the corresponding session structure which includes the attribute table
 * Output: 0 means success, -1 means failure.
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
int destroyAttrTable ( AttrTable *attrTable, Session_structp session )
{
	u_int32_t      i, attrCount = 0;
	AttrNode      *attrNode;
	AttrNode      *nextAttrNode;
	int error;
   
	if( attrTable == NULL )
	{
		return -1;
	}
	for (i=0; i<attrTable->tableSize; i++) 
	{
		attrNode = attrTable->attrEntries[i].node;
		if( (error = pthread_rwlock_wrlock (&(attrTable->attrEntries[i].lock))) > 0 ) 
		{
			log_err ("Failed to wrlock an entry in the attribute table: %s", strerror(error));
			return -1;
		}      	

		while (attrNode != NULL) 
		{
			nextAttrNode = attrNode->next;
			destroyAttrNode (attrNode, session);
			attrCount++;
         	        attrNode = nextAttrNode;
		}
		attrTable->attrEntries[i].nodeCount= 0;
		attrTable->attrEntries[i].node= NULL;
		pthread_rwlock_unlock(&(attrTable->attrEntries[i].lock));
	}

   	// sanity check
	if(attrTable->attrCount != attrCount)
		return -1;

	attrTable->attrCount= 0;
	attrTable->ocupiedSize= 0;
	attrTable->maxNodeCount= 0;  

	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Remove a prefix from the prefix reference list of a attribure node
 * Input:	 prefixNode - the pointer to the prefix node to be deleted
 *		 attrNode - the pointer to the associated attribute node of the prefix node to be deleted
 *		 session - the corresponding session structure
 * Output:  0 for success or -1 for failure
 * He Yan @ July 4th, 2008
 * -------------------------------------------------------------------------------------*/
int removePefixFomAttr( PrefixNode *prefixNode, AttrNode *attrNode, Session_structp session )
{
  PrefixRefNode *prefixRefNode = NULL;
  PrefixRefNode *prevNode = NULL;
  prefixRefNode = prefixNode->dataAttr->prefixRefNode;	
	   
  while( prefixRefNode != NULL && prefixRefNode->prefixNode != prefixNode ){
    prevNode = prefixRefNode;
    prefixRefNode = prefixRefNode->next;
  }

  if( prefixRefNode == NULL ) {
#ifdef DEBUG
		log_err ("Try to remove a non-exist prefix_ref_node in attr node.");
#endif 		
    return -1;
  }
  if( prevNode == NULL ){ // the removed node is the first node in the link list 
    prefixNode->dataAttr->prefixRefNode = prefixRefNode->next;
  }else{
    prevNode->next = prefixRefNode->next;   
  }
  free(prefixRefNode);
  session->stats.memoryUsed -= sizeof(prefixRefNode);
  return 0;		
}

/*----------------------------------------------------------------------------------------
 * Purpose: Remove the given attr node from attr table and free that attr node's space
 * Input:	 removedNode - the pointer to the attribute node to be deleted
 *		 session - the corresponding session structure
 * Output:  0 for success or -1 for failure
 * NOTE: The ref count of the attr to be removed should be 0.
 * He Yan @ July 4th, 2008
 * -------------------------------------------------------------------------------------*/
int removeAttrNode( AttrNode *removedNode, Session_structp session )
{
	INDEX          i;
	AttrNode      *node, *prevNode;
	prevNode = NULL;

	/* search the attr node */
	i = attr_hash(removedNode->asPath->asPathData.data, removedNode->asPath->asPathData.len, session->attributeTable->tableSize);      
	node =  session->attributeTable->attrEntries[i].node;
	while (node != NULL && node != removedNode) 
	{
		prevNode = node;
		node = node->next;
	}


//#ifdef DEBUG   
	if (node == NULL) 
	{
		log_err ("Try to remove a non-exist attr entry.");
		return -1;
	}

	if (node->refCount != 0) 
	{
		log_err ("Try to remove an attr entry with non-zero ref_count.");
		return -1;
	}
//#endif   
   assert (node != NULL);
   
	if (prevNode == NULL) /* the removed node is the first node in the link list */ 
		session->attributeTable->attrEntries[i].node = node->next;
	else 
		prevNode->next = node->next;   

	destroyAttrNode(node, session);
        node = NULL;

	if (session->attributeTable->attrEntries[i].node == NULL )
	  session->attributeTable->ocupiedSize--;

	session->attributeTable->attrEntries[i].nodeCount--;   
	session->attributeTable->attrCount--;
  	session->stats.attrCount--;
#ifdef DEBUG
   debug (__FUNCTION__,  "Successfully remove the given attr from the attr table.");
#endif

   return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Create and Insert a new attr node into the given attr table
 * Input:   bucketIndex - the index of bucket the new node will stay
 *		  asPath - a pointer to a ASPath structure
 *		  attr - a pointer to the attributes(normal attrs + mp reach attributes(without mp NLRI))
 *		  totalAttrLen - the length of all the attributes
 *		  basicAttrLen - the length of the normal attributes
 *		  session - the corresponding session structure
 * Output: success: the pointer to the new node
 *		   Failure: NULL
 * He Yan @ July 4th, 2008
 * -------------------------------------------------------------------------------------*/
AttrNode * createAttrNode( INDEX bucketIndex, ASPath *asPath, u_char *attr, u_int16_t totalAttrLen, u_int16_t basicAttrLen, Session_structp session )
{
   	AttrNode      *newNode = NULL;
	int            error;

   	/* create a new node for the new attr */
   	newNode = malloc(sizeof(AttrNode) + totalAttrLen);
	if( newNode == NULL )
	{
		log_err( "createAttrNode: malloc failed" );
		return NULL;
	}
	
	session->stats.memoryUsed += (sizeof(AttrNode) + totalAttrLen);
   	memcpy( newNode->attr, attr, totalAttrLen);
   	newNode->refCount = 0;
	
   	newNode->prefixRefNode = NULL;
	if( (error = pthread_rwlock_init(&(newNode->lock), NULL)) > 0 )       
    	log_fatal("createAttrNode: Failed to init rwlock: %s\n", strerror(error)); 

	newNode->asPath = asPath;
	newNode->asPath->refCount++;
	newNode->bucketIndex = bucketIndex;
	
   	newNode->totalAttrLen = totalAttrLen;
   	newNode->basicAttrLen = basicAttrLen;

   	if( session->attributeTable->attrEntries[bucketIndex].node == NULL )
    	session->attributeTable->ocupiedSize++;
   
   	newNode->next = session->attributeTable->attrEntries[bucketIndex].node;   //point to the head of current list   
   	session->attributeTable->attrEntries[bucketIndex].node = newNode;         //make new node as the head of the list 
   	session->attributeTable->attrEntries[bucketIndex].nodeCount++;
   	session->attributeTable->maxNodeCount = MAXV(session->attributeTable->maxNodeCount, session->attributeTable->attrEntries[bucketIndex].nodeCount);
   	if( session->attributeTable->maxNodeCount > session->attributeTable->maxCollision )
	{
		log_err("The maximum collision in the attribute hash table was reached.");
   	}   
   	session->attributeTable->attrCount++;
	session->stats.attrCount++;
   
   	return newNode;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Search the attribute table based on the AS path
 * Input:	asPath - the data of as path
 *		len  - the length of as Path in bytes
 *		session - the corresponding session structure
 * Output:  Success: the pointer to the existing attribute node or a new created node.
 *		   Failure: NULL
 * He Yan @ July 4th, 2008
 * -------------------------------------------------------------------------------------*/
AttrNode * searchAttrNode( u_char *asPathData, u_int16_t len, u_char *attr, u_int16_t totalAttrLen, u_int16_t basicAttrLen, Session_structp session )
{
	AttrNode		*node;
	INDEX			i;
	ASPath			*asPath;
	i = attr_hash(asPathData, len, session->attributeTable->tableSize);
	node = session->attributeTable->attrEntries[i].node;

	int maxPathID = 0;
	AttrNode	*tmpNode = NULL;
	// if AS path of the 'node' is same as the new AS path specified by 'asPathData'
	while( node != NULL )
	{
		// if AS paths are same, continue to check other attributes
		if( len == node->asPath->asPathData.len && !memcmp(node->asPath->asPathData.data, asPathData, len)) 	
		{
			if( totalAttrLen == node->totalAttrLen && !memcmp(node->attr, attr, totalAttrLen))
			{
				return node;
			}
			else
			{
				tmpNode = node;
			}
		}
		if( maxPathID < node->asPath->asPathID )
			maxPathID = node->asPath->asPathID;
     	node = node->next;   
	}

	if( tmpNode != NULL )
	{
		// create a new attr node with the existing ASPath
#ifdef DEBUG
		log_msg("--------------------------------");
		hexdump (LOG_INFO, asPathData, len);
		hexdump (LOG_INFO, tmpNode->asPath->asPathData.data, tmpNode->asPath->asPathData.len);
		hexdump (LOG_INFO, attr, totalAttrLen);
		hexdump (LOG_INFO, tmpNode->attr, tmpNode->totalAttrLen);
#endif
		ASPath *existingAsPath = tmpNode->asPath;
		node =	createAttrNode(i, existingAsPath, attr, totalAttrLen, basicAttrLen, session);
		return node;
	}
	else
	{
		//if none of the attr nodes of this bucket has the same AS path specified by 'asPathData' or the bucket is empty
		//create a new attr node with a new ASPath structure
		asPath = malloc(sizeof(ASPath));
		asPath->asPathData.data = malloc(sizeof(u_char)*len);
		asPath->asPathData.len = len;
		memcpy( asPath->asPathData.data, asPathData, len);
		asPath->asPathID = maxPathID;
		asPath->refCount = 0;

		// create the attr node			
		node = createAttrNode(i, asPath, attr, totalAttrLen, basicAttrLen, session);
		return node;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Initilize the parsed BGP update structure
 * Input:   parsedBGPUpdate - the pointer to parsed BGP update structure
 * Output:  none
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
void initParsedBGPUpdate(ParsedBGPUpdate *parsedBGPUpdate)
{
	int i;
	parsedBGPUpdate->unreachNlri.afi = BGP_AFI_IPv4;
	parsedBGPUpdate->unreachNlri.safi = BGP_MP_SAFI_UNICAST;
	parsedBGPUpdate->unreachNlri.nlriLen = 0;
	parsedBGPUpdate->unreachNlri.nlri = NULL;
	parsedBGPUpdate->reachNlri.afi = BGP_AFI_IPv4;
	parsedBGPUpdate->reachNlri.safi = BGP_MP_SAFI_UNICAST;
	parsedBGPUpdate->reachNlri.nlriLen = 0;      
	parsedBGPUpdate->reachNlri.nlri = NULL;

	for( i=0; i<MAX_MP_NUM; i++)
	{
		parsedBGPUpdate->mpNlri[i].afi = 0;
		parsedBGPUpdate->mpNlri[i].safi = 0;
		parsedBGPUpdate->mpNlri[i].nlriLen = 0;
		parsedBGPUpdate->mpNlri[i].nlri = NULL;
	}
	parsedBGPUpdate->numOfMpNlri = 0;

	parsedBGPUpdate->asPath.data = NULL;
	parsedBGPUpdate->asPath.len = 0;	
	parsedBGPUpdate->attr.totalLen= 0;
	parsedBGPUpdate->attr.basicAttrlen= 0;
	parsedBGPUpdate->attr.data = NULL;
	return;   
}

/*--------------------------------------------------------------------------------------
 * Purpose: Parse a BGP Update message into reach nlri, unreach nlri, mpreach
 * 		    nlri, mpunreach nlri and attribute whcih is basic attr + mpreach - mpreach nlri.
 * Input: rawBGPUpdate - Raw BGP update message
 *		len	- length of rw BGP update
 *		parsedBGPUpdate -  parsed result
 * Output:  0 for success or -1 for failure
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/ 
int parseBGPUpdate (void *rawBGPUpdate, u_int32_t len, ParsedBGPUpdate *parsedBGPUpdate)
{
	MSTREAM			s, b, b1;
	u_char			attrFlag, attrType, attrLenShort;
	u_char			lenC, snpaCount;
	u_int16_t		attrLen, startPos;
	u_int32_t		length = len-19;
	Helper			MP[MAX_MP_NUM];
	u_int8_t		numOfMP = 0;
	int 			i;
	
	/* initialize the parsed BGP update structure */
	initParsedBGPUpdate(parsedBGPUpdate);

	/* initialize buffer */
	mstream_init(&s, rawBGPUpdate+19, length);
	memset (buffer, 0, MAX_BGP_MESSAGE_LEN);
	mstream_init(&b, buffer, MAX_BGP_MESSAGE_LEN);
	memset (buffer1, 0, MAX_BGP_MESSAGE_LEN);
	mstream_init(&b1, buffer1, MAX_BGP_MESSAGE_LEN);	
	
	/* process IPv4 unicast unreach nlri */   
	mstream_getw( &s, &parsedBGPUpdate->unreachNlri.nlriLen );   
#ifdef DEBUG
	debug (__FUNCTION__, "IPv4 unicast unreach nlri length: %d", parsedBGPUpdate->unreachNlri.nlriLen);
#endif
	if( parsedBGPUpdate->unreachNlri.nlriLen != 0 ) 
	{
		if( mstream_can_read(&s) >= parsedBGPUpdate->unreachNlri.nlriLen ) 
		{
			parsedBGPUpdate->unreachNlri.nlri = mstream_current_address(&b);
			if( mstream_add(&b, mstream_current_address(&s), parsedBGPUpdate->unreachNlri.nlriLen) )
			{
		 		log_err("Buffer is overflow!");
		 		return -1;
			}			
			s.position += parsedBGPUpdate->unreachNlri.nlriLen;
		} 
		else 
		{
			log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
			return -1;
		}
	}


  	/* process IPv4 unicast reach nlri */
	if( mstream_getw(&s, &attrLen) )
	{
			log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
			return 1;
	}
#ifdef DEBUG
	debug ( __FUNCTION__, "Basic attribute length: %d", attrLen  );
#endif 
	parsedBGPUpdate->reachNlri.nlriLen= length - (s.position + attrLen);
#ifdef DEBUG
	debug ( __FUNCTION__, "IPv4 unicast reach nlri length: %d", parsedBGPUpdate->reachNlri.nlriLen );   
#endif
	if( parsedBGPUpdate->reachNlri.nlriLen != 0 ) 
	{
		parsedBGPUpdate->reachNlri.nlri = mstream_current_address(&b);
		if( mstream_add(&b, mstream_current_address(&s) + attrLen, parsedBGPUpdate->reachNlri.nlriLen) ) 
		{
	 		log_err("Buffer is overflow!");
	 		return -1;
		}
	}


	/* process BGP attributes */
	if( attrLen == 0 )  
		return 0;
	parsedBGPUpdate->attr.data = mstream_current_address(&b);
	mstream_init( &s, mstream_current_address(&s), attrLen );   
	while( mstream_can_read(&s) > 0 ) 
	{
		startPos = s.position;
		if( mstream_getc(&s, &attrFlag) )
		{
			log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
			return -1;
		}			
		if( mstream_getc(&s, &attrType) ) 
		{
			log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
			return -1;
		}		
		/* get attribute length */


		if( attrFlag & BGP_ATTR_FLAG_EXT_LEN  ) 
		{

			if( mstream_getw(&s, &attrLen) )
			{
				log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
				return -1;
			}					
		} 
		else 
		{
				

			if( mstream_getc(&s,&attrLenShort) )
			{
				log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
				return -1;
			}
			attrLen = attrLenShort;
		}

		if( mstream_can_read(&s) < attrLen) 
		{
			log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
			return -1;
		}			

		switch (attrType)
		{
	 		case BGP_MP_REACH:
	    		MP[numOfMP].dataLength = attrLen + s.position - startPos;
			    MP[numOfMP].data = s.start + startPos;
				MP[numOfMP].flag = 0;
				#ifdef DEBUG
			    debug(__FUNCTION__, "%d: MPREACH attribute length: %d", numOfMP, MP[numOfMP].dataLength);
				hexdump (LOG_INFO, MP[numOfMP].data, MP[numOfMP].dataLength);	
				#endif							
			    s.position += attrLen;
				numOfMP++;
	    		break;
				
	 		case BGP_MP_UNREACH:
				MP[numOfMP].dataLength = attrLen;
				MP[numOfMP].flag = 1;
				MP[numOfMP].data = mstream_current_address(&s);
	   			//parsedBGPUpdate->mpunreachNlri[parsedBGPUpdate->numOfMpunreachNlri].nlriLen = attrLen - 3;
				#ifdef DEBUG
				debug(__FUNCTION__, "%d: MPUNREACH attribute length: %d", numOfMP, attrLen + s.position - startPos);
				#endif
				numOfMP++;
	    		s.position += attrLen;
	    		break;

			case BGP_AS_PATH:
				//memcpy(parsedBGPUpdate->asPath.data, s.start+startPos, attrLen + s.position - startPos);
				parsedBGPUpdate->asPath.data = s.start+startPos;
				parsedBGPUpdate->asPath.len = attrLen + s.position - startPos;
				#ifdef DEBUG
				hexdump (LOG_DEBUG, parsedBGPUpdate->asPath.data, parsedBGPUpdate->asPath.len);

				if( attrFlag & BGP_ATTR_FLAG_EXT_LEN )
				{
	
				debug(__FUNCTION__, "As path +4 :%s\n", printASPath(parsedBGPUpdate->asPath.data+4, 2));
				}
				else
				{

				debug(__FUNCTION__, "As path +3 :%s\n", printASPath(parsedBGPUpdate->asPath.data+3, 2));
				}
				//	debug(__FUNCTION__, "As Path: %s", asBuffer);
				#endif
				s.position += attrLen;
				break;
				
			default:
				if( mstream_add( &b, s.start+startPos, attrLen + s.position - startPos ) )
				{
			   		log_err("Buffer is overflow!");
			   		return -1;
				}
				s.position += attrLen;
				break;
		}
	}   
	parsedBGPUpdate->attr.basicAttrlen = mstream_current_address(&b) - parsedBGPUpdate->attr.data;
	parsedBGPUpdate->attr.totalLen = parsedBGPUpdate->attr.basicAttrlen;
	
	/*parse bgp mpreach/mpunreach attributes */
	parsedBGPUpdate->numOfMpNlri= numOfMP;
	for(i=0; i<numOfMP; i++) 
	{
		if( MP[i].flag == 0 )// parse mp reach attribute
		{
			#ifdef DEBUG
			debug(__FUNCTION__, "Parse mpreach attributes ...");
			#endif
			mstream_init( &s, MP[i].data, MP[i].dataLength );

			/*attribute flag, type and length*/
			mstream_getc( &s, &attrFlag );
			s.position++;			
			if( attrFlag & BGP_ATTR_FLAG_EXT_LEN  )
			{

				if( mstream_getw( &s, &attrLen ) ) 
				{
					log_err("%s [%d] - Failed! Mpreach message was corrupt.", __FILE__, __LINE__);
					return -1;
				}
				assert( attrLen+4 == MP[i].dataLength );
			}
			else
			{

				if( mstream_getc(&s,&attrLenShort) ) 
				{
					log_err("%s [%d] - Failed! Mpreach message was corrupt.", __FILE__, __LINE__);
					return -1;
				}				
				attrLen = attrLenShort;
				assert( attrLen+3 == MP[i].dataLength );
			}
#ifdef DEBUG
			debug(__FUNCTION__, "Attribute length: %d bytes.", attrLen);
#endif
			/*afi and safi*/
			if( mstream_getw(&s, &parsedBGPUpdate->mpNlri[i].afi) )
			{
				log_err("%s [%d] - Failed! Mpreach message was corrupt.", __FILE__, __LINE__);
				return -1;
			}
			if( mstream_getc(&s, &parsedBGPUpdate->mpNlri[i].safi) )
			{
				log_err("%s [%d] - Failed! Mpreach message was corrupt.", __FILE__, __LINE__);
				return -1;
			}		
			
			/*next hop*/
			if( mstream_getc(&s, &lenC) )
			{
				log_err("%s [%d] - Failed! Mpreach message was corrupt.", __FILE__, __LINE__);
				return -1;
			}				
	   		s.position += lenC;

			/*snpa*/
			if( mstream_getc(&s, &snpaCount) )
			{
				log_err("%s [%d] - Failed! Mpreach message was corrupt.", __FILE__, __LINE__);
				return -1;
			}
			while (snpaCount !=0 ) 
			{
			  	if( mstream_getc(&s, &lenC) )
				{
					log_err("%s [%d] - Failed! Mpreach message was corrupt.", __FILE__, __LINE__);
					return -1;
				}		  	
				s.position += lenC;
				snpaCount--;
			}

			/*add the mp reach attribute*/
			if( mstream_add(&b, MP[i].data, s.position) )
			{
		 		log_err("Buffer is overflow!");
		 		return -1;
			}  	

			/*mp reach nlri*/
			parsedBGPUpdate->mpNlri[i].flag = 0;
			parsedBGPUpdate->mpNlri[i].nlriLen = mstream_can_read(&s);
			parsedBGPUpdate->mpNlri[i].nlri = mstream_current_address(&b1);
			if( mstream_add(&b1, mstream_current_address(&s), parsedBGPUpdate->mpNlri[i].nlriLen) )
			{
		 		log_err("Buffer is overflow!");
		 		return -1;
			} 		

			/*rewrite attribute length*/
			if (attrFlag & BGP_ATTR_FLAG_EXT_LEN)
			{
	        	*((u_int16_t *)(parsedBGPUpdate->attr.data + parsedBGPUpdate->attr.totalLen + 2)) = 
	            htons (attrLen - parsedBGPUpdate->mpNlri[i].nlriLen);
			}
			else
			{
				*((u_int8_t *) (parsedBGPUpdate->attr.data + parsedBGPUpdate->attr.totalLen + 2)) = 
	            attrLen - parsedBGPUpdate->mpNlri[i].nlriLen;
			}
			parsedBGPUpdate->attr.totalLen = parsedBGPUpdate->attr.totalLen + s.position;
		}
		else // parse mp unreach attribute
		{
			#ifdef DEBUG
			debug (__FUNCTION__, "Parse mp_unreach attributes ...");
			#endif
			mstream_init( &s, MP[i].data, MP[i].dataLength );
			if( mstream_getw(&s, &parsedBGPUpdate->mpNlri[i].afi) )
		    {
				log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
				return -1;			    	
		    }
		    if( mstream_getc(&s, &parsedBGPUpdate->mpNlri[i].safi) )
		    {
				log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
				return -1;				    	
		    }
			parsedBGPUpdate->mpNlri[i].flag = 1;
			parsedBGPUpdate->mpNlri[i].nlriLen = mstream_can_read(&s);
			parsedBGPUpdate->mpNlri[i].nlri = mstream_current_address(&b1);
			mstream_add(&b1, mstream_current_address(&s), parsedBGPUpdate->mpNlri[i].nlriLen);		
		}
	}

   return 0;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Sanity check of nlri
 * Input: nlri - pointer to the buffer of NLRI
 *		len - the length of NLRI
 * Output: 0 for success or -1 for failure
 * He Yan @ July 4th, 2008
 * -------------------------------------------------------------------------------------*/ 
int nlriCheck (u_char *nlri, u_int16_t len)
{
   MSTREAM  s;
   u_char   p_len;
   
   mstream_init(&s, nlri, len);
   while (mstream_can_read(&s) > 0) {
      // read a single character
      if (mstream_getc (&s, &p_len)){
         log_err("%s: failed on mstream_getc",__FUNCTION__);
         return -1;
      }
      // translate the length from bits to bytes
      p_len = PREFIX_SIZE(p_len);
      // fast forward through the actual prefix
      s.position += p_len;
   }
   
   if (s.position > s.len) {
     log_err("%s: failed because posion (%u) > len (%u)",__FUNCTION__,s.position,s.len);
     return -1;
   }
   return 0;
}


char * fcat ( char *dst, char *max, char *src );
/*-----------------------------------------------------------------------------
 * Purpose: Print an AS path
 * Input: asPath - pointer to the buffer of AS path
 * Output: asBuffer string of as path
 * He Yan @ July 4th, 2008
 * Mikhail Strizhov @ Feb 10, 2010
 * Cathie Olschanowksy @ June 2012
 * --------------------------------------------------------------------------*/
char * 
printASPath(u_char *asPath, int ASNumLen)
{
  char * asBuffer = (char *)malloc (MAX_BGP_MESSAGE_LEN);
  int l = asPath[1];
  int i;
  int strlen = 128;
  char str[strlen];
  char *dst = asBuffer;
  char *max = asBuffer+1024;
  *dst = '\0';

  for ( i = 2 ; i < (l*ASNumLen+2) ; i = i + ASNumLen ){
    if(ASNumLen == 2){
      snprintf(str,strlen,"%d",ntohs(*((u_int16_t *) (asPath+i))));
    }else if(ASNumLen == 4){
      //snprintf(str,strlen,"%d",ntohs(*((u_int16_t *) (asPath+i+ASNumLen))));
      snprintf(str,strlen,"%d",ntohl(*((u_int32_t *) (asPath+i)))) ;
    }else{
      free(asBuffer);
      return NULL;
    }
    dst = fcat( dst, max, str );
    if ( i < (l*ASNumLen+2) ){
      dst = fcat( dst, max, " " );
    }
  }
  asBuffer[dst-asBuffer] = 0;
  return asBuffer;
}

/*------------------------------------------------------------------------------
 * Purpose: prefixesEqual
 * Input: 
 * Output: 
 * Cathie Olschanowsky @ June 2012
 * ---------------------------------------------------------------------------*/
int
prefixesEqual(const PAddress* prefix1, const PAddress* prefix2){
  if(prefix1 == prefix2){
    return 1;
  }
  if(prefix1 == NULL){
    return 0;
  }
  if(prefix2 == NULL){
    return 0;
  }
  if(prefix1->p_len != prefix2->p_len){
    return 0;
  }
  int bytes = PREFIX_SIZE(prefix1->p_len);
  int i;
  for(i=0;i<bytes;i++){
    if(prefix1->paddr[i] != prefix2->paddr[i]){
      return 0;
    }
  }
  return 1;
}
/*------------------------------------------------------------------------------
 * Purpose: stringToPrefix
 * Input: 
 * Output: 
 * Cathie Olschanowsky @ June 2012
 * ---------------------------------------------------------------------------*/
PAddress*
stringToPrefix(const char* prefixIn){

  if(prefixIn == NULL){
    return NULL;
  }

  PAddress *prefixOut;
  char prefixCopy[MAX_PREFIX_LEN];
  int afi;
  char* prefix_len = strchr(prefixIn,'/');

  if(prefix_len == NULL){
    return NULL;
  }
  int len = atoi (prefix_len+1);

  // ipv4 or 6?
  if(strchr(prefixIn,':')!=NULL){
    if(strchr(prefixIn,'.')!=NULL){
      return NULL;
    }
    afi = 2;
  }else if(strchr(prefixIn,'.')){
    afi = 1;
  }else{
    return NULL;
  }

  // check for valid length
  if(len <=0 ){
    return NULL;
  } 
  if(afi == 1 && len >32){
    return NULL;
  }
  if(afi == 2 && len > 128){
    return NULL;
  }

  // calculate the length in bytes
  int bytes = PREFIX_SIZE(len);

  // allocate the space needed to hold the prefix itself 
  prefixOut = malloc(sizeof(PAddress) + bytes);
  if(prefixOut == NULL){
    return NULL;
  }
  int j;
  for(j=0;j<bytes;j++){
    prefixOut->paddr[j] = 0;
  }

  // set up the Prefix object
  prefixOut->p_len = len;
  int rem = prefixOut->p_len%8;
  uint8_t mask=255;
  if(rem){
    mask = mask << (8-rem); 
  }

  // make a copy
  strncpy(prefixCopy,prefixIn,MAX_PREFIX_LEN);

  // parse ipv4 addresses
  if(afi == 1){
    // there will be one byte between each delimiter
    char* start;
    char* end;
    int i=0;
    int val;
    start = prefixCopy;
    while(start != NULL && i<bytes){
      end = strchr(start,'.');
      if(end == NULL){
        end = strchr(start,'/');
        if(end == NULL){
          free(prefixOut);
          return NULL;
        }
        *end = '\0';
        if(strlen(start) ==0 || strlen(start) >3){
          free(prefixOut);
          return NULL;
        }
        val = atoi(start);
        if(val == 0){
          int j = 0;
          while(j<strlen(start)){
            if(start[j] != '0'){
              free(prefixOut);
              return NULL;
            }
            j++;
          }
        }
        start = NULL;
      }else{ 
        *end = '\0';
        val = atoi(start);
        if(val == 0){
          int j = 0;
          while(j<strlen(start)){
            if(start[j] != '0'){
              free(prefixOut);
              return NULL;
            }
            j++;
          }
        }
        start = end+1;
      }
      prefixOut->paddr[i] = val;
      i++;
    }
   prefixOut->paddr[bytes-1] &= mask;
  } else {
  // IPV6
    // there will be one byte between each delimiter
    char* start;
    char* end;
    int i=0;
    int val1,val2;

    // count the number of colons
    int col_count = 0;
    char *pch=strchr(prefixCopy,':');
    while (pch!=NULL) {
      col_count++;
      pch=strchr(pch+1,':');
    }
    // there should be 7 colons
    // if there are 7 and there are two colons next to eachother
    // we know that value is 0000
    // if there are 6 the value is 0000:0000 and so on
    // we have already verified that there is at least one
    int cons_zero_count = 8 - col_count;

    start = prefixCopy;
    int str_len;
    while(start != NULL && i<bytes){
      end = strchr(start,':');
      if(end == NULL){
        end = strchr(start,'/');
        if(end == NULL){
          free(prefixOut);
          return NULL;
        }
        *end = '\0';
        str_len = strlen(start);
        if(str_len ==0){
          val1 = 0;
          val2 = 0;
        }else if(str_len < 3){
          val1 = 0;
          if(sscanf(start,"%x",&val2) != 1){
            free(prefixOut);
            return NULL;
          }
        }else if(str_len == 3){
          if(sscanf(start,"%1x%2x",&val1,&val2) != 2){
            free(prefixOut);
            return NULL;
          }
        }else if(str_len == 4){
          if(sscanf(start,"%2x%2x",&val1,&val2) != 2){
            free(prefixOut);
            return NULL;
          }
        }else{
          free(prefixOut);
          return NULL;
        }
        start = NULL;
      }else{ 
        *end = '\0';
        str_len = strlen(start);
        if(str_len ==0){
          // this may end up being many
          val1 = 0;
          val2 = 0;
        }else if(str_len < 3){
          val1 = 0;
          if(sscanf(start,"%x",&val2) != 1){
            free(prefixOut);
            return NULL;
          }
        }else if(str_len == 3){
          if(sscanf(start,"%1x%2x",&val1,&val2) != 2){
            free(prefixOut);
            return NULL;
          }
        }else if(str_len == 4){
          if(sscanf(start,"%2x%2x",&val1,&val2) != 2){
            free(prefixOut);
            return NULL;
          }
        }else{
          free(prefixOut);
          return NULL;
        }
        start = end+1;
      }
      prefixOut->paddr[i] = val1;
      if((i+1)<bytes){
        prefixOut->paddr[i+1] = val2;
      }
      i+=2;
      if(str_len == 0){
        int k;
        for(k=0;k<(cons_zero_count-1) && i<bytes;k++){
          prefixOut->paddr[i] = val1;
          if((i+1)<bytes){
            prefixOut->paddr[i+1] = val2;
          }
          i+=2;
        }
      }
    }
   prefixOut->paddr[bytes-1] &= mask;
  }

  return prefixOut; 
}

/*------------------------------------------------------------------------------
 * Purpose: Print prefix 
 * Input: lenght of prefix which is stored in prefix structure
 * Output: prefix string like 1.2.3.4/24
 * Mikhail Strizhov @ Feb 10, 2010
 * Cathie Olschanowsky @ May 2012
 * ---------------------------------------------------------------------------*/
char*
printPrefix(const Prefix *prefix){

  char delim;
  int length,j=0;
  char *prefix_str = (char*)malloc(MAX_PREFIX_LEN);
  char *str = (char*)malloc(MAX_PREFIX_LEN);
  char *number_format;
  int group_size;
  int expected_delim_count;
  int delim_count = 0;

  if(prefix->afi == 1){
    // ipv4 address
    delim = '.';
    number_format = "%d";
    group_size = 1;
    expected_delim_count=3;

  }else if(prefix->afi == 2){
    // ipv6 address
    delim = ':';
    number_format = "%x";
    group_size = 2;
    expected_delim_count=7;
  }else{
    return NULL;
  }
  
  // retrieve the number of bits in the prefix
  length = prefix->addr.p_len/(8*group_size); 
  if(length <= 0){
    //prefix_str[0] = '0';
    prefix_str[0] = '\0';
  }else{
    // this adds the first digits to the string representation
    // IPV4 %d.
    // IPV6 %x%02x: OR %x:
    if(group_size ==1 ){
      snprintf(prefix_str,MAX_PREFIX_LEN,"%d",prefix->addr.paddr[0]);
    }else{
      if(prefix->addr.paddr[0] > 0){
        snprintf(prefix_str,MAX_PREFIX_LEN,"%x",prefix->addr.paddr[0]);
        snprintf((prefix_str+strlen(prefix_str)),MAX_PREFIX_LEN,"%02x",prefix->addr.paddr[1]);
      }else if(prefix->addr.paddr[1] > 0){
        snprintf(prefix_str,MAX_PREFIX_LEN,"%x",prefix->addr.paddr[1]);
      }else{
        str[0] = '\0';
      }
    }
    // now loop to do the formatting for the rest of the string
    for(j=1;j<length;j++){
      if(group_size ==1 ){
        snprintf(str,MAX_PREFIX_LEN,number_format,prefix->addr.paddr[j]);
      }else{
        if(prefix->addr.paddr[j*group_size] > 0){
          snprintf(str,MAX_PREFIX_LEN,"%x",prefix->addr.paddr[j*group_size]);
          snprintf((str+strlen(str)),MAX_PREFIX_LEN,"%02x",prefix->addr.paddr[(j*group_size)+1]);
        }else if(prefix->addr.paddr[(j*group_size)+1] > 0){
          snprintf(str,MAX_PREFIX_LEN,"%x",prefix->addr.paddr[(j*group_size)+1]);
        }else{
          str[0] = '0';
          str[1] = '\0';
        }
      }
      strncat(prefix_str,&delim,1);
      delim_count++;
      strcat(prefix_str,str);
    }
  }
  // we need to mask to get the value for the last part
  int modVal = prefix->addr.p_len%(8*group_size);
  if(modVal > 0){
    uint8_t lastVal;
    // ipv4
    if(group_size == 1){
      lastVal = prefix->addr.paddr[length];
      uint8_t mask = 255;
      mask = mask << ((8*group_size)-modVal); 
      lastVal = lastVal & mask;
      snprintf(str,MAX_PREFIX_LEN,number_format,lastVal);
    }else{
      if(modVal >= 8){
        // the second value in this pair needs to be masked
        lastVal = prefix->addr.paddr[(length*group_size)+1];
        uint8_t mask = 255;
        mask = mask << ((8*group_size)-modVal); 
        lastVal = lastVal & mask;
        if(prefix->addr.paddr[j*group_size] > 0){
          snprintf(str,MAX_PREFIX_LEN,"%x",prefix->addr.paddr[j*group_size]);
          snprintf((str+strlen(str)),MAX_PREFIX_LEN,"%02x",lastVal);
        }else if(prefix->addr.paddr[(j*group_size)+1] > 0){
          snprintf(str,MAX_PREFIX_LEN,"%x",
                   prefix->addr.paddr[(j*group_size)+1]);
        }else{
          str[0] = '0';
          str[1] = '\0';
        }
      }else{
        // the first value needs to be masked
        // the second value will be masked out to 0
        lastVal = prefix->addr.paddr[(length*group_size)];
        uint8_t mask = 255;
        mask = mask << (8-modVal); 
        lastVal = lastVal & mask;
        if(lastVal > 0){
          snprintf(str,MAX_PREFIX_LEN,"%x",lastVal);
          snprintf((str+strlen(str)),MAX_PREFIX_LEN,"%02x",0);
        }else{
          str[0] = '0';
          str[1] = '\0';
        }
      }
    }
    if(length > 0){
      strncat(prefix_str,&delim,1);
      delim_count++;
    }
    strcat(prefix_str,str);
  }
  while(delim_count < expected_delim_count){
    strncat(prefix_str,&delim,1);
    delim_count++;
    strcat(prefix_str,"0");
  }

  // copy prefix len	
  snprintf( str, MAX_PREFIX_LEN, "/%d", prefix->addr.p_len );
  strcat(prefix_str, str);
  free(str);

  return prefix_str;

}

/*----------------------------------------------------------------------------------------
 * Purpose: Apply a reachable prefix to the rib table
 * Input:	 prefix - the pointer to the prefix
 *		 attrNode - the associated attribute node of the prefix
 *		 originatedTS - the timestamp
 *		 session - the corresponding session structure
 *		 bmf - BMF message if labeling is enabled and the label will be appened to the BMF message
 * Output:  0 for success or -1 for failure
 * NOTE:    
 * He Yan @ July 4th, 2008 
 * -------------------------------------------------------------------------------------*/
int applyReachablePrefix (const Prefix *prefix, AttrNode *attrNode, u_int32_t originatedTS, Session_structp session, BMF bmf)
{
   	PrefixNode   *prefixNode = NULL;
   	INDEX          i;
   	int            error;

	i = prefix_hash((u_char *)prefix, (PREFIX_SIZE(prefix->addr.p_len))+sizeof(Prefix), session->prefixTable->tableSize);
   	prefixNode = session->prefixTable->prefixEntries[i].node;
   	while( prefixNode != NULL && memcmp(prefix, &(prefixNode->keyPrefix), (PREFIX_SIZE(prefix->addr.p_len))+sizeof(Prefix)) )
		prefixNode = prefixNode->next;   

   	/* If the prefix is not existing in the rib table */
   	if( prefixNode == NULL ) 
	{	   	
#ifdef DEBUG
	    debug(__FUNCTION__, "Given prefix was not found in the rib table, insert a new one.");
#endif

		if( bmf != NULL)
		{
			/* Add new announcement label to this prefix */
			u_char label = BGPMON_LABEL_ANNOUNCE_NEW;
			bgpmonMessageAppend( bmf, &label, 1);
			//createLabelNode(prefix, BGPMON_LABEL_ANNOUNCE_NEW, lt);
		}
		session->stats.nannRcvd++;

		/* Create and insert a new prefix node */
		#ifdef DEBUG
	    debug(__FUNCTION__, "Malloc a prefix node: %d %d %d", PREFIX_SIZE(prefix->addr.p_len), sizeof(PrefixNode), (PREFIX_SIZE(prefix->addr.p_len))+sizeof(PrefixNode));
		#endif		
	    prefixNode = malloc((PREFIX_SIZE(prefix->addr.p_len)) + sizeof(PrefixNode));  
		session->stats.memoryUsed += (sizeof(PrefixNode) + (PREFIX_SIZE(prefix->addr.p_len)));
		prefixNode->keyPrefix.afi = prefix->afi;
	 	prefixNode->keyPrefix.safi = prefix->safi;
		prefixNode->keyPrefix.addr.p_len = prefix->addr.p_len;
	        memcpy( prefixNode->keyPrefix.addr.paddr, prefix->addr.paddr,
                        PREFIX_SIZE(prefix->addr.p_len) );
	        prefixNode->dataAttr = attrNode;
		prefixNode->originatedTS = originatedTS;

		/* Create and insert a new prefix ref node in the prefix ref list of attribute node*/	
		if( (error = pthread_rwlock_wrlock (&(attrNode->lock))) > 0 ) 
		{
			log_fatal ("Failed to wrlock an attribute node in the attribute table: %s", strerror(error));
		}	
	    attrNode->refCount++;
		PrefixRefNode *newRefNode = NULL;
		newRefNode = malloc(sizeof(PrefixRefNode));
		session->stats.memoryUsed += sizeof(PrefixRefNode);
		newRefNode->prefixNode = prefixNode;
		newRefNode->next = attrNode->prefixRefNode;
		attrNode->prefixRefNode = newRefNode;		    
		pthread_rwlock_unlock(&(attrNode->lock));

		/*Update the prefix entry*/
	    if( session->prefixTable->prefixEntries[i].node == NULL )
	    	session->prefixTable->ocupiedSize++;
	    prefixNode->next = session->prefixTable->prefixEntries[i].node;
	    session->prefixTable->prefixEntries[i].node = prefixNode;	      
	    session->prefixTable->prefixEntries[i].nodeCount++;

		/*Check if the max collision happens */
	    session->prefixTable->maxNodeCount = MAXV(session->prefixTable->maxNodeCount, session->prefixTable->prefixEntries[i].nodeCount);
	    if( session->prefixTable->maxNodeCount > session->prefixTable->maxCollision )
		{
	    	log_err("The maximum collision in the prefix hash table was reached.");
	    }
		session->prefixTable->prefixCount++;
		session->stats.prefixCount++;
	} 
	/* If the prefix is already existing in the rib table */
	else 
	{
		/* If the attributes are same */
	    if( prefixNode->dataAttr == attrNode ) 
		{
			#ifdef DEBUG
		    debug (__FUNCTION__,  "Found a duplicate prefix.");
			#endif

			if( bmf != NULL)
			{
				// Add duplicate announcement label to this prefix
				u_char label = BGPMON_LABEL_ANNOUNCE_DUPLICATE;
				bgpmonMessageAppend( bmf, &label, 1);
				//createLabelNode(prefix, BGPMON_LABEL_ANNOUNCE_DUPLICATE, lt);
			}
			session->stats.dannRcvd++;

			// Update the timestamp of the existing prefix
			prefixNode->originatedTS = originatedTS;
		    return 0;
    	}
		
		/* If the attributes are not same, then check the AS path to determine it is a DPATH or SPATH update */	   
	    if( prefixNode->dataAttr->bucketIndex != attrNode->bucketIndex || 
			prefixNode->dataAttr->asPath->asPathID != attrNode->asPath->asPathID )
	    {
	    	#ifdef DEBUG
		    debug (__FUNCTION__,  "Found a DPATH prefix.");
			#endif
	    	// DPATH
			if( bmf != NULL)
			{	    
				u_char label = BGPMON_LABEL_ANNOUNCE_DPATH;
				bgpmonMessageAppend( bmf, &label, 1);
				//createLabelNode(prefix, BGPMON_LABEL_ANNOUNCE_DPATH, lt);  
			}
			session->stats.dpathRcvd++;
	    }
	    else
	    {
	    	#ifdef DEBUG
		    debug (__FUNCTION__,  "Found a SPATH prefix.");
			#endif		    
	    	//SPATH
			if( bmf != NULL)
			{	   
				u_char label = BGPMON_LABEL_ANNOUNCE_SPATH;
				bgpmonMessageAppend( bmf, &label, 1);
				//createLabelNode(prefix, BGPMON_LABEL_ANNOUNCE_SPATH, lt);   
			}
			session->stats.spathRcvd++;
		}

		/* Remove the prefix from the prefix ref list of old attribute node */
		if( (error = pthread_rwlock_wrlock (&(prefixNode->dataAttr->lock))) > 0 ) 
		{
			log_fatal ("Failed to wrlock an attribute node in the attribute table: %s", strerror(error));
		}	
		prefixNode->dataAttr->refCount--;
		if( removePefixFomAttr(prefixNode, prefixNode->dataAttr, session) )
			log_fatal("Failed to remove a prefix fom a attribute.");
		pthread_rwlock_unlock(&(prefixNode->dataAttr->lock));
		  	
	    /* If the old attribute node is not used by any prefixes, delete it*/    
	    if( prefixNode->dataAttr->refCount == 0 ) 
		{
			if( removeAttrNode( prefixNode->dataAttr, session ) ) 
					log_err ("Failed to remove given attr from attr table");
	    }

		/* Add the prefix to the prefix ref list of new attribute node */
		if( (error = pthread_rwlock_wrlock (&(attrNode->lock))) > 0 ) 
		{
			log_fatal ("Failed to wrlock an attribute node in the attribute table: %s", strerror(error));
		}	      
	    prefixNode->dataAttr= attrNode;
		prefixNode->originatedTS = originatedTS;
	    attrNode->refCount++;
	    PrefixRefNode *newRefNode = NULL;
	    newRefNode = malloc(sizeof(PrefixRefNode));
		session->stats.memoryUsed += sizeof(PrefixRefNode);
	    newRefNode->prefixNode = prefixNode;
	    newRefNode->next = attrNode->prefixRefNode;
	    attrNode->prefixRefNode = newRefNode;    
		pthread_rwlock_unlock(&(attrNode->lock));	
	}
	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Remove the given prefix from the rib table and free that attrnode's space when the 
 * reference count of the attr is changed to 0. 
 * Input:	 prefix - the pointer to the prefix to be removed
 *		 session - the corresponding session structure
 *		 bmf - BMF message if labeling is enabled and the label will be appened to the BMF message
 * Output:  0 for success or -1 for failure
 * He Yan @ July 4th, 2008
 * -------------------------------------------------------------------------------------*/
int applyUnreachablePrefix (const Prefix *prefix, Session_structp session, BMF bmf)
{
	INDEX			i;
	PrefixNode		*node, *prevNode;
	int				error;
   
	prevNode = NULL;
	i = prefix_hash((u_char *)prefix, (PREFIX_SIZE(prefix->addr.p_len))+sizeof(Prefix), session->prefixTable->tableSize);

	/* lookup the prefix */
 	node = session->prefixTable->prefixEntries[i].node;
	while( node != NULL && memcmp(prefix, &(node->keyPrefix), (PREFIX_SIZE(prefix->addr.p_len))+sizeof(Prefix)) )
	{
		prevNode = node;
    	node = node->next;
   	}
   
   	// create the corresponding entry in label table   
   	/* if nonexist */
   	if( node == NULL ) 
	{
   		// Duplicate withdraw
		if( bmf != NULL)
		{   		
			u_char label = BGPMON_LABEL_WITHDRAW_DUPLICATE;
			bgpmonMessageAppend( bmf, &label, 1);
    		//createLabelNode(prefix, BGPMON_LABEL_WITHDRAW_DUPLICATE, lt);
		}
		session->stats.duwiRcvd++;
#ifdef DEBUG
	log_warning( "Try to remove a non-exist prefix node from rib table.");
#endif
	return 0;
   	}
	
   	// Non Duplicate withdraw
	if( bmf != NULL)
	{   	
		u_char label = BGPMON_LABEL_WITHDRAW;
		bgpmonMessageAppend( bmf, &label, 1);
   		//createLabelNode(prefix, BGPMON_LABEL_WITHDRAW, lt); 
	}
	session->stats.withRcvd++;
			
	if( (error = pthread_rwlock_wrlock (&(node->dataAttr->lock))) > 0 ) 
	{
		log_fatal ("Failed to wrlock an entry in the rib table: %s", strerror(error));
	}
   	node->dataAttr->refCount--;
	
   	if( removePefixFomAttr(node, node->dataAttr, session) )
		log_err("Failed to remove a prefix fom a attribute.");
	pthread_rwlock_unlock(&(node->dataAttr->lock));
	
	  
   	if( node->dataAttr->refCount == 0 )
	{
    	if( removeAttrNode( node->dataAttr, session ) ) 
        	log_err ("Failed to remove given attr from attr table");
   	}

	if( prevNode == NULL )
    	session->prefixTable->prefixEntries[i].node = node->next;
   	else 
    	prevNode->next = node->next;
   
  	if (session->prefixTable->prefixEntries[i].node == NULL)
    	session->prefixTable->ocupiedSize--;
   
   	session->prefixTable->prefixEntries[i].nodeCount--;
	session->stats.memoryUsed -= ( sizeof(PrefixNode) + (PREFIX_SIZE(node->keyPrefix.addr.p_len)) );	
	free(node);
   	session->prefixTable->prefixCount--;
	session->stats.prefixCount--;
   	return 0;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Apply the reachable NLRI of a BGP update to the rib table
 * Input:  originatedTS - the timestamp
 *		 nlri - the pointer to the BGPNlri structure whcin includes multiple prefixes
 *		 attrNode - the associated attribute node of the nlri	
 *		 session - the corresponding session structure
 *		 bmf - BMF message if labeling is enabled and the labels of prefixes will be appened to the BMF message 
 * Output:
 * NOTE:  This function calls function 'applyReachablePrefix', one for each prefix.
 * He Yan @ July 4th, 2008 
 * -------------------------------------------------------------------------------------*/
void applyReachableNLRI (time_t originatedTS, BGPNlri *nlri, AttrNode	*attrNode, Session_structp session, BMF bmf)
{
	MSTREAM		s;
	u_int8_t	len;
	Prefix		*prefix = NULL;
	
	mstream_init(&s, nlri->nlri, nlri->nlriLen);
	while( mstream_can_read(&s) > 0 )
	{
		mstream_getc(&s, &len);
		prefix = malloc(sizeof(Prefix) + (PREFIX_SIZE(len)));
		prefix->afi = nlri->afi;
		prefix->safi = nlri->safi;
		prefix->addr.p_len = len;
		memcpy( prefix->addr.paddr, mstream_current_address(&s),
                        PREFIX_SIZE(len));
		if( applyReachablePrefix (prefix, attrNode, originatedTS, session, bmf))
		{
			log_err( "Failed to apply a reachable prefix to the rib table.");
	//#ifdef DEBUG		
			hexdump(LOG_ERR, prefix, (PREFIX_SIZE(prefix->addr.p_len))+sizeof(Prefix));
                        log_err( "%s: advancing position: %d",__FUNCTION__,prefix->addr.p_len);
	//#endif		
		}
	#ifdef DEBUG
	else
		debug (__FUNCTION__, "Succefully apply a reachable prefix to the rib table.");
	#endif
   		s.position += PREFIX_SIZE(prefix->addr.p_len);
		free(prefix);
		prefix = NULL;
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: Apply the unreachable NLRI of a BGP update to the rib table
 * Input:  nlri - the pointer to the BGPNlri structure whcin includes multiple prefixes
 *		 session - the corresponding session structure
 *		 bmf - BMF message if labeling is enabled and the labels of prefixes will be appened to the BMF message 
 * Output:
 * NOTE:  This function calls function 'applyUNreachablePrefix', one for each prefix.
 * He Yan @ July 4th, 2008 
 * -------------------------------------------------------------------------------------*/
void applyUnreachableNLRI (BGPNlri *nlri, Session_structp session, BMF bmf)
{
	MSTREAM		s;
	u_int8_t	len;
	Prefix		*prefix = NULL;
	
	mstream_init(&s, nlri->nlri, nlri->nlriLen);
	while( mstream_can_read(&s) > 0 )
	{
		mstream_getc(&s, &len);
		prefix = malloc(sizeof(Prefix) + (PREFIX_SIZE(len)));
		prefix->afi = nlri->afi;
		prefix->safi = nlri->safi;
		prefix->addr.p_len = len;
		memcpy( prefix->addr.paddr, mstream_current_address(&s),
                        PREFIX_SIZE(len));
		if( applyUnreachablePrefix (prefix, session, bmf) ) 
		{
			log_err ("Failed to withdraw a IPv4 prefix from rib table.");
			hexdump(LOG_ERR, prefix, (PREFIX_SIZE(prefix->addr.p_len))+sizeof(Prefix));
		}
	#ifdef DEBUG
		debug (__FUNCTION__, "Succefully withdraw a IPv4 prefix from rib table.");
	#endif
		s.position += PREFIX_SIZE(prefix->addr.p_len);
		free(prefix);
		prefix = NULL;
	}

}

/*--------------------------------------------------------------------------------------
 * Purpose: Apply a BGP Update message to a rib table
  * Input:  originatedTS - the timestamp
 *		  parsedUpdateMsg -  Parsed BGP update messages
 *		  session - the corresponding session structure
 *		  bmf - BMF message if labeling is enabled and the labels of prefixes will be appened to the BMF message 
 * Output: 0 for success or -1 for failure
 * He Yan @ July 4th, 2008
 * -------------------------------------------------------------------------------------*/ 
int applyBGPUpdate (time_t originatedTS, ParsedBGPUpdate *parsedUpdateMsg, Session_structp session, BMF bmf)
{
	AttrNode	*attrNode = NULL;
	int			i;
	assert(parsedUpdateMsg!=NULL && session != NULL);   

#ifdef DEBUG
	debug (__FUNCTION__, "Start to remove IPv4 unreachable nlri from the rib table...");
#endif
	/* Remove IPv4 unreachable nlri from the rib table */
	if( parsedUpdateMsg->unreachNlri.nlriLen )
	{
		/* nlri infomation sanity check */
		if( nlriCheck(parsedUpdateMsg->unreachNlri.nlri, parsedUpdateMsg->unreachNlri.nlriLen) )
		{
			log_err ("IPv4 unreachable nlri was corrupt!");
			return -1;
		} 
		else
		{
			/* start to delete the IPv4 unreachable nlri from the rib table */
			applyUnreachableNLRI(&(parsedUpdateMsg->unreachNlri), session, bmf);
		}
	}

	
	/* apply mp reachable/unreach nlri to rib table */
	for(i=0; i<parsedUpdateMsg->numOfMpNlri; i++)
	{
		if( parsedUpdateMsg->mpNlri[i].flag == 0 )//apply mp reachable
		{
		#ifdef DEBUG
			debug (__FUNCTION__, "Start to apply mp(afi:%d safi:%d)reachable nlri to the rib table...", parsedUpdateMsg->mpNlri[i].afi, parsedUpdateMsg->mpNlri[i].safi);
		#endif
	
			if( parsedUpdateMsg->attr.totalLen == 0 ) 
			{
				log_err("Received a mp reachable nlri without attr. Ignored!");
				return -1;
			}		
			/* nlri infomation sanity check */
			else if( nlriCheck (parsedUpdateMsg->mpNlri[i].nlri, parsedUpdateMsg->mpNlri[i].nlriLen) ) 
			{
				log_err("mp reachable nlri was corrupt!");
				return -1;
			} 
			else
			{	
		#ifdef DEBUG
				debug (__FUNCTION__, "Look up the attribute from attribute table ...");
		#endif

				attrNode = searchAttrNode( parsedUpdateMsg->asPath.data, parsedUpdateMsg->asPath.len,
							parsedUpdateMsg->attr.data, parsedUpdateMsg->attr.totalLen, parsedUpdateMsg->attr.basicAttrlen, session);
				if( attrNode == NULL )
				{
					log_err("Failed to find the posision of a given attr!");
					return -1;
				}
				else
				{
					/* start to apply the mp reachable nlri to the rib table */
					BGPNlri reachNlri;
					reachNlri.afi = parsedUpdateMsg->mpNlri[i].afi;
					reachNlri.safi = parsedUpdateMsg->mpNlri[i].safi;
					reachNlri.nlriLen = parsedUpdateMsg->mpNlri[i].nlriLen;
					reachNlri.nlri = parsedUpdateMsg->mpNlri[i].nlri;
					applyReachableNLRI(originatedTS, &reachNlri, attrNode, session, bmf);
				}
			}
		}
		else//apply mp unreachable
		{
			#ifdef DEBUG
			debug (__FUNCTION__, "Start to remove mp(afi:%d safi:%d) unreachable nlri from the rib table...", parsedUpdateMsg->mpNlri[i].afi, parsedUpdateMsg->mpNlri[i].safi);
			#endif	
			/* nlri infomation sanity check */
			if( nlriCheck(parsedUpdateMsg->mpNlri[i].nlri, parsedUpdateMsg->mpNlri[i].nlriLen) )
			{
				log_err ("mp unreachable nlri was corrupt!");
				return -1;
			} 
			else
			{
				/* start to delete the mp unreachable nlri from the rib table */
				BGPNlri unreachNlri;
				unreachNlri.afi = parsedUpdateMsg->mpNlri[i].afi;
				unreachNlri.safi = parsedUpdateMsg->mpNlri[i].safi;
				unreachNlri.nlriLen = parsedUpdateMsg->mpNlri[i].nlriLen;
				unreachNlri.nlri = parsedUpdateMsg->mpNlri[i].nlri;				
				applyUnreachableNLRI(&unreachNlri, session, bmf);
			}			
		}
	}


#ifdef DEBUG
	debug (__FUNCTION__, "Start to apply IPv4 reachable nlri to the rib table...");
#endif
	/* apply IPv4 reachable nlri to rib table */
	if( parsedUpdateMsg->reachNlri.nlriLen )
	{	

		if( parsedUpdateMsg->attr.basicAttrlen == 0 ) 
		{
			log_err("Received a IPv4 reachable nlri without attr. Ignored!");
			return -1;
		}		
 		/* nlri infomation sanity check */
      	else if( nlriCheck (parsedUpdateMsg->reachNlri.nlri, parsedUpdateMsg->reachNlri.nlriLen) ) 
		{
			log_err("IPv4 reachable nlri was corrupt!");
			return -1;
		}  
		else
		{   
			#ifdef DEBUG
			debug (__FUNCTION__, "Look up the attribute from attribute table ...");
			#endif
			attrNode = searchAttrNode( parsedUpdateMsg->asPath.data, parsedUpdateMsg->asPath.len,
							parsedUpdateMsg->attr.data, parsedUpdateMsg->attr.totalLen, parsedUpdateMsg->attr.basicAttrlen, session );
			if( attrNode == NULL )
			{
            	log_err("Failed to find the posision of a given attr!");
				return -1;
			}
         	else
			{
				/* start to apply the IPv4 reachable nlri to the rib table */
				applyReachableNLRI(originatedTS, &(parsedUpdateMsg->reachNlri), attrNode, session, bmf);
			}
		}
	}

	return 0;
}

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
static u_char	updateBuf[MAX_BGP_MESSAGE_LEN];
static u_char	mpAttrBuf[MAX_BGP_MESSAGE_LEN];
static u_char	nlriBuf[MAX_BGP_MESSAGE_LEN];
int sendBMFFromAttrNode(AttrNode *attrNode, int sessionID, QueueWriter labeledQueueWriter)
{
	MSTREAM			source;
	// initialize buffer for the mp attrbutes section in a update
	MSTREAM			mpAttr;
	memset (mpAttrBuf, 0, MAX_BGP_MESSAGE_LEN);
	mstream_init(&mpAttr, mpAttrBuf, MAX_BGP_MESSAGE_LEN);
	
	MSTREAM			nlri;
	memset (nlriBuf, 0, MAX_BGP_MESSAGE_LEN);
	mstream_init(&nlri, nlriBuf, MAX_BGP_MESSAGE_LEN);

	// lock this attribute node 
	int error;
	if( (error = pthread_rwlock_rdlock (&(attrNode->lock))) > 0 ) 
	{
		log_err("Failed to read lock an attribute node in the attribute table: %s", strerror(error));
		return -1;
	}	
	
	//1. process the mp attributes
	u_int16_t startPos;
	u_int16_t mpAttrLen;
	u_char mpAttrFlag, mpAttrType, ampAttrLenShort;
	PrefixRefNode *prefixRefNode = NULL;
	mstream_init(&source, attrNode->attr+attrNode->basicAttrLen, attrNode->totalAttrLen-attrNode->basicAttrLen);
	while( mstream_can_read(&source) > 0 ) 
	{
		startPos = source.position;
		if( mstream_getc(&source, &mpAttrFlag) )
		{
			log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
			pthread_rwlock_unlock(&(attrNode->lock));
			return -1;
		}			
		if( mstream_getc(&source, &mpAttrType) ) 
		{
			log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
			pthread_rwlock_unlock(&(attrNode->lock));
			return -1;
		}		
		/* get attribute length */
		if( mpAttrFlag & BGP_ATTR_FLAG_EXT_LEN ) 
		{
			if( mstream_getw(&source, &mpAttrLen) )
			{
				log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
				pthread_rwlock_unlock(&(attrNode->lock));
				return -1;
			}					
		} 
		else 
		{
			if( mstream_getc(&source,&ampAttrLenShort) )
			{
				log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
				pthread_rwlock_unlock(&(attrNode->lock));
				return -1;
			}
			mpAttrLen = ampAttrLenShort;
		}

		if( mstream_can_read(&source) < mpAttrLen) 
		{
			log_err("%s [%d] - Failed! Message was corrupt.", __FILE__, __LINE__);
			pthread_rwlock_unlock(&(attrNode->lock));
			return -1;
		}			

		switch (mpAttrType)
		{
			case BGP_MP_REACH:
				{
					// get the afi and afi
					u_int8_t safi;
					u_int16_t afi;
					if( mstream_getw(&source, &afi) )
					{
						log_err("%s [%d] - Failed! Mpreach message was corrupt.", __FILE__, __LINE__);
						pthread_rwlock_unlock(&(attrNode->lock));
						return -1;
					}
					if( mstream_getc(&source, &safi) )
					{
						log_err("%s [%d] - Failed! Mpreach message was corrupt.", __FILE__, __LINE__);
						pthread_rwlock_unlock(&(attrNode->lock));
						return -1;
					}

					// find all prefixes with the same afi&safi as this mp attribute.
					prefixRefNode = attrNode->prefixRefNode;
					int flag = 0;
					u_int16_t mpStartPos = mpAttr.position;;
					while( prefixRefNode != NULL )
					{
						//find one matched prefix with the same afi&safi as this mp attribute.
						if( prefixRefNode->prefixNode->keyPrefix.afi == afi
							&& prefixRefNode->prefixNode->keyPrefix.safi == safi )
						{
							if( flag == 0)
							{
								// insert the mpreach attribute without NLRI. Only do once.
								if(mstream_add( &mpAttr, source.start+startPos, mpAttrLen + source.position - startPos -3 ))
								{
							 		log_err("Buffer is overflow!1");
									pthread_rwlock_unlock(&(attrNode->lock));
							 		return -1;
								}		
								flag = 1;
							}
							// insert every matched prefix
							u_int16_t prefixLenInBytes = PREFIX_SIZE(prefixRefNode->prefixNode->keyPrefix.addr.p_len);
							if( mstream_add( &mpAttr, &prefixRefNode->prefixNode->keyPrefix.addr, prefixLenInBytes+1 ) )
							{
								// BGP update message is full, send it and start new message
								if (createAndSendBMFFromAttr(sessionID, attrNode,mpAttr, nlri, labeledQueueWriter) == -1)
								{
									log_err("%s [%d] Could not send BMF message!", __FILE__, __LINE__);
									pthread_rwlock_unlock(&(attrNode->lock));
									return -1;
								}
								// reset mp reach to 0
								memset (mpAttrBuf, 0, MAX_BGP_MESSAGE_LEN);
								mstream_init(&mpAttr, mpAttrBuf, MAX_BGP_MESSAGE_LEN);
								if(mstream_add( &mpAttr, source.start+startPos, mpAttrLen + source.position - startPos -3 ))
								{
							 		log_err("Buffer is overflow!1");
									pthread_rwlock_unlock(&(attrNode->lock));
							 		return -1;
								}		
								if( mstream_add( &mpAttr, &prefixRefNode->prefixNode->keyPrefix.addr, prefixLenInBytes+1 ) )

								{
							 		log_err("Buffer is overflow!2");
									pthread_rwlock_unlock(&(attrNode->lock));
							 		return -1;
								}		
							}									

							/*rewrite attribute length*/
							if (mpAttrFlag & BGP_ATTR_FLAG_EXT_LEN)
							{
					        	*((u_int16_t *)(mpAttr.start + mpStartPos + 2)) += htons(prefixLenInBytes+1);
							}
							else
							{
								*((u_int8_t *)(mpAttr.start + mpStartPos + 2)) += (prefixLenInBytes+1);
							}
						}
						prefixRefNode = prefixRefNode->next;
					}	
					source.position += mpAttrLen-3;
				}				
				break;

			case BGP_MP_UNREACH:
				log_err("%s [%d] - Failed! Found a mp unreach attribute.", __FILE__, __LINE__);
				pthread_rwlock_unlock(&(attrNode->lock));
				return -1;
				break;
				
			default:
				log_err("%s [%d] - Failed! Found a non-mp reach attribute(%d).", __FILE__, __LINE__, mpAttrType);
#ifdef DEBUG
				hexdump(LOG_INFO, attrNode->attr, attrNode->totalAttrLen);
				log_msg("--------------------------------------");
				hexdump(LOG_INFO, attrNode->attr+attrNode->basicAttrLen, attrNode->totalAttrLen-attrNode->basicAttrLen);
#endif				
				pthread_rwlock_unlock(&(attrNode->lock));
				return -1;
				break;
		}
	} 


	// 2. find all prefixes with afi:1 and safi:1 and insert them into the normal(afi:1 and safi:1) NLRI
	// initialize buffer for the NLRI section(afi:1 and safi:1) in a update
	// calculate the remaining len of update message buffer
	int remainingLen = MAX_BGP_MESSAGE_LEN - 2 - 2 - attrNode->basicAttrLen - attrNode->asPath->asPathData.len - mpAttr.position;
	prefixRefNode = attrNode->prefixRefNode;
	while( prefixRefNode != NULL )
	{
		//find a prefix with afi:1 and safi:1
		//log_msg("preifx loop %d %d", prefixRefNode->prefixNode->keyPrefix.afi, prefixRefNode->prefixNode->keyPrefix.safi);
		if( prefixRefNode->prefixNode->keyPrefix.afi == 1
			&& prefixRefNode->prefixNode->keyPrefix.safi == 1 )
		{
			// insert every matched prefix
			u_int16_t prefixLenInBytes = PREFIX_SIZE(prefixRefNode->prefixNode->keyPrefix.addr.p_len);
			// check if the remaining buffer len is suffcient
			if( remainingLen < prefixLenInBytes + 1 )
			{
				// BGP update message is full, send it and start new message
				if (createAndSendBMFFromAttr(sessionID, attrNode,mpAttr, nlri, labeledQueueWriter) == -1)
				{
					log_err("%s [%d] Could not send BMF message!", __FILE__, __LINE__);
					pthread_rwlock_unlock(&(attrNode->lock));
					return -1;
				}
				// reset mp reach to 0
				memset (mpAttrBuf, 0, MAX_BGP_MESSAGE_LEN);
				mstream_init(&mpAttr, mpAttrBuf, MAX_BGP_MESSAGE_LEN);
				// reset nrli to 0
				memset (nlriBuf, 0, MAX_BGP_MESSAGE_LEN);
				mstream_init(&nlri, nlriBuf, MAX_BGP_MESSAGE_LEN);
				remainingLen = MAX_BGP_MESSAGE_LEN - 2 - 2 - attrNode->basicAttrLen - attrNode->asPath->asPathData.len;
			}	
			if( mstream_add( &nlri, &prefixRefNode->prefixNode->keyPrefix.addr, prefixLenInBytes+1 ))
			{
				log_err("Buffer is overflow %d!3", nlri.position);
				pthread_rwlock_unlock(&(attrNode->lock));
		 		return -1;
			}	
			remainingLen -= (prefixLenInBytes + 1);
			
		}
		prefixRefNode = prefixRefNode->next;
	}
	pthread_rwlock_unlock(&(attrNode->lock));
		
	error = createAndSendBMFFromAttr(sessionID, attrNode, mpAttr, nlri, labeledQueueWriter);

	return error;
}


/*--------------------------------------------------------------------------------------
 * Purpose: create and send BMF message from Attributes
 * Input: attrNode -  the attribute node used to create a BMF
 * 	  nlri - NLRI structure
 *	  labeledQueueWriter - name of queue for sending BMF messages	
 * Output: 0 for success or -1 for failure
 * Mikhail Strizhov @ July 21st, 2010
 * -------------------------------------------------------------------------------------*/ 
int createAndSendBMFFromAttr(int sessionID,AttrNode *attrNode, MSTREAM mpAttr, MSTREAM nlri,  QueueWriter labeledQueueWriter) 
{

	// initialize buffer for the body of update 
	MSTREAM		update;	
	memset (updateBuf, 0, MAX_BGP_MESSAGE_LEN);
	mstream_init(&update, updateBuf, MAX_BGP_MESSAGE_LEN);


	// 3. add withdraw routes len to 0 in a update
	u_int16_t withdrawLen = htons(0);
	if( mstream_add(&update, &withdrawLen, 2) ) 
	{
		log_err("Buffer is overflow!4");
		return -1;
	}	

	// 4. add attributes len in a update
	int attrLen;
	attrLen = htons(attrNode->basicAttrLen + attrNode->asPath->asPathData.len + mpAttr.position);
	if( mstream_add(&update, &attrLen, 2) )
	{
		log_err("Buffer is overflow!5");
		return -1;
	}		

	// 5. add basic attributes(without AS_PATH) section into a update
	if( mstream_add(&update, attrNode->attr, attrNode->basicAttrLen) )
	{
		log_err("Buffer i/s overflow!6");
		return -1;
	}	

	// 6. add AS Path attribute section into a update
	if( mstream_add(&update, attrNode->asPath->asPathData.data, attrNode->asPath->asPathData.len) )
	{
		log_err("Buffer is overflow!7");
		return -1;
	}	

	// 7. add mp attributes section into a update
	if( mstream_add(&update, mpAttr.start, mpAttr.position) )
	{
		log_err("Buffer is overflow!8");
		return -1;
	}	
	
	// 8. add NLRI section(afi:1 and safi:1) in a update
	if( mstream_add(&update, nlri.start, nlri.position) )
	{
		log_err("Buffer is overflow!9");
		return -1;
	}
	#ifdef DEBUG
	hexdump(LOG_INFO, update.start, update.position);
	#endif

	// 9. create BGP message header
	PBgpHeader hdr = createBGPHeader( typeUpdate );
	setBGPHeaderLength( hdr, update.position );

	// 10. create the BMF message
	BMF bmf = createBMF(sessionID, BMF_TYPE_TABLE_TRANSFER);
	bgpmonMessageAppend( bmf, hdr, 19);
	bgpmonMessageAppend( bmf, update.start, update.position );	
	
	// write BMF message to queue
	writeQueue (labeledQueueWriter, bmf);

	free(hdr);
	return 0;
}	
/* END */

