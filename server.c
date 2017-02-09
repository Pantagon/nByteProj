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
#define MAX_TO_WRITE 1024000
#define MAX_REQ_LEN 12

#define min(a, b) ((a < b)? a : b)

char traffic[MAX_TO_WRITE] = {0};
int server_port = DEFAULT_SERVER_PORT;
/*handle an incoming connection*/
void* handle_connection(void* ptr);
/*print usage*/
void usage_serv();

/*struct for passing parameter to thread*/
struct parameter_serv
{
	struct sockaddr_in cli_addr;
	int sockfd_ptr;
};


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
                usage_serv();
        }
    }
    if (server_port < 0 || server_port > 65535) {
        perror("invalid port number");
        usage_serv();
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

    printf("Server listening on port %d...\n", server_port);


    while(1){
        struct parameter_serv *threadArg = (struct parameter_serv*) malloc(sizeof(struct parameter_serv));
        if(!threadArg){
            perror("malloc");
            return -1;
        }
        if(((*threadArg).sockfd_ptr = accept(listen_fd,(struct sockaddr *)&((*threadArg).cli_addr), &len)) <0){
            close(listen_fd);
            free(threadArg);
            perror("accept");
            return -1;
        }
        printf("    Client connected (%s: %d) \n",inet_ntoa((*threadArg).cli_addr.sin_addr),ntohs((*threadArg).cli_addr.sin_port));

        if(pthread_create(&serv_thread, NULL, handle_connection, (void*)threadArg)<0){
            close(listen_fd);
            free(threadArg);
            perror("pthread");
            return -1;
        }        
    }

    return 0;
}

void* handle_connection(void* threadArg){
    struct sockaddr_in cli_addr = (*(struct parameter_serv*) threadArg).cli_addr;
    int sockfd = (*(struct parameter_serv*) threadArg).sockfd_ptr;
    free(threadArg);
    char buffer[MAX_REQ_LEN] = {0};
    int next_to_wrt = 0;
    long long nByte = 0;

    int sent = 0;

    int data_len = 0;


    while(1) {  	

        if((data_len = recv(sockfd, buffer + next_to_wrt, MAX_REQ_LEN - next_to_wrt, 0)) <= 0){
			perror("read");
			close(sockfd);
			return NULL;
        }

        next_to_wrt += data_len;

        if (MAX_REQ_LEN <= next_to_wrt || buffer[next_to_wrt - 1] == '\0') break;

    }

    printf("    Client (%s: %d) requests for %s bytes of data\n",inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port), buffer);
    if((nByte = atoll(buffer)) <= 0){
        printf("num of byte cannot be non-positive");

    	close(sockfd);
    	return NULL;
    }

    long long remain_byte_to_sent = nByte;
    while (remain_byte_to_sent > 0){

        if ((sent = send(sockfd, traffic, min(remain_byte_to_sent, MAX_TO_WRITE) ,0)) == -1) {
        	perror("send");
			close(sockfd);
			return NULL;
    	}
        remain_byte_to_sent -= sent;
    }

    printf("        Finish sending for client (%s: %d)\n",inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
    close(sockfd);
    return NULL;
}
void usage_serv(){
    printf("Usage: server -p <port>\n");
    exit(-1);
}
