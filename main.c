#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>  
#include <signal.h>  
#include <time.h>     

#define RCVBUFSIZE 255

struct icmp_struct {
    unsigned char type;
    unsigned char code;
    unsigned short id;
    unsigned short checksum;
    unsigned short seq;
    char data[56];
};

void DieWithError(char *str) {
    fprintf(stderr, "Error: %s", str);
    exit(1);
}

double getTime() {
    double time = 0;
    
    return time;
}

int main(int argc, char **argv) {
    char echoBuffer[RCVBUFSIZE];    
    char* servIP = "8.8.8.8";
    int timeout = 1000;  
    int sock;                        
    struct sockaddr_in echoServAddr; 
    struct icmp_struct icmp, *recv_icmp;
    pid_t pid = getpid();

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
        DieWithError("socket() failed");

    bzero(&echoServAddr, sizeof(echoServAddr));

    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    
    // srand(time(NULL));

    icmp.type = 8;      // 8 - запрос, 0 - ответ
    icmp.code = 0;
    // icmp.id = rand();
    icmp.id = pid;
    icmp.checksum = 0;
    icmp.seq = 1;
    strcpy(icmp.data, "abcdefghi");

    double requestTime = getTime();

    if(!sendto(sock, &icmp, sizeof(icmp), 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)))
        DieWithError("send() sent a different number of bytes than expected");

    recvfrom(sock, echoBuffer, 255, 0, 0, 0);
    recv_icmp = &echoBuffer[20];

    printf("recv packet with id = %i\n", recv_icmp->id);

    double replayTime = getTime();
    double deltaTime = requestTime - replayTime;

    if(recv_icmp->type == 0 && recv_icmp->id == pid) {
        printf("seq= %i timeout= %i time= %f\n", recv_icmp->seq, timeout, deltaTime);
    }

    return 0;
}