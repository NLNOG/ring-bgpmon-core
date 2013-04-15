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
 *  File: address.c
 * 	Authors:  Kevin Burnett, Dan Massey
 *  Date: July 21, 2008 
 */


// needed for malloc and free
#include <stdlib.h>

// needed for memset, strcmp and so on
#include <string.h>

// needed for function inet_pton
#include <arpa/inet.h>

// needed for logging
#include "log.h"

// its own head file
#include "address.h"

// needed for TRUE/FALSE definitions
#include "bgpmon_defaults.h"

// needed for errno
#include <errno.h>

// needed for bind
#include <sys/socket.h>
#include <netinet/in.h>

// needed for DEFAULT_LOGIN_PORT
#include "../site_defaults.h"

// needed for close()
#include <unistd.h>

/*--------------------------------------------------------------------------------------
 * Purpose: Create the addrinfo struct based a add and port in 
 * Input:  1. addr: IPv4,  IPv6 address or four keywords(ipv4any, ipv6any, ipv4loopback, ipv6loopback). 
 *		 2. port: a integer from 0 to 65535
 *
 * Output:  the created addrinfo struct
 *
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
struct addrinfo *createAddrInfo( char *addr, int portnumber)
{
	// convert the port from int to string
	char * port = malloc(6 * sizeof(char));
	memset(port, 0, 6);
	sprintf(port, "%d", portnumber);

	struct addrinfo hints, *res0, *res=NULL;
	memset(&hints, 0x00, sizeof(hints));

#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif
	
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
	if(strcmp(addr, IPv4_ANY) == 0 || strcmp(addr, IPv6_ANY) == 0)
		hints.ai_flags |= AI_PASSIVE;
	int rc;

	if(strcmp(addr, IPv4_LOOPBACK) == 0 || strcmp(addr, IPv6_LOOPBACK) == 0 
		|| strcmp(addr, IPv4_ANY) == 0 ||  strcmp(addr, IPv6_ANY) == 0)
		rc = getaddrinfo(NULL, port, &hints, &res);
	else
		rc = getaddrinfo(addr, port, &hints, &res);

	if (rc != 0)
	{
		log_msg("getAddrInfo: Host (%s, %s) not found --> %s\n", addr, port, gai_strerror(rc));
		if (rc == EAI_SYSTEM)
			log_msg("getaddrinfo() failed"); 		
	}

	if(strcmp(addr, IPv4_LOOPBACK) == 0 || strcmp(addr, IPv6_LOOPBACK) == 0 
		|| strcmp(addr, IPv4_ANY) == 0 ||  strcmp(addr, IPv6_ANY) == 0)
	{
		for(res0 = res; res0!=NULL; res0 = res0->ai_next)
		{
			if(strcmp(addr, IPv4_LOOPBACK) == 0 || strcmp(addr, IPv4_ANY) == 0)
			{
				if(res0->ai_family == 2)
				{
					char a[100];
					char p[100];
					getnameinfo((struct sockaddr *)res0->ai_addr, res0->ai_addrlen, a, sizeof(a), p, sizeof(p), NI_NUMERICHOST | NI_NUMERICSERV);
					debug(__FUNCTION__, "address: %s, port: %s ", a, p);
					free(port);
					return res0;
				}
				
			}
			else if(strcmp(addr, IPv6_LOOPBACK) == 0 || strcmp(addr, IPv6_ANY) == 0)
			{
				if(res0->ai_family == 10)
				{
					char a[100];
					char p[100];
					getnameinfo((struct sockaddr *)res0->ai_addr, res0->ai_addrlen, a, sizeof(a), p, sizeof(p), NI_NUMERICHOST | NI_NUMERICSERV);
					debug(__FUNCTION__, "address: %s, port: %s ", a, p);
					free(port);
					return res0;
				}
			}
		}
		free(port);
		log_fatal("Shoud never reach here!");
		return NULL;
	}
	else {
		free(port);
		return res;
	}
}

/*------------------------------------------------------------------------------
 * Purpose: check an address based on the 'passive/active flag'
 *   The address in the configuration file should one of them:
 *    1. IPv4 address (1.2.3.4)
 *    2. IPv6 address (fe80::211:11ff:fea1:6361%eth0)
 *    3. IPv4 any address (ipv4any)
 *    4. IPv6 any address (ipv6any)
 *    5. IPv4 loopback address (ipv4loopback) = 127.0.0.1
 *    6. IPv6 loopback address (ipv6loopback) = ::1
 *
 * Input: 1. addr: IPv4,IPv6 address or four keywords
 *           (ipv4any, ipv6any, ipv4loopback, ipv6loopback).
 *        2. passive: ADDR_PASSIVE(1) means passive address which must be one
 *           of local addresses or addrany.
 *
 *          ADDR_ACTIVE(0) means active address which cannot be addrany.
 * Output:  ADDR_VALID(0) this is a good address
 *          ADDR_ERR_INVALID_FORMAT(-1)  this is a invalid-format address
 *          ADDR_ERR_INVALID_PASSIVE(-2) this is not a valodpassive address
 *          ADDR_ERR_INVALID_ACTIVE(-3)  this is not a valodpassive address
 * Note:        
 * He Yan @ September 3, 2008   
 * ---------------------------------------------------------------------------*/
int checkAddress( char *addr, int passive )
{

	// first check if the addr is in a valid format(IPv4 or IPv6)
	struct in6_addr addr_s; 
	int rc = inet_pton(AF_INET, addr, &addr_s);
	if (rc != 1)    /* if it is a valid IPv4 address */
	{
		rc = inet_pton(AF_INET6, addr, &addr_s);
		if (rc != 1) /* if it is a valid IPv6 address*/
		{
			if(strcmp(addr, IPv4_ANY) != 0 && strcmp(addr, IPv6_ANY) != 0
					&& strcmp(addr, IPv4_LOOPBACK) != 0 && strcmp(addr, IPv6_LOOPBACK) != 0)
				return ADDR_ERR_INVALID_FORMAT;
		}
	}

	// check if it is a valid passive addr
	struct addrinfo * testRes = NULL;
	int testSock;
	if ( passive == ADDR_PASSIVE )
	{
		if(strcmp(addr, IPv4_ANY) != 0 && strcmp(addr, IPv6_ANY) != 0
				&& strcmp(addr, IPv4_LOOPBACK) != 0 && strcmp(addr, IPv6_LOOPBACK) != 0)
		{
			// use DEFAULT_LOGIN_PORT to test address, sorry Kevin.
			testRes = createAddrInfo( addr, DEFAULT_LOGIN_PORT );
			if( testRes == NULL )
			{
				return ADDR_ERR_INVALID_FORMAT;
			}
			else
			{
				testSock = socket(testRes->ai_family, testRes->ai_socktype, testRes->ai_protocol);
				// bind to test if the address a local
				if (bind(testSock, testRes->ai_addr, testRes->ai_addrlen) < 0)
				{
					if(errno != EADDRINUSE)
					{
						close(testSock);
						return ADDR_ERR_INVALID_PASSIVE;
					}
				}
				close(testSock);
			}
		}
	}

	// check if it is a valid active addr
	if ( passive == ADDR_ACTIVE )
	{
		if ( strcmp(addr, IPv4_ANY) == 0 || strcmp(addr, IPv6_ANY) == 0 )
			return ADDR_ERR_INVALID_ACTIVE; 
	}
	free(testRes);
	return ADDR_VALID;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Given a port number, checks to see if the port is >0 and <=65535
 * Input: port - the port number that needs to be checked
 * Output: returns 0 on success and 1 on failure
 * Kevin Burnett @ October 25, 2008
 * -------------------------------------------------------------------------------------*/
int checkPort(int port) 
{
	if(port>0 && port<=65535)
		return 0;
	return 1;
}

/*--------------------------------------------------------------------------------------
 * Purpose: Set the passed in addrinfo pointer based a addr and port passed in together. Call checkAddress inside.
 * Input:  1. addr: IPv4,  IPv6 address or four keywords(ipv4any, ipv6any, ipv4loopback, ipv6loopback). 
 *		 2. port: a integer from 0 to 65535
 *		 3. ai: the passed in addrinfo pointer, will be modified inside this function.
 * 		 3. passive: ADDR_PASSIVE(1) means passive address which must be one of local addresses or addrany.
 *			   	   ADDR_ACTIVE(0) means active address which cannot be addrany.	
 *
 * Output:  ADDR_VALID(0) means it is a good address
 *		   ADDR_ERR_INVALID_FORMAT(-1) means this is a invalid-format address
 *		   ADDR_ERR_INVALID_PASSIVE(-2) means this is not a valodpassive address
 *		   ADDR_ERR_INVALID_ACTIVE(-3) means this is not a valodpassive address
 *
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int setAddrInfo( char *addr, int portnumber, struct addrinfo** ai, int passive)
{
	// check if the addr is valid
	int result = checkAddress(addr, passive);
	if(result != ADDR_VALID)
	{
		*ai = NULL;
		return result;
	}

	*ai = createAddrInfo(addr, portnumber);
	return ADDR_VALID;
}


/*--------------------------------------------------------------------------------------
 * Purpose: Given a sockaddr structure, return address(char *) and port number(int)
 * Input:  1. addr: pointer of struct sockaddr
 *		 2. address: address passing back to called
 *		 2. port: port passing back to called	
 *
 * Output:  0 means ok, -1 means errors.
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
int getAddressFromSockAddr(struct sockaddr *addr, char **address, int *port)
{
	struct sockaddr_in * sa4;
	struct sockaddr_in6 * sa6;
	char *addrBuf = malloc(ADDR_MAX_CHARS);
	memset(addrBuf, 0, ADDR_MAX_CHARS);
	switch(addr->sa_family) 
	{
		case AF_INET:
			sa4 = (struct sockaddr_in*)addr;
			if( inet_ntop(AF_INET, &sa4->sin_addr, addrBuf, ADDR_MAX_CHARS) == NULL )
			{
				free(addrBuf);
				return -1;
			}
			else
			{
				*address = addrBuf;
				*port = sa4->sin_port;
			}
			break;
		case AF_INET6:
			sa6 = (struct sockaddr_in6*)addr;
			if( inet_ntop(AF_INET6, &sa6->sin6_addr, addrBuf, ADDR_MAX_CHARS) == NULL )
			{
				free(addrBuf);
				return -1;
			}
			else
			{
				*address = addrBuf;
				*port = sa6->sin6_port;			}
			
			break;
		default:
			free(addrBuf);
			return -1;
	}
	return 0;
}


