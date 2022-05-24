#include "getChecksum.c"
#include "header.h"
#include "helpFuncServer.c"

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
  rtp *packets = malloc(sizeof(rtp)*NUMBEROFPACKAGES);
  int test = 0, send;
  int expectedPackageNumber = 0;
  int seqNumber;
  char *flagString;
  clock_t serverStart = clock();
  while (1) {
    if (isTimeOut(serverStart, TIMEOUT_SERVER)) {
      printf("Server has not received messages for 1 minute. Closing "
             "Connection!\n");
      return 2;
    }
    rcvMessage(socketfd, clientName, buffer);

    if (shouldTerminate(buffer)) {
      printf("Packages received from client: \n");
      for(int i = 0; i<NUMBEROFPACKAGES; i++){
        printf("Package %d data: %s \n", i, (packets[i]).data);
      }
      break;
    }

    //(wasReceived(buffer, expectedPackageNumber) == 1
    if ((buffer->seq == expectedPackageNumber) && (isCorrupt(buffer) == 0) &&
        expectedPackageNumber < NUMBEROFPACKAGES) {

      if (readFlag(buffer) == DATA) {
        strcpy((packets[expectedPackageNumber]).data,buffer->data );
        printMessage(buffer);
      }

      seqNumber = buffer->seq;
      memset(buffer, 0, sizeof(*buffer));
      buffer->seq = seqNumber;
      buffer->id = clientID;

      send = sendMessage(ACK, socketfd, buffer, clientName);
      if (send == 1) {
        printf("Sent ACK for package %d \n", buffer->seq);
      }
      expectedPackageNumber++;
      // wasReceived(buffer, buffer->seq)

    } else if (isCorrupt(buffer)) {
      sendNack(socketfd, buffer, clientName, seqNumber);
      flagString = translateFlagNumber(buffer->flags);
      printf("Package with flag %s and seq %d is corrupt, tossing!\n",
             flagString, buffer->seq);

    } else if ((buffer->seq < expectedPackageNumber) ||
               (buffer->seq > expectedPackageNumber)) {
      printf("Received Out of order, expected %d! Throwing package %d!\n",
             expectedPackageNumber, buffer->seq);

      buffer->seq = expectedPackageNumber;
      send = sendMessage(NACK, socketfd, buffer, clientName);
      if (send == 1) {
        printf("Sent NACK for package %d\n", buffer->seq);
      }
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
  printf("Sent DRACK!\n");
  startACK = clock();
  while (1) {

    ioctl(socketfd, FIONREAD, &status);
    if (status > 0) {
      rcvMessage(socketfd, clientName, buffer);
      int flag = readFlag(buffer);
      // if (flag == ACK) {
      //   buffer->checksum = 999;
      // }
      if ((flag == ACK) && !isCorrupt(buffer)) {
        printf("Received ACK from server!\n");
        break;
      } else if (flag == ACK) {
        printf("ACK Corrupt!\n");
        sleep(3);
      }
    }

    if (isTimeOut(startDR, TIMEOUT_SERVER)) {
      printf("Server timed out!\n");
      break;
    }

    if ((isTimeOut(startACK, TIMEOUT_DR) == 1) || (flag == NACK)) {

      buffer->id = clientID;
      sendMessage(DRACK, socketfd, buffer, clientName);
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
