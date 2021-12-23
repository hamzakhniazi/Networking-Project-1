//
// File: ABP.h
//
// Author: Hamza Sultan Khan Niazi
//
// Description: This is a simple implementation of reliable transmission
// that uses the alternating bit protocol (i.e., stop and wait algorithm.  
// UDP datagrams are used to send data packets and acknowledgements.
//
// The following functions are defined:
//    ABP_sendInit (char *hostname,int portNum)
//    ABP_send (char *buf, int length)
//    ABP_flush(void)
//
//    ABP_recvInit (int portNum)
//    ABP_recv (char *buf, int *length)

#ifndef _ABP_H_
#define _ABP_H_

int ABP_sendInit (char *hostname, short portNum);
// initializes the ABP protocol so that messags subsequently sent using
// ABP_send will be sent to the ABP protocol running on hostname using UDP
// port portnum.
//
// A negative return value indicates an error.

void ABP_send (char *buf, int length);
// sends a message to the host specified when ABP_sendInit was called. length
// bytes will be sent, starting at buf.  ABP_send may return before the 
// message is sent, but ABP_send will make a copy of the message so the caller
// can change the buffer.  Currently there is no way for the caller to verify
// that the message was successfully sent.

void ABP_flush(void);
// does not return until all previously sent messages have been successfully
// received.

int ABP_recvInit (short portNum);
// initializes the ABP protocol to receive messages on UDP port portnum.
//
// A negative return value indicates an error.

void ABP_recv (char *buf, int *length);
// receive a message using the ABP protocol.  On entry, buf is a pointer to
// a buffer of at least length bytes.  On return length contains the number
// of bytes actually read.
#endif
