#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>  
#include <signal.h>  
#include <sys/time.h>     

#define RCVBUFSIZE 255

struct icmp_struct {
    unsigned char type;
    unsigned char code;
    unsigned short id;
    unsigned short checksum;
    unsigned short seq;
    char data[56];
};

void DieWithError(char *str, int errorCode) {
    fprintf(stderr, "Error: %s. Error code %i\n", str, errorCode);
    exit(1);
}

long mtime() {
  struct timeval t;

  gettimeofday(&t, NULL);
  long mt = (long)t.tv_sec * 1000 + t.tv_usec / 1000;
  return mt;
}

int main(int argc, char **argv) {
    char* servIP = "8.8.8.8";
    char echoBuffer[RCVBUFSIZE];
    int sock;
    int timeout = 1000;
    int timeoutCounter = 0;
    pid_t pid = getpid();
    long _time;
    struct sockaddr_in echoServAddr;
    struct icmp_struct icmp, *recv_icmp;

    if(argc == 2) servIP = argv[1];
    if(argc == 3) {
        servIP = argv[1];
        timeout = atoi(argv[2]);
    }
    if(argc > 3) {
       fprintf(stderr, "Usage: %s [<Server IP>] [<timeout>]\n", argv[0]);
       exit(1);
    }

    if((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
        DieWithError("socket() failed", sock);

    bzero(&echoServAddr, sizeof(echoServAddr));

    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    
    icmp.type = 8;      // 8 - запрос, 0 - ответ
    icmp.code = 0;
    icmp.id = pid;
    icmp.checksum = 0;
    icmp.seq = 1;

    _time = mtime();
    sprintf(icmp.data, "%ld", _time);

    // while (1) {
    //     if(!sendto(sock, icmp.data, sizeof(icmp) + 10, 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)))
    //         DieWithError("send() sent a different number of bytes than expected", 1);

    //     // timeoutCounter++;
    //     // if(timeoutCounter > timeout) {
    //         recvfrom(sock, echoBuffer, RCVBUFSIZE, 0, 0, 0);
    //         // recv_icmp = &echoBuffer[20];
    //         printf("recv packet with id = %hu\n", recv_icmp->id);
    //         break;
    //     // }
    // }

    _time = mtime() - _time;

    // if(recv_icmp->type == 0 && recv_icmp->id == pid) {
    //     printf("seq= %i timeout= %i time= %ld ms\n", recv_icmp->seq, timeout, _time);
    // }

    return 0;
}