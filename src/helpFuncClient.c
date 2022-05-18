#include "header.h"

int produceError;

int createRandomID(void) {
  srand(clock());
  int integer = rand() % 1000;
  return integer;
}

void travelTime(void) { sleep((double)0.001); }

rtp *createPackages(rtp *packages, int windowSize, int clientID) {
  for (int i = 0; i < NUMBEROFPACKAGES; i++) {
    srand(clock());
    int dataInteger = rand()%1000;
    char *data = malloc(sizeof("1000"));
    sprintf(data, "%d", dataInteger );

    strcpy(packages[i].data, data);
    //*(packages[i]).data = i * 100;
    (packages[i]).id = clientID;
    (packages[i]).checksum = getChecksum(&(packages[i]).data);
    (packages[i]).seq = i;
    (packages[i]).flags = 0;
    (packages[i]).windowsize = windowSize;
  }
  return packages;
}

int createSocketClient(struct sockaddr_in *serverName, char *argv) {

  int *dstHost = "127.0.0.1";

  int socketfd;
  struct hostent *hostinfo;

  if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    printf("Can't create UDP socket\n");

  memset(serverName, 0, sizeof(*serverName));

  serverName->sin_addr.s_addr = inet_addr(dstHost);
  serverName->sin_family = AF_INET;
  serverName->sin_port = PORT;

  return socketfd;
}

int isCorrupt(rtp *buffer) {
  if (getChecksum(buffer->data) == buffer->checksum)
    return 0;
  printCorrupt(buffer->flags, buffer->seq);
  return 1;
}

int rcvMessage(int socketfd, struct sockaddr_in *serverName, rtp *buffer) {

  socklen_t socklen = sizeof(struct sockaddr);
  int recvResult = recvfrom(socketfd, buffer, MAXMSG, MSG_WAITALL,
                            (struct sockaddr *)serverName, &socklen);
  if (recvResult < 0) {
    printf("Could not recieve message!\n");
  }
  return recvResult;
}

int readFlag(rtp *buffer) {

  if (buffer->flags == ACK) {
    return ACK;
  } else if (buffer->flags == SYNACK) {
    return SYNACK;
  } else if (buffer->flags == SYN) {
    return SYN;
  } else if (buffer->flags == DRACK) {
    return DRACK;
  }
  return 0;
}

int readMessage(rtp *buffer) {

  int flag = readFlag(buffer);

  if (flag == 0) {
    // printMessage
  }
  return flag;
}

int sendMessage(int flag, int socketfd, rtp *buffer,
                struct sockaddr_in *serverName) {
  int result = 0;
  int correctSeqNumb = buffer->seq;
  buffer->flags = flag;
  buffer->windowsize = WINDOWSIZE;
  buffer->checksum = getChecksum(buffer->data);
  
  if(SHOULD_ERROR){
  result = makeCorrupt(buffer);
  }
  if (result < 0) {
    printLost(flag, correctSeqNumb);
    return 0;
  } else {
    
    result = sendto(socketfd, buffer, sizeof(*buffer), 0,
                    (const struct sockaddr *)serverName, sizeof(*serverName));

    if (result < 0) {
      printf("Error the sendto() didn't return correct value.\n");
      return 0;
    }
  }

  return 1;
}

int wait_SYNACK(int socketfd, rtp *buffer, struct sockaddr_in *serverName) {

  int wait = 1, status;
  clock_t start, stop;
  double time_passed;
  int test;

  start = clock();

  while (1) {

    ioctl(socketfd, FIONREAD, &status);
    if (status > 0) {
      wait = rcvMessage(socketfd, serverName, buffer);
      if (wait > 0) {
        wait = readFlag(buffer);

        if ((!isCorrupt(buffer)) && wait == SYNACK) {
          printf("Received SYNACK from server!\n");
          sendMessage(ACK, socketfd, buffer, serverName);
          break;
        };
      }
    }

    stop = clock();

    time_passed = (double)(stop - start) / CLOCKS_PER_SEC;
    if ((time_passed >= TIMEOUT) || wait == NACK) {
      sendMessage(SYN, socketfd, buffer, serverName);
      start = clock();
    }
  }
  printf("Client initiated!\n");
  return 0;
}

void makePacket(rtp *buffer, int packetNumber, char data[], int checksum) {

  buffer->seq = packetNumber;
  strcpy(buffer->data, data);
  buffer->checksum = checksum;
}

int getAckNumber(rtp *buffer) {
  int ackNumber = buffer->seq;

  return ackNumber;
}

int packetInWindow(int ackNumber, int base) {
  if ((ackNumber <= (base + WINDOWSIZE - 1)) && (ackNumber >= base))
    return 1;

  return 0;
}

int isTimeOut(clock_t start, int timeout_type) {
  clock_t stop = clock();
  double timePassed = (double)(stop - start) / CLOCKS_PER_SEC;
  if (timeout_type == TIMEOUT) {
    if (timePassed >= TIMEOUT) {
      return 1;
    }
  } else if (timeout_type == TIMEOUT_DR) {
    if (timePassed >= TIMEOUT_DR) {
      return 1;
    }
  } else if (timeout_type == TIMEOUT_ACK) {
    if (timePassed >= TIMEOUT_ACK) {
      return 1;
    }
  }
  return 0;
}

int isNextInWindow(int nextPacket, int base) {
  if (nextPacket - base < WINDOWSIZE) {
    return 1;
  }
  return 0;
}

char *translateFlagNumber(int flag) {
  char *syn = "SYN";
  char *ack = "ACK";
  char *synack = "SYNACK";
  char *nack = "NACK";
  char *dr = "DR";
  char *data = "DATA";
  char *drack = "DRACK";
  char *corruptFlag = "Corrupt flag";

  if (flag == 1) {
    return syn;
  }
  if (flag == 2) {
    return ack;
  }
  if (flag == 3) {
    return synack;
  }
  if (flag == 8) {
    return nack;
  }
  if (flag == 9) {
    return dr;
  }
  if (flag == 0) {
    return data;
  }
  if (flag == 11) {
    return drack;
  }
  if (flag == 64) {
    return corruptFlag;
  } else
    return "Error translating";
}

/*This function changes some random values.*/
int makeCorrupt(rtp *buffer) {
  int errorRate;
  int corruptSend;
  char *flag;

  flag = translateFlagNumber(buffer->flags);
  srand(clock());

  errorRate = rand() % MOD;

  /*10% chance of entering this statement. If you want to increase errorRate add
   * (|| errorRate == x%10).*/
  if (errorRate == (rand() % MOD)) {
    /*for checksum*/
    printf("Simulate corrupt checksum with flag %s on package %d (Client)\n", flag,
           buffer->seq);
    buffer->checksum = (rand() % 255);
  }
  if (errorRate == (rand() % MOD)) {
    /*Corrupt the ACK and or SYN and or DR*/
    printf("Simulate corrupt flag %s on package %d (Client)\n", flag,
           buffer->seq);
    buffer->flags = 64;
  }
  if (errorRate == (rand() % MOD)) {
    printf(
        "Simulate package out of order on package %d with flag %s (Client)\n",
        buffer->seq, flag);
    buffer->seq = rand() % 9;
  }
  if (errorRate == (rand() % MOD)) {
    /*Make sure that the return value automatically makes the sendMessage not
     * send a message by returning a value less than 0.*/
    corruptSend = -2;

    return corruptSend;
  }

  /*If the program does not enter corruptSend statement then return 1 to make
   * sure that the package is sent.*/
  return 1;
}

void printLost(int flag, int seqNumb) {
  int state;
  while (state != 99) {
    switch (state) {
    case ACK:
      printf("ACK of package %d lost! \n", seqNumb);
      state = 99;
      break;
    case SYN:
      printf("SYN lost!\n");
      state = 99;
      break;
    case SYNACK:
      printf("SYNACK lost!\n");
      state = 99;
      break;
    case DR:
      printf("DR lost!\n");
      state = 99;
      break;
    case DRACK:
      printf("DRACK lost!\n");
      state = 99;
      break;
    case DATA:
      printf("Datapackage nr: %d lost!\n", seqNumb);
      state = 99;
      break;
    default:
      state = flag;
      break;
    }
  }
}

void printCorrupt(int flag, int seqNumb) {
  int state;
  switch (state) {
  case ACK:
    printf("ACK of package %d corrupt! \n", seqNumb);
    break;
  case SYN:
    printf("SYN corrupt!\n");
    break;
  case SYNACK:
    printf("SYNACK corrupt!\n");
    break;
  case DR:
    printf("DR corrupt!\n");
    break;
  case DRACK:
    printf("DRACK corrupt!\n");
    break;
  case DATA:
    printf("Datapackage nr: %d corrupt!\n", seqNumb);
    break;
  default:
    state = flag;
    break;
  }
}