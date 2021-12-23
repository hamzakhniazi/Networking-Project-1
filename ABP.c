//
// File: ABP.c
//
// Author: Hamza Sultan Khan Niazi
//
// Description: Implements the alternating bit protocol (i.e., stop and wait)
// defined in ABP.h
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include "calcChecksum.h"
#include "unreliableSend.h"
#include <sys/file.h>   // for FASYNC
#include <sys/time.h>   // timer
#include <stdio.h>
#include <string.h>     // memmove
#include <unistd.h>     // getpid, pause
#include "ABP.h"

// define constants and structs

#define ABP_PAYLOAD_SIZE  1024 /* this value MUST be a multiple of 4*/
#define ABP_TIMEOUT_SECS 0
#define ABP_TIMEOUT_USECS 250000
#define ABP_MAX_TIMEOUTS 25

struct ABP_dataMsg {
  unsigned char seqNum;
  int length;
  unsigned char data[ABP_PAYLOAD_SIZE];
  unsigned int crc;
};

struct ABP_ackMsg {
  unsigned char ackNum;
  unsigned int crc;
};

// define state variables

// status of the module
static int ABP_sendWait;
static int ABP_recvWait;

// socket variables and addresses
static int ABP_sendDataSock, ABP_recvDataSock;
static struct sockaddr_in ABP_sendDataAddr, ABP_recvDataAddr;

// sequence numbers for send and receive
static int ABP_nextSendSeqNum, ABP_nextRecvSeqNum;

// buffers for sending and receiving data and acks
static struct ABP_dataMsg ABP_sendMsg, ABP_recvMsg;

// number of timeouts
static int ABP_numTimeouts;

// indicates if a timeout is set, and if so, when it expires
static int ABP_sendTimeoutSet;
static struct timeval ABP_sendTimeout;

// define prototypes for asynchronous handlers
static void ABP_ackSIGIO (int signalType);
static void ABP_sendTimer(int signalType);
static void ABP_dataSIGIO (int signalType);

// define prototypes for utility routines
static void ABP_setSendTimeout ();
static void ABP_clearSendTimeout ();

///////////////////////////////////////////////////////////////////////////////
//
// ABP_sendInit
//
///////////////////////////////////////////////////////////////////////////////
int ABP_sendInit (char *hostname,short portNum)
{
  struct hostent *hp;
  struct sigaction handler1;
  struct sigaction handler2;
  struct itimerval timeVal;

  // translate hostname into host's IP address
  hp = gethostbyname(hostname);
  if (!hp){
    perror ("sendInit: gethostbyname");
    return -1;
  }

  // build address data structures
  memset (&ABP_sendDataAddr, 0, sizeof(ABP_sendDataAddr));
  ABP_sendDataAddr.sin_family = AF_INET;
  memmove (&ABP_sendDataAddr.sin_addr, hp->h_addr_list[0], hp->h_length);
  ABP_sendDataAddr.sin_port = htons(portNum);

  // create send socket
  if((ABP_sendDataSock = socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0){
    printf ("sendInit: socket error\n");
    return -1;
  }

  // set up SIGIO handler for received acks
  handler1.sa_handler = ABP_ackSIGIO;
  if (sigfillset (&handler1.sa_mask) < 0){
    printf ("sendInit: segfillset1 error\n");
    return -1;
  }
  handler1.sa_flags = 0;
  if (sigaction(SIGIO, &handler1, 0) < 0){
    printf ("sendInit: sigaction1 error\n");
    return -1;
  }

  if (fcntl(ABP_sendDataSock, F_SETOWN, getpid()) < 0){
    perror("sendInit:fcntl1 ");
    return -1;
  }
  if (fcntl(ABP_sendDataSock, F_SETFL, O_NONBLOCK|FASYNC) < 0){
    printf ("sendInit: fcntl2 error\n");
    return -1;
  }

  // no send timeouts yet
  ABP_sendTimeoutSet = 0;

  // set up timer handler so it ticks every tenth of a second
  if (sigfillset (&handler2.sa_mask) < 0){
    printf ("sendInit: segfillset2 error\n");
    return -1;
  }

  handler2.sa_handler = ABP_sendTimer;
  handler2.sa_flags = 0;
  if (sigaction(SIGALRM, &handler2, 0) < 0){
    printf ("sendInit: sigaction3 error\n");
    return -1;
  }

  timeVal.it_interval.tv_sec = 0;
  timeVal.it_interval.tv_usec = 100000;
  timeVal.it_value.tv_sec = 0;
  timeVal.it_value.tv_usec = 100000;
  if (setitimer (ITIMER_REAL,&timeVal,0) < 0) {
    perror ("ABP_sendInit: setitimer error");
    return -1;
  }
  
  // initialize initial sequence number
  ABP_nextSendSeqNum  = 0;

  // we're not waiting for an ack
  ABP_sendWait = 0;

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// ABP_send
//
///////////////////////////////////////////////////////////////////////////////
void ABP_send (char *buf, int length)
{
  sigset_t oldsigset,sigset;

  // wait until it's OK to proceed (i.e., we're not waiting for an ACK
  while (ABP_sendWait)
    pause();

  // can't send more than payload size
  if (length > ABP_PAYLOAD_SIZE)
    length = ABP_PAYLOAD_SIZE;

  // copy data into message buffer
  memmove (&ABP_sendMsg.data, buf, length);
  ABP_sendMsg.length = length;
  ABP_sendMsg.seqNum = ABP_nextSendSeqNum;
  // *** calculate checksum of ABP_sendMesg and place in ABP_sendMsg.crc ***
  ABP_sendMsg.crc=0;
  ABP_sendMsg.crc = calcChecksum( (char *)&ABP_sendMsg, sizeof(ABP_sendMsg) );

  // block SIGIO and SIGALRM so that we can't get a signal between
  // the sendto and setting the timers.
  sigemptyset (&sigset);
  sigaddset (&sigset,SIGALRM);
  sigaddset (&sigset,SIGIO);
  sigprocmask (SIG_BLOCK,&sigset,&oldsigset);

  // send the message
  US_sendto(ABP_sendDataSock,(char *)&ABP_sendMsg,sizeof(ABP_sendMsg),0,
	    (struct sockaddr *)&ABP_sendDataAddr,sizeof(ABP_sendDataAddr));

  // set timeout
  ABP_setSendTimeout ();

  // no timeouts yet
  ABP_numTimeouts = 0;

  // alter status
  ABP_sendWait = 1;

  // restore signal mask
  sigprocmask (SIG_SETMASK,&oldsigset,0);
}

///////////////////////////////////////////////////////////////////////////////
//
// ABP_flush
//
///////////////////////////////////////////////////////////////////////////////
void ABP_flush (void)
{
  // wait until all data has been acknowledged (i.e., we're not waiting for
  // an ACK)
  while (ABP_sendWait)
    pause();
}

///////////////////////////////////////////////////////////////////////////////
//
// ABP_ackSIGIO
//
///////////////////////////////////////////////////////////////////////////////
void ABP_ackSIGIO (int signalType)
{
  // SIGIO callback for received ack
  int ackAddrSize;
  int ackSize;
  struct sockaddr_in ABP_recvAckAddr;
  struct ABP_ackMsg ABP_recvAck;
  struct itimerval timeVal;

  // receive message
  ackSize = recvfrom(ABP_sendDataSock,
		     (char *)&ABP_recvAck,sizeof(ABP_recvAck),0,
		     (struct sockaddr *)&ABP_recvAckAddr,&ackAddrSize);

  // discard ack if it's not the expected size
  if (ackSize != sizeof(ABP_recvAck)) {
    printf("ABP_ackSIGIO:received ack not correct size\n");
    return;
  }
  // discard ack if error in transmission
  // *** calculate checksum of ABP_recvAck, and discard packet if it's not correct ***
  if( calcChecksum((char *)&ABP_recvAck,sizeof(ABP_recvAck))!=0)
      return;
    
  // ignore if we weren't expecting this ack
  if (ABP_recvAck.ackNum != ABP_nextSendSeqNum)
    return;

  // ack received so cancel timeout
  ABP_clearSendTimeout ();

  // increment sequence number
  ABP_nextSendSeqNum = (ABP_nextSendSeqNum+1) & 0x1;

  // we're no longer waiting for the ack
  ABP_sendWait = 0;
}
 
///////////////////////////////////////////////////////////////////////////////
//
// ABP_sendTimer
//
///////////////////////////////////////////////////////////////////////////////
void ABP_sendTimer(int signalType)
{
  struct itimerval timeVal;
  struct timeval currTime;
  // timer ticked, which means one tenth of a second has passed.

  // nothing to do unless there is a timeout set
  if (!ABP_sendTimeoutSet)
    return;

  // still nothing to do unless the timeout time has passed
  gettimeofday (&currTime,0);
  if (timercmp(&currTime,&ABP_sendTimeout,<))
    return;

  // timeout has occurred, so handle it
  // increment number of timeouts
  ABP_numTimeouts++;

  // if too many timeouts we'll just give up
  if (ABP_numTimeouts > ABP_MAX_TIMEOUTS) {
    ABP_nextSendSeqNum = (ABP_nextSendSeqNum + 1) & 0x1;
    ABP_sendWait = 0;
    printf ("Too many timeouts - giving up\n");
    ABP_clearSendTimeout ();
    return;
  }

  // resend message
  US_sendto(ABP_sendDataSock,(char *)&ABP_sendMsg,sizeof(ABP_sendMsg),0,
	    (struct sockaddr *)&ABP_sendDataAddr,sizeof(ABP_sendDataAddr));

  // reset timeout
  ABP_setSendTimeout ();
}

///////////////////////////////////////////////////////////////////////////////
//
// ABP_setSendTimeout
//
///////////////////////////////////////////////////////////////////////////////
static void ABP_setSendTimeout ()
{
  // set the send timeout time to be the current time + ABP_TIMEOUT

  sigset_t oldsigset,sigset;

  // first, block SIGALRM and SIGIO so the timer and asynchronous input
  // can't happen while we're playing with the timeout structures
  sigemptyset (&sigset);
  sigaddset (&sigset,SIGALRM);
  sigaddset (&sigset,SIGIO);
  sigprocmask (SIG_BLOCK,&sigset,&oldsigset);
  
  // get the current time
  gettimeofday (&ABP_sendTimeout,0);

  // add ABP_TIMEOUT constants to the time to get the time the timeout
  // expires
  ABP_sendTimeout.tv_usec += ABP_TIMEOUT_USECS;
  if (ABP_sendTimeout.tv_usec >= 1000000)
    {
      ABP_sendTimeout.tv_sec++;
      ABP_sendTimeout.tv_usec -= 1000000;
    }
  ABP_sendTimeout.tv_sec += ABP_TIMEOUT_SECS;

  // the timeout is now set
  ABP_sendTimeoutSet = 1;

  // restore signal mask
  sigprocmask (SIG_SETMASK,&oldsigset,0);
}

///////////////////////////////////////////////////////////////////////////////
//
// ABP_clearSendTimeout
//
///////////////////////////////////////////////////////////////////////////////
static void ABP_clearSendTimeout ()
{
  // clear the send timeout

  sigset_t oldsigset,sigset;

  // first, block SIGALRM and SIGIO so the timer and asynchronous input
  // can't happen while we're playing with the timeout structures
  sigemptyset (&sigset);
  sigaddset (&sigset,SIGALRM);
  sigaddset (&sigset,SIGIO);
  sigprocmask (SIG_BLOCK,&sigset,&oldsigset);
  
  // the timeout is not set anymore
  ABP_sendTimeoutSet = 0;

  // restore signal mask
  sigprocmask (SIG_SETMASK,&oldsigset,0);
}

///////////////////////////////////////////////////////////////////////////////
//
// ABP_recvInit
//
///////////////////////////////////////////////////////////////////////////////
int ABP_recvInit (short portNum)
{
  struct sigaction handler;

  // build address data structures
  memset (&ABP_recvDataAddr, 0, sizeof(ABP_recvDataAddr));
  ABP_recvDataAddr.sin_family = AF_INET;
  ABP_recvDataAddr.sin_addr.s_addr = INADDR_ANY;
  ABP_recvDataAddr.sin_port = htons(portNum);

  // create socket for receiving data and bind it to port
  if((ABP_recvDataSock = socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0){
    perror("recvInit:socket");
    return -1;
  }

  if (bind (ABP_recvDataSock,(struct sockaddr *)&ABP_recvDataAddr,
	    sizeof(ABP_recvDataAddr)) < 0){
    perror("recvInit:bind");
    return -1;
  }

  // set up SIGIO handler for received data
  handler.sa_handler = ABP_dataSIGIO;
  if (sigfillset (&handler.sa_mask) < 0){
    perror("recvInit:sigfillset");
    return -1;
  }
  handler.sa_flags = 0;
  if (sigaction(SIGIO, &handler, 0) < 0){
    perror("recvInit:sigaction:SIGIO");
    return -1;
  }

  if (fcntl(ABP_recvDataSock, F_SETOWN, getpid()) < 0){
    perror("recvInit:fcntl1 ");
    return -1;
  }
  if (fcntl(ABP_recvDataSock, F_SETFL, O_NONBLOCK|FASYNC) < 0){
    perror("recvInit:fcntl2 ");
    return -1;
  }

  // initialize initial sequence numbers
  ABP_nextRecvSeqNum = 0;

  // we're waiting for data
  ABP_recvWait = 1;

}

///////////////////////////////////////////////////////////////////////////////
//
// ABP_recv
//
///////////////////////////////////////////////////////////////////////////////
void ABP_recv (char *buf, int *length)
{
  // wait for message to come in
  while (ABP_recvWait)
    pause();

  // copy message data to parameters
  memmove (buf,&ABP_recvMsg.data,ABP_recvMsg.length);
  *length = ABP_recvMsg.length;

  // we must wait for next message
  ABP_recvWait = 1;
}

///////////////////////////////////////////////////////////////////////////////
//
// ABP_dataSIGIO
//
///////////////////////////////////////////////////////////////////////////////
void ABP_dataSIGIO (int signalType)
{
  // SIGIO callback for received data
  int addrSize;
  int dataSize;
  struct sockaddr_in fromAddr;
  struct ABP_ackMsg ackMsg;
  struct ABP_dataMsg tempMsg;

  // receive message
  addrSize = sizeof(fromAddr);
  dataSize = recvfrom(ABP_recvDataSock,(char *)&tempMsg,
		      sizeof(tempMsg),0,
		      (struct sockaddr *)&fromAddr,&addrSize);

  // discard data if application hasn't consumed previous data
  if (!ABP_recvWait) {
    printf ("ABP_dataSIGIO: received data overrun\n");
    return;
  }

  // discard data if it's not the expected size
  if (dataSize != sizeof(tempMsg)) {
    printf("ABP_dataSIGIO:received data not correct size\n");
    return;
  }

  // discard data if error in transmission
  // *** calculate checksum of tempMsg and discard if it's not what we expect ***
  if (calcChecksum((char *)&tempMsg,sizeof(tempMsg))!=0)
    return;
  // send ACK
  ackMsg.ackNum = tempMsg.seqNum;
  // *** calculate checksum of ackMsg and place in ackMsg.crc ***
    ackMsg.crc=0;
    ackMsg.crc=calcChecksum((char *)&ackMsg,sizeof(ackMsg));

  US_sendto(ABP_recvDataSock,(char *)&ackMsg,sizeof(ackMsg),0,
	    (struct sockaddr *)&fromAddr,sizeof(fromAddr));

  // ignore data packet if we weren't expecting it
  if (tempMsg.seqNum != ABP_nextRecvSeqNum)
    return;

  // copy message to buffer
  ABP_recvMsg = tempMsg;

  // increment sequence number
  ABP_nextRecvSeqNum = (ABP_nextRecvSeqNum+1) & 0x1;

  // we're no longer waiting for the ack
  ABP_recvWait = 0;
}
