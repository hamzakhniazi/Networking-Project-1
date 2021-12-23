// File: receiver.c

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "ABP.h"
#include "unreliableSend.h"
#include <stdbool.h>

#define MAX_LINE 1024

#define MAX_PENDING 5
#define SERVER_PORT 50000
int main (int argc, char *argv[]) {
  char buf[MAX_LINE];
  int len;
  int packetPlace = 1;
  bool correctRec = true;
  // intialize reveiver
  if(ABP_recvInit(SERVER_PORT)<0)
    printf ("recvinit failed\n");

  // set failure probability for acks
  US_SetFailureProb (5);

  // wait for message  and print text
  while (packetPlace <= 1024) {
    printf ("\n");
    ABP_recv (buf,&len);
    for (int i = 0; i < 1024; i++) {
       int num = packetPlace %2;
       if( num != buf[i]) {
          correctRec = false;
       }
    }
   if(correctRec) {
    printf ("packet received:\n");
   }
   else {
     printf("Error in message");
   }
    packetPlace = packetPlace + 1; 
  }
}

