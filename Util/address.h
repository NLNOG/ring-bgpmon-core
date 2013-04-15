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
 *  File: address.h
 * 	Authors:  Kevin Burnett, Dan Massey
 *  Date: Oct 7, 2008 
 */

#ifndef ADDRESS_H_
#define ADDRESS_H_

// needed for addrinfo struct
#include <netdb.h>

// four address keywords
#define IPv4_LOOPBACK "ipv4loopback"
#define IPv6_LOOPBACK "ipv6loopback"
#define IPv4_ANY "ipv4any"
#define IPv6_ANY "ipv6any"


// address type
enum addrType { 
	ADDR_ACTIVE = 0,
	ADDR_PASSIVE
};

// address check error code
enum addrErrCode { 
	ADDR_VALID = 0,
	ADDR_ERR_INVALID_FORMAT = -1,
	ADDR_ERR_INVALID_PASSIVE = -2,
	ADDR_ERR_INVALID_ACTIVE = -3
};

/*--------------------------------------------------------------------------------------
 * Purpose: Create the addrinfo struct based a add and port in 
 * Input:  1. addr: IPv4,  IPv6 address or four keywords(ipv4any, ipv6any, ipv4loopback, ipv6loopback). 
 *		 2. port: a integer from 0 to 65535
 *
 * Output:  the created addrinfo struct
 *
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
struct addrinfo *createAddrInfo( char *addr, int portnumber);

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
int setAddrInfo( char *addr, int portnumber, struct addrinfo** ai, int passive);


/*----------------------------------------------------------------------------------------
 * Purpose: check an address based on the 'passive/active flag'
 * 		    The address in the configuration file should one of them:
 *			1. IPv4 address (1.2.3.4)
 *			2. IPv6 address (fe80::211:11ff:fea1:6361%eth0)
 *			3. IPv4 any address (ipv4any)
 *			4. IPv6 any address (ipv6any)
 *			5. IPv4 loopback address (ipv4loopback) = 127.0.0.1
 *			6. IPv6 loopback address (ipv6loopback) = ::1
 *
 * Input: 1. addr: IPv4,  IPv6 address or four keywords(ipv4any, ipv6any, ipv4loopback, ipv6loopback).
 * 		2. passive: ADDR_PASSIVE(1) means passive address which must be one of local addresses or addrany.
 *			   	   ADDR_ACTIVE(0) means active address which cannot be addrany.			
 *
 * Output:  ADDR_VALID(0) means it is a good address
 *		   ADDR_ERR_INVALID_FORMAT(-1) means this is a invalid-format address
 *		   ADDR_ERR_INVALID_PASSIVE(-2) means this is not a valodpassive address
 *		   ADDR_ERR_INVALID_ACTIVE(-3) means this is not a valodpassive address
 * Note:        
 * He Yan @ September 3, 2008   
 * -------------------------------------------------------------------------------------*/
 int checkAddress( char *addr, int passive );

/*--------------------------------------------------------------------------------------
 * Purpose: Given a port number, checks to see if the port is >0 and <=65535
 * Input: port - the port number that needs to be checked
 * Output: returns 0 on success and 1 on failure
 * Kevin Burnett @ October 25, 2008
 * -------------------------------------------------------------------------------------*/
int checkPort(int port);

/*--------------------------------------------------------------------------------------
 * Purpose: Given a sockaddr structure, return address(char *) and port number(int)
 * Input:  1. addr: pointer of struct sockaddr
 *		 2. address: address passing back to called
 *		 3. port: port passing back to called	
 *
 * Output:  0 means ok, -1 means errors.
 * He Yan @ June 15, 2008
 * -------------------------------------------------------------------------------------*/
 
int getAddressFromSockAddr(struct sockaddr *addr, char **address, int *port);

#endif /*ADDRESS_H_*/
