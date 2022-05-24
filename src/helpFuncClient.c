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
    (packages[i]).checksum = getChecksum((packages[i]).data);
    (packages[i]).seq = i;
    (packages[i]).flags = 0;
    (packages[i]).windowsize = windowSize;
  }
  return packages;
}

int createSocketClient(struct sockaddr_in *serverName, char *argv) {

  char *dstHost = "127.0.0.1";

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

/*isCorrupt() checks if the received message is corrupt or not by checking the checksum and compare
it to the received one.*/
int isCorrupt(rtp *buffer) {
  if (getChecksum(buffer->data) == buffer->checksum)
    return 0;
  printCorrupt(buffer->flags, buffer->seq);
  return 1;
}

/*Receives the message from client. If return value of recvfrom() is less
than 0 print an error message.*/
int rcvMessage(int socketfd, struct sockaddr_in *serverName, rtp *buffer) {

  socklen_t socklen = sizeof(struct sockaddr);
  int recvResult = recvfrom(socketfd, buffer, MAXMSG, MSG_WAITALL,
                            (struct sockaddr *)serverName, &socklen);
  if (recvResult < 0) {
    printf("Could not recieve message!\n");
  }
  return recvResult;
}

/*Read all the different types of flags that can be received. If not
one of the if statements then return 0.*/
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
  else if((buffer->flags)== NACK){
    return NACK;
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

/*Sends a message to the client. First the struct is filled, adding some errors if that option is
choosen.  If result is less than zero print an error message.*/
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

/*Waits for SYNACK from the server. If no message is received within the alloted time
  or the flag is not a SYNACK resend the SYN to the server.*/
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

/*Fills the header with the sent in info in the function header.*/
void makePacket(rtp *buffer, int packetNumber, char data[], int checksum) {

  buffer->seq = packetNumber;
  strcpy(buffer->data, data);
  buffer->checksum = checksum;
}

/*Copies the sequence number and returns it.*/
int getAckNumber(rtp *buffer) {
  int ackNumber = buffer->seq;

  return ackNumber;
}

/*If the packet is within windowsize and the ACK is equal or greater than base return true (1). Else return (0).*/
int packetInWindow(int ackNumber, int base) {
  if ((ackNumber <= (base + WINDOWSIZE - 1)) && (ackNumber >= base))
    return 1;

  return 0;
}

/*Stops the timer, calculates the duration of the timeout and then compares the computed value
to the timeout type sent in the function header. If true return 1 else return 0.*/
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

/*Does what the function header implies*/
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

/*This function changes some random values inorder to corrupt the packages.*/
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

/*Prints out what is lost so that the user know the progress of the program.*/
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

/*Prints out the package number and what is corrupt so that the user know the progress of the program.*/
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