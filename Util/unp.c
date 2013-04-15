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
 *  Adapted from:
 *  W. Richard Stevens, Bill Fenner, and Andrew M. Rudoff, 
 *  Unix Network Programming Volume 1 The Sockets Networking API
 *  Pearson Education, 2004
 */

/* Adapted from:
 * 
 * W. Richard Stevens, Bill Fenner, and Andrew M. Rudoff, 
 * Unix Network Programming Volume 1 The Sockets Networking API
 * Pearson Education, 2004
 * 
 * These routines handle read and write operations on a socket
 * that might complete with fewer bytes read or written than 
 * specified.  This is not an error condition and may occur due
 * to buffer limits for sockets.  The operation must be
 * repeated until the specified number of characters are 
 * processed.
 */


#include "unp.h"
#include "log.h"

ssize_t
readn( int fd, void *vptr, size_t n)
{
	/* Read N bytes from a socket.
	 */
	u_char 			*ptr;
	ptr = vptr;

	size_t 		nleft;
	ssize_t 	nread;
	nleft = n;
	while (nleft > 0)
	{
		if ((nread = read(fd, ptr, nleft)) < 0)
		{
			if (errno == EINTR)
				nread = 0;
			else if (errno == EWOULDBLOCK)
			{
				log_err("Read socket timeout!!!!");
				return(-1);
			}
			else
				return(-1);
		}
		else if (nread == 0) // EOF
			break;

		nleft -= nread;
		ptr += nread;
	}
	return (n - nleft); // return >= 0
}

ssize_t
writen(int fd, const void *vptr, size_t n)
{
	/* Write N bytes to a socket.
	 */
	const u_char *ptr;
	ptr = vptr;

	size_t nleft;
	ssize_t nwritten;
	nleft = n;
	while (nleft > 0)
	{
		if ( (nwritten = write(fd, ptr, nleft)) <= 0 )
		{
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0; // call write again
			else
				return(-1); // error
		}
		
		nleft -= nwritten;
		ptr += nwritten;
	}
	return( n );
}

