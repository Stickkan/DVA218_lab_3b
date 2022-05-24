#include "getChecksum.c"
#include "header.h"
#include "helpFuncClient.c"

int clientID;
rtp * packageArray;
rtp packets[NUMBEROFPACKAGES];

int clientStart(int socketfd, rtp *buffer, struct sockaddr_in *serverName) {

  int starting = 1, state = INIT, recvResult, wait = 1, timeout = 0, sendR;
  time_t start, stop;
  double time_passed;
  pthread_t timeThread;

  while (starting) {
    switch (state) {
      /*Initialize the connection by sending a SYN to the server.*/
      case INIT:
        buffer->id = createRandomID();
        clientID = buffer->id;
        sendR = sendMessage(SYN, socketfd, buffer, serverName);
        state = SYNACK;

        break;
      /*If a SYNACK has been received then send an ACK.*/  
      case SYNACK:
        wait_SYNACK(socketfd, buffer, serverName);
        starting = 0;
        break;
      default:
        break;
      }
  }
  /*For the user to know what the random generated client ID is.*/
  printf("Client ID: %d\n", clientID);
  return 1;
}

void resetBuffer(rtp * buffer){
  /*Does what the function header implies.*/
  memset(buffer, 0, sizeof(rtp));
}

int clientSlidingWindows(int socketfd, rtp *buffer,
                         struct sockaddr_in *serverName) {

  /*Creation of a package and decleration of some variables.*/
  createPackages(packets, WINDOWSIZE, clientID);
  int status, base = 0, nextPacket = 0, checksum, flag, ackNumber;
  clock_t start, stop;
  double timePassed;
  int test = 0;
  char *flagString;
  

  while (1) {
    if (isNextInWindow(nextPacket, base)) {
      if (nextPacket == base) {
        start = clock();
      }
      /*Checks whether or not the next package to be sent is in the correct order 
      and wihthin the the agreed upon number of packages.*/
      if (isNextInWindow(nextPacket, base) &&
          nextPacket <= NUMBEROFPACKAGES - 1) {

        memcpy(buffer, &(packets[nextPacket]), sizeof(rtp));
        sendMessage(0, socketfd, buffer, serverName);
        flagString = translateFlagNumber(buffer->flags);
        printf("Sent message with flag %s and sequence number %d\n", flagString,
               buffer->seq);
        nextPacket++;
      }
    }

    /*Checks if there is a message to be read. If there is a messge to be read then
    receive it and copy the package header for its info*/
    ioctl(socketfd, FIONREAD, &status);
    if (status > 0) {
      rcvMessage(socketfd, serverName, buffer);
      flag = readFlag(buffer);
      ackNumber = getAckNumber(buffer);

      /*If an ACK is received increase the base value.*/
      if ((flag == ACK) && packetInWindow(ackNumber, base)) {
        printf("Received ACK %d\n", ackNumber);
        base = ackNumber + 1;
        printf("Increased base to %d\n", base);

        start = clock();
        if (base == nextPacket) {
          start = 0;
        }
      }

      if (flag == SYNACK) {
        sendMessage(ACK, socketfd, buffer, serverName);
        start = clock();
      }
    }

    

    //
    flag = readFlag(buffer);
    if ((flag == NACK) && isCorrupt(buffer) == 0) {
      start = clock();
      if (buffer->seq <= NUMBEROFPACKAGES) {
        base = buffer->seq;
        printf("Base set to %d!\n", base);
        if ((base) == NUMBEROFPACKAGES) {
          break;
        }
      }
      printf("Received NACK. Expecting package: %d, resending from base!\n",
             base);

      for (int i = base; i == nextPacket; i++) {
        printf("packets is: %d before memcpy\n", packets[i].seq);
        memcpy(buffer, &packets[i], sizeof(packets[i]));
        sendMessage(0, socketfd, buffer, serverName);
      }
    }
    int timeOut = isTimeOut(start, TIMEOUT_ACK);
    if (timeOut == 1) {
      start = clock();
      if (base < NUMBEROFPACKAGES) {
        printf("Timeout! Resending from base %d!\n", base);
        for (int i = base; i < (base+WINDOWSIZE); i++) {
          rtp *tes = &packets[i];
          memcpy(buffer, &packets[i], sizeof(packets[i]));
          int send = sendMessage(0, socketfd, buffer, serverName);
          if (send == 1) {
            printf("Resent package %d\n ", i);
          }
        }
      }
    }

    if ((base) == NUMBEROFPACKAGES) {
      break;
    }
    resetBuffer(buffer);
  }
  printf("All packages sent and ACK'd!\n");
  printf("Packets sent from client: \n");
   for(int i = 0; i<NUMBEROFPACKAGES; i++){
    printf("Packet %d Data: %s\n", i, (packets[i]).data);
  }
  return 1;
}

int clientTearDown(rtp *buffer, int socketfd, struct sockaddr_in *serverName) {
  /*
  1) Send a disconnect request.
  2) Start a timer.
  3) If a NACK is recieved resend the disconnect request.
  4) If no NACK is received, terminate the connection.
  5) Return a 0 on success, a 1 on failure.
  */

  int timer;
  int status;
  int test = 0;

  /*1) Send a disconnect request.*/
  char dr[] = "Disconnect Request";
  int length = strlen(dr);
  dr[length] = '\0';
  strcpy(buffer->data, dr);
  buffer->checksum = getChecksum(buffer->data);
  buffer->id = clientID;
  sendMessage(DR, socketfd, buffer, serverName);

  /*Start timer*/
  clock_t start = clock();
  clock_t tearTimer = clock();

  /*Check the recieved flag from the recieved message*/
  while (1) {

    timer = isTimeOut(start, TIMEOUT_DR);
    ioctl(socketfd, FIONREAD, &status);
    if (status > 0) {
      rcvMessage(socketfd, serverName, buffer);
    }
    
    if ((readFlag(buffer) == NACK) || (timer == 1)) {
      /*Restart clock if a NACK is received*/
      start = clock();
      buffer->id = clientID;
      sendMessage(DR, socketfd, buffer, serverName);
      
    /*Send an ACK if a DRACK is received*/
    } else if ((readFlag(buffer) == DRACK) && !isCorrupt(buffer)) {
      printf("DRACK received from server!\n");
      sendMessage(ACK, socketfd, buffer, serverName);
      printf("Sent ACK to server!\n");
      break;
    }

    clock_t tearTimerStop = isTimeOut(tearTimer, TIMEOUT_SERVER);
    if (tearTimerStop == 1) {
      printf("Client Timed out.\n");
      break;
    }
  }
 
  return 0;
}

int main(int argc, char *argv[]) {

  int state = START, start = 0, opened = 0, closed = 0, bindResult;
  int teardown;
  rtp buffer;
  struct sockaddr_in serverName;
  argv[1] = "localhost";

  int socketfd = createSocketClient(&serverName, argv[1]);

  while (state != 99) {

    /*The different states in which the program can operate.*/
    switch (state) {
    case START:
      start = clientStart(socketfd, &buffer, &serverName);
      if (start == 1) {
        printf("Opened!\n");
        state = OPENED;
      }
      break;

    case OPENED:
      clientSlidingWindows(socketfd, &buffer, &serverName);
      state = CLOSE;
      break;

    case CLOSE:
      teardown = clientTearDown(&buffer, socketfd, &serverName);      
      closed =close(socketfd);
      if(closed ==0){
        printf("Client has disconnected from server!\n");
      }
      state = 99;
      break;

    default:
      state = START;
      break;
    }
  }

  return 0;
}