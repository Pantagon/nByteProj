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

#include <pthread.h>
#define DEFAULT_SERVER_PORT 5001
#define MAX_CLIENT 10
#define MAX_DATA 1024
#define MAX_REQUEST_STRING 24

int server_port = DEFAULT_SERVER_PORT;
/*handle an incoming connection*/
void* handle_connection(void* ptr);
/*print usage*/
void usage();
/*generate string with length n*/
char* getString(long int n);
/*
void* testhread(void* ptr){
	printf("testing\n");
	return NULL;
}
*/
int main(int argc, char **argv)
{
    pid_t pid, tid;
    pthread_t serv_thread;
    int listen_fd;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;

    int* sockfd_ptr = NULL;
    socklen_t len = sizeof(struct sockaddr_in);

    /*get option*/
    int opt;
    while((opt = getopt(argc,argv, "p:")) != -1){
        switch (opt){
            case 'p':
                server_port = atoi(optarg);
                break;
            default:
                usage();
        }
    }
    if (server_port < 0 || server_port > 65535) {
        perror("invalid port number");
        usage();
    }
    /* initialize local server address */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(server_port);

    /* initialize server socket */
    listen_fd = socket(AF_INET, SOCK_STREAM,0);
    if(listen_fd < 0){
        perror("initialize socket");
        return -1;
    }
    if(bind(listen_fd,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr))<0){
        perror("bind");
        return -1;
    }
    if(listen(listen_fd, MAX_CLIENT)<0){
        perror("listen");
        return -1;
    }
	    
    printf("Server listening on port %d\n", server_port);


    while(1){
        sockfd_ptr = (int*)malloc(sizeof(int));
        if(!sockfd_ptr){
            perror("malloc");
            return -1;
        }
        *sockfd_ptr = accept(listen_fd,(struct sockaddr *)&cli_addr, &len);
        if(*sockfd_ptr <0){
            close(listen_fd);
            free(sockfd_ptr);
            perror("accept");
            return -1;
        }
        printf("Client connected from port no %d and IP %s\n",ntohs(cli_addr.sin_port),inet_ntoa(cli_addr.sin_addr));
        
        if(pthread_create(&serv_thread, NULL, handle_connection, (void*)sockfd_ptr)<0){
            close(listen_fd);
            free(sockfd_ptr);
            perror("pthread");
            return -1;
        }
        
        /*
        if(pthread_create(&serv_thread, NULL, testhread, (void*)sockfd_ptr)<0){
            close(listen_fd);
            free(sockfd_ptr);
            perror("pthread");
            return -1;
        }*/        
        printf("Server listening on port %d\n\n\n", server_port);
    }

    return 0;
}

void* handle_connection(void* ptr){
    
    int sockfd = *(int*)ptr;      
    free(ptr);
    char* buffer = (char* )malloc(100);
    bzero(buffer,MAX_REQUEST_STRING);
    long int nByte = 0;

    int sent = 0;   
    
    int data_len = 0;
    
   
    while(1){
    	data_len=recv(sockfd,buffer,sizeof(buffer),0);
    	printf("data_len: %d\n",data_len);    	
        if(data_len<0){
            perror("read");            
            free(buffer);
    		close(sockfd);
    		return NULL;
        }
        else if(data_len>0){
        	break;
        }
        
    }    
    printf("Client requests for %s bytes of data\n",buffer);
    if((nByte= atol(buffer))<=0){
        printf("num of byte cannot be non-positive");
        free(buffer);
    	close(sockfd);
    	return NULL;
    }


	char* traffic = NULL;
    while(nByte>0){
    	
        if(nByte<=MAX_DATA){
            traffic = getString(nByte);
            if((sent=send(sockfd, traffic, nByte ,0))==-1){
                perror("send");
                free(traffic);
                free(buffer);
			    close(sockfd);
			    return NULL;
            }
            printf("Sent %d bytes\n",sent);
            nByte -= sent;
            
        }
        else{
            traffic = getString(MAX_DATA);
            if((sent=send(sockfd, traffic, MAX_DATA ,0))==-1){
                perror("send");
                free(traffic);
                free(buffer);
			    close(sockfd);
			    return NULL;
            }
            //printf("Sent %d bytes\n",sent);
            nByte -= sent;
            
        }
    }
    printf("finish sending\n");
	
    
    free(buffer);
    close(sockfd);
    
    
    return NULL;
}
void usage(){
    printf("Usage: server -p <port>\n");
    exit(-1);
}
char *getString(long int n){
    char *input = (char *)malloc(sizeof(char)*n);
    int  i;
    for(i=0; i<n-1;i++){
        *(input+i) = '*';
    }
    *(input+n)='\0';
    return input;
}
int nonblock_read(int sockfd, void* buffer, size_t len){
    int data_len = read(sockfd,buffer,len);
    if(data_len<0){
        if(errno==EWOULDBLOCK){
        	return 0;
        } 
        else {
        	return -1;
        }
    }
    else {    	
    	return data_len;
    }

}