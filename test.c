#include "header.h"
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 5555
// #define hostNameLength 50
// #define messageLength 256
// #define MAXMSG 512

int createSocket(struct sockaddr_in *serverName,char *argv) {

  // int dstHost = "127.0.0.1";
  int socketfd;
  if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    printf("Can't create UDP socket\n");

  memset(serverName, 0, sizeof(*serverName));

  serverName->sin_family = AF_INET;
  serverName->sin_addr.s_addr = inet_addr(argv);
  if(inet_addr(argv) == -1){
      printf("Error!");
  }
  printf("s_addr: %d", serverName->sin_addr.s_addr);

  return socketfd;
}

int main(int argc, char *argv[]) {

  struct sockaddr_in serverName;
  argv[1] = "localhost";
  createSocket(&serverName, argv[1]);

int main(){
    printf("This is a test in which we would like to see what git overwrites and what it doesn't.\n");
    printf("Snopp");
    return 0;
}

