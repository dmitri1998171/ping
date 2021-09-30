#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>  
#include <signal.h>  
#include <sys/time.h>     
#include <pthread.h>

#define RCVBUFSIZE 255
#define TIME_AVG_SIZE 5

ushort termFlag = 0;

struct icmp_struct {
    unsigned char type;
    unsigned char code;
    unsigned short id;
    unsigned short checksum;
    unsigned short seq;
    char data[56];
};

typedef struct thr_struct {
    int sock;
    int _time;
    char* echoBuffer;
}recv_thr_struct;

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

void findAvg(float *arr, float *timeAvg) {
    for(int i = 0; i < TIME_AVG_SIZE; i++)
        *timeAvg += arr[i];
    
    *timeAvg = *timeAvg / TIME_AVG_SIZE;
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

void *receiveFunc(void *args) {
    recv_thr_struct *recv_struct = (recv_thr_struct*) args;
    recvfrom(recv_struct->sock, recv_struct->echoBuffer, RCVBUFSIZE, 0, 0, 0);
    return EXIT_SUCCESS;
}

void *sleepFunc(void *args) {
    int *sleepArr = (int*) args;
    usleep(sleepArr[0] * 1000);
    sleepArr[1] = 1;
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    char* servIP = "8.8.8.8";
    int sock;
    int sleepArr[2];
    int seq = 1, recvCount = 0, lostPackets = 0;
    int status_addr = 0;
    int timeout = 1000;
    int timeoutCounter = 0;
    int thread_recv_status, thread_sleep_status;
    pthread_t thread_recv, thread_sleep;
    pid_t pid = getpid();
    float _time;
    float* timeAvgArr = (float*) malloc(TIME_AVG_SIZE * sizeof(float));
    char echoBuffer[RCVBUFSIZE];
    struct timeval tv;
    struct sockaddr_in echoServAddr;
    struct icmp_struct icmp, *recv_icmp;
    recv_thr_struct recv_struct;
    
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
    
    tv.tv_sec = timeout;                                                    //
    tv.tv_usec = 0;                                                         // timeout for recvfrom
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv); //

    bzero(&echoServAddr, sizeof(echoServAddr));

    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    
    recv_struct.sock = sock;
    recv_struct._time = _time;
    recv_struct.echoBuffer = echoBuffer;
    
    int recvRun = 0;

    while(1) {
        signal(SIGINT, listener);

        if(termFlag) {
            float timeAvg, timeMin, timeMax = 0;

            findAvg(timeAvgArr, &timeAvg);
            timeMin = findMin(timeAvgArr, seq);
            timeMax = findMax(timeAvgArr, seq);

            printf("\n---------------------------------------------\n\n");
            printf("time avg: %.3f ms | min: %.3f ms | max: %.3f ms\n", timeAvg, timeMin, timeMax);
            printf("%i packets transmitted, %i packets received, %.2d%% packet loss\n\n", seq - 1, recvCount, recvCount / seq);
            exit(0);
        }

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

        recvfrom(sock, echoBuffer, RCVBUFSIZE, 0, 0, 0);
        recv_icmp = (struct icmp_struct *)(echoBuffer + 20);

        if(recv_icmp->type == 0 && recv_icmp->id == icmp.id + 8) {
            _time = mtime() - _time;
            
            if(_time > 0) {
                printf("seq = %i timeout = %i time = %.3f ms\n", recv_icmp->seq, timeout, _time);
                
                if(seq >= TIME_AVG_SIZE)
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
