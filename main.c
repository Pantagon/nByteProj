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

#define PORT    10000
#define BUFFER  1024
#define ERROR   -1
#define MAX_CLIENT 2
#define MAX_DATA 1024
int main(int argc, char **argv)
{
    char role = 'x';
    char *serverAddr = NULL;
    unsigned int nByte = 0;
    int opt = 0;
    int serverFlag = 0;
    int clientFlag = 0;
    static const char *optString= "sc:n:?";
    opt = getopt(argc,argv,optString);

    while(opt!=-1){
        switch(opt){
        case 's':
            if(clientFlag){
                printf("cannot be both server and client\n");
                exit(-1);
            }
            role = 's';
            serverFlag = 1;
            break;
        case 'c':
            if(serverFlag){
                printf("cannot be both server and client\n");
                exit(-1);
            }
            role = 'c';
            clientFlag = 1;
            if(isIpAddr(optarg)==0){
                printf("invalid server IP address\n");
                exit(-1);
            }
            serverAddr = optarg;
            break;
        case 'n':
            if(!clientFlag){
                printf("-n is for client only\n");
                exit(-1);
            }
            if(atoi(optarg)<=0){
               printf("-n must follow a positive int from 1~2,147,483,647\n");
               exit(-1);
            }
            nByte = atoi(optarg);
            break;
        case '?':
            printf("nByteProj -s | -c <server_IP> -n <numOfBytes> | -?\n");
            exit(-1);
            break;
        default:
                printf("illegal opt parsing, plz type -? for help\n");
                exit(-1);
            break;
        }
        opt = getopt(argc,argv,optString);
    }

    if(role=='x'){
        printf("Plz specify server or client\n");
        exit(-1);
    }

    if(serverFlag){
        if(doServer() == ERROR){
            printf("something goes wrong in server\n");
            exit(-1);
        }
    }
    if(clientFlag){
        if(nByte==0){
            printf("client should specify num of bytes: -n \n");
            exit(-1);
        }
        if(doClient(serverAddr,nByte) == ERROR){
            printf("something goes wrong in client\n");
            exit(-1);
        };

    }
    return 0;
}


int doServer(){
    struct sockaddr_in server;
    struct sockaddr_in client;
    int sock;
    int target;
    socklen_t sockaddr_len =sizeof(struct sockaddr_in);
    int total=0;
    int data_len;
    char data[MAX_DATA];

    if((sock = socket(AF_INET, SOCK_STREAM,0)) == ERROR){
        perror("server socket");
        return -1;
    }
    printf("server socket created\n");
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero,8);

    if((bind(sock,(struct sockaddr *)&server,sockaddr_len)) == ERROR){
        perror("bind");
        return -1;
    }
    printf("socket bind\n");
    if((listen(sock,MAX_CLIENT))==ERROR){
        perror("listen");
        return -1;
    }
    printf("server listening...\n");
    while(1){
        if((target = accept(sock,(struct sockaddr *)&client, &sockaddr_len))==ERROR){
            perror("accept");
            return -1;
        }
        printf("Client connected from port no %d and IP %s\n",ntohs(client.sin_port),inet_ntoa(client.sin_addr));
        data_len = 1;
        while(data_len){
            if((data_len = recv(target,data,MAX_DATA,0))==ERROR){
                perror("receive");
                return -1;
            };
            if(data_len){
                total += data_len;
            }
        }
        printf("Client disconnected, total receive traffic: %d byte(s)\n",total);
        close(target);
        printf("server listening...\n");
        total = 0;
    }
}

char *getString(unsigned int n){
    char *input = (char *)malloc(sizeof(char)*n);
    int  i;
    for(i=0; i<n-1;i++){
        *(input+i) = '*';
    }
    *(input+n)='\0';
    return input;
}

int isIpAddr(char *ipAddress){
    struct sockaddr_in sa;
    return inet_pton(AF_INET,ipAddress,&(sa.sin_addr));
}

int doClient(char * servAddr, unsigned int nByte){
    struct sockaddr_in remote_server;
    int sock;
    char *traffic = NULL;
    if((sock = socket(AF_INET,SOCK_STREAM,0)) == ERROR){
        perror("client socket");
        return -1;
    }
    remote_server.sin_family = AF_INET;
    remote_server.sin_port = htons(PORT);
    remote_server.sin_addr.s_addr = inet_addr(servAddr);
    bzero(&remote_server.sin_zero,8);

    if((connect(sock,(struct sockaddr *)&remote_server,sizeof(struct sockaddr_in))) == ERROR){
        perror("connect");
        return -1;
    }
    unsigned int toBeSent = nByte;
    int sent =0;
    while(toBeSent>0){
        if(toBeSent<=MAX_DATA){
            traffic = getString(toBeSent);
            if((sent=send(sock, traffic, toBeSent ,0))==ERROR){
                perror("send");
                free(traffic);
                return -1;
            }
            printf("Sent %d bytes\n",sent);
            toBeSent -= sent;
            free(traffic);
        }
        else{
            traffic = getString(MAX_DATA);
            if((sent=send(sock, traffic, MAX_DATA ,0))==ERROR){
                perror("send");
                free(traffic);
                return -1;
            }
            printf("Sent %d bytes\n",sent);
            toBeSent -= sent;
            free(traffic);
        }
    }
    printf("finish sending\n");
    return 0;


}

