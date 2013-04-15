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
 *  File: bgpmessagetypes.h
 * 	Authors: Dave Matthews, He Yan, Pei-chun Cheng, Jason Bartlett
 *  Date: 18 Oct 2010
 */
 
#ifndef BGPMESSAGETYPES_H_
#define BGPMESSAGETYPES_H_

/* BGP Messages types defined in RFCs 4271, 2918
 * 
 * Defines the message types that appear in BGP packet headers.
 * 
 * Cisco specific value included to compatibility with older routers
 * whose software predates RFC 2918.
 */
 
enum bgpMessageTypes
{
	// defined in RFC 4271
	typeOpen = 1,
	typeUpdate,
	typeNotification,
	typeKeepalive,
	// defined in RFC 2918
	typeRouteRefresh = 5,
	// implementation specific
	typeRouteRefreshCisco = 128
};

/* BGP OPEN optional parameter  */
#define BGP_OPEN_OPT_AUTH           1
#define BGP_OPEN_OPT_CAP            2

/* BGP MP attributes */
#define BGP_AS_PATH            	    2
#define BGP_MP_REACH                14
#define BGP_MP_UNREACH              15

/* AFI  */
#define BGP_AFI_IPv4                1
#define BGP_AFI_IPv6                2

/* SAFI */
#define BGP_MP_SAFI_UNICAST         1
#define BGP_MP_SAFI_MULTICAST       2
#define BGP_MP_SAFI_UNI_MULTICAST   3
#define BGP_MP_SAFI_MPLS            4
#define BGP_MP_SAFI_ENCAP			7

/* Tunnel Encapsulation Types */
#define ENCAP_L2TPV3_IP				1
#define ENCAP_GRE					2
#define ENCAP_IP_IN_IP				7
#define ENCAP_TRANS_END				3	//RFC 5566
#define ENCAP_IPSEC_TUN				4	//RFC 5566
#define ENCAP_IP_IP_IPSEC			5	//RFC 5566
#define ENCAP_MPLS_IP				6	//RFC 5566

/* Tunnel Encapsulation Sub-TLV Types */
#define BGP_ENCAP_TLV				1
#define BGP_PROTO_TLV				2
#define BGP_COLOR_TLV				4
#define BGP_TUNNEL_AUTH_TLV			3	//RFC 5566
#define BGP_LOAD_BAL_TLV			5	//RFC 5640

/* Traffic Engineering Encoding values */
#define LSP_PACKET_ENC				1
#define LSP_ETHERNET_ENC			2
#define LSP_PDH_ENC					3
#define LSP_G707_T1105_ENC			5
#define LSP_DIG_WRAP_ENC			7
#define LSP_LAMBDA_ENC				8
#define LSP_FIBER_ENC				9
#define LSP_FIBERCHANNEL_ENC		11

/* Traffic Engineering Switching Types */
#define LSC_PSC1					1
#define LSC_PSC2					2
#define LSC_PSC3					3
#define LSC_PSC4					4
#define LSC_L2SC					51
#define LSC_TDM						100
#define LSC_LSC						150
#define LSC_FSC						200


/* The following constants are from Mrt/bgp_attr.h */
/* http://www.mrt.net */

/* BGP Attribute flags. */ 
#define BGP_ATTR_FLAG_OPTIONAL  0x80
#define BGP_ATTR_FLAG_TRANS     0x40
#define BGP_ATTR_FLAG_PARTIAL   0x20
#define BGP_ATTR_FLAG_EXT_LEN   0x10

/* BGP update origin.  */
#define BGP_ORIGIN_IGP                           0
#define BGP_ORIGIN_EGP                           1
#define BGP_ORIGIN_INCOMPLETE                    2

/* BGP4 attribute type codes.  */
#define BGP_ATTR_ORIGIN                          1
#define BGP_ATTR_AS_PATH                         2
#define BGP_ATTR_NEXT_HOP                        3
#define BGP_ATTR_MULTI_EXIT_DISC                 4
#define BGP_ATTR_LOCAL_PREF                      5
#define BGP_ATTR_ATOMIC_AGGREGATE                6
#define BGP_ATTR_AGGREGATOR                      7
#define BGP_ATTR_COMMUNITIES                     8
#define BGP_ATTR_ORIGINATOR_ID                   9
#define BGP_ATTR_CLUSTER_LIST                   10
#define BGP_ATTR_DPA                            11
#define BGP_ATTR_ADVERTISER                     12
#define BGP_ATTR_RCID_PATH                      13
#define BGP_ATTR_MP_REACH_NLRI                  14
#define BGP_ATTR_MP_UNREACH_NLRI                15
#define BGP_ATTR_EXT_COMMUNITIES                16
#define BGP_ATTR_AS4_PATH                       17
#define BGP_ATTR_AS4_AGGREGATOR                 18
#define BGP_ATTR_AS_PATHLIMIT                   21
#define BGP_ATTR_TUNNEL_ENCAP					23
#define BGP_ATTR_TRAFFIC_ENGR					24
#define BGP_ATTR_IPV6_EXT_COM					25

#define ASN16_LEN  16                                
#define ASN32_LEN  32                               


#endif /*BGPMESSAGETYPES_H_*/
