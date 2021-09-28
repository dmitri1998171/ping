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

float mtime() {
    struct timeval t;

    gettimeofday(&t, NULL);
    float mt = (float)t.tv_usec / 1000 ;
    return mt;
}

unsigned short in_cksum(unsigned short *addr, int len) {
    int             nleft = len;
    int             sum = 0;
    unsigned short  *w = addr;
    unsigned short  answer = 0;
    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)  {
        sum += *w++;
        nleft -= 2;
    }
 
        /* 4mop up an odd byte, if necessary */
    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w ;
        sum += answer;
    }
 
        /* 4add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return(answer);
}

int main(int argc, char **argv) {
    char* servIP = "8.8.8.8";
    int sock;
    int seq = 1;
    int timeout = 1000;
    int timeoutCounter = 0;
    pid_t pid = getpid();
    float _time;
    char echoBuffer[RCVBUFSIZE];
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
    
    while(1) {
        icmp.type = 8;          // 8 - запрос, 0 - ответ
        icmp.code = 0;
        icmp.id = pid;
        icmp.seq = seq;
        icmp.checksum = 0;
        
        _time = mtime();
        sprintf(icmp.data, "%f", _time);

        icmp.checksum = in_cksum((u_short *) &icmp, sizeof(icmp));

        if(!sendto(sock, &icmp, sizeof(icmp), 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)))
            DieWithError("send() sent a different number of bytes than expected", 1);

        // timeoutCounter++;
        // if(timeoutCounter > timeout) {
        recvfrom(sock, echoBuffer, RCVBUFSIZE, 0, 0, 0);
        recv_icmp = (struct icmp_struct *)(echoBuffer + 20);

        if(recv_icmp->type == 0 && recv_icmp->id == pid + 8) {
            _time = mtime() - _time;
            
            if(_time > 0)
                printf("seq = %i timeout = %i time = %.3f ms\n", recv_icmp->seq, timeout, _time);
        }
        
        seq++;
        usleep(timeout * 1000);
    }

    return 0;
}
