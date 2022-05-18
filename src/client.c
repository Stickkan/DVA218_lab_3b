#include "getChecksum.c"
#include "header.h"
#include "helpFuncClient.c"

int clientID;

int clientStart(int socketfd, rtp *buffer, struct sockaddr_in *serverName) {

  int starting = 1, state = INIT, recvResult, wait = 1, timeout = 0, sendR;
  time_t start, stop;
  double time_passed;
  pthread_t timeThread;

  while (starting) {
    switch (state) {
    case INIT:
      buffer->id = createRandomID();
      clientID = buffer->id;
      sendR = sendMessage(SYN, socketfd, buffer, serverName);
      state = SYNACK;

      break;
    case SYNACK:
      wait_SYNACK(socketfd, buffer, serverName);
      starting = 0;
      break;
    default:
      break;
    }
  }
  printf("Client ID: %d\n", clientID);
  return 1;
}

int clientSlidingWindows(int socketfd, rtp *buffer,
                         struct sockaddr_in *serverName) {

  rtp packets[NUMBEROFPACKAGES];
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
      if (isNextInWindow(nextPacket, base) &&
          nextPacket <= NUMBEROFPACKAGES - 1) {
        // makePacket(buffer, nextPacket, (packets[0]).data, checksum);
        // buffer->id = clientID;

        memcpy(buffer, &(packets[nextPacket]), sizeof(rtp));
        sendMessage(0, socketfd, buffer, serverName);
        flagString = translateFlagNumber(buffer->flags);
        printf("Sent message with flag %s and sequence number %d\n", flagString,
               buffer->seq);
        nextPacket++;
      }
    }
    ioctl(socketfd, FIONREAD, &status);
    if (status > 0) {
      rcvMessage(socketfd, serverName, buffer);
      flag = readFlag(buffer);
      ackNumber = getAckNumber(buffer);

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

    if ((flag == NACK) && !isCorrupt(buffer)) {
      start = clock();
      if (buffer->seq < NUMBEROFPACKAGES) {
        base = buffer->seq;
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
        for (int i = base; i < (nextPacket); i++) {
          rtp *tes = &packets[i];
          memcpy(buffer, &packets[i], sizeof(packets[i]));
          sendMessage(0, socketfd, buffer, serverName);
          printf("Resent package %d\n ",i );
        }
      }
    }

    if ((base) == PACKETSTOSEND) {
      break;
    }
  }
  printf("All packages sent and ACK'd!\n");
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

  /*Start timer (is this needed?!) yes because we will terminate if we do not
   * receive a NACK*/
  clock_t start = clock();
  clock_t tearTimer = clock();

  /*Check the recieved flag from the recieved message*/
  while (1) {

    timer = isTimeOut(start, TIMEOUT_DR);
    ioctl(socketfd, FIONREAD, &status);
    if (status > 0) {
      rcvMessage(socketfd, serverName, buffer);
    }
    // if(getChecksum(buffer->data)==buffer->checksum){
    if ((readFlag(buffer) == NACK) || (timer == 1)) {
      start = clock();
      buffer->id = clientID;
      sendMessage(DR, socketfd, buffer, serverName);
      /*Is a timer needed here?*/

    } else if ((readFlag(buffer) == DRACK) && !isCorrupt(buffer)) {
      printf("DACK received from server!\n");
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
  /*Terminte the connection => close socket*/
  return 0;
}

int main(int argc, char *argv[]) {

  int state = START, start = 0, opened = 0, close = 0, bindResult;
  int teardown;
  rtp buffer;
  struct sockaddr_in serverName;
  argv[1] = "localhost";

  int socketfd = createSocketClient(&serverName, argv[1]);

  while (state != 99) {

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
      printf("Client has disconnected from server!\n");
      state = 99;
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