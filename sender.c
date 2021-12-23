// File: sender.c

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>  // exit
#include <time.h>    //time
#include "ABP.h"
#include "unreliableSend.h"

#define SERVER_PORT 50000
#define MAX_LINE 1024

char* readString (char *buf,int len){
  char *s;
  while ((s=fgets(buf,len,stdin))==0 && !feof(stdin));
  return s;
}

int main (int argc, char *argv[]) {
  //  FILE *fp;
  struct hostent *hp;
  struct sockaddr_in sin;
  char *host;
  char buf[MAX_LINE];
  int s;
  int packetPlace;
  int len;
  int ilen;
  int startTime, endTime, totalTime;

  if (argc==2) {
    host = argv[1];
  }
  else {
    perror("usage: client <hostname>");
    exit (1);
  }

  // initialize stopwait send
  if(ABP_sendInit(host,SERVER_PORT)){
    printf("sendInit Failed\n");
    exit (1);
  }

  // set failure probability of outgoing packets
  US_SetFailureProb (5);

  startTime = time(NULL); 

  printf("Control D terminates \n");
  // main loop get and send lines of text
  packetPlace = 1;
  while (packetPlace <= 1024){
    for(int i = 0; i < 1024; i++){
       if(packetPlace%2 != 0) {
          buf[i] = 1;
       }
      else {
        buf[i] = 0;
      }
    }
    ABP_send(buf,1024);
    packetPlace = packetPlace + 1;
    }
  printf ("eof encountered - thanks!\n");

  // now wait for all mesages to arrive
  ABP_flush();

  endTime = time(NULL);
  totalTime = endTime - startTime;
  
  printf ("All data has been successfully received!\n");
  printf ("The transfer took %i seconds\n", totalTime );

}

    
  
