// File: calcChecksum.h
//
// Author: Hamza Sultan Khan Niazi
//
// Description:  function to calculate the 8-bit internet checksum
//
//
#ifndef _CALCCHECKSUM_H
#define _CALCCHECKSUM_H
#include <arpa/inet.h>  // htonl

int calcChecksum (unsigned char *buf,int length)
{
  int sum = 0;
  // process all bytes in buffer
  for (int i=0;i<length;i++) {
    sum = sum+buf[i];
    if(sum > 255) {
	sum = sum - 256;
	sum = sum + 1;
    }
  }
  
   // return 1's complement of sum
  
  return htonl( (~sum) & 0xff );
}

#endif
