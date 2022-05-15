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
          state = SYNACK;
          printf("Received SYN from client!\n");
          break;
        }
      }
      break;
    case SYNACK:
      start = clock();
      int status;
      while (wait != ACK) {
        ioctl(socketfd, FIONREAD, &status);
        if (status >= 0) {
          wait = rcvMessage(socketfd, clientName, buffer);
          if (wait > 0) {
            wait = readMessage(buffer);
          }
        }
        stop = clock();
        time_passed = (double)(stop - start) / CLOCKS_PER_SEC;
        if ((time_passed >= TIMEOUT) || wait == NACK) {
          buffer->id = clientID;
          sendMessage(SYNACK, socketfd, buffer, clientName);
          start = clock();
        }
      }
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
    if ((wasReceived(buffer, expectedPackageNumber) == 1) &&
        (isCorrupt(buffer)) == 0) {

      if (readFlag(buffer) == DATA) {
        printMessage(buffer);
      }

      seqNumber = buffer->seq;
      memset(buffer, 0, sizeof(*buffer));
      buffer->seq = seqNumber;
      buffer->id = clientID;
      sendMessage(ACK, socketfd, buffer, clientName);
      expectedPackageNumber++;
    } else {
      sendNack(socketfd, buffer, clientName);
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
      if (flag == ACK) {
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
          printf("Test\n");
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
