#include "getChecksum.c"
#include "header.h"
#include "helpFuncClient.c"

int clientStart(int socketfd, rtp *buffer, struct sockaddr_in *serverName) {

  int starting = 1, state = INIT, recvResult, wait = 1, timeout = 0, sendR;
  time_t start, stop;
  double time_passed;
  pthread_t timeThread;

  while (starting) {
    switch (state) {
    case INIT:

      sendR = sendMessage(SYN, socketfd, buffer, serverName);
      if (sendR == 1) {
        state = SYNACK;
      }
      break;
    case SYNACK:
      wait_SYNACK(socketfd, buffer, serverName);
      starting = 0;
      break;
    default:
      break;
    }
  }
  return 1;
}

int clientSlidingWindows(int socketfd, rtp *buffer,
                         struct sockaddr_in *serverName) {

  rtp packets[WINDOWSIZE];
  int status, base = 0, nextPacket = 0, checksum, flag, ackNumber;
  clock_t start, stop;
  double timePassed;

  while (1) {
    if (isNextInWindow(nextPacket, base)) {
      if (nextPacket == base) {
        start = clock();
      }
      if (isNextInWindow(nextPacket, base) &&
          nextPacket <= NUMBEROFPACKAGES - 1) {
        makePacket(buffer, nextPacket, (packets[0]).data, checksum);
        sendMessage(0, socketfd, buffer, serverName);
        nextPacket++;
      }
    }
    ioctl(socketfd, FIONREAD, &status);
    if (status >= 0) {
      rcvMessage(socketfd, serverName, buffer);
      flag = readFlag(buffer);
      ackNumber = getAckNumber(buffer);
      if ((flag == ACK) && packetInWindow(ackNumber, base)) {
        base = ackNumber;
        if (base == nextPacket) {
          start = 0;
        } else {
          start = clock();
        }
      }
    }
    int timeOut = isTimeOut(start,TIMEOUT_ACK);
    if (timeOut == 1) {
      printf("Placeholder");
    }
    if ((base) == PACKETSTOSEND - 1) {
      break;
    }
  }
  printf("All packages sent and ACK'd!\n");
  return 1;
}

int clientTearDown(rtp* buffer, int socketfd, struct sockaddr_in* serverName){
  /*
  1) Send a disconnect request.
  2) Start a timer.
  3) If a NACK is recieved resend the disconnect request.
  4) If no NACK is received, terminate the connection.
  5) Return a 0 on success, a 1 on failure.
  */

  int timer;

  /*1) Send a disconnect request.*/
  char dr[]= "Disconnect Request";
  int length = strlen(dr);
  dr[length] = '\0';
  strcpy(buffer->data, dr);
  buffer->checksum = getChecksum(buffer->data);
  sendMessage(ACK, socketfd, buffer, serverName);

  /*Start timer (is this needed?!) yes because we will terminate if we do not receive a NACK*/
  clock_t start = clock();
  //timer = isTimeOut(start);

  /*Check the recieved flag from the recieved message*/
  while(1){
  rcvMessage(socketfd, serverName, buffer);
  //if(getChecksum(buffer->data)==buffer->checksum){
  if(readFlag(buffer)==NACK)
    sendMessage(ACK, socketfd, buffer, serverName);
    /*Is a timer needed here?*/
  else
    break;
  }
  /*Terminte the connection => close socket*/
  return 0;
}

int main(int argc, char *argv[]) {

  int state = START, start = 0, opened = 0, close = 0, bindResult;
  rtp buffer;
  struct sockaddr_in serverName;
  argv[1] = "localhost";

  int socketfd = createSocketClient(&serverName, argv[1]);

  while (1) {

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
      break;
    default:
      state = START;
      break;
    }
  }

  // int result = sendto(socketfd, &testMessage, sizeof(testMessage), 0,
  //                     (const struct sockaddr *)&serverName,
  //                     sizeof(serverName));

  // if (result < 0) {
  //   printf("Could not send message!\n");
  // }

  return 0;
}