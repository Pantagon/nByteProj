#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/time.h>

#define BUFFER 1024
#define MAX_REQ_LEN 12
void usage_cli();
int isValidNumber(char* input);
int isValidPort(int port);
int isValidIP(char* IP);
/*
struct parameter_cli
{
    struct timeval start;
    struct timeval end;
    long int flow_complete_time;
    float throughput;

};*/ //TODO: use struct to store args

int main(int argc, char** argv) {

    int opt;
    char* servIP = NULL;
    int port = -1;
    long long numBytes = 0;
    char* numBytes_string = NULL;
    int loopflag = 0;
    int sockfd;
    while ((opt = getopt(argc, argv, "s:n:p:")) != -1) {
        loopflag = 1;
        switch (opt) {
            case 's':
                servIP = optarg;
                break;
            case 'n':
                numBytes_string = optarg;                               
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                usage_cli();
        }
    }
    if(!loopflag) usage_cli();

    if((isValidPort(port) && isValidNumber(numBytes_string) && isValidIP(servIP)) == 0) usage_cli();
    numBytes = atoll(numBytes_string);

    printf("Requesting server (%s: %d) to send %s bytes of data.\n", servIP, port, numBytes_string);
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Failed to initialize socket.\n");
        exit(-1);
    }

    struct sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    servAddr.sin_addr.s_addr = inet_addr(servIP);
    if (connect(sockfd, (struct sockaddr*) &servAddr, sizeof(struct sockaddr)) == -1) {
        perror("Failed to connect to server.\n");
        exit(-1);
    }
    
    struct timeval start;
    struct timeval end;
    unsigned long flow_complete_time;
    float throughput;

    char* data = numBytes_string;
    int req_len = strlen(numBytes_string) + 1;    
    int written;

    if ((written = write(sockfd, data, req_len)) < 0) {
        if (errno == EINTR) {
            written = 0;
        } 
        else {
            perror("Error writing number of bytes to server.\n");
            exit(-1);
        }
    }

    if(gettimeofday(&start, NULL)==-1){
        perror("Timer\n");
        exit(-1);
    }

    printf("Finished sending the number of bytes to the server.\n");
    printf("Now ready for receiving data from the server...\n");

    long long dataLeft = numBytes;
    long long total = 0;
    int dataLength = 0;
    char buffer[BUFFER] = {0};           
    
    while (dataLeft > 0) {
        if ((dataLength = read(sockfd, buffer, BUFFER)) < 0) {
            
            if (errno == EINTR) {
                dataLength = 0;
            } else {
                perror("Error receiving data.\n");
                exit(-1);
            }
        }       
        
        dataLeft -= (long long)dataLength;
        total += (long long)dataLength;
        if ((dataLength == 0) && (total > 0)) break;        
    }

    if(gettimeofday(&end, NULL)==-1){
        perror("Timer\n");
        exit(-1);
    }

    flow_complete_time = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    printf ("   Compute %lld B / %ld us \n", total, flow_complete_time);
    throughput = ((float)total / ((float)flow_complete_time)) ;
    printf ("   Finished receiving. Totally received: %lld / %s bytes.\n", total,numBytes_string);
    printf ("   Throughput: %.3f MB/S\n", throughput);
    return 0;
}



void usage_cli(){
    printf("Usage: ./client -s <servIP> -n <numBytesToRcv> -p <port>\n");
    exit(-1);
}

int isValidIP(char* servIP) {
    struct sockaddr_in sa;
    if (servIP == NULL || inet_pton(AF_INET, servIP, &(sa.sin_addr)) == 0) {
        printf("Invalid IP.\n");
        return 0;
    }
    return 1;    
}

int isValidNumber(char* numBytes_string){
    if (strlen(numBytes_string) > MAX_REQ_LEN){
        printf("num of bytes should smaller than 999,999,999,999B (~1TB)\n");
        return 0;
    }     
    if (atoll(numBytes_string) <= 0) {
        printf("Number of bytes must be positive.\n");
        return 0;
    }
    return 1;
}

int isValidPort(int port){
    if (port <= 0 || port > 65535) {
        printf("Invalid port number.\n");
        return 0;
    }
    return 1;
}
