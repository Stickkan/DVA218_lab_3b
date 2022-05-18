#include "header.h"
int produceError;
int windowSize;
int packageArray[NUMBEROFPACKAGES] = {0};

void travelTime(void) { sleep((double)0.001); }

int createSocketServer(struct sockaddr_in *clientName) {

  int socketfd = socket(AF_INET, SOCK_DGRAM, 0);

  if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0) {
    perror("setsockopt(SO_REUSEADDR) failed");
  }

  if (socketfd < 0) {
    printf("Could not open socket!\n");
  }

  memset(clientName, 0, sizeof(*clientName));

  clientName->sin_addr.s_addr = INADDR_ANY;
  clientName->sin_port = PORT;
  clientName->sin_family = AF_INET;

  return socketfd;
}

int bindSocket(int socketfd, struct sockaddr_in *clientName) {

  int bindReturn =
      bind(socketfd, (const struct sockaddr *)clientName, sizeof(*clientName));
  if (bindReturn < 0) {
    fprintf(stdout, "Errno: %d", errno);
    printf("Could not bind socket!\n");
  }

  return bindReturn;
}

int rcvMessage(int socketfd, struct sockaddr_in *clientName, rtp *buffer) {
  socklen_t socklen = sizeof(struct sockaddr);
  int recvResult = recvfrom(socketfd, buffer, MAXMSG, MSG_WAITALL,
                            (struct sockaddr *)clientName, &socklen);

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
  } else if (buffer->flags == DATA) {
    return DATA;
  } else if (buffer->flags == DR) {
    return DR;
  }
  return 0;
}

/*Tanken är att isCorrupt() tar *buffer samt den checksumma som skickas med i
headern. Jämför dem och returnerar 0 eller 1 beroende på om de är samma eller
inte.*/
int isCorrupt(rtp *buffer) {
  if (getChecksum(buffer->data) == buffer->checksum)
    return 0;
  //printCorrupt(buffer->flags, buffer->seq);
  return 1;
}

int sendMessage(int flag, int socketfd, rtp *buffer,
                struct sockaddr_in *clientName) {
  int result=0;
  int correctSeqNumb = buffer->seq;
  buffer->windowsize = windowSize;
  buffer->flags = flag;
  buffer->checksum = getChecksum(buffer->data);
  if (SHOULD_ERROR) {
    result = makeCorrupt(buffer);
  }
  if (result < 0) {
    printLost(flag, correctSeqNumb);
    return 0;
  } else {
    result = sendto(socketfd, buffer, sizeof(*buffer), 0,
                    (const struct sockaddr *)clientName, sizeof(*clientName));
    if (result < 0)
      printf("Error! The sendto() didn't return correct value.\n");
    return 0;
  }

  return 1;
}

void printMessage(rtp *buffer) {
  printf("Message received from client ID %d with package number %d: %s\n",
         buffer->id, buffer->seq, buffer->data);
}

int wasReceived(rtp *buffer, int expectedSeqNumber) {
  if (packageArray[expectedSeqNumber] == 1) {
    return 0;
  }
  return 1;
}

int shouldTerminate(rtp *buffer) {
  int flag = readFlag(buffer);
  if ((flag == DR) && !isCorrupt(buffer)) {
    printf("Disconnect request received from client!\n Initiating "
           "shutdown!\n");
    return 1;
  }
  return 0;
}

void sendNack(int socketfd, rtp *buffer, struct sockaddr_in *clientName,
              int seq) {
  int seqNumber;
  seqNumber = buffer->seq;
  memset(buffer, 0, sizeof(*buffer));
  buffer->seq = seq;
  buffer->seq = seqNumber;

  sendMessage(NACK, socketfd, buffer, clientName);
}

int isDRACK(rtp *buffer) {

  if (buffer->flags == DRACK)
    return 1;

  return 0;
}

int isTimeOut(clock_t start, int timeout_type) {
  clock_t stop = clock();
  double timePassed = (double)(stop - start) / CLOCKS_PER_SEC;
  if (timeout_type == TIMEOUT) {
    if (timePassed >= TIMEOUT) {
      printf("Timeout!\n");
      return 1;
    }
  } else if (timeout_type == TIMEOUT_DR) {
    if (timePassed >= TIMEOUT_DR) {
      printf("Timeout!\n");
      return 1;
    }
  } else if (timeout_type == TIMEOUT_ACK) {
    if (timePassed >= TIMEOUT_ACK) {
      printf("Timeout!\n");
      return 1;
    }

  } else if (timeout_type == TIMEOUT_SERVER) {
    if (timePassed >= TIMEOUT_SERVER) {
      printf("Timeout!\n");
      return 1;
    }
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
    printf("Modified checksum on %s (Client)\n", flag);
    buffer->checksum = (rand() % 255);
  }
  if (errorRate == (rand() % MOD)) {
    printf("Modified flag %s on package %d (Client)\n", flag, buffer->seq);
    /*Corrupt the ACK and or SYN and or DR*/
    buffer->flags = 64;
  }
  if (errorRate == (rand() % MOD)) {
    printf("Modified seq on package with flag %s (Client)\n", flag);
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