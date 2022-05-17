#include "getChecksum.c"
#include "header.h"
#include "helpFuncServer.c"

#define PORT 5555
#define hostNameLength 50
#define messageLength 256
#define MAXMSG 512
#define NUMBEROFPACKAGES 10

int windowSize;
int clientID;
// int packageArray[NUMBEROFPACKAGES] = {0};

int serverStart(int socketfd, rtp *buffer, struct sockaddr_in *clientName) {

  int starting = 1, state = SYN, recvResult, wait = 1, timeout = 0;
  time_t start, stop;
  double time_passed;
  pthread_t timeThread;

  while (starting) {
    int test = 0;
    // recvResult = rcvMessage(socketfd, clientName, buffer);
    // state = readFlag(buffer);

    switch (state) {
    case SYN:
      while (1) {
        rcvMessage(socketfd, clientName, buffer);
        clientID = buffer->id;
        windowSize = buffer->windowsize;
        int flag = readFlag(buffer);

        if (!isCorrupt(buffer) && flag == SYN) {
          buffer->id = clientID;

          sendMessage(SYNACK, socketfd, buffer, clientName);

          test = 0;
          state = SYNACK;
          printf("Received SYN from client!\n");
          break;
        }
      }
      break;
    case SYNACK:
      start = clock();
      int status;
      int flag;
      while (1) {
        ioctl(socketfd, FIONREAD, &status);
        if (status > 0) {
          wait = rcvMessage(socketfd, clientName, buffer);
          if (wait > 0) {
            flag = readFlag(buffer);
          }
        }
        if ((flag == ACK) && !isCorrupt(buffer)) {

          break;
        }
        stop = clock();
        time_passed = (double)(stop - start) / CLOCKS_PER_SEC;
        if ((time_passed >= TIMEOUT)) {
          buffer->id = clientID;
          printf("Timeout SYNACK! Resending SYNACK!\n");
          sendMessage(SYNACK, socketfd, buffer, clientName);
          start = clock();
        }
      }
      /*Here we need a function that reads an ACK from the client. If no ACK is
       * received resend SYNACK.*/
      printf("Received ACK From client!\n");
      starting = 0;
      break;
    default:
      break;
    }
  }
  printf("Connection established with client %d!\n", clientID);
  return 1;
}

int serverSlidingWindows(int socketfd, rtp *buffer,
                         struct sockaddr_in *clientName) {
  int test = 0;
  int expectedPackageNumber = 0;
  int seqNumber;
  clock_t serverStart = clock();
  while (1) {
    if (isTimeOut(serverStart, TIMEOUT_SERVER)) {
      printf("Server has not received messages for 1 minute. Closing "
             "Connection!\n");
      return 2;
    }
    rcvMessage(socketfd, clientName, buffer);

    if (shouldTerminate(buffer)) {
      break;
    }

    //(wasReceived(buffer, expectedPackageNumber) == 1
    if ((buffer->seq == expectedPackageNumber) && (isCorrupt(buffer) == 0)) {

      if (readFlag(buffer) == DATA) {
        printMessage(buffer);
      }

      seqNumber = buffer->seq;
      memset(buffer, 0, sizeof(*buffer));
      buffer->seq = seqNumber;
      buffer->id = clientID;

      sendMessage(ACK, socketfd, buffer, clientName);
      printf("Sent ACK %d \n", buffer->seq);

      expectedPackageNumber++;
      // wasReceived(buffer, buffer->seq)
    } else if (isCorrupt(buffer)) {
      sendNack(socketfd, buffer, clientName, seqNumber);
    } else if (buffer->seq < expectedPackageNumber) {
      printf("Received duplicate, expected %d! Throwing package %d!\n", expectedPackageNumber,buffer->seq);
      buffer->seq = expectedPackageNumber-1;
      sendMessage(ACK, socketfd, buffer, clientName);
      printf("Sent ACK %d again\n", buffer->seq);
    }
  }
  // printf("Teardown request received. Closing sequence initiated!\n");
  return 1;
}

int serverTeardown(int socketfd, rtp *buffer, struct sockaddr_in *clientName) {

  clock_t startDR, stop, startACK;
  double timePassed;
  int status, flag;
  startDR = clock();
  buffer->id = clientID;

  sendMessage(DRACK, socketfd, buffer, clientName);
  startACK = clock();
  while (1) {

    ioctl(socketfd, FIONREAD, &status);
    if (status > 0) {
      rcvMessage(socketfd, clientName, buffer);
      int flag = readFlag(buffer);
      if ((flag == ACK) && !isCorrupt(buffer)) {
        break;
      }
    }

    if ((isTimeOut(startACK, TIMEOUT_DR)) || flag == NACK) {
      buffer->id = clientID;
      sendMessage(DRACK, socketfd, buffer, clientName);
    }
    if (isTimeOut(startDR, TIMEOUT_SERVER)) {
      break;
    }
  }
  printf("Server has closed!\n");
  return 1;
}

int main(int argc, char *argv[]) {
  int state = START, start = 0, opened = 0, closed = 0, bindResult;
  int teardown;
  rtp buffer;
  struct sockaddr_in clientName;

  int socketfd = createSocketServer(&clientName);

  if ((bindResult = bindSocket(socketfd, &clientName)) >= 0) {

    printf("Waiting for client to connect: \n");

    while (state != 99) {

      switch (state) {
      case START:
        start = serverStart(socketfd, &buffer, &clientName);
        if (start == 1) {
          state = OPENED;
        }
        break;

      case OPENED:
        opened = serverSlidingWindows(socketfd, &buffer, &clientName);
        if (opened == 1) {
          state = CLOSE;
        }
        break;
      case CLOSE:
        teardown = serverTeardown(socketfd, &buffer, &clientName);
        if ((teardown == 1) || teardown == 2) {
          close(socketfd);
          printf("Server has closed!\n");
          state = 99;
        }

        break;
      default:
        state = START;
        break;
      }
    }
  }
  return 0;
}
