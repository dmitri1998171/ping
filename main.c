#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>  
#include <sys/time.h>     
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netdb.h> 
#include <signal.h>  

#define RCVBUFSIZE 255

ushort termFlag = 0;

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

float mtime(struct timeval* tv) {
    // struct timeval t;
    gettimeofday(tv, NULL);
    float mt = (float)tv->tv_usec / 1000 ;
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

float findAvg(float *arr, int seq) {
    float timeAvg = 0;

    for(int i = 0; i < seq; i++)
        timeAvg += arr[i];
    
    timeAvg = timeAvg / seq;

    return timeAvg;
}

float findMin(float *arr, int seq) {
    float min = arr[0];

    for(int i = 0; i < seq - 1; i++)
        if(arr[i] < min) min = arr[i];

    return min;
}

float findMax(float *arr, int seq) {
    float max = arr[0];
    for(int i = 0; i < seq - 1; i++)
        if(arr[i] > max) max = arr[i];

    return max;
}

void listener(int signum) {
    termFlag = 1;
}

int checkIP(char *str) {
    int octetCounter = 0;
    char *token, *last;
    char *tmp = strdup(str);
    char *ip = strtok(tmp, ":");

    token = strtok_r(tmp, ".", &last);
    while(token != NULL) {
        if(atoi(token) > 255)
            DieWithError("Invalid IP!", 1);
        token = strtok_r(NULL, ".", &last);
        octetCounter++;
    }
    
    if(octetCounter == 4) 
        return 1;
    else
        return 0;
}

int isNumber(char number[]) {
    int i = 0;

    if(!strcmp(&number[0], "-"))
        i = 1;

    for(; i < strlen(number); i++)
        if(number[i] >= 0x41 && number[i] <= 0x7A)
            return 0;

    return 1;
}

int main(int argc, char **argv) {
    int sock;
    int seq = 1, recvCount = 0, lostPackets = 0;
    int status_addr = 0;
    int timeout = 1000;
    int thread_recv_status, thread_sleep_status;
    pthread_t thread_recv, thread_sleep;
    pid_t pid = getpid();
    float _time;
    float* timeAvgArr = (float*) malloc(1 * sizeof(float));
    char* hostname = "8.8.8.8";
    char echoBuffer[RCVBUFSIZE];
    struct timeval tv;
    struct hostent *he;
    struct sockaddr_in echoServAddr;
    struct icmp_struct icmp, *recv_icmp;
    
    if(argc > 1) hostname = argv[1];
    if(argc > 2) {
        timeout = atoi(argv[2]);
    }
    if(argc > 3) {
       fprintf(stderr, "Usage: %s [<Server IP>] [<timeout>]\n", argv[0]);
       exit(1);
    }

    if(isNumber(hostname)) {
        if(!checkIP(hostname))
            DieWithError("Invalid IP", 1);
    }else {
        he = gethostbyname(hostname);
        if(he == NULL)
            DieWithError("gethostbyname", 1);

        hostname = inet_ntoa(*(struct in_addr*)he->h_addr);
        printf("DNS name: %s\n", he->h_name);
    }
    printf("IP-address: %s\n\n", hostname);

    if((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
        DieWithError("socket() failed", sock);

    tv.tv_sec = timeout;                                                    //
    tv.tv_usec = 0;                                                         // timeout for recvfrom
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv); //

    bzero(&echoServAddr, sizeof(echoServAddr));

    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(hostname);
    
    while(1) {
        signal(SIGINT, listener);

        if(termFlag) {
            float timeAvg, timeMin, timeMax = 0;

            timeAvg = findAvg(timeAvgArr, seq);
            timeMin = findMin(timeAvgArr, seq);
            timeMax = findMax(timeAvgArr, seq);

            printf("\n---------------------------------------------\n\n");
            printf("time avg: %.3f ms | min: %.3f ms | max: %.3f ms\n", timeAvg, timeMin, timeMax);
            printf("%i packets transmitted, %i packets received, %d%% packet loss\n\n", seq - 1, recvCount, recvCount / seq);
            exit(0);
        }

        icmp.type = 8;          // 8 - запрос, 0 - ответ
        icmp.code = 0;
        icmp.id = pid;
        icmp.seq = seq;
        icmp.checksum = 0;
        
        _time = mtime(&tv);
        sprintf(icmp.data, "%f", _time);

        icmp.checksum = in_cksum((u_short *) &icmp, sizeof(icmp));

        if(!sendto(sock, &icmp, sizeof(icmp), 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)))
            DieWithError("send() sent a different number of bytes than expected", 1);

        recvfrom(sock, echoBuffer, RCVBUFSIZE, 0, 0, 0);
        recv_icmp = (struct icmp_struct *)(echoBuffer + 20);

        if(recv_icmp->type == 0 && recv_icmp->id == icmp.id + 8) {
            _time = mtime(&tv) - _time;
            
            if(_time > 0) {
                printf("seq = %i timeout = %i time = %.3f ms\n", recv_icmp->seq, timeout, _time);
                
                if(seq >= 1)
                    timeAvgArr = (float*) realloc(timeAvgArr, seq + 1);
                
                timeAvgArr[seq - 1] = _time;
                recvCount++;
            }
        }

        seq++;
        usleep(timeout * 1000);
    }

    return 0;
}
