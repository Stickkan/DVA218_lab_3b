#include "header.h"

int windowSize;
int packageArray[NUMBEROFPACKAGES] = {0};

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
  int ret_makeCorr;
  socklen_t socklen = sizeof(struct sockaddr);
  int recvResult = recvfrom(socketfd, buffer, MAXMSG, MSG_WAITALL,
                            (struct sockaddr *)clientName, &socklen);
  /*Insert makeCorrupt()*/
  ret_makeCorr = makeCorrupt(buffer);
  if (recvResult < 0) {
    printf("Could not recieve message!\n");
  }
  return recvResult;
}

int readFlag(rtp *buffer) {

  int ret_makeCorr = makeCorrupt(buffer);

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

int readMessage(rtp *buffer) {

  int flag = readFlag(buffer);
  /*Insert makeCorrupt()*/
  int ret_makeCorr = makeCorrupt(buffer);
  if (flag == 0) {
    // printMessage
  }
  return flag;
}

/*Tanken är att isCorrupt() tar *buffer samt den checksumma som skickas med i
headern. Jämför dem och returnerar 0 eller 1 beroende på om de är samma eller
inte.*/
int isCorrupt(rtp *buffer) {
  if (getChecksum(buffer->data) == buffer->checksum)
    return 0;

  return 1;
}

int sendMessage(int flag, int socketfd, rtp *buffer,
                struct sockaddr_in *clientName) {
  int correctSeqNumb = buffer->seq;
  buffer->windowsize = windowSize;
  buffer->flags = flag;
  buffer->checksum = getChecksum(buffer->data);

  while (1) {
    int result = makeCorrupt(buffer);
    if (result < 0){
      printLost(flag, correctSeqNumb);
      return 0;
    }
    else{
      result = sendto(socketfd, buffer, sizeof(*buffer), 0,
                      (const struct sockaddr *)clientName, sizeof(*clientName));
       if (result < 0)
          printf("Error! The sendto() didn't return correct value.\n");
    }
  }
  return 1;
}

void printMessage(rtp *buffer) {
  printf("Message received from client %d: %s\n", buffer->id, buffer->data);
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

void sendNack(int socketfd, rtp *buffer, struct sockaddr_in *clientName, int seq) {
  int seqNumber;
  seqNumber = buffer->seq;
  memset(buffer, 0, sizeof(*buffer));
  buffer->seq = seq;
  buffer->seq = seqNumber;
  /*Insert makeCorrupt()*/
  int ret_makeCorr = makeCorrupt(buffer);
  sendMessage(NACK, socketfd, buffer, clientName);
}

int isDRACK(rtp *buffer) {
  /*Insert makeCorrupt()*/
  int ret_makeCorr = makeCorrupt(buffer);
  if (buffer->flags == DRACK)
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

  } else if (timeout_type == TIMEOUT_SERVER) {
    if (timePassed >= TIMEOUT_SERVER) {
      return 1;
    }
  }
  return 0;
}

/*This function changes some random values.*/
int makeCorrupt(rtp *buffer) {
  int errorRate;
  int corruptSend;

  srand(time(0));

  errorRate = rand() % 10;

  /*10% chance of entering this statement. If you want to increase errorRate add
   * (|| errorRate == x%10).*/
  if (errorRate == 2) {
    /*for checksum*/
    buffer->checksum = (rand() % 255);
  }
  if (errorRate == 1) {
    /*Corrupt the ACK and or SYN and or DR*/
    buffer->flags = 64;
  }
  if (errorRate == 8) {
    buffer->seq = 132;
  }
  if (errorRate == 5) {
    /*Make sure that the return value automatically makes the sendMessage not
     * send a message by returning a value less than 0.*/
    corruptSend = -2;

    return corruptSend;
  }

  /*If the program does not enter corruptSend statement then return 1 to make
   * sure that the package is sent.*/
  return 1;
}

void printLost(int flag, int seqNumb){
int state;
switch(state){
    case ACK:
    printf("ACK of package %d lost! \n", seqNumb);
    break;
    case SYN:
    printf("SYN lost!\n");
    break;
    case SYNACK:
    printf("SYNACK lost!\n");
    break;
    case DR:
    printf("DR lost!\n");
    break;
    case DRACK:
    printf("DRACK lost!\n");
    break;
    case DATA: 
    printf("Datapackage nr: %d lost!\n", seqNumb);
    break;
    default: 
    state = flag;
    break;
}
}