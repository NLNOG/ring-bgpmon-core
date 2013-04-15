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
 *  File:    xmldata.c
 *  Authors: Pei-chun Cheng (Modified from xml.c by He Yan)
 *          Jason Bartlett
 *  Date:    Dec 20, 2008
 *
 */

 /*
 * The purpose of xmldata.c is to convert an internal BMF to a xml tree or string.
 * We may further export the xml tree with different formats.
 * Currely, we only export plain xml string.
 */

/* needed for malloc and free */
#include <stdlib.h>

/* needed for function inet_ntoa */
#include <arpa/inet.h>

/* needed for time related functions */
#include <time.h>

/* needed for string and math operation */
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>

/* needed for xml operation tring and math operation */
#include <libxml/parser.h>
#include <libxml/tree.h>

/* needed for queues*/
#include "../Queues/queue.h"

/* needed for chains*/
#include "../Chains/chains.h"

/* needed for TRUE/FALSE definitions */
#include "../Util/bgpmon_defaults.h"

/* needed for logging definitions */
#include "../Util/log.h"

/* needed for parsing bgp messages */
#include "../Peering/peersession.h"
#include "../Peering/bgppacket.h"
#include "../Peering/bgpmessagetypes.h"
#include "../Peering/bgpstates.h"

/* needed for getSessionString */
#include "../Labeling/label.h"

/* needed for MRT information functions */
#include "../Mrt/mrt.h"

/* needed for GMT_TIME_STAMP and ASCII_MESSAGES flags */
#include "../site_defaults.h"
#include <netinet/in.h>
#include <sys/socket.h>

/* needed for accessing bgpmon listening addr and port */
#include "../Clients/clientscontrol.h"

/* needed for interanl functions */
#include "xmlinternal.h"

#include "xmldata.h"

//#define DEBUG


/* constant */
static char* _VERSION = XFB_VERSION;  /* The implemented XFB version,    specified in xmldata.h */
static char* _XMLNS   = XFB_NS;       /* The implemented XFB name space, specified in xmldata.h */

/* buffer used for xml conversion */
static char _XML[XML_BUFFER_LEN];    /* Global xml buffer, XML_BUFFER_LEN defined in xmlinternal.h */
static int  _XML_LEN_DIGITS = 0;     /* Global variable, number of digits for XML message length,
                                        would be calculated by ceil(log10(XML_BUFFER_LEN)) 
                                      */

/* for improving perfomance */
//static xmlNodePtr static_marker_node = NULL;

/* placeholder struct for STATE report data, only for internal use */
typedef struct
{
    long current;
    long avg;
    long sdv;
    long min;
    long max;
    long accu;
    long limit;
} stat_data_t; 

/* placeholder struct for TIME report data, only for internal use */
typedef struct
{
    long current;
    long first_startup;
    long first_down;
    long last_startup;
    long last_down;
    long first_action;
    long last_action;
} time_data_t; 

/*----------------------------------------------------------------------------------------
 * Purpose: add an afi child with value attribute to an xml node
 * input:   node  - pointer to the xml node
 *          afi   - an afi number
 * Output:  the xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * Jason Bartlett @ 14 Oct 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
xmlNewChildAFI(xmlNodePtr node, int afi)
{
    char *afi_str   = ""; /* null string */

    switch(afi)
    {
        case BGP_AFI_IPv4:  afi_str = "IPV4";  break;
        case BGP_AFI_IPv6:  afi_str = "IPV6";  break;
        default:            afi_str = "OTHER"; break;
    }

    xmlNodePtr afi_node = xmlNewChildString(node,"AFI",afi_str);    //human-readable
    xmlNewPropInt(afi_node,"value",afi);                            //machine-readable

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: add a safi child with value attribute to an xml node
 * input:   node  - pointer to the xml node
 *          safi  - a  safi number
 * Output:  the xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * Jason Bartlett @ 14 Oct 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
xmlNewChildSAFI(xmlNodePtr node, int safi)
{
    char *safi_str   = ""; /* null string */

    switch(safi)
    {
        case BGP_MP_SAFI_UNICAST:    safi_str = "UNICAST";   break;
        case BGP_MP_SAFI_MULTICAST:  safi_str = "MULTICAST"; break;
        case BGP_MP_SAFI_MPLS:       safi_str = "MPLS";      break;
        case BGP_MP_SAFI_ENCAP:      safi_str = "ENCAPSULATION";  break;
        default: safi_str = "OTHER";     break;
    }

    xmlNodePtr safi_node = xmlNewChildString(node,"SAFI",safi_str); //human readable
    xmlNewPropInt(safi_node,"value",safi);                          //machine readable

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate a xml node with a BGPID
 * input:   tag   - tag string
 *          ip    - a long long integer for ip address
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 *
 * Ex:
 *     <BGPID>128.223.51.102</BGPID>
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
xmlNewNodeBGPID(char *tag, long long ip)
{
    static char buff[XML_TEMP_BUFFER_LEN];
    memset(buff, 0, XML_TEMP_BUFFER_LEN);
    buff[0] = '\0';

    if (ip > LONG_MAX) { inet_ntop(AF_INET6, &ip, buff, XML_TEMP_BUFFER_LEN); } /* >  4 bytes */
    else               { inet_ntop(AF_INET,  &ip, buff, XML_TEMP_BUFFER_LEN); } /* <= 4 bytes */

    return xmlNewNodeString(tag, buff);
}

/*----------------------------------------------------------------------------------------
 * Purpose: add a BGPID child node to a xml node
 * input:   parent_node - parent xml node
 *          tag   - tag string for the child node
 *          ip    - a "long long" integer ip for the child node
 * Output:  the child node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
xmlNewChildBGPID(xmlNodePtr parent_node, char *tag, long long ip)
{
    return xmlAddChild(parent_node, xmlNewNodeBGPID(tag, ip));
}


/*----------------------------------------------------------------------------------------
 * Purpose: generate a xml node with general network address, could be v4 or v6
 * input:   tag  - tag string
 *          addr - pointer to the address data
 *          len  - length of the address data
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
xmlNewNodeNetAddr(char *tag, u_char *addr, int len)
{
    char *stag = "";
    int  i;

    static char netaddr_str[XML_TEMP_BUFFER_LEN];
    static char str[XML_TEMP_BUFFER_LEN];
    netaddr_str[0] = '\0';

    for ( i = 0; i < len; i++ )
    {
        snprintf(str, XML_TEMP_BUFFER_LEN, "%d", addr[i]);
        strcat(netaddr_str, stag); // separator
        strcat(netaddr_str, str);
        stag = ".";
    }

   return xmlNewNodeString( tag, netaddr_str );
}

/*----------------------------------------------------------------------------------------
 * Purpose: add a network address child node to a xml node
 * input:   parent_node - parent xml node
 *          addr - pointer to the address data
 *          len  - length of the address data
 * Output:  the child node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
xmlNewChildNetAddr(xmlNodePtr parent_node, char *tag, u_char *addr, int len)
{
    return xmlAddChild(parent_node, xmlNewNodeNetAddr(tag, addr, len));
}

/*----------------------------------------------------------------------------------------
 * Purpose: add PREFIX child nodes to a xml node
 * input:   parent_node - parent xml node
 *          prefix - pointer to the prefix
 *          len    - length of prefix
 *          afi    - address family of the prefix
 *          safi   - sub address family of the prefix
 *          lt     - pointer to an array of labels. it could be NULL.
 * Output:  the child node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 14 Oct 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
xmlNewChildPrefixes(xmlNodePtr parent_node, u_char *prefix, int len, u_int16_t afi, u_int8_t safi, u_char **lt )
{
	xmlNodePtr prefix_node = NULL;

	//char *stag = "";
	int i = 0, l = 0;
	int bits = 0;
	static char prefix_str[XML_TEMP_BUFFER_LEN];
	static char str[XML_TEMP_BUFFER_LEN];
	
	u_int8_t  prefix_value[16];


	for ( i = 0; i < len; i = i + 1 + l )
	{
	        /* Get prefix string */
	        prefix_str[0] = '\0';
	        bits = prefix[i];
	        l = (bits + 7)/8;
		memset(prefix_value, 0, 16);
		memcpy(prefix_value, &prefix[i+1], l);	
		// IPv4 
		if (afi == 1)
		{
			if( inet_ntop(AF_INET, prefix_value, str, ADDR_MAX_CHARS) == NULL )
			{
				log_err("xmlNewChildPrefixes, could not convert IPv4 prefix");
				strcat(prefix_str, "0");
			}
		}
		else // IPv6
		if (afi == 2)
		{
			if( inet_ntop(AF_INET6, prefix_value, str, ADDR_MAX_CHARS) == NULL )
			{
				log_err("xmlNewChildPrefixes, could not convert IPv6 prefix");
				strcat(prefix_str, "0");
			}
		}
	        else
		{
			strcat(prefix_str, "0");
		}	
        	snprintf(prefix_str , XML_TEMP_BUFFER_LEN, "%s", str );
	        snprintf(prefix_str+strlen(str) , XML_TEMP_BUFFER_LEN, "/%d", bits );

        	/* generate prefix node */
	        prefix_node = xmlNewNode(NULL,BAD_CAST "PREFIX");
        	xmlNewChild(prefix_node,NULL,BAD_CAST "ADDRESS",BAD_CAST prefix_str);

	        /* add label */
        	if (*lt != NULL) // add label
	        {
			int label = **lt;
			*lt = *lt + 1;
			char *label_str;

			switch (label)
			{
				case BGPMON_LABEL_NULL:                label_str = "NULL";    break;
				case BGPMON_LABEL_WITHDRAW:            label_str = "WITH";    break;
				case BGPMON_LABEL_WITHDRAW_DUPLICATE:  label_str = "DUPW";    break;
				case BGPMON_LABEL_ANNOUNCE_NEW:        label_str = "NANN";    break;
				case BGPMON_LABEL_ANNOUNCE_DUPLICATE:  label_str = "DANN";    break;
				case BGPMON_LABEL_ANNOUNCE_DPATH:      label_str = "DPATH";   break;
				case BGPMON_LABEL_ANNOUNCE_SPATH:      label_str = "SPATH";   break;
				default: label_str = "UNKNOWN"; break;
			}
			xmlNewProp(prefix_node, BAD_CAST "label", BAD_CAST label_str);
		}
		xmlNewChildAFI(prefix_node,  afi);
		xmlNewChildSAFI(prefix_node, safi);
		xmlAddChild(parent_node, prefix_node);
	}

	return prefix_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate a xml node for state report 
 * input:   tag       - tag string
 *          stat_data - stat_data structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
xmlNewNodeStat(char *tag, stat_data_t *stat_data)
{
    xmlNodePtr node = NULL;

    /* Current value */
    node = xmlNewNodeInt(tag, stat_data->current);

    /* Currently only report the max and limit value */
    xmlNewPropInt(node, "max", stat_data->max);
    xmlNewPropInt(node, "limit", stat_data->limit);

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: add a child a xml node for state report 
 * input:   tag       - tag string
 *          stat_data - stat_data structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
inline xmlNodePtr
xmlNewChildStat(xmlNodePtr parent_node,  char *tag, stat_data_t *stat_data)
{
    return xmlAddChild(parent_node, xmlNewNodeStat(tag, stat_data));
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate a xml node for time report 
 * input:   tag       - tag string
 *          time_data - time_data structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
xmlNewNodeTime(char *tag, time_data_t *time_data)
{
    xmlNodePtr node = NULL;

    /* Current value */
    node = xmlNewNodeInt(tag, time_data->current);

    /* Currently only report the last_down and last_action time */
    xmlNewPropInt(node, "last_down",   time_data->last_down);
    xmlNewPropInt(node, "last_action", time_data->last_action);

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: add a child xml node for time report 
 * input:   tag       - tag string
 *          time_data - time_data structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
inline xmlNodePtr
xmlNewChildTime(xmlNodePtr parent_node,  char *tag, time_data_t *time_data)
{
    return xmlAddChild(parent_node, xmlNewNodeTime(tag, time_data));
}


/*----------------------------------------------------------------------------------------
 * Purpose: generate the OPT_PARAMETER node with child AUTHENTICATION/CAPABILITY node
 * input:    parms - parameters           
             len   - number of parameters 
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * Jason Bartlett @ 14 Oct 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpOpenOptionalParameterNode(u_char *parms, int len )
{

    xmlNodePtr node       = NULL;

    if ( len > 0 )
    {
        /* OPT_PAR Node */
        node = xmlNewNode(NULL, BAD_CAST "OPT_PAR");

        /* Child nodes */
        int i;
        int l = 0;
        int type;
        for ( i = 0; i<len ; i = i + 2 + l )
        {
            xmlNodePtr par_node       = xmlNewNode(NULL, BAD_CAST "PARAMETER");
            xmlNodePtr cap_node       = NULL;

            type = parms[i];
            l    = parms[i+1];


            switch ( type )
            {
                case BGP_OPEN_OPT_AUTH: // 1: authentication
                {
                    char *atag = "AUTHENTICATION";
                    xmlNewChildString(par_node,"TYPE",atag);
                    xmlNewChildOctets(par_node,"DATA",parms + i + 2,l - 2);
                    //Actual authentication data is added as octets
                    break;
                }
                case BGP_OPEN_OPT_CAP:  // 2: capabilities
                {
                    char *atag = "CAPABILITIES";
                    xmlNewChildString(par_node,"TYPE",atag);
                    cap_node  = xmlAddChild(par_node, xmlNewNode(NULL, BAD_CAST "CAP"));

                    int capType = parms[i+1+1];
                    int capl    = parms[i+1+1+1];
                    switch ( capType ) /* Defined in bgppacket.h */
                    {
                        case multiprotocolCapability:
                        {
                            /* MULTIPROTOCOL */
                            char *atag = "MULTIPROTOCOL";
                            xmlNewChildString(cap_node,"TYPE",atag);

                            xmlNodePtr mp_node = xmlNewChildString(cap_node, "MULTIPROTOCOL", "");
                            u_int16_t afi = ntohs( *((u_int16_t *) (parms+i+2+2) ) );
                            u_int8_t safi = parms[i+5+2];
                            xmlNewChildUnsignedInt(mp_node,"AFI",afi);
                            xmlNewChildInt(mp_node,"SAFI",safi);
                            break;
                        }
                        case routeRefreshCapability:
                        {
                            /* ROUTE_REFRESH */
                            char *atag = "ROUTE_REFRESH";
                            xmlNewChildString(cap_node,"TYPE",atag);
                            xmlNewChildString(cap_node, "ROUTE_REFRESH", "");
                            break;
                        }
                        // [EXTEND] additional capabilities
                        default:
                        {
                            char *atag = "OTHER";
                            xmlNewChildString(cap_node,"TYPE",atag);
                            xmlNodePtr unknown_node = xmlNewChildString(cap_node, "OTHER", "");
                            if ( capl > 0 )
                            {
                                xmlNewChildOctets(unknown_node, "OCTETS", (parms+i+2+2), capl);
                            }
                            break;
                        }
                    }
                    xmlNewPropInt(cap_node, "value", capType);
                    break;
                }
                default:
                {
                    char *atag = "UNKNOWN";
                    xmlNewChildString(par_node,"TYPE",atag);
                    xmlNodePtr unknown_node = xmlNewChildString(par_node, "UNKNOWN", "");
                    if ( l > 0 )
                    {
                        xmlNewChildOctets(unknown_node, "OCTETS", (parms+i+1+1), l);
                    }
                    log_err("Unknown open parameter type!");
                    break;
                }
            }
            xmlNewPropInt(par_node, "value", type);

        }
    }

    return node;
}


/*----------------------------------------------------------------------------------------
 * Purpose: generate the WITHDRAWN node with child PREFIX nodes
 * input:   prefix  - pointer to the first prefix node
 *          afi     - afi value
 *          safi    - safi value
 *          lt      - pointer to an array of labels. it could be NULL
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genUpdateWithdrawnNode(u_char *prefix, int len, u_int16_t afi, u_int8_t safi, u_char **lt )
{
    xmlNodePtr child_node = NULL;
    int count = 0;
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "WITHDRAWN");
    xmlNewChildPrefixes(node, prefix, len, afi, safi, lt);
    if(node == NULL){
      log_err("genUpdateWithdrawnNode: possible memory error\n");
      return NULL;
    }
    for (child_node=node->children; child_node!= NULL; child_node=child_node->next)
    {
        if (child_node->name && (strcmp((char*)child_node->name,"PREFIX") == 0))
        {
            count++;
        }
    }
    /* update the count of prefixes */
    xmlNewPropInt(node, "count", count);

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the NLRI node with child PREFIX nodes
 * input:   prefix  - pointer to the first prefix node
 *          afi     - afi value
 *          safi    - safi value
 *          lt      - pointer to an array of labels. it could be NULL
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genUpdateNlriNode(u_char *prefix, int len, u_int16_t afi, u_int8_t safi, u_char **lt )
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "NLRI");
    xmlNewChildPrefixes(node, prefix, len, afi, safi, lt);
    xmlNodePtr child_node = NULL;
    int count = 0;
    for (child_node=node->children; child_node!= NULL; child_node=child_node->next)
    {
        if (child_node->name && (strcmp((char*)child_node->name,"PREFIX") == 0))
        {
            count++;
        }
    }
    /* update the count of prefixes */
    xmlNewPropInt(node, "count", count);
    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the flag node for an attribute
 * input:   flags   - flags in an interger
 * Output:  the new xml node with indicated flags as boolean attributes
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 15 Sep 2010
 *
 * Ex:  <FLAGS optional="TRUE" transitive="TRUE"/>
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genAttributeFlagNode(int flags)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "FLAGS");
    if ( (flags & BGP_ATTR_FLAG_OPTIONAL) > 0 ) {
        xmlNewProp(node,BAD_CAST "optional",BAD_CAST "TRUE");
    }
    if ( (flags & BGP_ATTR_FLAG_TRANS)    > 0 ) { 
        xmlNewProp(node,BAD_CAST "transitive",BAD_CAST "TRUE");
    }
    if ( (flags & BGP_ATTR_FLAG_PARTIAL)  > 0 ) {   
        xmlNewProp(node,BAD_CAST "partial",BAD_CAST "TRUE");
    }
    if ( (flags & BGP_ATTR_FLAG_EXT_LEN) > 0 ) {
        xmlNewProp(node,BAD_CAST "extended",BAD_CAST "TRUE");
    }

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the ORIGIN attribute node
 * input:   origin  - origin value in an interger
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 14 Oct 2010
 *
 * Ex:  <ORIGIN value="0">IGP</ORIGIN>
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpOriginNode(BMF bmf, int origin)
{
    char *otag;
    int strlen = XML_TEMP_BUFFER_LEN;
    char str[strlen];

    snprintf( str, strlen, "%d", origin );
    switch ( origin )
    {
        case BGP_ORIGIN_IGP:
            otag = "IGP";
            break;
        case BGP_ORIGIN_EGP:
            otag = "EGP";
            break;
        case BGP_ORIGIN_INCOMPLETE:
            otag = "INCOMPLETE";
            break;
        default:
            otag = "OTHER";
            break;
    }
    xmlNodePtr node = xmlNewNode(NULL,BAD_CAST "ORIGIN");
    xmlNodeAddContent(node,BAD_CAST otag);
    xmlNewPropInt(node, "value", origin);

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the AS_SEG node
 * input:   value  - pointer to AS Segment data
 *          as_len - length of single AS number in bytes
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpASSegNode(u_char *value, int as_len)
{
  xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "AS_SEG");

  static char str[XML_TEMP_BUFFER_LEN]; /* buffer for one single AS number */

  /* For each AS Segment */
  char *tag = NULL;
  int type = value[0];  /* Segment type */
  int l    = value[1];  /* Segment length */
  int i;
  str[0] = '\0';

  /* type */
  if      ( type == 1 ) tag = "AS_SET";
  else if ( type == 2 ) tag = "AS_SEQUENCE";
  else if ( type == 3 ) tag = "AS_CONFED_SEQUENCE";
  else if ( type == 4 ) tag = "AS_CONFED_SET";
  else                  tag = "OTHER";

  xmlNewPropString(node,  "type",   tag);
  xmlNewPropInt(node,     "length", l);

    /* AS */
  for ( i = 0 ; i < l ; i = i + 1 ){
    int as = 0;
    u_char *as_value = value + 2 + i*as_len;
    if (as_len == 4){
      as += ntohl(*((u_int32_t *) (as_value))); /* 4-byte AS */
    }else{
      as += ntohs(*((u_int16_t *) (as_value))); /* 2-byte AS */
    }

    if(as >> 16){
      sprintf(str, "%d", as);                                    /* PLAIN presentation */
    }else{
      sprintf(str, "%d", as);
    }

    xmlNewChildString(node, "AS", str);
  }

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the AS_PATH attribute node
 * input:   value - pointer to AS path data
 *          len - length of AS path data
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 *
 * Ex:
 *      <AS_PATH>
 *        <AS_SEG type="AS_SEQUENCE" length="4">
 *          <AS>286</AS>
 *          <AS>3549</AS>
 *          <AS>6453</AS>
 *          <AS>9583</AS>
 *        </AS_SEG>
 *        <AS_SEG type="AS_SET" length="1">
 *          <AS>10380</AS>
 *        </AS_SEG>
 *      </AS_PATH>
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpASPathNode(u_char *value, int len, int asn_len)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "AS_PATH");

    int index = 0;
    while (index < len)
    {
        /* For each AS Segment */
        int l    = value[index + 1];  /* Segment length */

        xmlAddChild(node, genBgpASSegNode(value + index, asn_len));

        /* move the index to the start of next AS Segment */
        index = index + 2 + l*asn_len;
    }

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the AS_PATH attribute node
 * input:   value - pointer to AS path data
 *          len - length of AS path data
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpAS4PathNode(u_char *value, int len )
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "AS4_PATH");

    int index = 0;
    while (index < len)
    {
        /* For each AS Segment */
        int l    = value[index + 1];  /* Segment length */

        xmlAddChild(node, genBgpASSegNode(value + index, 4)); /* 4-byte AS */

        /* move the index to the start of next AS Segment */
        index = index + 2 + l*4;
    }

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the COMMUNITIES attribute node
 * input:   list - pointer to a list of BGP communities
 *          len - length of BGP communities
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 14 Oct 2010
 *
 * Ex:
 *  <COMMUNITIES>
 *     <COMMUNITY><AS>286</AS><VALUE>19</VALUE>
 *     <COMMUNITY><AS>286</AS><VALUE>29</VALUE>
 *  </COMMUNITIES>
 *
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpCommunitiesNode(BMF bmf, u_char *list, int len )
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "COMMUNITIES");

    char *ctag = "COMMUNITY";
    char *rtag = "RESERVED_COMMUNITY";
    char *atag = "AS";
    char *vtag = "VALUE";
    int i;

    for ( i = 0; i < len; i = i + 4 )
    {
        u_int16_t as  = ntohs( *((u_int16_t *) list));
        u_int16_t val = ntohs(*((u_int16_t *) (list+2)));

        if ( as == 0xFFFF && val ==  0xFF01 )
        {
            xmlAddChild(node, xmlNewNode(NULL, BAD_CAST "NO_EXPORT"));
        }
        else if ( as == 0xFFFF && val == 0xFF02 )
        {
            xmlAddChild(node, xmlNewNode(NULL, BAD_CAST "NO_ADVERTISE"));
        }
        else if ( as == 0xFFFF && val == 0xFF03 )
        {
            xmlAddChild(node, xmlNewNode(NULL, BAD_CAST "NO_EXPORT_SUBCONFED"));
        }
        else if ( as == 0x0000 || as == 0xFFFF )
        {
            xmlNodePtr child_node = xmlNewNode(NULL, BAD_CAST rtag);
            xmlNewChildInt(child_node, atag, as);
            xmlNewChildInt(child_node, vtag, val);
            xmlAddChild(node, child_node);
        }
        else
        {
            xmlNodePtr child_node = xmlNewNode(NULL, BAD_CAST ctag);
            xmlNewChildInt(child_node, atag, as);
            xmlNewChildInt(child_node, vtag, val);
            xmlAddChild(node, child_node);
        }
        list = list+4;
    }

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the CLUSTER_LIST attribute node
 * input:   value - pointer to cluster list data
 *          len - length of cluster list data
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpClusterListNode(u_char *value, int len )
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "CLUSTER_LIST");

    int index = 0;
    while (index < len)
    {
        /* Each ID is a 4-byte IP */
        xmlNewChildIP(node, "ID", *((u_int32_t *) value + index ));

        /* Move the index to the next cluster id */
        index = index + 32; /* advance the index by 4-byte */
    }

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the MP_REACH_NLRI attribute node
 * input:   attr - pointer to mpreach attribute
 *          len  - length of mpreach attribute
 *          lt   - pointer to an array of labels. it could be NULL
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 21 Jul 2011
 *
 * Ex:
 *  <MP_REACH_NLRI>
 *      <AFI value="2">IPV6</AFI>
 *      <SAFI value="1">UNICAST</SAFI>
 *      <NEXT_HOP_LEN>16</NEXT_HOP_LEN>
 *      <NEXT_HOP>2610:38:1::1</NEXT_HOP>
 *      <NLRI count="1">
 *        <PREFIX label="NANN">
 *	   <ADDRESS>2001:12ac::/32</ADDRESS>
 *	   <AFI value="2">IPV6</AFI>
 *	   <SAFI value="1">UNICAST</SAFI>
 *	  </PREFIX>
 *       </NLRI>
 *   </MP_REACH_NLRI>
 *
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpMPReachNode(u_char *attr, int len, u_char **lt)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "MP_REACH_NLRI");

    u_int16_t afi =  ntohs(*((u_int16_t *) attr));
    u_int8_t safi =  *((u_int8_t *) (attr+2));

    /* afi / safi */
    xmlNewChildAFI(node,afi);
    xmlNewChildSAFI(node,safi);

    /* next hop */
    int nhlen = *(u_int8_t *) (attr+3);

    xmlNewChildInt(node,"NEXT_HOP_LEN",nhlen);

    xmlNodePtr nh_node = xmlNewNode(NULL,BAD_CAST "NEXT_HOP");

    // get and convert ip address
    static char str[XML_TEMP_BUFFER_LEN];

    if( !(afi == 1 || afi == 2) ){  //currently we only support IPv4 and IPv6
        xmlNewChildOctets(nh_node,"OCTETS",attr+4,nhlen);
    }
    else{
        if(afi == 1){   //we're dealing with one (or more) v4 address(es)
            int i = 0;
            u_int8_t ip_value[4];   //initialize empty buffer for IPv4 address
            for(;i < nhlen;i+=4){
                memset(ip_value,0,4);
                memcpy(ip_value,&attr[4+i],4);  //copy IPv4 address to buffer
	            if( inet_ntop(AF_INET, ip_value, str, ADDR_MAX_CHARS) == NULL )
	            {
		            log_err("genBgpMPReachNode, could not convert IPv4 address");
		            strcat(str, "0");
	            }
                xmlNewChildString(nh_node,"ADDRESS", str);
            }
        }
        else if(afi == 2){  //otherwise we're dealing with v6 address(es)
            int i = 0;
            u_int8_t ip_value[16];   //initialize empty buffer for IPv6 address
            for(;i < nhlen;i+=16){
                memset(ip_value,0,16);
                memcpy(ip_value,&attr[4+i],16);  //copy IPv6 address to buffer
	            if( inet_ntop(AF_INET6, ip_value, str, ADDR_MAX_CHARS) == NULL )
	            {
		            log_err("genBgpMPReachNode, could not convert IPv6 address");
		            strcat(str, "0");
	            }
                xmlNewChildString(nh_node,"ADDRESS", str);
            }
        }
    }

/*    //BGPmon (as of 7.2) supports IPv4, IPv6, and multi-IPv6 next-hop addresses
    //any unrecognized next-hop address or protocol must be preserved
    if( !(nhlen == 4 || nhlen == 16 || nhlen == 32) || !(afi == 1 || afi == 2) ){
        xmlNodePtr nh_oct = xmlNewChild(node,NULL,BAD_CAST "NEXT_HOP",NULL);
        xmlNewChildOctets(nh_oct,"OCTETS",attr+4,nhlen);
    }
    else{
        // get and convert ip address
        static char str[XML_TEMP_BUFFER_LEN];
	
        //initialize buffer to store 1 v4 address or 1 or 2 v6 address(es)
        u_int8_t ip_value[33];
        memset(ip_value, 0, 33);
        memcpy(ip_value, &attr[4], nhlen);

        // IPv4 
        if (afi == 1)
        {
	        if( inet_ntop(AF_INET, ip_value, str, ADDR_MAX_CHARS) == NULL )
	        {
		        log_err("genBgpMPReachNode, could not convert IPv4 address");
		        strcat(str, "0");
	        }
            xmlNewChildString(node,"NEXT_HOP", str);
        }
        else{ // IPv6
            if (afi == 2)
            {
                //if we only get 1 v6 address, convert it and go on
                if(nhlen == 16){
	                if( inet_ntop(AF_INET6, ip_value, str, ADDR_MAX_CHARS) == NULL )
	                {
		                log_err("genBgpMPReachNode, could not convert IPv6 address");
		                strcat(str, "0");
	                }
                    xmlNewChildString(node,"NEXT_HOP", str);
                }
                //otherwise we got 2 addresses, so convert them both
                else{
                    //create "root" next_hop node to add child addresses to.
                    xmlNodePtr nh_node = xmlNewChild(node,NULL,BAD_CAST "NEXT_HOP",NULL);
                    //split up ip_value so we can pass each part through inet_ntop
                    u_int8_t ip6_value1[17];
                    u_int8_t ip6_value2[17];
                    memset(ip6_value1,0,17);
                    memset(ip6_value2,0,17);
                    memcpy(ip6_value1,ip_value,16);
                    memcpy(ip6_value2,ip_value+16,16);
                    if( inet_ntop(AF_INET6, ip6_value1, str, ADDR_MAX_CHARS) == NULL )
	                {
		                log_err("genBgpMPReachNode, could not convert global IPv6 address");
		                strcat(str, "0");
	                }
                    xmlNewChildString(nh_node,"ADDRESS",str);
	                if( inet_ntop(AF_INET6, ip6_value2, str, ADDR_MAX_CHARS) == NULL )
	                {
		                log_err("genBgpMPReachNode, could not convert link-local IPv6 address");
		                strcat(str, "0");
	                }
                    xmlNewChildString(nh_node,"ADDRESS",str);
                }
            }
        }
    }*/

    //add NEXT_HOP node to MP_REACH_NODE
    xmlAddChild(node,nh_node);

    //Create pointer to start of NLRI data
    u_char *nlri;
    nlri = attr + 5 + nhlen;    //5 = 2B for AFI, 1B for SAFI, 1B for NH_LEN, 1B for SNPA/Reserved 0

    /* nlri */
    xmlAddChild(node, genUpdateNlriNode(nlri, len-5-nhlen, afi, safi, lt));

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the MP_UNREACH_NLRI attribute node
 * input:   attr - pointer to mpreach attribute
 *          len  - length of mpreach attribute
 *          lt   - pointer to an array of labels. it could be NULL
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 14 Oct 2010
 *
 * Ex:
 *
 *  <MP_UNREACH_NLRI>
 *     <AFI value="1">IPV4</AFI>
 *     <SAFI value="2">MULTICAST</SAFI>
 *     <WITHDRAWN count="1">
 *       <PREFIX label="NULL"><AFI value="1">IPV4</AFI><SAFI value="2">MULTICAST</SAFI>208.28.235/24</PREFIX>
 *     </WITHDRAWN>
 *  </MP_UNREACH_NLRI>
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpMPUnreachNode(u_char *attr, int len, u_char **lt)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "MP_UNREACH_NLRI");

    u_int16_t afi =  ntohs(*((u_int16_t *) attr));
    u_int8_t safi =  *((u_int8_t *) (attr+2));

    /* afi / safi */
    node = xmlNewChildAFI(node,afi);
    node = xmlNewChildSAFI(node,safi);

    /* withdrawn */
    xmlAddChild(node, genUpdateWithdrawnNode(attr+3, len-3, afi, safi, lt));

    return node;
}

/*---------------------------------------------------------------------------------------
 * Purpose: generate an Extended Communities node
 * input:   value - a pointer to a list of extended community values
 *          len - the length of the attribute
 * output:  a node containing some representation of extended communities
 * Jason Bartlett @ 14 Oct 2010
 *--------------------------------------------------------------------------------------*/
xmlNodePtr genBgpExtCommunitiesNode(u_char *value,int len){
    xmlNodePtr top_node = xmlNewNode(NULL,BAD_CAST "EXTENDED_COMMUNITIES");
    xmlNodePtr x_com_node;
    int i;

    //Extended Communities are defined as 8-octet fields
    for(i = 0; i < len; i += 8){
        x_com_node = xmlNewNode(NULL,BAD_CAST "EXT_COM");

        u_char type_h = value[i];   //first octet defines type field
        u_char type_l = value[i + 1];   //second octet defines subtype field

        switch(type_h){
            case 0x00:      //these cases are defined in RFC 4360
            {               //as 2-Octet AS_Specific Extended Communities
                xmlNewPropString(x_com_node,"transitive","TRUE");
                xmlNewChildString(x_com_node,"TYPE","2-OCTET AS-SPECIFIC EXT COM");
                switch(type_l){
                case 0x02:
                    {
                        xmlNewChildString(x_com_node,"SUBTYPE","ROUTE TARGET");
                        break;
                    }
                case 0x03:
                    {
                        xmlNewChildString(x_com_node,"SUBTYPE","ROUTE ORIGIN");
                        break;
                    }
                default:
                    {
                        xmlNewChildString(x_com_node,"SUBTYPE","UNKNOWN");
                        break;
                    }
                }
                u_int16_t as = value[i + 2]<<8 | value[i + 3];  //2-octet field for an IANA-assigned AS
                xmlNewChildInt(x_com_node,"AS_NUM",as);
                //value field is defined by Local Administrator and so is preserved in binary
                u_char data[] = {value[i + 4],value[i + 5],value[i + 6],value[i + 7]};
                xmlNewChildOctets(x_com_node,"VALUE",data,sizeof(data));
                break;
            }
            case 0x40:      
            {
                xmlNewChildString(x_com_node,"TYPE","2-OCTET AS-SPECIFIC EXT COM");
                switch(type_l){
                default:
                    {
                        xmlNewChildString(x_com_node,"SUBTYPE","UNKNOWN");
                        break;
                    }
                }
                u_int16_t as = value[i + 2]<<8 | value[i + 3];  //2-octet field for an IANA-assigned AS
                xmlNewChildInt(x_com_node,"AS_NUM",as);
                //value field is defined by Local Administrator and so is preserved in binary
                u_char data[] = {value[i + 4],value[i + 5],value[i + 6],value[i + 7]};
                xmlNewChildOctets(x_com_node,"VALUE",data,sizeof(data));
                break;
            }
            case 0x01:      //these types are defined as
            {               //IPv4 Address Specific Extended Communities
                xmlNewPropString(x_com_node,"transitive","TRUE");
                xmlNewChildString(x_com_node,"TYPE","IPV4 ADDRESS SPECIFIC EXT COM");
                switch(type_l){
                case 0x02:
                    {
                        xmlNewChildString(x_com_node,"SUBTYPE","ROUTE TARGET");
                        break;
                    }
                case 0x03:
                    {
                        xmlNewChildString(x_com_node,"SUBTYPE","ROUTE ORIGIN");
                        break;
                    }
                default:
                    {
                        xmlNewChildString(x_com_node,"SUBTYPE","UNKNOWN");
                        break;
                    }
                }
                //4-octet field for an assigned IPv4 address
                u_int32_t v4 = value[i + 2]<<24 | value[i + 3]<<16 | value[i + 4]<<8 | value[i + 5];
                xmlNewChildIP(x_com_node,"IPV4_ADDR",v4);
                //Local Administrator field is preserved as binary
                u_char data[] = {value[i + 6],value[i + 7]};
                xmlNewChildOctets(x_com_node,"VALUE",data,sizeof(data));
                break;
            }
            case 0x41:      
            {
                xmlNewChildString(x_com_node,"TYPE","IPV4 ADDRESS SPECIFIC EXT COM");
                switch(type_l){
                default:
                    {
                        xmlNewChildString(x_com_node,"SUBTYPE","UNKNOWN");
                        break;
                    }
                }
                //4-octet field for an assigned IPv4 address
                u_int32_t v4 = value[i + 2]<<24 | value[i + 3]<<16 | value[i + 4]<<8 | value[i + 5];
                xmlNewChildIP(x_com_node,"IPV4_ADDR",v4);
                //Local Administrator field is preserved as binary
                u_char data[] = {value[i + 6],value[i + 7]};
                xmlNewChildOctets(x_com_node,"VALUE",data,sizeof(data));
                break;
            }
            case 0x03:      //these types are defined
            {               //as Opaque Extended Communities
                xmlNewPropString(x_com_node,"transitive","TRUE");
                xmlNewChildString(x_com_node,"TYPE","OPAQUE EXTENDED COMMUNITY");
                //BGP Encapsulation Ext. Com. defined in RFC 5512, additional tunnel types in RFC 5566
                switch(type_l){
                    case 0x0c:
                    {
                        xmlNewChildString(x_com_node,"SUBTYPE","BGP ENCAPSULATION EXTENDED COMMUNITY");
                        u_int16_t tunnel_type = value[i + 6]<<8 | value[i + 7];
                        switch(tunnel_type){
                            case ENCAP_L2TPV3_IP:     xmlNewChildString(x_com_node,"VALUE","L2TPV3");     break;
                            case ENCAP_GRE:           xmlNewChildString(x_com_node,"VALUE","GRE");        break;
                            case ENCAP_IP_IN_IP:      xmlNewChildString(x_com_node,"VALUE","IP IN IP");   break;
                            case ENCAP_TRANS_END:       xmlNewChildString(x_com_node,"VALUE","TRANSMIT TUNNEL ENDPOINT");   break;
                            case ENCAP_IPSEC_TUN:       xmlNewChildString(x_com_node,"VALUE","IPSEC IN TUNNEL MODE");       break;
                            case ENCAP_IP_IP_IPSEC:     xmlNewChildString(x_com_node,"VALUE","IP IN IP TUNNEL WITH IPSEC"); break;
                            case ENCAP_MPLS_IP:     xmlNewChildString(x_com_node,"VALUE","MPLS IN IP WITH IPSEC");      break;
                            default:                xmlNewChildString(x_com_node,"VALUE","UNKNOWN");    break;
                        }
                    }
                    default:
                    {
                        //subtype is to be assigned by IANA
                        xmlNewChildInt(x_com_node,"SUBTYPE",(u_int8_t)type_l);
                        //value field is preserved in binary
                        u_char data[] = {value[i + 2],value[i + 3],value[i + 4],value[i + 5],value[i + 6],value[i + 7]};
                        xmlNewChildOctets(x_com_node,"VALUE",data,sizeof(data));
                        break;
                    }
                }
                break;
            }
            case 0x43:      
            {
                xmlNewChildString(x_com_node,"TYPE","OPAQUE EXTENDED COMMUNITY");
                //subtype is to be assigned by IANA
                xmlNewChildInt(x_com_node,"SUBTYPE",(u_int8_t)type_l);
                //value field is preserved in binary
                u_char data[] = {value[i + 2],value[i + 3],value[i + 4],value[i + 5],value[i + 6],value[i + 7]};
                xmlNewChildOctets(x_com_node,"VALUE",data,sizeof(data));
                break;
            }
            //additional types may be defined
            default:
            {
                xmlNewChildString(x_com_node,"TYPE","UNKNOWN EXTENDED COMMUNITY");
                xmlNewChildInt(x_com_node,"SUBTYPE",(u_int8_t)type_l);
                //regardless, preserve value field
                u_char data[] = {value[i + 2],value[i + 3],value[i + 4],value[i + 5],value[i + 6],value[i + 7]};
                xmlNewChildOctets(x_com_node,"VALUE",data,sizeof(data));
                break;
            }
        }
        xmlAddChild(top_node,x_com_node);
    }
    return top_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate a TUNNEL_ENCAP node
 * input:   input - a pointer to the attribute
 *          len - the length of the attribute
 * output:  the new TUNNEL_ENCAP node
 * Jason Bartlett @ 18 Oct 2010
 *--------------------------------------------------------------------------------------*/
xmlNodePtr genTunnelEncapNode(u_char* input,int len){

    xmlNodePtr encap_node = xmlNewNode(NULL, BAD_CAST "TUNNEL_ENCAP");

    int base_pos = 0;   //index in the full attribute
    u_int16_t curr_length;  //length of value field of current top-level TLV
    u_int16_t type;     //type of current top-level TLV

    //step through each top-level TLV
    while(base_pos < len){
        xmlNodePtr top_tlv = xmlNewNode(NULL,BAD_CAST "ENCAP");

        //generate type child node
        type = input[base_pos + 0]<<8 | input[base_pos + 1];
        char *type_tag = "";
        switch(type){
            case ENCAP_L2TPV3_IP:     type_tag = "L2TPV3 OVER IP";  break;
            case ENCAP_GRE:           type_tag = "GRE";             break;
            case ENCAP_IP_IN_IP:      type_tag = "IP OVER IP";      break;
            case ENCAP_TRANS_END:       type_tag = "TRANSMIT TUNNEL ENDPOINT";   break;    //RFC 5566
            case ENCAP_IPSEC_TUN:       type_tag = "IPSEC IN TUNNEL MODE";       break;    //RFC 5566
            case ENCAP_IP_IP_IPSEC:     type_tag = "IP IN IP TUNNEL WITH IPSEC"; break;    //RFC 5566
            case ENCAP_MPLS_IP:     type_tag = "MPLS IN IP WITH IPSEC";      break;        //RFC 5566
            default:                type_tag = "UNKNOWN";           break;
        }

        xmlNodePtr type_node = xmlNewChildString(top_tlv,"TYPE",type_tag);
        xmlNewPropInt(type_node,"value",type);

        //generate length attribute for top-level TLV's value field
        curr_length = input[base_pos + 2]<<8 | input[base_pos + 3];
        xmlNewPropInt(top_tlv,"length",curr_length);

        //establish index for sub-TLV
        int curr_pos = base_pos + 4;
        u_int8_t sub_tlv_type;
        u_int8_t sub_tlv_len;

        do{
            xmlNodePtr tlv_node = xmlNewNode(NULL,BAD_CAST "TLV");
            sub_tlv_type = (u_int8_t)input[curr_pos];       //gets 1-octet sub-TLV type field
            sub_tlv_len = (u_int8_t)input[curr_pos + 1];    //gets 1-octet sub-TLV length of value field
            xmlNewPropInt(tlv_node,"length",sub_tlv_len);   //adds length attribute to sub-TLV node
            switch(sub_tlv_type){
                case BGP_ENCAP_TLV:
                {
                    xmlNodePtr tlv_type_node = xmlNewChildString(tlv_node,"TYPE","ENCAP");
                    xmlNewPropInt(tlv_type_node,"type",sub_tlv_type);
                    switch(type){
                        case ENCAP_L2TPV3_IP:
                        {
                            u_int32_t id = input[curr_pos + 2]<<24 | input[curr_pos + 3]<<16 | input[curr_pos + 4]<<8 | input[curr_pos + 5];
                            xmlNewChildInt(tlv_type_node,"SESSION_ID",id);
                            if(sub_tlv_len - 4 > 0)     //cookie may not be present, but is variable-length
                                xmlNewChildOctets(tlv_type_node,"COOKIE",input + curr_pos + 6,sub_tlv_len-4);
                            break;
                        }
                        case ENCAP_GRE:
                        {
                            u_int32_t key = input[curr_pos + 2]<<24 | input[curr_pos + 3]<<16 | input[curr_pos + 4]<<8 | input[curr_pos + 5];
                            xmlNewChildInt(tlv_type_node,"KEY",key);
                            break;
                        }
                        case ENCAP_IP_IN_IP:  break;
                        default:            break;
                    }
                    xmlAddChild(tlv_node,tlv_type_node);
                    break;
                }//end encapsulation sub-TLV case
                case BGP_PROTO_TLV:
                {
                    xmlNodePtr tlv_type_node = xmlNewChildString(tlv_node,"TYPE","PROTO");
                    xmlNewPropInt(tlv_type_node,"type",sub_tlv_type);
                    u_int16_t proto = input[curr_pos + 2]<<8 | input[curr_pos + 3];
                    switch(proto){
                        case 0x0800:
                        {
                            xmlNewChildString(tlv_type_node,"PROTOCOL","IPV4");
                            break;
                        }
                        case 0x86dd:
                        {
                            xmlNewChildString(tlv_type_node,"PROTOCOL","IPV6");
                            break;
                        }
                        case 0x8847:
                        {
                            xmlNewChildString(tlv_type_node,"PROTOCOL","MPLS");
                            break;
                        }
                        default:
                        {
                            xmlNewChildString(tlv_type_node,"PROTOCOL","UNKNOWN");
                            break;
                        }
                    }
                    xmlAddChild(tlv_node,tlv_type_node);
                    break;
                }//end protocol sub-TLV case
                case BGP_COLOR_TLV:
                {
                    xmlNodePtr tlv_type_node = xmlNewChildString(tlv_node,"TYPE","COLOR");
                    xmlNewPropInt(tlv_type_node,"type",sub_tlv_type);
                    xmlNodePtr color_node = genBgpExtCommunitiesNode(input + curr_pos + 2,sub_tlv_len);
                    xmlAddChild(tlv_type_node,color_node);
                    xmlAddChild(tlv_node,tlv_type_node);
                    break;
                }//end color sub-TLV case
                case BGP_TUNNEL_AUTH_TLV:   //RFC 5566
                {
                    xmlNodePtr tlv_type_node = xmlNewChildString(tlv_node,"TYPE","IPSEC TUNNEL AUTH");
                    xmlNewPropInt(tlv_type_node,"type",sub_tlv_type);
                    u_int16_t auth_type = input[curr_pos + 2]<<8 | input[curr_pos + 3];
                    switch(auth_type){
                        case 1:     //type 1 (RFC 5566) is SHA-1 (RFC 4306)
                        {
                            xmlNewChildString(tlv_type_node,"AUTH_TYPE","SHA-1");
                            break;
                        }
                        default:
                        {
                            xmlNewChildString(tlv_type_node,"AUTH_TYPE","UNKNOWN");
                            break;
                        }
                    }
                    xmlNewChildOctets(tlv_type_node,"KEY",input + curr_pos + 4,sub_tlv_len - 2);
                    xmlAddChild(tlv_node,tlv_type_node);
                    break;
                }//end Tunnel authentication case
                case BGP_LOAD_BAL_TLV:      //RFC 5640
                {
                    xmlNodePtr tlv_type_node = xmlNewChildString(tlv_node,"TYPE","LOAD BALANCING BLOCK");
                    xmlNewPropInt(tlv_type_node,"type",sub_tlv_type);
                    u_int16_t value = input[curr_pos + 2]<<8 | input[curr_pos + 3];
                    xmlNewChildInt(tlv_type_node,"VALUE",value);
                    xmlAddChild(tlv_node,tlv_type_node);
                    break;
                }//end Load Balancing Block sub-TLV
                default:
                {
                    xmlNewChildOctets(tlv_node,"UNKNOWN",input + curr_pos + 2,sub_tlv_len);
                    break;
                }
            }
            //after sub-TLV is processed, add it to its owner and increment counter
            xmlAddChild(top_tlv,tlv_node);  //adds sub-TLV to main TLV
            curr_pos = curr_pos + 2 + sub_tlv_len;  //2-octet header and length of value field
        }while(curr_pos < sub_tlv_len);
        //after sub-TLVs are processed, increment base index and add TLV
        base_pos = base_pos + curr_length + 4;
        xmlAddChild(encap_node,top_tlv);
    }
    return encap_node;
}

/*---------------------------------------------------------------------------------------
 * Purpose: generate the TRAFFIC_ENGR node
 * input:   input - pointer to the traffic engineering attribute
 *          len - length of the attribute
 * output:  the new TRAFFIC_ENGR node
 * Jason Bartlett @ 18 Oct 2010
 *--------------------------------------------------------------------------------------*/
xmlNodePtr genTrafficEngineeringNode(u_char *input,int len){
    xmlNodePtr engr = xmlNewNode(NULL,BAD_CAST "TRAFFIC_ENGR");
    int next = 0;   //index of the next engineering block
    while(next < len){  //exits once all blocks have been handled
        xmlNodePtr engr_node = xmlNewNode(NULL,BAD_CAST "ENGR");
        //define Switching Capability (octet 0)
        switch(input[next]){
            case LSC_PSC1:  xmlNewChildString(engr_node,"SWITCH_CAP","PSC-1");  break;
            case LSC_PSC2:  xmlNewChildString(engr_node,"SWITCH_CAP","PSC-2");  break;
            case LSC_PSC3:  xmlNewChildString(engr_node,"SWITCH_CAP","PSC-3");  break;
            case LSC_PSC4:  xmlNewChildString(engr_node,"SWITCH_CAP","PSC-4");  break;
            case LSC_L2SC:  xmlNewChildString(engr_node,"SWITCH_CAP","L2SC");  break;
            case LSC_TDM:  xmlNewChildString(engr_node,"SWITCH_CAP","TDM");  break;
            case LSC_LSC:  xmlNewChildString(engr_node,"SWITCH_CAP","LSC");  break;
            case LSC_FSC:  xmlNewChildString(engr_node,"SWITCH_CAP","FSC");  break;
            default:        xmlNewChildString(engr_node,"SWITCH_CAP","UNKNOWN");    break;
        }
        //define Encoding (octet 1)
        switch(input[next + 1]){
            case LSP_PACKET_ENC:        xmlNewChildString(engr_node,"ENCODING","PACKET");   break;
            case LSP_ETHERNET_ENC:      xmlNewChildString(engr_node,"ENCODING","ETHERNET");   break;
            case LSP_PDH_ENC:           xmlNewChildString(engr_node,"ENCODING","PDH");   break;
            case LSP_G707_T1105_ENC:    xmlNewChildString(engr_node,"ENCODING","SDH ITU-T G.707/SONET ANSI T1.105");   break;
            case LSP_DIG_WRAP_ENC:      xmlNewChildString(engr_node,"ENCODING","DIGITAL WRAPPER");   break;
            case LSP_LAMBDA_ENC:        xmlNewChildString(engr_node,"ENCODING","LAMBDA (PHOTONIC)");   break;
            case LSP_FIBER_ENC:         xmlNewChildString(engr_node,"ENCODING","FIBER");   break;
            case LSP_FIBERCHANNEL_ENC:  xmlNewChildString(engr_node,"ENCODING","FIBERCHANNEL");   break;
            default:                    xmlNewChildString(engr_node,"ENCODING","UNKNOWN");      break;
        }

        //Each Engineering Attribute defines 8 priorities
        int i;
        for(i = 0;i < 8;i++){
            u_int32_t max = input[next + 5 + (4 * i)]<<24 | input[next+ 6 + (4 * i)]<<16 | input[next + 7 + (4 * i)]<<8 | input[next + 8 + (4 * i)];
            xmlNodePtr lsp_node = xmlNewChildInt(engr_node,"MAX_LSP_BANDWIDTH",max);
            xmlNewPropInt(lsp_node,"priority",i);
        }

        //some Switching Capabilities indicate additional information
        switch(input[next]){
            case LSC_PSC1:
            case LSC_PSC2:
            case LSC_PSC3:
            case LSC_PSC4:
            {
                u_int32_t min_lsp_bw = input[next + 36]<<24 | input[next + 37]<<16 | input[next + 38]<<8 | input[next + 39];
                u_int16_t int_mtu = input[next + 40]<<8 | input[next + 41];
                xmlNewChildInt(engr_node,"MINIMUM_LSP_BANDWIDTH",min_lsp_bw);
                xmlNewChildInt(engr_node,"INTERFACE_MTU",int_mtu);
                next = next + 42;
                break;
            }
            case LSC_TDM:
            {
                u_int32_t min_lsp_bw = input[next + 36]<<24 | input[next + 37]<<16 | input[next + 38]<<8 | input[next + 39];
                xmlNewChildInt(engr_node,"MINIMUM_LSP_BANDWIDTH",min_lsp_bw);
                xmlNewChildInt(engr_node,"INDICATION",(u_int8_t)input[next + 40]);
                next = next + 41;
                break;
            }
            case LSC_L2SC:
            case LSC_LSC:
            case LSC_FSC:
            default:
            {
                next = next + 36;
                break;
            }
        }
        xmlAddChild(engr,engr_node);
    }
    return engr;
}

/*---------------------------------------------------------------------------------------
 * purpose: generate the IPv6-Specific Extended Community node
 * input:   input - pointer to the attribute
 *          len - the length of the attribute
 * output:  the new extended community node
 * Jason Bartlett @ 18 Oct 2010
 *--------------------------------------------------------------------------------------*/
xmlNodePtr genIPv6ExtCommunityNode(u_char *input,int len){
    xmlNodePtr top_node = xmlNewNode(NULL,BAD_CAST "EXTENDED_COMMUNITIES");
    xmlNodePtr com_node;
    int i;
    //each extended community is defined as 20 octets long
    for(i = 0;i < len;i += 20){
        com_node = xmlNewNode(NULL,BAD_CAST "IPV6_SPECIFIC_EXT_COM");
        u_char type_h = input[i];   //first octet defines type (either 0x00 or 0x40)
        u_char type_l = input[i + 1];    //second octet defines subtype

        switch(type_h){
            case 0x00:
            {
                xmlNewPropString(com_node,"transitive","TRUE");
                switch(type_l){
                    case 0x02:
                    {
                        xmlNewChildString(com_node,"SUBTYPE","ROUTE_TARGET");
                        break;
                    }
                    case 0x03:
                    {
                        xmlNewChildString(com_node,"SUBTYPE","ROUTE_ORIGIN");
                        break;
                    }
                    default:
                    {
                        xmlNewChildString(com_node,"SUBTYPE","UNKNOWN");
                        break;
                    }
                    break;
                }
            }
            case 0x40:
            {
                switch(type_l){
                    default:
                    {
                        xmlNewChildString(com_node,"SUBTYPE","UNKNOWN");
                        break;
                    }
                }
                break;
            }
        }
        xmlNewChildNetAddr(com_node,"IPV6_ADDR",input + i + 2,16);
        xmlNewChildOctets(com_node,"LOCAL",input + i + 18,2);
        xmlAddChild(top_node,com_node);
    }
    return top_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the PATH_ATTRIBUTES node
 * input:   attr - pointer to normal attribute
 *          len  - length of normal attribute
 *          lt   - pointer to an array of labels. it could be NULL
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 21 Sep 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genUpdateAttributesNode(BMF bmf, u_char *attr, int len, u_char **lt )
{
    xmlNodePtr attributes_node = NULL;
    attributes_node = xmlNewNode(NULL, BAD_CAST "PATH_ATTRIBUTES");

    int i;
    int hl;
    int l;
    int flags;
    int type;
    int count = 0;
    u_char *value;

    /* Session specific setting */
    Session_structp sp = getSessionByID(bmf->sessionID);
    int asn_len = sp->fsm.ASNumlen;

    /* For each attribute */
    for ( i = 0; i < len; i = i + 2 + hl + l )
    {
        xmlNodePtr attr_node = xmlNewNode(NULL, BAD_CAST "ATTRIBUTE");

        /* flags */
        flags = attr[i];
        type  = attr[i+1];

        if ( (flags & 0x10) > 0 )
        {
            hl = 2;
            l = ntohs( *((u_int16_t *) (attr+i+2)) );
            value = attr+i+4;
        }
        else
        {
            hl = 1;
            l = attr[i+2];
            value = attr+i+3;
        }
        xmlAddChild(attr_node, genAttributeFlagNode(flags));

        /* process each attribute */
        xmlNodePtr type_node   = NULL;
        xmlNewPropInt(attr_node,"length",l);
        switch ( type ) /* Defined in bgpmessagetypes.h */
        {
            case BGP_ATTR_ORIGIN:
            {
                /* ORIGIN(1) */
                char *atag = "ORIGIN";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, genBgpOriginNode(bmf, value[0]));
                break;
            }
            case BGP_ATTR_AS_PATH:
            {
                /* AS PATH(2) */
                char *atag = "AS_PATH";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, genBgpASPathNode(value, l, asn_len));
                break;
            }
            case BGP_ATTR_NEXT_HOP:
            {
                /* NEXT HOP(3) */
                char *atag = "NEXT_HOP";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, xmlNewNodeIP(atag, *((u_int32_t *) value )));
                break;
            }
            case BGP_ATTR_MULTI_EXIT_DISC:
            {
                /* MED(4) */
                char *atag = "MULTI_EXIT_DISC";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, xmlNewNodeInt(atag, ntohl( *((u_int32_t *) value ))));
                break;
            }
            case BGP_ATTR_LOCAL_PREF:
            {
                /* LOCAL_PREF(5) */
                char *atag = "LOCAL_PREF";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, xmlNewNodeInt(atag, ntohl(*((u_int32_t *) value ))));
                break;
            }
            case BGP_ATTR_ATOMIC_AGGREGATE:
            {
                /* ATOMIC_AGGREGATE(6) */
                char *atag = "ATOMIC_AGGREGATE";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, xmlNewNode(NULL, BAD_CAST atag));
                break;
            }
            case BGP_ATTR_AGGREGATOR:
            {
                /* AGGREGATOR(7) */
                char *atag = "AGGREGATOR";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlNodePtr temp_node = xmlAddChild(attr_node, xmlNewNode(NULL, BAD_CAST atag));
                xmlNewChildInt(temp_node, "AS",   ntohs( *((u_int16_t *) value ))); /* 2-byte AS */
                xmlNewChildIP(temp_node,  "ADDR", *((u_int32_t *) value+2 ));       /* 4-byte address */
                break;
            }
            case BGP_ATTR_COMMUNITIES: // RFC 1997
            {
                /* COMMUNITIES(8) */
                char *atag = "COMMUNITIES";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, genBgpCommunitiesNode(bmf, value, l));
                break;
            }
            case BGP_ATTR_ORIGINATOR_ID: // RFC 2796
            {
                /* ORIGINATOR_ID(9) */
                char *atag = "ORIGINATOR_ID";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlNewChildIP(attr_node, atag, *((u_int32_t *) value ));
                break;
            }
            case BGP_ATTR_CLUSTER_LIST: // RFC 2796
            {
                /* CLUSTER_LIST(10) */
                char *atag = "CLUSTER_LIST";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, genBgpClusterListNode(value, l));
                break;
            }
            case BGP_ATTR_DPA: // expired!!
            {
                /* DESTINATION_PREFERENCE */
                char *atag = "DESTINATION_PREFERENCE";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlNodePtr temp_node = xmlAddChild(attr_node, xmlNewNode(NULL, BAD_CAST atag));
                if ( l > 0 ) {
                    xmlNewChildOctets(temp_node, "OCTETS", value, l);
                }
                break;
            }
            case BGP_ATTR_ADVERTISER: // Historic!!
            {
                /* ADVERTISER(12) */
                char *atag = "ADVERTISER";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlNodePtr temp_node = xmlAddChild(attr_node, xmlNewNode(NULL, BAD_CAST atag));
                if ( l > 0 ) {
                    xmlNewChildOctets(temp_node, "OCTETS", value, l);
                }
                break;
            }
            case BGP_ATTR_RCID_PATH: //  Historic!!
            {
                /* RCID_PATH(13) */
                char *atag = "RCID_PATH";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlNodePtr temp_node = xmlAddChild(attr_node, xmlNewNode(NULL, BAD_CAST atag));
                if ( l > 0 ) {
                    xmlNewChildOctets(temp_node, "OCTETS", value, l);
                }
                break;
            }
            case BGP_ATTR_MP_REACH_NLRI: // RFC 2858
            {
                /* MP_REACH_NLRI(14) */
                char *atag = "MP_REACH_NLRI";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, genBgpMPReachNode(value, l, lt));
                break;
            }
            case BGP_ATTR_MP_UNREACH_NLRI: // RFC 2858
            {
                /* MP_UNREACH_NLRI(15) */
                char *atag = "MP_UNREACH_NLRI";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, genBgpMPUnreachNode(value, l, lt));
                break;
            }
            case BGP_ATTR_EXT_COMMUNITIES: // RFC 4360
            {
                /* EXTENDED_COMMUNITIES(16) */
                char *atag = "EXTENDED_COMMUNITIES";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node,genBgpExtCommunitiesNode(value,l));
                break;
            }
            case BGP_ATTR_AS4_PATH: // RFC 4893
            {
                /* AS4_PATH(17) */
                char *atag = "AS4_PATH";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node, genBgpAS4PathNode(value, l));
                break;
            }
            case BGP_ATTR_AS4_AGGREGATOR: // RFC 4893
            {
                /* AS4_AGGREGATOR(18) */
                char *atag = "AS4_AGGREGATOR";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlNodePtr temp_node = xmlAddChild(attr_node, xmlNewNode(NULL, BAD_CAST atag));
                xmlNewChildInt(temp_node, "AS",   ntohl( *((u_int32_t *) value ))); /* 4-byte AS */
                xmlNewChildIP(temp_node,  "ADDR", *((u_int32_t *) value+4 ));       /* 4-byte address */
                break;
            }

            case BGP_ATTR_TUNNEL_ENCAP: //RFC 5512
            {
                char *atag = "TUNNEL_ENCAPSULATION";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node,genTunnelEncapNode(value,l));
                break;
            }
            case BGP_ATTR_TRAFFIC_ENGR: //RFC 5543
            {
                char *atag = "TRAFFIC_ENGINEERING";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node,genTrafficEngineeringNode(value,l));
                break;
            }
            case BGP_ATTR_IPV6_EXT_COM: //RFC 5701
            {
                char *atag = "EXTENDED_COMMUNITIES";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlAddChild(attr_node,genIPv6ExtCommunityNode(value,l));
                break;
            }
            //[EXTEND] Additional attributes here
            default:
            {
                char *atag = "OTHER";
                type_node = xmlNewChildString(attr_node,"TYPE",atag);
                xmlNodePtr other_node = xmlAddChild(attr_node, xmlNewNode(NULL, BAD_CAST atag));
                if ( l > 0 ) {
                    xmlNewChildOctets(other_node, "OCTETS", value, l);
                }
                break;
            }
        }
        xmlNewPropInt(type_node, "value", type);
        xmlAddChild(attributes_node, attr_node);
        count++;
    }
    xmlNewPropInt(attributes_node, "count", count);

    return attributes_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate OPEN node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 14 Oct 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpOpenNode(BMF bmf)
{
    xmlNodePtr bgp_node = NULL;
    PBgpOpen open = (PBgpOpen)(bmf->message + BGP_HEADER_LEN);

    /* BGP OPEN node */
    bgp_node = xmlNewNode(NULL, BAD_CAST "OPEN");

    /* Child nodes and attributes*/
    /* VERSION     */ xmlNewChildInt(bgp_node, "VERSION",     getBGPOpenVersion(open));
    /* SRC_AS      */ xmlNewChildInt(bgp_node, "SRC_AS",      getBGPOpenAutonomousSystem(open));
    /* HOLD_TIME   */ xmlNewChildInt(bgp_node, "HOLD_TIME",   getBGPOpenHoldTime(open));
    /* SRC_BGP     */ xmlNewChildIP(bgp_node,  "SRC_BGP",     getBGPOpenIdentifier(open));
    /* OPT_PAR_LEN */ xmlNewChildInt(bgp_node, "OPT_PAR_LEN", getBGPOpenOptionalParameterLength(open));
    /* OPT_PAR     */ xmlAddChild(bgp_node, genBgpOpenOptionalParameterNode(/* params */ open->optionalParameter,
                                                                            /* length */ getBGPOpenOptionalParameterLength(open)));
    return bgp_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate UPDATE node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 15 Sep 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpUpdateNode(BMF bmf)
{
    xmlNodePtr bgp_node = NULL;
    /* BGP Update node */
    bgp_node = xmlNewNode(NULL, BAD_CAST "UPDATE");

    /* header */
    PBgpHeader hdr = NULL;
//    u_int16_t  bgpType   = 0;
    u_int32_t  bgpMsgLen = 0;

    hdr = (PBgpHeader)(bmf->message);
//    bgpType   = hdr->type;
    bgpMsgLen = getBGPHeaderLength(hdr);

    u_char *lt1 = NULL;
    if ( bmf->type == BMF_TYPE_MSG_LABELED ) lt1 = (u_char *)(bmf->message + bgpMsgLen);

    u_char *update = bmf->message + BGP_HEADER_LEN;
    int len        = bgpMsgLen   - BGP_HEADER_LEN;
    int real_len   = bmf->length - BGP_HEADER_LEN; /* In case of prefixes have been truncated */
    u_char **lt    = &lt1;

    /* len */
    int wlen = ntohs( *((u_int16_t *) update)  );           if (wlen > real_len - 4) wlen = real_len - 4; /* prefixes are truncated */
    int alen = ntohs( *((u_int16_t *) (update+2+wlen)) );
    int nlen = len - alen - wlen - 4;                       if (nlen > real_len - 4) nlen = real_len - 4; /* prefixes are truncated */

    /* Child nodes and attributes */
    /* WITHDRAWN_LEN  */
    xmlNewPropInt(bgp_node,"withdrawn_len",wlen);
    /* WITHDRAWN      */ xmlAddChild(bgp_node, genUpdateWithdrawnNode(update+2, wlen, 1, 1, lt));
    /* ATTRIBUTES_LEN  */
    xmlNewPropInt(bgp_node,"path_attr_len",alen);
    /* ATTRIBUTES     */ xmlAddChild(bgp_node, genUpdateAttributesNode(bmf, update+4+wlen, alen, lt));
    /* NLRI           */ xmlAddChild(bgp_node, genUpdateNlriNode(update+4+wlen+alen, nlen, 1, 1, lt));

    return bgp_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate TIME node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 16 Sep 2010
 *
 * Ex:
 *  <TIME timestamp="1229905411" datetime="2008-12-22T00:23:31Z" precision_time="300"/>
 * -------------------------------------------------------------------------------------*/
xmlNodePtr genTimeNode(BMF bmf)
{
    xmlNodePtr time_node = NULL;

    /* TIME node */
    time_node = xmlNewNode(NULL, BAD_CAST "TIME");

    /* Attributes */
    /* TIMESTAMP      */  xmlNewPropUnsignedInt(time_node,"timestamp",bmf->timestamp);
    /* DATETIME       */ if (GMT_TIME_STAMP == TRUE) xmlNewPropGmtTime(time_node, "datetime", bmf->timestamp);
    /* PRECISION_TIME */ xmlNewPropUnsignedInt(time_node, "precision_time", bmf->precisiontime);

    return time_node;
}

/*------------------------------------------------------------------------------
 * Purpose: generate PEERING node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 16 Sep 2010
 * Cathie Olschanowsky @ June 2012 BGPMON-115
 *
 * Ex:
 *  <PEERING as_num_len="2">
 *    <SRC_ADDR>
 *        <AFI value="1">IPV4</AFI>
 *        <ADDRESS>208.51.134.246</ADDRESS>
 *    </SRC_ADDR>
 *    <SRC_PORT>0</SRC_PORT>
 *    <SRC_AS>3549</SRC_AS>
 *    <DST_ADDR>
 *        <AFI value="1">IPV4</AFI>
 *        <ADDRESS>128.223.51.102</ADDRESS>
 *    </DST_ADDR>
 *    <DST_PORT>0</DST_PORT>
 *    <DST_AS>6447</DST_AS>
 *    <BGPID>128.223.51.102</BGPID>
 *  </PEERING>
 * ---------------------------------------------------------------------------*/
xmlNodePtr genPeeringNode(BMF bmf)
{
  xmlNodePtr peering_node = NULL;
  peering_node = xmlNewNode(NULL, BAD_CAST "PEERING");

  char *srcAddr, *dstAddr;
  u_int16_t srcPort, dstPort;
  u_int32_t srcAS, dstAS;

  xmlNodePtr src_addr_node, dst_addr_node;

  /* PEERING NODE */
  Session_structp sp = getSessionByID(bmf->sessionID);

  if(sp){
    // Have to know how to directly access
    //ddr, srcport, arcas, dstaddr, dstport, dstas
    dstAddr = sp->sessionRealSrcAddr;    // Use real source address
    dstPort = sp->configInUse.localPort;
    dstAS   = sp->configInUse.localAS2;
    srcAddr = sp->configInUse.remoteAddr;
    srcPort = sp->configInUse.remotePort;
    srcAS   = sp->configInUse.remoteAS2;

    /* SRC_ADDR */
    src_addr_node = xmlNewChild(peering_node,NULL,BAD_CAST "SRC_ADDR",NULL);
    xmlNewChildString(src_addr_node,"ADDRESS",srcAddr);
    xmlNewChildAFI(src_addr_node, get_afi(srcAddr));
    xmlNewChildInt(peering_node,    "SRC_PORT", srcPort);
    xmlNewChildUnsignedInt(peering_node,    "SRC_AS",   srcAS);
    dst_addr_node = xmlNewChild(peering_node,NULL,BAD_CAST "DST_ADDR",NULL);
    xmlNewChildString(dst_addr_node,"ADDRESS",dstAddr);
    xmlNewChildAFI(dst_addr_node, get_afi(dstAddr));
    xmlNewChildInt(peering_node,  "DST_PORT", dstPort);
    xmlNewChildUnsignedInt(peering_node,  "DST_AS",   dstAS);
    xmlNewChildBGPID(peering_node,"BGPID",    sp->configInUse.remoteBGPID);
    xmlNewPropInt(peering_node,    "as_num_len",   sp->fsm.ASNumlen);
  }

  return peering_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate NOTIFICATION node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * Jason Bartlett @ 16 Sep 2010
 *
 * Ex: <NOTIFICATION>
 *          <TYPE value="1">MESSAGE HEADER ERROR</TYPE>
 *          <SUBTYPE value="3">BAD MESSAGE TYPE</SUBTYPE>
 *     </NOTIFICATION>
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpNotificationNode(BMF bmf)
{
    xmlNodePtr node = NULL;
    PBgpNotification ntf = (PBgpNotification)(bmf->message + BGP_HEADER_LEN);

    char *code_str    = NULL;
    char *subcode_str = NULL;
    
    switch(ntf->errorCode)
    {
        case 1:
        {
            code_str = "MESSAGE HEADER ERROR"; 
            switch(ntf->errorSubcode)
            {
                case 1: { subcode_str = "CONNECTION NOT SYNCHRONIZED"; break; }
                case 2: { subcode_str = "BAD MESSAGE LENGTH";          break; }
                case 3: { subcode_str = "BAD MESSAGE TYPE";            break; }
                default:{ subcode_str = "UNKNOWN ERROR SUBTYPE";         break; }
            }
            break;
        }
        case 2:
        {
            code_str = "OPEN MESSAGE ERROR"; 
            switch(ntf->errorSubcode)
            {
                case 1: { subcode_str = "UNSUPPORTED VERSION NUMBER";         break; }
                case 2: { subcode_str = "BAD PEER AS";                        break; }
                case 3: { subcode_str = "BAD BGP IDENTIFIER";                 break; }
                case 4: { subcode_str = "UNSUPPORTED OPTIONAL PARAMETER";     break; }
                case 5: { subcode_str = "AUTHENTICATION FAILURE";             break; }
                case 6: { subcode_str = "UNACCEPTABLE HOLD TIME";             break; }
                default:{ subcode_str = "UNKNOWN ERROR SUBTYPE";         break; }
            }
            break;
        }
        case 3:
        {
            code_str = "UPDATE MESSAGE ERROR"; 
            switch(ntf->errorSubcode)
            {
                case 1:  { subcode_str = "MALFORMED ATTRIBUTE LIST";          break; }
                case 2:  { subcode_str = "UNRECOGNIZED WELL-KNOWN ATTRIBUTE"; break; }
                case 3:  { subcode_str = "MISSING WELL-KNOWN ATTRIBUTE";      break; }
                case 4:  { subcode_str = "ATTRIBUTE FLAGS ERROR";             break; }
                case 5:  { subcode_str = "ATTRIBUTE LENGTH ERROR";            break; }
                case 6:  { subcode_str = "INVALID ORIGIN ATTRIBUTE";          break; }
                case 7:  { subcode_str = "AS ROUTING LOOP";                   break; }
                case 8:  { subcode_str = "INVALID NEXT_HOP ATTRIBUTE";        break; }
                case 9:  { subcode_str = "OPTIONAL ATTRIBUTE ERROR";          break; }
                case 10: { subcode_str = "INVALID NETWORK FIELD";             break; }
                case 11: { subcode_str = "MALFORMED AS PATH";                 break; }
                default:{ subcode_str = "UNKNOWN ERROR SUBTYPE";         break; }
            }
            break;
        }
        case 4:
        {
            code_str = "HOLD TIMER EXPIRED"; 
            break;
        }
        case 5:
        {
            code_str = "FINITE STATE MACHINE ERROR"; 
            break;
        }
        case 6:
        {
            code_str = "CEASE";
            switch(ntf->errorSubcode)
            {
                case 1: { subcode_str = "MAXIMUM NUMBER OF PREFIXES REACHED"; break; }
                case 2: { subcode_str = "ADMINISTRATIVE SHUTDOWN";            break; }
                case 3: { subcode_str = "PEER DE-CONFIGURED";                 break; }
                case 4: { subcode_str = "ADMINISTRATIVE RESET";               break; }
                case 5: { subcode_str = "CONNECTION REJECTED";                break; }
                case 6: { subcode_str = "OTHER CONFIGURATION CHANGE";         break; }
                case 7: { subcode_str = "CONNECTION COLLISION RESOLUTION";    break; }
                default:{ subcode_str = "UNKNOWN ERROR SUBTYPE";         break; }
            }
            break;
        }
        default:
        {
            code_str = "UNKNOWN ERROR";
            break;
        }

    }

    /* NOTIFICATION node */
    node = xmlNewNode(NULL, BAD_CAST "NOTIFICATION");

    /* Child nodes */
    //Modification: Put strings in children, numbers as children's attribute

    xmlNodePtr code_node = xmlNewChildString(node,"TYPE",code_str);
    xmlNewPropInt(code_node, "error_code", ntf->errorCode);

    if(ntf->errorCode == 1 || ntf->errorCode == 2 || ntf->errorCode == 3 || ntf->errorCode == 6){
        xmlNodePtr sub_code_node = xmlNewChildString(node,"SUBTYPE", subcode_str);
        xmlNewPropInt(sub_code_node,"error_subcode", ntf->errorSubcode);
    }

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate KEEPALIVE node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpKeepaliveNode(BMF bmf)
{
    xmlNodePtr node = NULL;
    node = xmlNewNode(NULL, BAD_CAST "KEEPALIVE");
    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate ROUTE_REFRESH node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * Jason Bartlett @ 17 Sep 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpRouteRefreshNode(BMF bmf)
{
    xmlNodePtr node = NULL;
    u_char *msg;
    node = xmlNewNode(NULL, BAD_CAST "ROUTE_REFRESH");
    msg = bmf->message + BGP_HEADER_LEN;
    u_int16_t afi = msg[0]<<8 | msg[1];
    u_int8_t safi = msg[3];
    xmlNewChildAFI(node,afi);
    xmlNewChildSAFI(node,safi);
    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate STATE_CHANGE node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpStateChangeNode(BMF bmf)
{
    xmlNodePtr state_change_node = NULL;
    StateChangeMsg *sc    = NULL;

    sc = (StateChangeMsg *)(bmf->message);

    /* STATE_CHANGE node */ state_change_node = xmlNewNode(NULL, BAD_CAST "STATE_CHANGE");

    /* Child nodes */
    /* OLD_STATE */ xmlNewChildInt(state_change_node, "OLD_STATE",   sc->oldState);
    /* NEW_STATE */ xmlNewChildInt(state_change_node, "NEW_STATE",   sc->newState);
    /* REASON    */ xmlNewChildInt(state_change_node, "REASON",      sc->reason);

    return state_change_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate UNKNOWN node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpUnknownNode(BMF bmf)
{
    xmlNodePtr node = NULL;

//    PBgpHeader hdr = NULL;
//    u_int16_t  bgpType = 0;
//    u_int32_t  bgpMsgLen = 0;

//    hdr       = (PBgpHeader)(bmf->message);
//    bgpType   = hdr->type;
//    bgpMsgLen = getBGPHeaderLength(hdr);

    node = xmlNewNode(NULL, BAD_CAST "UNKNOWN");
    return node;
}

/*---------------------------------------------------------------------------------
 * Purpose: Return a string representation of a BGP message type
 * Input:   a u_int16 containing the type code of the BGP message
 * Output:  the human-readable version of the type code
 * Jason Bartlett @ 16 Sep 2010
 *--------------------------------------------------------------------------------*/
char* getAsciiMsgType(u_int16_t type){
    char* result = NULL;
        /* Choice of message types */
        switch ( type )
        {
            case typeOpen:
            { 
                result = "OPEN";
                break;
            }
            case typeUpdate:
            {
                result = "UPDATE";
                break;
            }
            case typeNotification:
            {
                result = "NOTIFICATION";
                break;
            }
            case typeKeepalive:
            {
                result = "KEEPALIVE";
                break;
            }
            case typeRouteRefresh:
            {
                result = "ROUTE_REFRESH";
                break;
            }
            default:
            {
                result = "UNKNOWN";
                break;
            }
        }
    return result;
}


/*----------------------------------------------------------------------------------------
 * Purpose: generate ASCII_MSG node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * Jason Bartlett @ 14 Oct 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genAsciiMsgNode(BMF bmf)
{
    xmlNodePtr ascii_node = NULL;

    if (ASCII_MESSAGES == TRUE)
    {

        PBgpHeader hdr       = NULL;
        u_int16_t  bgpType   = 0;
        u_int32_t  bgpMsgLen = 0;

        hdr       = (PBgpHeader)(bmf->message);
        bgpType   = hdr->type;
        bgpMsgLen = getBGPHeaderLength(hdr);

        if(bgpMsgLen > 4096)
        {
          log_err("msg len is %d",bgpMsgLen);
        }

        /* ASCII_MSG node */
        ascii_node = xmlNewNode(NULL, BAD_CAST "ASCII_MSG");

        xmlNewPropUnsignedInt(ascii_node,"length",bgpMsgLen);
        //xmlNewChildString(ascii_node,"TYPE",getAsciiMsgType(bgpType));
        //xmlNewPropUnsignedInt(ascii_node,"type",(u_int32_t)bgpType);
        xmlNewChildOctets(ascii_node,"MARKER",hdr->mask,sizeof(hdr->mask));

        /* Choice of message types */
        switch ( bgpType )
        {
            case typeOpen:
            {
                /* OPEN */
                xmlAddChild(ascii_node, genBgpOpenNode(bmf));
                break;
            }
            case typeUpdate:
            {
                /* UPDATE */
                xmlAddChild(ascii_node, genBgpUpdateNode(bmf));
                break;
            }
            case typeNotification:
            {
                /* NOTIFICATION */
                xmlAddChild(ascii_node, genBgpNotificationNode(bmf));
                break;
            }
            case typeKeepalive:
            {
                /* KEEPALIVE */
                xmlAddChild(ascii_node, genBgpKeepaliveNode(bmf));
                break;
            }
            case typeRouteRefresh:
            {
                /* ROUTE_REFRESH */
                xmlAddChild(ascii_node, genBgpRouteRefreshNode(bmf));
                break;
            }
            default:
            {
                /* UNKNOWN */
                xmlAddChild(ascii_node, genBgpUnknownNode(bmf));
                break;
            }
        }
    }
    return ascii_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate OCTET_MSG node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * Jason Bartlett @ 16 Sep 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genOctetMsgNode(BMF bmf)
{
    xmlNodePtr octet_node = NULL;
    PBgpHeader hdr        = NULL;
//    u_int16_t  bgpType    = 0;
    u_int32_t  bgpMsgLen  = 0;

    // Due to the labels at the end of BMF, we needs to know the real length of raw bgp update
    hdr       = (PBgpHeader)(bmf->message);
//    bgpType   = hdr->type;
    bgpMsgLen = getBGPHeaderLength(hdr);

    /* OCTET_MSG node */
    octet_node = xmlNewNode(NULL, BAD_CAST "OCTET_MSG");
    xmlNewChildOctets(octet_node, "OCTETS", bmf->message, bgpMsgLen);

    return octet_node;
}

/*---------------------------------------------------------------------------------------
 * purpose: generate the BGPMON_SEQ node
 * output:  the new sequence number node
 * Jason Bartlett @ 21 Oct 2010
 *-------------------------------------------------------------------------------------*/
xmlNodePtr genSequenceNode(){
    xmlNodePtr seq_node = xmlNewNode(NULL, BAD_CAST "BGPMON_SEQ");
    xmlNewPropInt(seq_node,"id",ClientControls.bgpmon_id);
    xmlNewPropInt(seq_node,"seq_num",ClientControls.seq_num);

    return seq_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: convert the queue status message to an XML text representation
 * input:	queueName - queue name
 * Output: the position of pointer in the buffer
 * He Yan @ Jun 22, 2008
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/ 
xmlNodePtr
genPacingNode(char *queueName)
{
    xmlNodePtr node = NULL;
    /* QUEUE node */ node = xmlNewNode(NULL, BAD_CAST "PACING");

    stat_data_t pc_data, wl_data;
    memset(&pc_data, 0, sizeof(stat_data_t));
    memset(&wl_data, 0, sizeof(stat_data_t));

    // Accumulated and current count
    pc_data.accu    = getPacingCount(queueName);
    pc_data.current = getCurrentPacingCount(queueName);
    wl_data.current = getCurrentWritesLimit(queueName);

    int   flag     = getPacingState(queueName);
    char* flag_str = NULL;
    if (flag == TRUE) { flag_str = "1"; }
    else              { flag_str = "0"; }

    xmlNewChildString(node, "FLAG",         flag_str);
    xmlNewChildStat(node,   "COUNT",        &pc_data);
    xmlNewChildStat(node,   "WRITE_LIMIT",  &wl_data);

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the QUEUE node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genQueueNode(char *queueName)
{
    xmlNodePtr queue_node = NULL;

    /* QUEUE node */ 
    queue_node = xmlNewNode(NULL, BAD_CAST "QUEUE");

    xmlNewChildString(queue_node, "NAME", queueName);

    // Prepare statistic data
    stat_data_t item_data, writer_data, reader_data;
    memset(&item_data,   0, sizeof(stat_data_t));
    memset(&writer_data, 0, sizeof(stat_data_t));
    memset(&reader_data, 0, sizeof(stat_data_t));

    /* ITEMS/WRITER/READER */
    item_data.current   = getItemsUsed(queueName);
    item_data.max       = getLoggedMaxItems(queueName);
    item_data.limit     = getItemsTotal(queueName);
    writer_data.current = getWriterCount(queueName);
    writer_data.max     = getLoggedMaxWriters(queueName);
    reader_data.current = getReaderCount(queueName);
    reader_data.max     = getLoggedMaxReaders(queueName);	

    xmlNewChildStat(queue_node, "ITEM",   &item_data);
    xmlNewChildStat(queue_node, "WRITER", &writer_data);
    xmlNewChildStat(queue_node, "READER", &reader_data);

    xmlAddChild(queue_node, genPacingNode(queueName));
	return queue_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the QUEUE_STATUS node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genQueueStatusNode(BMF bmf)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "QUEUE_STATUS");

    int count = 0; 
    count++; xmlAddChild(node, genQueueNode(PEER_QUEUE_NAME));
    count++; xmlAddChild(node, genQueueNode(LABEL_QUEUE_NAME));
    count++; xmlAddChild(node, genQueueNode(XML_U_QUEUE_NAME));
    count++; xmlAddChild(node, genQueueNode(XML_R_QUEUE_NAME));
            
    xmlNewPropInt(node, "count", count);
    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: convert the chain status message to an XML text representation
 * input:   chainID - chain's ID
 * Output: the position of pointer in the buffer
 * He Yan @ Jun 22, 2008
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/ 
xmlNodePtr
genChainNode(int chainID)
{
    xmlNodePtr chain_node = NULL;
    /* CHAIN node */ chain_node = xmlNewNode(NULL, BAD_CAST "CHAIN");

	/* UPDATE LISTENER */
    /* ADDR/PORT */
	xmlNewChildString(chain_node, "UPDATE_ADDR", getChainAddress(chainID));
	xmlNewChildInt(chain_node,    "UPDATE_PORT", getUChainPort(chainID));

    /* STATE */
    xmlNewChildInt(chain_node, "UPDATE_STATE", getChainConnectionState(chainID, UPDATE_STREAM_CHAIN));
    
    /* COUNTs */
    time_data_t op_data;
    memset(&op_data,   0, sizeof(time_data_t));

    // Current value
	op_data.current   = getUChainConnectedTime(chainID);
	op_data.last_down = getULastConnectionDownTime(chainID);
    xmlNewChildTime(chain_node, "UPDATE_UPTIME",       &op_data);

    stat_data_t rv_data, rs_data;
    memset(&rv_data,   0, sizeof(stat_data_t));
    memset(&rs_data,   0, sizeof(stat_data_t));
    rv_data.current   = getUChainReceivedMessages(chainID);
    rs_data.current   = getUChainConnectionResetCount(chainID);
    xmlNewChildStat(chain_node, "UPDATE_RECV_MESSAGE", &rv_data);
    xmlNewChildStat(chain_node, "UPDATE_RESET",        &rs_data);

	return chain_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the CHAIN_STATUS node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genChainStatusNode(BMF bmf)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "CHAIN_STATUS");

    /*chains' status message*/
    int *chainIDs;
    int len = getActiveChainsIDs(&chainIDs);
    int count = 0; 
    int i;
    for(i=0; i<len; i++)
    {
        xmlAddChild(node, genChainNode(chainIDs[i]));
    }				
    /* update the count of prefixes */
    xmlNewPropInt(node, "count", count);
    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the SESSION node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genSessionNode(BMF bmf, int sessionID, int state_change)
{
    xmlNodePtr session_node = NULL;
    /* SESSION node */ session_node = xmlNewNode(NULL, BAD_CAST "SESSION");

    /* ADDR/PORT/AS */
    char *dstAddr = NULL;
    u_int16_t dstPort = 0;
    u_int16_t dstAS   = 0;
    Session_structp sp = getSessionByID(sessionID);
    if (sp)
    {
        dstAddr = sp->configInUse.remoteAddr;
        dstPort = sp->configInUse.remotePort;
        dstAS   = sp->configInUse.remoteAS2;
    }
	xmlNewChildString(session_node, "ADDR", dstAddr);
	xmlNewChildInt(session_node,    "PORT", dstPort);
	xmlNewChildInt(session_node,    "AS",   dstAS);

    if (state_change>0)
    {
        /* STATE_CHANGE */
        xmlAddChild(session_node, genBgpStateChangeNode(bmf));
    }
    else
    {
        /* STATE */
        xmlNewChildInt(session_node, "STATE", getSessionState(sessionID));
        
        /* COUNTs */
        time_data_t op_data;
        stat_data_t rv_data, rs_data;
        memset(&op_data,   0, sizeof(time_data_t));
        memset(&rv_data,   0, sizeof(stat_data_t));
        memset(&rs_data,   0, sizeof(stat_data_t));

        /* OPTIME/RECV_MESSAGE/RESET */
        op_data.current     = getSessionUPTime(sessionID);
        op_data.last_down   = getSessionLastDownTime(sessionID);
        op_data.last_action = getSessionLastActionTime(sessionID);

        rv_data.current     = getSessionMsgCount(sessionID);
        rs_data.current     = getSessionDownCount(sessionID);

        xmlNewChildTime(session_node, "UPTIME",       &op_data);
        xmlNewChildStat(session_node, "RECV_MESSAGE", &rv_data);
        xmlNewChildStat(session_node, "RESET",        &rs_data);

        /* COUNTs for session variables */
        stat_data_t pr_data,  // prefix
                    at_data,  // attirbute
                    mm_data,  // memory usage
                    na_data,  // announcements
                    da_data,  // dup announcements
                    sp_data,  // same path
                    dp_data,  // dup path
                    nw_data,  // withdrawal
                    dw_data;  // dup withdrawal
        memset(&pr_data,   0, sizeof(stat_data_t));
        memset(&at_data,   0, sizeof(stat_data_t));
        memset(&mm_data,   0, sizeof(stat_data_t));
        memset(&na_data,   0, sizeof(stat_data_t));
        memset(&da_data,   0, sizeof(stat_data_t));
        memset(&sp_data,   0, sizeof(stat_data_t));
        memset(&dp_data,   0, sizeof(stat_data_t));
        memset(&nw_data,   0, sizeof(stat_data_t));
        memset(&dw_data,   0, sizeof(stat_data_t));

        pr_data.current = getSessionPrefixCount(sessionID);
        at_data.current = getSessionAttributeCount(sessionID);
        mm_data.current = getSessionMemoryUsage(sessionID);

        na_data.current = getSessionCurrentNannCount(sessionID);
        da_data.current = getSessionCurrentDannCount(sessionID);
        sp_data.current = getSessionCurrentSPathCount(sessionID);
        dp_data.current = getSessionCurrentDPathCount(sessionID);
        nw_data.current = getSessionCurrentWithCount(sessionID);
        dw_data.current = getSessionCurrentDWithCount(sessionID);

        xmlNewChildStat(session_node, "PREFIX",       &pr_data);
        xmlNewChildStat(session_node, "ATTRIBUTE",    &at_data);
        xmlNewChildStat(session_node, "MEMORY_USAGE", &mm_data);

        xmlNewChildStat(session_node, "ANNOUNCEMENT",     &na_data);
        xmlNewChildStat(session_node, "DUP_ANNOUNCEMENT", &da_data);
        xmlNewChildStat(session_node, "SAME_PATH",        &sp_data);
        xmlNewChildStat(session_node, "DIFF_PATH",        &dp_data);
        xmlNewChildStat(session_node, "WITHDRAWAL",       &nw_data);
        xmlNewChildStat(session_node, "DUP_WITHDRAWAL",   &dw_data);


    }
	return session_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the SESSION_STATUS node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genSessionStatusNode(BMF bmf)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "SESSION_STATUS");

    int count = 0;
    int i = 0;
    switch ( bmf->type )
    {
        /* Periodic session status report */
        case BMF_TYPE_SESSION_STATUS:
        {
            for(i=0; i<MAX_SESSION_IDS; i++)
            {
                if( Sessions[i] != NULL && getSessionState(Sessions[i]->sessionID) == stateEstablished  )
                {
                    count++;
                    xmlAddChild(node, genSessionNode(bmf, i, 0));
                }
            }
            break;
        }
        /* Single session state change */
        case BMF_TYPE_FSM_STATE_CHANGE:
        {
            count++;
            xmlAddChild(node, genSessionNode(bmf, bmf->sessionID, 1));
            break;
        }
    }

    /* update the count of prefixes */
    xmlNewPropInt(node, "count", count);

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate an individual MRT node
 * input:   none
 * Output:  the new xml node
 * Jason Bartlett @ 14 Feb 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genMRTNode(long mrt_id){
    xmlNodePtr mrt_node = xmlNewNode(NULL,BAD_CAST "MRT");

    //get MRT's address and port (AS is currently not included)
    char * tmp = getMrtAddress(mrt_id);
    xmlNewChildString(mrt_node,"ADDRESS",tmp);
    free(tmp);
    xmlNewChildInt(mrt_node,"PORT",getMrtPort(mrt_id));
    
    time_data_t time_info;

    memset(&time_info,0,sizeof(time_data_t));

    time_info.current = getMrtConnectedTime(mrt_id);
    //MRT does not save last-down time

    xmlNewChildInt(mrt_node,"UPTIME",(int)time_info.current);
    
    return mrt_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the MRT_STATUS node
 * input:   none
 * Output:  the new xml node
 * Jason Bartlett @ 14 Feb 2010
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genMRTStatusNode(BMF bmf){
    xmlNodePtr mrt_root_node = xmlNewNode(NULL,BAD_CAST "MRT_STATUS");

    int i,mrt_count = 0,session_count = 0;
    long* mrt_ids;
    int num_mrt_ids;

    num_mrt_ids = getActiveMrtsIDs(&mrt_ids);
    if(num_mrt_ids == -1) return mrt_root_node;
    else{
        //add nodes for all the MRT connections
        for( i = 0; i< num_mrt_ids; i++){
            xmlAddChild(mrt_root_node, genMRTNode(mrt_ids[i]));
            mrt_count++;
        }
        //add nodes for all the sessions connected VIA an MRT
        for( i = 0; i < MAX_SESSION_IDS; i++ ){
            if( Sessions[i] != NULL && getSessionState(Sessions[i]->sessionID) == stateMrtEstablished  )
            {
                session_count++;
                xmlAddChild(mrt_root_node, genSessionNode(bmf, i, 0));
            }
        }
    }
    xmlNewPropInt(mrt_root_node,"mrt_count",mrt_count);
    xmlNewPropInt(mrt_root_node,"session_count",session_count);

    free(mrt_ids);
    return mrt_root_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate BGPMON_TIME node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Jason Bartlett @ 14 Feb 2011
 * -------------------------------------------------------------------------------------*/
xmlNodePtr genBgpmonTimeNode(BMF bmf){
    xmlNodePtr time_node = xmlNewNode(NULL,BAD_CAST "BGPMON_TIME");
    xmlNewChildInt(time_node,"UPTIME",time(NULL) - bgpmon_start_time);   //bgpmon_start_time saved in bgpmon_defaults.h
    return time_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate BGPMON_START node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpmonStartNode(BMF bmf)
{
    /* BGPMON_START node */
    xmlNodePtr bgpmon_node = NULL;
    bgpmon_node = xmlNewNode(NULL, BAD_CAST "START");
    return bgpmon_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate BGPMON_STOP node
 * input:   bmf - pointer to BMF structure
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * He Yan @ Jun 22, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpmonStopNode(BMF bmf)
{
    /* BGPMON_STOP node */
    xmlNodePtr bgpmon_node = NULL;
    bgpmon_node = xmlNewNode(NULL, BAD_CAST "STOP");
    return bgpmon_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the BGPMON_STATUS node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpmonStatusNode(BMF bmf)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "BGPMON_STATUS");

    switch ( bmf->type )
    {
        case BMF_TYPE_BGPMON_START: { /* bgpmon start */ xmlAddChild(node, genBgpmonTimeNode(bmf)); break; }
        case BMF_TYPE_BGPMON_STOP:  { /* bgpmon stop  */ xmlAddChild(node, genBgpmonTimeNode(bmf));  break; }
        case BMF_TYPE_QUEUES_STATUS:{ /* queue status */ xmlAddChild(node, genBgpmonTimeNode(bmf)); break;  }
    }
    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the STATUS_MSG node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genStatusMsgNode(BMF bmf)
{
    xmlNodePtr status_node = NULL;

    /*
     * Creates BGP_MESSAGE node
     */
    status_node = xmlNewNode(NULL, BAD_CAST "STATUS_MSG");

    switch ( bmf->type )
    {
        /* bgpmon status  */  
        case BMF_TYPE_BGPMON_START:
        case BMF_TYPE_BGPMON_STOP:
		case BMF_TYPE_QUEUES_STATUS:
        {
            xmlAddChild(status_node, genBgpmonStatusNode(bmf));
            xmlAddChild(status_node, genQueueStatusNode(bmf));
            break;
        }
        /* chain status */
		case BMF_TYPE_CHAINS_STATUS:
        {
            xmlAddChild(status_node, genChainStatusNode(bmf));
            break;
        }
        /* session state change */
        case BMF_TYPE_FSM_STATE_CHANGE:
        {
            if ( getSessionByID(bmf->sessionID) == NULL ) return 0;
            xmlAddChild(status_node, genSessionStatusNode(bmf));
            break;
        }
        /* session status */
        case BMF_TYPE_SESSION_STATUS:
        {
            xmlAddChild(status_node, genSessionStatusNode(bmf));
            break;
        }
        /* MRT status */
        case BMF_TYPE_MRT_STATUS:
        {
            xmlAddChild(status_node, genMRTStatusNode(bmf));
            break;
        }
    }

    return status_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate the TABLE_STOP node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Mikhail Strizhov @ Feb 15, 2011
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genTableStopNode(BMF bmf)
{
    xmlNodePtr node = NULL;

    //Creates BGP_MESSAGE node
    node = xmlNewNode(NULL, BAD_CAST "TABLE_STOP_MSG");

    // get value from bmf message
    u_int32_t counter =  ntohl(*((u_int32_t *) (bmf->message))); 

    // Counter 
    xmlNewPropUnsignedInt( node, "counter", counter);

    return node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate BGP_MESSAGE node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpMessageNodeWithDummyLength(BMF bmf)
{
    xmlNodePtr bgp_message_node = NULL; /* node pointers */

    /*
    * Creates BGP_MESSAGE node
    */
    bgp_message_node = xmlNewNode(NULL, BAD_CAST "BGP_MESSAGE");

    int xml_len = XML_BUFFER_LEN;
    char *type_str;

    xmlNewPropInt(bgp_message_node,    "length",  xml_len);  /* dummy length with the maximum length */
    xmlNewPropString(bgp_message_node, "version", _VERSION); /* XFB version */
    xmlNewPropString(bgp_message_node, "xmlns",   _XMLNS);   /* XFB namespace */

    /* Child nodes */
    switch ( bmf->type )
    {
        case BMF_TYPE_MSG_TO_PEER:
        case BMF_TYPE_MSG_FROM_PEER:
        case BMF_TYPE_MSG_LABELED:
        {
            /* sequence num  */ xmlAddChild(bgp_message_node, genSequenceNode());
            /* time          */ xmlAddChild(bgp_message_node, genTimeNode(bmf));
            /* peering       */ xmlAddChild(bgp_message_node, genPeeringNode(bmf));
            /* ascii message */ xmlAddChild(bgp_message_node, genAsciiMsgNode(bmf));
            /* octet message */ xmlAddChild(bgp_message_node, genOctetMsgNode(bmf));

            PBgpHeader hdr       = NULL;
            u_int16_t  bgpType   = 0;
            hdr       = (PBgpHeader)(bmf->message);
            bgpType   = hdr->type;
            type_str = getAsciiMsgType(bgpType);

            xmlNewPropInt(bgp_message_node,    "type_value",  bgpType); /* bgpmon message type value */
            xmlNewPropString(bgp_message_node, "type",        type_str);  /* bgpmon message type */
            break;
        }
        case BMF_TYPE_TABLE_TRANSFER:
        {
            /* sequence num  */ xmlAddChild(bgp_message_node, genSequenceNode());
            /* time          */ xmlAddChild(bgp_message_node, genTimeNode(bmf));
            /* peering       */ xmlAddChild(bgp_message_node, genPeeringNode(bmf));
            /* pseudo message*/ xmlAddChild(bgp_message_node, genAsciiMsgNode(bmf));
            /* octet message */ xmlAddChild(bgp_message_node, genOctetMsgNode(bmf));
            type_str = "TABLE";

            xmlNewPropInt(bgp_message_node,    "type_value",  bmf->type); /* bgpmon message type value */
            xmlNewPropString(bgp_message_node, "type",        type_str);  /* bgpmon message type */
            break;
        }
	/* Table start messages */
	case BMF_TYPE_TABLE_START:
        {
            /* sequence num   */ xmlAddChild(bgp_message_node, genSequenceNode());
            /* time           */ xmlAddChild(bgp_message_node, genTimeNode(bmf));
            /* peering       */  xmlAddChild(bgp_message_node, genPeeringNode(bmf));
            type_str = "TABLE_START";

            xmlNewPropInt(bgp_message_node,    "type_value",  bmf->type); /* bgpmon message type value */
            xmlNewPropString(bgp_message_node, "type",        type_str);  /* bgpmon message type */
            break;
        }
	/* Table sttop messages */
	case BMF_TYPE_TABLE_STOP:
        {
            /* sequence num   */ xmlAddChild(bgp_message_node, genSequenceNode());
            /* time           */ xmlAddChild(bgp_message_node, genTimeNode(bmf));
            /* peering       */  xmlAddChild(bgp_message_node, genPeeringNode(bmf));
	    /* stop message  */  xmlAddChild(bgp_message_node, genTableStopNode(bmf));
            type_str = "TABLE_STOP";

            xmlNewPropInt(bgp_message_node,    "type_value",  bmf->type); /* bgpmon message type value */
            xmlNewPropString(bgp_message_node, "type",        type_str);  /* bgpmon message type */
            break;
        }
        /* State change messages */
        case BMF_TYPE_FSM_STATE_CHANGE:
        {
            /* sequence num   */ xmlAddChild(bgp_message_node, genSequenceNode());
            /* time           */ xmlAddChild(bgp_message_node, genTimeNode(bmf));
            /* peering        */ xmlAddChild(bgp_message_node, genPeeringNode(bmf));
            /* status message */ xmlAddChild(bgp_message_node, genStatusMsgNode(bmf));
            type_str = "STATUS";

            xmlNewPropInt(bgp_message_node,    "type_value",  bmf->type); /* bgpmon message type value */
            xmlNewPropString(bgp_message_node, "type",        type_str);  /* bgpmon message type */
            break;
        }
        /* Status messages */
	case BMF_TYPE_CHAINS_STATUS:
	case BMF_TYPE_QUEUES_STATUS:
	case BMF_TYPE_SESSION_STATUS:
        case BMF_TYPE_MRT_STATUS:
        case BMF_TYPE_BGPMON_START:
        case BMF_TYPE_BGPMON_STOP:
        {
            /* sequence num   */ xmlAddChild(bgp_message_node, genSequenceNode());
            /* time           */ xmlAddChild(bgp_message_node, genTimeNode(bmf));
            /* status message */ xmlAddChild(bgp_message_node, genStatusMsgNode(bmf));
            type_str = "STATUS";

            xmlNewPropInt(bgp_message_node,    "type_value",  bmf->type); /* bgpmon message type value */
            xmlNewPropString(bgp_message_node, "type",        type_str);  /* bgpmon message type */
            break;
        }
        default:
        {
            log_err ("BMF2XML: unknown type!");
            type_str = "UNKNOWN";

            xmlNewPropInt(bgp_message_node,    "type_value",  bmf->type); /* bgpmon message type value */
            xmlNewPropString(bgp_message_node, "type",        type_str);  /* bgpmon message type */
            break;
        }
    }

    return bgp_message_node;
}

/*----------------------------------------------------------------------------------------
 * Purpose: generate BGP_MESSAGE node
 * input:   bmf - our internal BMF message
 * Output:  the new xml node
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
xmlNodePtr
genBgpMessageNodeWithStr(BMF bmf, char* xml)
{
    xmlNodePtr bgp_message_node = NULL; /* node pointers */

    /*-------------------------------------------------------
     * Create a BGP_MESSAGE node with dummy length attribute
     *------------------------------------------------------*/
    bgp_message_node = genBgpMessageNodeWithDummyLength(bmf);

    /*-------------------------------------------------------
     * Steps to replace the dummy length attribute
     * . Dump XML to a string buffer
     * . Obtain the string length
     * . Replace dummy length with the real length value
     *------------------------------------------------------*/
    xmlBufferPtr buff = xmlBufferCreate();
    xmlNodeDump(buff, NULL, bgp_message_node, 0, 0);

    static char format[XML_TEMP_BUFFER_LEN];
    static char length_str[XML_TEMP_BUFFER_LEN];
    static char old_length_str[XML_TEMP_BUFFER_LEN];
    static char new_length_str[XML_TEMP_BUFFER_LEN];
    format[0] = '\0';

    /* Generate format */
    if (_XML_LEN_DIGITS == 0) 
        _XML_LEN_DIGITS = ceil(log10(XML_BUFFER_LEN));

    sprintf(format, "%s%d.%dd", "%", _XML_LEN_DIGITS, _XML_LEN_DIGITS);
    /* Genearte length string from format and REAL length */
    sprintf(length_str, format, strlen((char *)buff->content));
    strcpy(xml,  (char *)buff->content);

    /* Replace length string */
    sprintf(old_length_str, "length=\"%d\"", XML_BUFFER_LEN); /* Old length string */
    sprintf(new_length_str, "length=\"%s\"", length_str);     /* New length string */
    replace_str(xml, old_length_str, new_length_str);

    /* Update length attribute */
    xmlSetProp (bgp_message_node, BAD_CAST "length", BAD_CAST length_str);
    xmlBufferFree(buff);  /* free xml buffer       */

    return bgp_message_node;
}


/*----------------------------------------------------------------------------------------
 * Purpose: entry fucntion which converts all types of BMF messages to XML text representations
 * input:   bmf - our internal BMF message
 *          xml - pointer to the buffer used to store the result XML string
 *          maxlen - max length of the buffer
 * Pei-chun Cheng @ Dec 20, 2008
 * -------------------------------------------------------------------------------------*/
int BMF2XMLDATA(BMF bmf, char *xml, int maxlen)
{
    xmlInitParser();

    xmlDocPtr  doc              = NULL;  /* document pointer */
    xmlNodePtr bgp_message_node = NULL;  /* xml node pointer */

    /*-------------------------------------------------------
     * Implementation:
     *------------------------------------------------------*/
    doc = xmlNewDoc(BAD_CAST "1.0");
    bgp_message_node = genBgpMessageNodeWithStr(bmf, xml);
    xmlDocSetRootElement(doc, bgp_message_node);
    strcpy(_XML, xml);

    /*-------------------------------------------------------
     * Clean up
     *------------------------------------------------------*/
    xmlFreeDoc(doc);      /* Free the xml document */

    return strlen(xml);
}


/* vim: sw=4 ts=4 sts=4 expandtab
 */
