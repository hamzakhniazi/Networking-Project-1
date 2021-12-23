//
// File: checksum-checker-client.c
//
// Description: This program takes the word on the command line, creates a
// packet containing that word and a CRC, and sends it to a CRC server.  The
// server checks the calculation of the CRC and sends back a response that
// indicates whether the CRC was calculated correctly.  This program simply
// prints out the response received from the server.
//
// This could be useful for students to check their checksum or CRC calculations.
//
// A typical invocation of this would be:
//
//    checksum-checker-client alucard.csc.depauw.edu NetworkIsFun
//

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <netdb.h>

// #include the checksum calculator file here.
#include "calcChecksum.h"

#define ECHOMAX 255     /* Longest string to echo */
#define BUF_SIZE 16

// define the port number for the appropriate server.  
#define SERVER_PORT 50001 /* for Internet Checksum */

struct msgStruct {
  char data[BUF_SIZE];
  unsigned int crc;
};

void DieWithError(char *errorMessage)
{
  perror(errorMessage);
  exit(1);
}

int main(int argc, char *argv[])
{
  int sock;                        /* Socket descriptor */
  struct sockaddr_in echoServAddr; /* Echo server address */
  struct sockaddr_in fromAddr;     /* Source address of echo */
  unsigned int fromSize;           /* In-out of address size for recvfrom() */
  char *hostname;                  /* hostname of server */
  struct hostent *hp;
  struct msgStruct message;        /* message to send to server */
  char echoBuffer[ECHOMAX+1];      /* Buffer for receiving echoed string */
  int respStringLen;               /* Length of received response */
  char *echoString;
  
  if (argc != 3)    /* Test for correct number of arguments */
    {
      fprintf(stderr,"Usage: %s <Server hostname> <Echo Word>\n", 
	      argv[0]);
      exit(1);
    }
  
  hostname = argv[1];           /* First arg: server hostname */
  echoString = argv[2];

  // translate hostname into host's IP address
  hp = gethostbyname(hostname);
  if (!hp){
    perror ("sendInit: gethostbyname");
    return -1;
  }

  /* Create a datagram/UDP socket */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    DieWithError("socket() failed");
  
  /* Construct the server address structure */
  memset(&echoServAddr, 0, sizeof(echoServAddr));    /* Zero out structure */
  echoServAddr.sin_family = AF_INET;                 /* Internet addr family */
  memmove (&echoServAddr.sin_addr, hp->h_addr_list[0], hp->h_length);
  echoServAddr.sin_port   = htons(SERVER_PORT);     /* Server port */
  
  // copy the string to the message
  strncpy (message.data,echoString,BUF_SIZE);

  // Cacluate the CRC.  Here's where you need to call calcCRC and place the
  // result in message.crc

  // now send the message to the server
  if (sendto(sock,&message,sizeof(message), 0, (struct sockaddr *)
	     &echoServAddr, sizeof(echoServAddr)) != sizeof(message))
    DieWithError("sendto() sent a different number of bytes than expected");
  
  /* Recv a response */
  fromSize = sizeof(fromAddr);
  recvfrom(sock, echoBuffer, ECHOMAX,0,(struct sockaddr *) &fromAddr, 
	   &fromSize);
  
  if (echoServAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr)
    {
      fprintf(stderr,"Error: received a packet from unknown source.\n");
      exit(1);
    }
  
  printf("%s\n", echoBuffer);    /* Print the echoed arg */
  
  close(sock);
  exit(0);
}
