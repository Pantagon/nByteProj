#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>

#include "../common/common.h"
#define SERVADDR1 ((unsigned long int) 0xc0a86501 )	// "192.168.101.1"
#define SERVADDR1 ((unsigned long int) 0x0A590B91 )	// "10.89.11.145"

/*Modified: no rate limiting*/

int server_port = TG_SERVER_PORT;
unsigned int sleep_overhead_us = 50;
bool verbose_mode = false;  /* by default, we don't give more detailed output */
bool daemon_mode = false;   /* by default, we don't run the server as a daemon */

/* print usage of the program */
void print_usage(char *program);
/* read command line arguments */
void read_args(int argc, char *argv[]);
/* handle an incomming connection */
void* handle_connection(void* ptr);
/* get usleep overhead in microsecond (us) */
unsigned int get_sleep_overhead(int iter_num);

int main(int argc, char *argv[])
{
    pid_t pid, sid;
    int listen_fd;
    struct sockaddr_in serv_addr;   /* local server address */
    struct sockaddr_in cli_addr;    /* remote client address */
    int sock_opt = 1;
    pthread_t serv_thread;  /* server thread */
    struct Thread_arg* thread_ptr = NULL;
    socklen_t len = sizeof(struct sockaddr_in);

    /* read arguments */
    read_args(argc, argv);

    /* initialize local server address */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;    
    serv_addr.sin_port = htons(server_port);

    /* initialize server socket */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        error("Error: initialize socket");

    /* set socket options */
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt)) < 0)
        error("Error: set SO_REUSEADDR option");
    if (setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY, &sock_opt, sizeof(sock_opt)) < 0)
        error("ERROR: set TCP_NODELAY option");

    if (bind(listen_fd,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr)) < 0)
        error("Error: bind");

    if (listen(listen_fd, TG_SERVER_BACKLOG_CONN) < 0)
        error("Error: listen");

    printf("Traffic Generator Server listens on 0.0.0.0:%d\n", server_port);

    /* if we run the server as a daemon */
    
	if (daemon_mode)
    {
        /* fork off the parent process */
        pid = fork();
        /* no child process is created */
        if (pid < 0)
            exit(EXIT_FAILURE);
        /* if we get the process ID of the child process, then we can exit the parent process */
        if (pid > 0)
        {
            printf("Running Traffic Generator Server as a daemon\n");
            printf("The daemon process ID: %d\n", pid);
            exit(EXIT_SUCCESS);
        }

        /* change the file mode mask */
        umask(0);

        /* create a new SID for the child process */
        sid = setsid();
        if (sid < 0)
            exit(EXIT_FAILURE);

        /* change the current working directory */
        if ((chdir("/")) < 0)
            exit(EXIT_FAILURE);

        /* close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }


    while (1)
    {        
	int sockfd;
	if(!(thread_ptr = (struct Thread_arg*)malloc(sizeof(struct Thread_arg))))
	    error(malloc);
	
        if((sockfd = accept(listen_fd, (struct sockaddr *)&cli_addr, &len)) < 0)
        {
            close(listen_fd);
            error("accept");
        }
	printf("Client connected from port no %d and IP %s\n\n", ntohs(cli_addr.sin_port), inet_ntoa(cli_addr.sin_addr));

	
	(*thread_ptr).sockfd = sockfd;
	(*thread_ptr).daddr = cli_addr.sin_addr.s_addr;
	(*thread_ptr).dport = ntohs(cli_addr.sin_port);
	(*thread_ptr).saddr = SERVADDR;
	(*thread_ptr).sport = server_port;
	(*thread_ptr).sockfd = sockfd;	

        if (pthread_create(&serv_thread, NULL, handle_connection, (void*)thread_ptr) < 0)
        {
            close(listen_fd);
            free(thread_ptr);
            error("Error: create pthread");
        }
    }

    return 0;
}

/* handle an incomming connection */
void* handle_connection(void* ptr)
{
    struct flow_metadata flow;
    struct Thread_arg* arguments = (struct Thread_arg*)ptr;
    int sockfd = (*arguments).sockfd;
    uint32_t saddr = (*arguments).saddr;
    uint32_t daddr = (*arguments).daddr;
    uint16_t sport = (*arguments).sport;
    uint16_t dport = (*arguments).dport;
    uint32_t size_in_bytes = 0;
    uint32_t microsecs_to_ddl = 0;    
    free(ptr);

    while (1)
    {
        /* read meta data from the request */
        if (!read_flow_metadata(sockfd, &flow))
        {
            if (verbose_mode)
                printf("Cannot read metadata from the request\n");
            break;
        }

        if (verbose_mode)
            printf("Flow request: ID: %u Size: %lu bytes\n Deadline: %lu \n", flow.id, flow.size, flow.ddl);
		size_in_bytes = flow.size;
		microsecs_to_ddl = flow.ddl;
        /*
	if (nl_send_d2tcp_ctrl_msg(saddr, sport, daddr, dport, size_in_bytes, microsecs_to_ddl) < 0)
	{
            printf("socket() in nl_send_d2tcp_ctrl_msg");
	    break;
	}
	*/
        /* generate the flow response */
        if (!write_flow(sockfd, &flow))
        {
            if (verbose_mode)
                printf("Cannot generate the response\n");
            break;
        }
    }

    close(sockfd);
    return (void*)0;
}

/* Print usage of the program */
void print_usage(char *program)
{
    printf("Usage: %s [options]\n", program);
    printf("-p <port>   port number (default %d)\n", TG_SERVER_PORT);
    printf("-v          give more detailed output (verbose)\n");
    printf("-d          run the server as a daemon\n");
    printf("-h          display help information\n");
}

/* Read command line arguments */
void read_args(int argc, char *argv[])
{
    int i = 1;

    while (i < argc)
    {
        if (strlen(argv[i]) == 2 && strcmp(argv[i], "-p") == 0)
        {
            if (i+1 < argc)
            {
                server_port = atoi(argv[i+1]);
                if (server_port < 0 || server_port > 65535)
                    error("Invalid port number");
                i += 2;
            }
            /* cannot read port number */
            else
            {
                printf("Cannot read port number\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-v") == 0)
        {
            verbose_mode = true;
            i += 1;
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-d") == 0)
        {
            daemon_mode = true;
            i += 1;
        }
        else if (strlen(argv[i]) == 2 && strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("Invalid option %s\n", argv[i]);
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}
