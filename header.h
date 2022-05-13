#ifndef MY_GUARD
#define MY_GUARD 1

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>


// #define PORT 5555
// #define hostNameLength 50
#define messageLength 256
#define INIT 0
#define SYN 1
#define ACK 2
#define SYNACK 3
#define START 4
#define OPENED 5
#define CLOSE 6
#define CORRUPT 7
#define TIMEOUT 1
#define NACK 8
#define MAXMSG 512
#define DR 9
#define DATA 0

#define PACKETSTOSEND 10

typedef struct rtp_struct{
    int flags;
    int id;
    int seq;
    int windowsize;
    int checksum;
    char data[messageLength];
}rtp;

/*---------------Declaration of all functions------------------*/

/*-------------------Functions in client.c---------------------*/

/*Get the checksum*/
int getChecksum();

/*Create socket*/
int createSocketClient(struct sockaddr_in *serverName, char *argv);

/*Waiting for a SYNACK*/
int wait_SYNACK(int socketfd, rtp *buffer, struct sockaddr_in *serverName);

/*Start the client => make sure there is a connection*/
int clientStart(int socketfd, rtp *buffer, struct sockaddr_in *serverName);

/*Terminate the connection*/
int clientTearDown(rtp* buffer, int socketfd, struct sockaddr_in* serverName);

/*Make the header to each packet*/
void makePacket(rtp *buffer, int packetNumber, char data[], int checksum);

/*Get the ACK number where each number represents a specific ACK type*/
int getAckNumber(rtp *buffer);

/*Checks if the package is wihtin the window size*/
int packetInWindow(int ackNumber, int base);

/*Timeout function*/
int isTimeOut(clock_t start);

/*Checks if the package is the next one to be sent*/
int isNextInWindow(int nextPacket, int base);

/*The implementation of sliding window from the clients perspective*/
int clientSlidingWindows(int socketfd, rtp *buffer, struct sockaddr_in *serverName);



/*-------------------Functions in server.c----------------------*/

/*Creation of the server socket*/
int createSocketServer(struct sockaddr_in *clientName);

/*Bind the socket*/
int bindSocket(int socketfd, struct sockaddr_in *clientName);

/*Handshake from the servers perspective*/
int serverStart(int socketfd, rtp *buffer, struct sockaddr_in *clientName);


/*-------------Functions in client.c and server.c---------------*/

/*Receive message / packet*/
int rcvMessage(int socketfd, struct sockaddr_in *serverName, rtp *buffer);

/*Read the flag that is sent in the header of each message / packet*/
int readFlag(rtp *buffer);

/*Read the recieved message / packet*/
int readMessage(rtp *buffer);

/*Send a message*/
int sendMessage(int flag, int socketfd, rtp *buffer, struct sockaddr_in *serverName);

/*Check if the received package is corrupted */
int isCorrupt(rtp *buffer);

#endif