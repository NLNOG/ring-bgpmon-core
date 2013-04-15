/* 
 *  Copyright (c) 2010 Colorado State University
 * 
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom
 *  the Software is furnished to do so, subject to the following
 *  conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 * 
 * 
 *  File:    xml.c
 *  Authors: He Yan
 *           Pei-chun (Payne) Cheng
 *  Date:    Jun 22, 2008 
 */

/* needed for pthread related functions */
#include <pthread.h>

/* needed for queues*/
#include "../Queues/queue.h"

/* needed for TRUE/FALSE definitions */
#include "../Util/bgpmon_defaults.h"

/* needed for BMF types */
#include "../Util/bgpmon_formats.h"

/* needed for GMT_TIME_STAMP and ASCII_MESSAGES flags */
#include "../site_defaults.h"

/* needed for logging definitions */
#include "../Util/log.h"

/* needed for parsing bgp messages */
#include "../Peering/peersession.h"
#include "../Peering/bgppacket.h"
#include "../Peering/bgpmessagetypes.h"

/* needed for getSessionString */
#include "../Labeling/label.h"

#include "xml.h"

/* needed for internal functions */
#include "xmlinternal.h"

/* For XML conversion */
#include "xmldata.h"

//needed for sequence number management
#include "../Clients/clientscontrol.h"

//needed for loop cache
#include "../Chains/chains.h"

//#define DEBUG

// buffer used to do xml conversion
static char xml[XML_BUFFER_LEN]; /* XML_BUFFER_LEN defined in xmlinternal.h */

/*----------------------------------------------------------------------------------------
 * Purpose: get the length of a XML message,
 *          assuming that there exists a "length" attribute in the root element 
 * Input:   pointer to the XML message or partial XML message
 * Output:  length of the message
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
int getXMLMessageLen( char *xmlMsg )
{
    char *p;
    int  len = 0;
    int  xmllen = strlen(xmlMsg);

    char * xml_preamble = malloc(sizeof(char) * (xmllen+1));
    memset(xml_preamble, '\0', xmllen);
    strncpy(xml_preamble, xmlMsg, xmllen);

    /* 
     * Find the open tag for the root element and enclose it 
     * The enclosed element would be in the form <TAG .... length="?" ..../>
     */
    p = strtok (xml_preamble,">");
    if (p)
    {
        strcat(p, "/>");
    }

    xmlDocPtr  header_doc  = xmlReadMemory(p, strlen(p), "getXMLMessageLen.xml", NULL, 0);
    xmlNodePtr header_node = xmlDocGetRootElement(header_doc); /* Get the root element node */

    /* Retrieve the value of "length" attribute */
    char * len_str = (char *)xmlGetProp(header_node, BAD_CAST "length");
    if ( len_str == NULL )
    {
        // In order to be compatible with v5 message
        len_str = (char *)xmlGetProp(header_node, BAD_CAST "len");
    }

    /* Convert ascii length to numberic value */
    if ( len_str != NULL )
    {
        len = atoi(len_str);
	    free(len_str);
    }

    /* Clean up */
    xmlFreeDoc(header_doc);  /* Free the xml data */
    free(xml_preamble);

    return len;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Get the BGPmon ID and sequence number from incoming message
 * Input:	msg - a pointer to the message data
 *			msglen - the length of the message
 *			id - a pointer to the ID value
 *			seq - a pointer to the Sequence number value
 * Output:	0 on success, -1 on failure
 * Jason Bartlett @ 7 Apr 2011
 *------------------------------------------------------------------------------------*/
int getMsgIdSeq(char* msg,int msglen,u_int32_t *id,u_int32_t *seq)
{
	int retval = -1;

	if(msglen <= 0) return -1;

	xmlDocPtr msg_tree = xmlParseMemory(msg,msglen);
	xmlNodePtr root = xmlDocGetRootElement(msg_tree);
	xmlNodePtr curr_child;

	for(curr_child=xmlFirstElementChild(root);curr_child != NULL;curr_child=curr_child->next)
	{
		if(!xmlStrEqual(curr_child->name,BAD_CAST "BGPMON_SEQ")) 
		{
			continue;
		}
		else
		{
			char *buf = (char *)xmlGetProp(curr_child, BAD_CAST "id");
			if (buf !=NULL)
			{
				*id=(u_int32_t)strtoul(buf,NULL,10);
			}
			else 
			{
				return -1;
			}
			free(buf);
			char *buf2=(char*)xmlGetProp(curr_child,BAD_CAST "seq_num");
			if (buf2 != NULL)
			{
				*seq=(u_int32_t)strtoul(buf2,NULL,10);
			}
			else 
			{
				return -1;
			}
			free(buf2);
			retval =  0;
			break;
		}
	}
	xmlFreeDoc(msg_tree);
	return retval;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Entry function of xml thread
 * Input:
 * Output:
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
void * 
xmlThread( void *arg )
{
	log_msg( "XML thread started" );
	XMLControls.lastAction = time(NULL);
	XMLControls.shutdown = FALSE;
	
	QueueReader labeledQueueReader =  createQueueReader( &labeledQueue, 1 );
	QueueWriter xmlUQueueWriter = createQueueWriter( xmlUQueue );
	QueueWriter xmlRQueueWriter = createQueueWriter( xmlRQueue );	

	while( XMLControls.shutdown==FALSE )
	{
		BMF bmf = NULL;	
		readQueue( labeledQueueReader);
                bmf = (BMF)labeledQueueReader->items[0];
	
		// update time - make sure thread is alive
		XMLControls.lastAction = time(NULL);
	
        /* Reset xml buffer */
		xml[0] = '\0';
		char *xmlp = xml;
		int  len   = 0;

        /* Convert BMF internal structure to XMl text string */
		len = BMF2XMLDATA( bmf, xmlp, XML_BUFFER_LEN );
		if(len > 0)
		{
			switch ( bmf->type )
			{
				//write out newly-generated messages and increment sequence number
				case BMF_TYPE_MSG_TO_PEER:
				case BMF_TYPE_MSG_LABELED:
				case BMF_TYPE_MSG_FROM_PEER:
					{	
						u_char *xmlData = malloc(len);
						memcpy(xmlData, xml, len);
						writeQueue( xmlUQueueWriter, xmlData );
						break;
					}
				case BMF_TYPE_TABLE_TRANSFER:
				case BMF_TYPE_TABLE_START:
				case BMF_TYPE_TABLE_STOP:
				case BMF_TYPE_FSM_STATE_CHANGE:
					{
						u_char *xmlData = malloc(len);
						memcpy(xmlData, xml, len);
						writeQueue( xmlRQueueWriter, xmlData );
						break;
					}

				case BMF_TYPE_CHAINS_STATUS:
				case BMF_TYPE_QUEUES_STATUS:
				case BMF_TYPE_SESSION_STATUS:
				case BMF_TYPE_MRT_STATUS:
				case BMF_TYPE_BGPMON_START:
				case BMF_TYPE_BGPMON_STOP:
					{
						u_char *UxmlData = malloc(len);
						u_char *RxmlData = malloc(len);
						memcpy(UxmlData, xml, len);
						memcpy(RxmlData, xml, len);
						writeQueue( xmlUQueueWriter, UxmlData );
						writeQueue( xmlRQueueWriter, RxmlData );
						break;    	
					}

				default:
					{
						log_err ("BMF2XML: unknown type!!!!!!!!!!!!!!!");
						break;
					}

			}
			//increment sequence number, wrap around if necessary
			if(ClientControls.seq_num != UINT_MAX)
				ClientControls.seq_num++;
			else ClientControls.seq_num = 0;
		}
		
		/* delete the session structure of closed session */
		if( bmf->type == BMF_TYPE_FSM_STATE_CHANGE )
		{
			if( checkStateChangeMessage(bmf) )
			{
				destroySession(bmf->sessionID);
				log_msg( "Successfully destroy the session %d!", bmf->sessionID);
			}
		}

		/* Delete bmf structure */
		destroyBMF( bmf );
    }
    destroyQueueReader(labeledQueueReader);
    destroyQueueWriter(xmlUQueueWriter);
    destroyQueueWriter(xmlRQueueWriter);
    log_warning( "XML thread exiting" );

    return NULL;
}

/*--------------------------------------------------------------------------------------
 * Purpose: launch xml converter thread, called by main.c
 * Input:   none
 * Output:  none
 * He Yan @ July 22, 2008
 * -------------------------------------------------------------------------------------*/
void launchXMLThread()
{
    int error;
    
    pthread_t XMLThreadID;

    if ((error = pthread_create(&XMLThreadID, NULL, xmlThread, NULL)) > 0 )
        log_fatal("Failed to create XML thread: %s\n", strerror(error));

    XMLControls.xmlThread = XMLThreadID;

    debug(__FUNCTION__, "Created XML thread!");
}

/*--------------------------------------------------------------------------------------
 * Purpose: get the last action time of the XML thread
 * Input:
 * Output: last action time
 * Mikhail Strizhov @ Jun 25, 2009
 * -------------------------------------------------------------------------------------*/
time_t getXMLThreadLastAction()
{
	return XMLControls.lastAction;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Intialize the shutdown process for the xml module
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void signalXMLShutdown() 
{
#ifdef DEBUG
	log_msg("shutdown XML");
#endif
	XMLControls.shutdown = TRUE;
}

/*--------------------------------------------------------------------------------------
 * Purpose: wait on all xml pieces to finish closing before returning
 * Input:  none
 * Output: none
 * Kevin Burnett @ July 10, 2009
 * -------------------------------------------------------------------------------------*/
void waitForXMLShutdown()
{
	void * status = NULL;

	// wait for xml control thread exit
	pthread_join(XMLControls.xmlThread, status);
}

