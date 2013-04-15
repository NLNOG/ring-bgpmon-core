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
 *  File: rtable_t.c
 *  Authors: Catherine Olschanowsky
 *  Date: June 2012
 */
#include <CUnit/Basic.h>
#include <stdlib.h>
#include <stdio.h>
#include "rtable.h"

/* a few global variables to play with across tests */



void
testRTABLE_stringToPrefixV6(void){

  PAddress *prefix = stringToPrefix("");
  CU_ASSERT(prefix == NULL);
  prefix = stringToPrefix(NULL);
  CU_ASSERT(prefix == NULL);
  prefix = stringToPrefix("1.2:3.4/128");
  CU_ASSERT(prefix == NULL);
  prefix = stringToPrefix("102:304:506:708:910.7/128");
  CU_ASSERT(prefix == NULL);
  prefix = stringToPrefix("1A:2B:3C:4D::/0");
  CU_ASSERT(prefix == NULL);

  Prefix *test_prefix = malloc(sizeof(Prefix) + (PREFIX_SIZE(128)));
  test_prefix->afi = 2;
  PAddress* testAddr = &test_prefix->addr;
  int i;
  for(i=0;i<16;i++){
    testAddr->paddr[i] = 255;
  }

  testAddr->p_len = 128;
  prefix = stringToPrefix("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF/128");
  CU_ASSERT(prefix->p_len==128);
  CU_ASSERT(prefixesEqual(prefix,testAddr));
  free(prefix);

  testAddr->p_len = 121;
  prefix = stringToPrefix("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFF0/121");
  testAddr->paddr[15] = 0x80;
  CU_ASSERT(prefix->p_len==121);
  CU_ASSERT(prefixesEqual(prefix,testAddr));
  free(prefix);

  testAddr->p_len = 120;
  prefix = stringToPrefix("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FF00/120");
  CU_ASSERT(prefix->p_len==120);
  testAddr->paddr[15] = 0x00;
  CU_ASSERT(prefixesEqual(prefix,testAddr));
  free(prefix);

  testAddr->p_len = 119;
  prefix = stringToPrefix("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF/119");
  CU_ASSERT(prefix->p_len==119);
  testAddr->paddr[14] = 0xFE;
  CU_ASSERT(prefixesEqual(prefix,testAddr));
  free(prefix);

  testAddr->p_len = 112;
  testAddr->paddr[14] = 0x00;
  prefix = stringToPrefix("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:/112");
  CU_ASSERT(prefix->p_len==112);
  CU_ASSERT(prefixesEqual(prefix,testAddr));
  free(prefix);

  testAddr->p_len = 112;
  testAddr->paddr[2] = 0x00;
  testAddr->paddr[3] = 0x00;
  testAddr->paddr[4] = 0x00;
  testAddr->paddr[5] = 0x00;
  testAddr->paddr[6] = 0x00;
  testAddr->paddr[7] = 0x00;
  prefix = stringToPrefix("FFFF::FFFF:FFFF:FFFF:/112");
  CU_ASSERT(prefix->p_len==112);
  CU_ASSERT(prefixesEqual(prefix,testAddr));
  free(prefix);

  testAddr->p_len = 112;
  testAddr->paddr[2] = 0x80;
  testAddr->paddr[8] = 0x00;
  testAddr->paddr[9] = 0x00;
  testAddr->paddr[10] = 0x00;
  testAddr->paddr[11] = 0x00;
  testAddr->paddr[12] = 0x00;
  testAddr->paddr[13] = 0x00;
  testAddr->paddr[14] = 0x00;
  testAddr->paddr[15] = 0x00;
  prefix = stringToPrefix("FFFF:8000::/112");
  CU_ASSERT(prefix->p_len==112);
  CU_ASSERT(prefixesEqual(prefix,testAddr));
  free(prefix);

  testAddr->p_len = 7;
  testAddr->paddr[0] = 0xfc;
  testAddr->paddr[1] = 0x00;
  prefix = stringToPrefix("fc00::/7");
  CU_ASSERT(prefix->p_len==7);
  CU_ASSERT(prefixesEqual(prefix,testAddr));

  char* str = printPrefix(test_prefix);
  CU_ASSERT(strcmp(str,"fc00:0:0:0:0:0:0:0/7")==0);
  free(prefix);
  free(str);

  free(test_prefix);
  return;
}

void
testRTABLE_stringToPrefixV4(void){

  PAddress *prefix = stringToPrefix("");
  CU_ASSERT(prefix == NULL);
  prefix = stringToPrefix(NULL);
  CU_ASSERT(prefix == NULL);
  prefix = stringToPrefix("1.2:3.4/32");
  CU_ASSERT(prefix == NULL);
  prefix = stringToPrefix("1.2.3.4/128");
  CU_ASSERT(prefix == NULL);
  prefix = stringToPrefix("1.2.3.4/0");
  CU_ASSERT(prefix == NULL);

  PAddress* testAddr = malloc(sizeof(PAddress)+4);
  testAddr->paddr[0] = 1;
  testAddr->paddr[1] = 2;
  testAddr->paddr[2] = 3;
  testAddr->paddr[3] = 4;
  testAddr->p_len = 32;
  prefix = stringToPrefix("1.2.3.4/32");
  CU_ASSERT(prefix->p_len == 32);
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  free(prefix);

  prefix = stringToPrefix("1.2.3.4/24");
  testAddr->p_len = 24;
  testAddr->paddr[3] = 0;
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  CU_ASSERT(prefix->p_len == 24);
  free(prefix);

  prefix = stringToPrefix("1.2.3.4/20");
  CU_ASSERT(prefix->p_len == 20);
  testAddr->p_len = 20;
  testAddr->paddr[2] = 0;
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  free(prefix);

  prefix = stringToPrefix("1.2.3.4/17");
  CU_ASSERT(prefix->p_len == 17);
  testAddr->p_len = 17;
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  free(prefix);

  prefix = stringToPrefix("1.2.3.4/8");
  CU_ASSERT(prefix->p_len == 8);
  testAddr->p_len = 8;
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  free(prefix);

  testAddr->paddr[0] = 255;
  testAddr->paddr[1] = 255;
  testAddr->paddr[2] = 255;
  testAddr->paddr[3] = 255;
  testAddr->p_len = 32;
  prefix = stringToPrefix("255.255.255.255/32");
  CU_ASSERT(prefix->p_len == 32);
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  free(prefix);

  testAddr->p_len = 24;
  prefix = stringToPrefix("255.255.255.0/24");
  CU_ASSERT(prefix->p_len == 24);
  testAddr->paddr[3] = 0;
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  free(prefix);

  testAddr->p_len = 20;
  prefix = stringToPrefix("255.255.240.0/20");
  CU_ASSERT(prefix->p_len == 20);
  testAddr->paddr[2] = 240;
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  free(prefix);

  testAddr->p_len = 17;
  prefix = stringToPrefix("255.255.128.0/17");
  CU_ASSERT(prefix->p_len == 17);
  testAddr->paddr[2] = 128;
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  free(prefix);

  testAddr->p_len = 8;
  prefix = stringToPrefix("255.0/8");
  testAddr->paddr[2] = 0;
  CU_ASSERT(prefix->p_len == 8);
  CU_ASSERT(prefixesEqual(testAddr,prefix));
  free(prefix);

  free(testAddr);
}

/* TEST: RTABLE_printPrefix
 */
void 
testRTABLE_printPrefixV4(void){

  int i;
  char *prefix_str;

  Prefix *test_prefix = malloc(sizeof(Prefix) + (PREFIX_SIZE(32)));
  CU_ASSERT_FATAL(test_prefix != NULL);

  test_prefix->afi = 1;
  for(i=0;i<4;i++){
    test_prefix->addr.paddr[i] = (i+1);
  }

  test_prefix->addr.p_len = 32;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"1.2.3.4/32",10) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 31;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"1.2.3.4/31",10) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 30;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"1.2.3.4/30",10) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 29;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"1.2.3.0/29",10) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 24;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"1.2.3.0/24",10) == 0);
  free(prefix_str);

  // testing 255.255.255.255
  for(i=0;i<4;i++){
    test_prefix->addr.paddr[i] = 255;
  }

  test_prefix->addr.p_len = 32;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"255.255.255.255/32",20) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 30;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"255.255.255.252/30",20) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 28;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"255.255.255.240/28",20) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 24;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"255.255.255.0/24",20) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 2;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"192.0.0.0/2",20) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 4;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"240.0.0.0/4",20) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 8;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"255.0.0.0/8",20) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 7;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"254.0.0.0/7",20) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 9;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"255.128.0.0/9",20) == 0);
  free(prefix_str);

  free(test_prefix);
  // end ipv4 tests

}

testRTABLE_printPrefixV6(void){

  int i;
  char *prefix_str;

  Prefix *test_prefix = malloc(sizeof(Prefix) + (PREFIX_SIZE(128)));
  CU_ASSERT_FATAL(test_prefix != NULL);
  // begin ipv6 tests
  test_prefix->afi = 2;
  
  for(i=0;i<16;i++){
    test_prefix->addr.paddr[i] = (i+1);
  }

  test_prefix->addr.p_len = 128;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"102:304:506:708:90a:b0c:d0e:f10/128",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 127;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"102:304:506:708:90a:b0c:d0e:f10/127",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 119;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"102:304:506:708:90a:b0c:d0e:e00/119",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 112;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"102:304:506:708:90a:b0c:d0e:0/112",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 110;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"102:304:506:708:90a:b0c:d0c:0/110",40) == 0);
  free(prefix_str);

  // try with FFFF*8
  for(i=0;i<16;i++){
    test_prefix->addr.paddr[i] = 255;
  }

  test_prefix->addr.p_len = 128;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 120;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ff00/120",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 113;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"ffff:ffff:ffff:ffff:ffff:ffff:ffff:8000/113",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 112;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"ffff:ffff:ffff:ffff:ffff:ffff:ffff:0/112",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 111;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"ffff:ffff:ffff:ffff:ffff:ffff:fffe:0/111",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 63;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"ffff:ffff:ffff:fffe:0:0:0:0/63",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 64;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"ffff:ffff:ffff:ffff:0:0:0:0/64",40) == 0);
  free(prefix_str);

  test_prefix->addr.p_len = 65;
  prefix_str = printPrefix(test_prefix);
  CU_ASSERT_FATAL(prefix_str != NULL);
  CU_ASSERT(strncmp(prefix_str,"ffff:ffff:ffff:ffff:8000:0:0:0/65",40) == 0);
  free(prefix_str);

  free(test_prefix);

}

/* The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
int init_RTABLE(void){
  return 0;
}

/* The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
int clean_RTABLE(void){
  return 0;
}
