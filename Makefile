# Makefile for the Alternating Bit Protocol project
#

all : unreliableSend.o ABP.o sender receiver checksum-checker-client crc-checker-client

sender: sender.c ABP.o unreliableSend.o
	gcc sender.c ABP.o unreliableSend.o -o sender

receiver: receiver.c ABP.o unreliableSend.o
	gcc receiver.c ABP.o unreliableSend.o -o receiver

unreliableSend.o: unreliableSend.c unreliableSend.h
	gcc -c unreliableSend.c
	
ABP.o: ABP.h ABP.c calcChecksum.h
	gcc -c ABP.c
	
checksum-checker-client: checksum-checker-client.c calcChecksum.h
	gcc checksum-checker-client.c -o checksum-checker-client
	
crc-checker-client: crc-checker-client.c calcChecksum.h
	gcc crc-checker-client.c -o crc-checker-client
	
clean:
	rm -f *.o sender receiver checksum-checker-client crc-checker-client
