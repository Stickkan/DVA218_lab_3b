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

int readMessage(rtp *buffer) {

  int flag = readFlag(buffer);
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

  buffer->windowsize = windowSize;
  buffer->flags = flag;
  while (1) {
    int result =
        sendto(socketfd, buffer, sizeof(*buffer), 0,
               (const struct sockaddr *)clientName, sizeof(*clientName));

    if (result < 0) {
      printf("Could not send message!\n");
      return 0;
    } else
      break;
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
  if (flag == DR) {
    printf("Disconnect request received from client!\n Initiating "
           "shutdown!\n");
    return 1;
  }
  return 0;
}

void sendNack(int socketfd, rtp *buffer, struct sockaddr_in *clientName) {
  int seqNumber;
  seqNumber = buffer->seq;
  memset(buffer, 0, sizeof(*buffer));
  buffer->seq = seqNumber;
  sendMessage(NACK, socketfd, buffer, clientName);
}

int isDRACK(rtp *buffer) {
  if (buffer->flags == DRACK) {
    return 1;
  }
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