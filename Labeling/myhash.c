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
 *  File: myhash.c
 * 	Authors: Yan Chen
 *  Data: May 31, 2007 
 */

#include <sys/types.h>
#include <stdio.h>

#include "myhash.h"
#include "../Util/log.h"


//#define DEBUG

/*----------------------------------------------------------------------------------------
 * Purpose: Hash function, used to computed the position of the prefix in the rib table
 * Input:   The pointer and len of the prefix, the rib table size
 * Return:  The position of prefix in the table
 * NOTE:    Now prefix_hash is same as attr_hash. A different hash function may be needed.
 * Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/
INDEX prefix_hash ( const u_char *key, u_int16_t len, u_int32_t table_size )
{
/*   INDEX hash_val = 0;
   while(len) {
      hash_val = ( hash_val << 11 ) + *key++;
      len--;
   }

#ifdef DEBUG
   debug(__FUNCTION__, "The computed prefix hash value is %ld.", hash_val % table_size);
#endif

   return hash_val % table_size;

*/

   u_int16_t i;
   INDEX hash_val = 0;
     
   for (i = 0; i < len; i++) {
      hash_val += key[i];
      hash_val += (hash_val << 10);
      hash_val ^= (hash_val >> 6);
   }
   hash_val += (hash_val << 3);
   hash_val ^= (hash_val >> 11);
   hash_val += (hash_val << 15);

#ifdef DEBUG
   debug(__FUNCTION__, "The computed attr hash value is %ld.", hash_val % table_size);
#endif

   return hash_val % table_size;


}


/*----------------------------------------------------------------------------------------
 * Purpose: Hash function, used to computed the position of the attr in the attr table
 * Input:   The ptr and len of the attr, the attr table size
 * Return:  The position of the attr in the table
 * NOTE:    We need a better hash function
 * Yan @ Nov 7, 2006
 * -------------------------------------------------------------------------------------*/
INDEX attr_hash ( const u_char *key, u_int16_t len, u_int32_t table_size )
{
   u_int16_t i;
   INDEX hash_val = 0;
     
   for (i = 0; i < len; i++) {
      hash_val += key[i];
      hash_val += (hash_val << 10);
      hash_val ^= (hash_val >> 6);
   }
   hash_val += (hash_val << 3);
   hash_val ^= (hash_val >> 11);
   hash_val += (hash_val << 15);

#ifdef DEBUG
   debug(__FUNCTION__, "The computed attr hash value is %ld.", hash_val % table_size);
#endif

   return hash_val % table_size;
}
