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
#include <math.h>
#include <pthread.h>

#include "cdf.h"
#include "conn.h"

#define TG_PAIR_INIT_CONN 5
#define TG_GOODPUT_RATIO (1448.0 / (1500 + 14 + 4 + 8 + 12))
#define BUFFER 1024
#define MAX_REQ_LEN 12
#define max(a, b) ((a > b)? a : b)
#define min(a, b) ((a < b)? a : b)

void usage_cli();
int isValidNumber(char* input);
int isValidPort(int port);
int isValidIP(char* IP);
int isValidBandWidth(double bandwidth);
void set_req_variables();
void read_args(int argc, char **argv);
void exit_connections();
void exit_connection(struct conn_node *node);
void *listen_connection(void *ptr);
void run_requests();
void run_request(unsigned int id);

void cleanup();
void print_statistic();

/*per-server*/
unsigned int num_server = 1;
char dist_file_name[80] = {0};
char config_file_name[80] = {0};
struct cdf_table *req_size_dist = NULL;

int seed = 0;
double bandwidth = 0; // network load in Mbps

struct timeval tv_start, tv_end; // start and end time of traffic 
unsigned int req_total_num = 0;
unsigned int req_total_time = 0;
unsigned int avg_transmit_us = 0; // average flow completion time in microsec
unsigned int usleep_overhead_us = 0;

unsigned int req_current_index = 0; //current index of requests, so that every listenning thread can get its flow information
pthread_mutex_t req_current_index_lock; 

/*per-flow (per-request)*/
unsigned int *req_size = NULL; //flow size in bytes
unsigned int *req_sleep_us = NULL;
struct timeval *req_tv_start;
struct timeval *req_tv_end;
struct conn_list *connection_lists = NULL; /*connection pool*/

/*input arguments*/
bool verbose_mode = false;
char* servIP = NULL;
int serv_port = -1;
char fct_log_name[80] = "flows.txt";    /* default log file */



int main(int argc, char** argv)
{
    int i = 0;
    struct conn_node *ptr = NULL;
    /*read input arguments*/
    read_args(argc, argv);

    /*read cdf file*/
    req_size_dist = (struct cdf_table*)malloc(sizeof(struct cdf_table));
    if (!req_size_dist)
    {
        cleanup();
        printf("Error: malloc req_size_dist");
        exit(-1);
    }
    init_cdf(req_size_dist);
    load_cdf(req_size_dist, dist_file_name);
    
    /*set seed value*/
    if (seed == 0)
    {
        gettimeofday(&tv_start,NULL);
        srand((tv_start.tv_sec*1000000) + tv_start.tv_usec);
    }
    else    
        srand(seed);

    /* calculate usleep overhead */
    usleep_overhead_us = get_usleep_overhead(20);
    
    /*set request variables*/
    set_req_variables();

    /*intialize struct conn_list*/
    if (!(connection_lists = (struct conn_list*)calloc(num_server, sizeof(struct conn_list))))
    {
        cleanup();
        perror("calloc connection_lists");
        exit(-1);
    }

    /* initialize connection pool and establish connections to servers */
    for (i = 0; i < num_server; ++i)
    {
        /* initialize server IP and port information */
        if (!init_conn_list(&connection_lists[i], i, servIP, (unsigned short)serv_port))
        {
            cleanup();
            perror("Error: init_conn_list");
            exit(-1);
        }
        /* establish TG_PAIR_INIT_CONN connections to server_addr[i]:server_port[i] */
        if (!insert_conn_list(&connection_lists[i], min(req_total_num, TG_PAIR_INIT_CONN)))
        {
            cleanup();
            perror("Error: insert_conn_list");
            exit(-1);
        }
    }

    /* start threads to receive traffic */
    for (i = 0; i < num_server; i++)
    {
        ptr = connection_lists[i].head;
        while (true)
        {
            if (!ptr)
                break;
            else
            {
                pthread_create(&(ptr->thread), NULL, listen_connection, (void*)ptr);
                ptr = ptr->next;
            }
        }
    }    
    
    printf("===========================================\n");
    printf("Start to generate requests\n");
    printf("===========================================\n");
    gettimeofday(&tv_start, NULL);
    run_requests();

    /* close existing connections */
    printf("===========================================\n");
    printf("Exit connections\n");
    printf("===========================================\n");
    
    exit_connections();
    gettimeofday(&tv_end, NULL);

    printf("===========================================\n");
    for (i = 0; i < num_server; i++)
        print_conn_list(&connection_lists[i]);
    printf("===========================================\n");
    
    print_statistic();

    cleanup();
    return 0;
}


void read_args(int argc, char** argv)
{
    int opt;
    int loopflag = 0;
    int errorflag = 0;    
    while ((opt = getopt(argc, argv, "s:b:p:c:n:t:h")) != -1) {
        loopflag = 1;
        switch (opt) {
            case 's':
                servIP = optarg;
                break;
            case 'b':
                bandwidth = atof(optarg);
                break;
            case 'p':
                serv_port = atoi(optarg);
                break;
            case 'c':
                strncpy(dist_file_name, optarg, sizeof(dist_file_name));    //current we take dist_file directly                
                break;
            case 'n':
                req_total_num = (unsigned int)strtoul(optarg, NULL, 10);    
                break;
            case 't':
                req_total_time = (unsigned int)strtoul(optarg, NULL, 10);
                break;
            case 'h':
                usage_cli();
                break;
            default:
                usage_cli();
                break;
        }
    }
    if (!loopflag) usage_cli();
    if ((isValidPort(serv_port) && isValidIP(servIP) && isValidBandWidth(bandwidth)) == 0) errorflag = 1;
    if (req_total_num > 0 && req_total_time > 0 )
    {
        printf("Cannot specify BOTH the number of requests (-n) and the time to generate requests (-t)\n");
        errorflag = 1;
    }
    if (req_total_num == 0 && req_total_time == 0 )
    {
        printf("Must specify EITHER the number of requests (-n) OR the time to generate requests (-t)\n");
        errorflag = 1;
    }
    if (errorflag) usage_cli();

}



void set_req_variables()
{
    int i = 0;
    unsigned long req_size_total = 0;
    unsigned long req_interval_total = 0;

    avg_transmit_us = avg_cdf(req_size_dist) * 8 / bandwidth / TG_GOODPUT_RATIO;
    
    if (req_total_num == 0 && req_total_time > 0)
    {
        req_total_num = max((unsigned long)req_total_time * 1000000 / avg_transmit_us, 1);
    }

    req_size = (unsigned int*)calloc(req_total_num, sizeof(unsigned int));
    req_sleep_us = (unsigned int*)calloc(req_total_num, sizeof(unsigned int));
    req_tv_start = (struct timeval*)calloc(req_total_num, sizeof(struct timeval));
    req_tv_end = (struct timeval*)calloc(req_total_num, sizeof(struct timeval));


    if(!req_size || !req_sleep_us || !req_tv_start || !req_tv_end)
    {
        cleanup();
        perror("calloc per-request variables");
        exit(-1);
    }


    for (i = 0; i < req_total_num; ++i)
    {
        req_size[i] = gen_random_cdf(req_size_dist);
        req_sleep_us[i] = poission_gen_interval(1.0 / avg_transmit_us);
        req_size_total += req_size[i];
        req_interval_total += req_sleep_us[i];        

    }
    printf("\n");
    printf("Generate %u requests in total\n", req_total_num);
    printf("The average request arrival interval is %lu us\n", req_interval_total / req_total_num);
    printf("The average request size is %lu bytes\n", req_size_total / req_total_num);
    printf("The expected experiment duration is %lu ms\n", req_interval_total / 1000);
}

void usage_cli()
{
    printf("Usage: ./client [options]\n");
    printf("-s <server IP>          (required)\n");
    printf("-p <server port>        (required)\n");    
    printf("-n <number of requests> (instead of -t)\n");
    printf("-t <time in seconds>    (instead of -n)\n");
    printf("-c <configuration file> (required)\n");
    printf("-b <bandwidth in Mbp>   (required)\n");
    printf("-h                      (get help information)\n");
    exit(-1);
}

void *listen_connection(void* ptr)
{
    struct conn_node* node = (struct conn_node*)ptr;

    pthread_mutex_lock(&req_current_index_lock);
    unsigned int req_index = req_current_index; // from 0 to req_total_num - 1
    if (req_current_index >= req_total_num)
    {        
        pthread_mutex_unlock(&req_current_index_lock);
        printf("req_current_index >= req_total_num\n");
        return (void*) 0;
    }
    else req_current_index ++;
    pthread_mutex_unlock(&req_current_index_lock);
    //printf("req_current_index: %u\n", req_index);


    unsigned int dataLeft = req_size[req_index];
    unsigned int total = 0;
    int dataLength = 0;
    char buffer[BUFFER] = {0};           
    
    while (dataLeft > 0) 
    {
        if ((dataLength = read(node->sockfd, buffer, BUFFER)) < 0) {
            printf("dataLength: %d\n",dataLength);
            if (errno == EINTR) {
                dataLength = 0;
            } else {
                perror("Error receiving data");
                break;
            }
        } 
        dataLeft -= (unsigned int)dataLength;
        total += (unsigned int)dataLength;
        if ((dataLength == 0) && (total > 0)) break;        
    }

    if(gettimeofday(&req_tv_end[req_index], NULL)==-1){
        perror("Timer\n");        
    }
    printf("total data received: %u\n",total);
    node->busy = false;
    pthread_mutex_lock(&(node->list->lock));
    node->list->flow_finished++;
    node->list->available_len++;
    pthread_mutex_unlock(&(node->list->lock));

    close(node->sockfd);
    node->connected = false;
    

    return (void*)0;        
}

void run_requests()
{
    unsigned int i = 0;
    unsigned int sleep_us = 0;
    for (i = 0; i < req_total_num; ++i)
    {
        sleep_us = req_sleep_us[i]; //question why += ?
        if (sleep_us > usleep_overhead_us)
        {
            usleep(sleep_us - usleep_overhead_us);
            sleep_us = 0;
        }
        run_request(i);
    }
}
void run_request(unsigned int id)
{
    int sockfd;
    /*find available node*/
    struct conn_node* node = search_conn_list(&connection_lists[0]);    // single server, thus [0].  

    char data[MAX_REQ_LEN] = {0};
    sprintf(data, "%u", req_size[id]);
    int req_len = strlen(data) + 1;    
    int written;

    if (!node)
    {
        if (insert_conn_list(&connection_lists[0], 1))
        {
            node = connection_lists[0].tail;            
            pthread_create(&(node->thread), NULL, listen_connection, (void*)node);
        }
        else
        {            
            printf("Cannot establish a new connection to %s:%u\n", servIP, (unsigned int)serv_port);
            return;
        }
    }

    sockfd = node->sockfd;
    node->busy = true;
    pthread_mutex_lock(&(node->list->lock));
    node->list->available_len--;
    pthread_mutex_unlock(&(node->list->lock));

    if(gettimeofday(&req_tv_start[id], NULL)==-1)
    {
        perror("Timer\n");        
    }

    if ((written = write(sockfd, data, req_len)) < 0) 
    {
        if (errno == EINTR) {
            written = 0;
        } 
        else {
            perror("Error writing number of bytes to server.");
            return;
        }
    }


}

/* Terminate all existing connections */
void exit_connections()
{
    unsigned int i = 0;
    struct conn_node *ptr = NULL;
    unsigned int num = 0;

    /* Start threads to receive traffic */
    for (i = 0; i < num_server; i++)
    {
        num = 0;
        ptr = connection_lists[i].head;
        while (true)
        {
            if (!ptr)
                break;
            else
            {
                if (ptr->connected)
                {
                    exit_connection(ptr);
                    num++;
                }
                ptr = ptr->next;
            }
        }
        wait_conn_list(&connection_lists[i]);
        if (verbose_mode)
            printf("Exit %u/%u connections to %s:%u\n", num, connection_lists[i].len, servIP, (unsigned int)serv_port);
    }
}

/* Terminate a connection */
void exit_connection(struct conn_node *node)
{
    if (!node)
        return;
    pthread_mutex_lock(&(node->list->lock));
    node->list->available_len--;
    pthread_mutex_unlock(&(node->list->lock));

    /*   
    int sockfd;
    struct flow_metadata flow;
    flow.id = 0;   // a special flow ID to terminate connection 
    flow.size = 100;
    flow.tos = 0;
    flow.rate = 0;
    sockfd = node->sockfd;

    if (!write_flow_req(sockfd, &flow))
        perror("Error: generate request");
    */
}

/* clean up resources */
void cleanup()
{
    unsigned int i = 0;

    free_cdf(req_size_dist);
    free(req_size_dist);

    free(req_size);
    free(req_sleep_us);
    free(req_tv_start);
    free(req_tv_end);

    if (connection_lists)
    {
        if (verbose_mode)
            printf("===========================================\n");

        for(i = 0; i < num_server; i++)
        {
            if (verbose_mode)
                printf("Clear connection list %u to %s:%u\n", i, connection_lists[i].ip, connection_lists[i].port);
            clear_conn_list(&connection_lists[i]);
        }
    }
    free(connection_lists);
}

void print_statistic()
{
    unsigned long long duration_us = (tv_end.tv_sec - tv_start.tv_sec) * 1000000 + tv_end.tv_usec - tv_start.tv_usec;
    unsigned long long req_size_total = 0;
    unsigned long long fct_us;
    unsigned int flow_goodput_mbps;    /* per-flow goodput (Mbps) */
    unsigned int goodput_mbps; /* total goodput (Mbps) */
    unsigned int i = 0;
    FILE *fd = NULL;

    fd = fopen(fct_log_name, "w");
    if (!fd)
        error("Error: open the FCT result file");

    for (i = 0; i < req_total_num; i++)    {
        
        if ((req_tv_end[i].tv_sec == 0) && (req_tv_end[i].tv_usec == 0))
        {
            printf("Unfinished flow request %u\n", i);
            continue;
        }
        req_size_total += req_size[i];
        fct_us = (req_tv_end[i].tv_sec - req_tv_start[i].tv_sec) * 1000000 + req_tv_end[i].tv_usec - req_tv_start[i].tv_usec;
        if (fct_us > 0)
            flow_goodput_mbps = req_size[i] * 8 / fct_us;
        else
            flow_goodput_mbps = 0;

        /* size (bytes), FCT(us), DSCP, sending rate (Mbps), goodput (Mbps) */
        fprintf(fd, "Request size: %u MB, FCT: %llu us, Flow goodput: %u Mbps.\n", req_size[i], fct_us, flow_goodput_mbps);
    }

    fclose(fd);
    goodput_mbps = req_size_total * 8 / duration_us;
    printf("The actual bandwidth (RX throughput) is %u Mbps\n", (unsigned int)(goodput_mbps / TG_GOODPUT_RATIO));
    printf("The actual duration is %llu ms\n", duration_us/1000);
    printf("===========================================\n");
    printf("Write FCT results to %s\n", fct_log_name);
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
        printf("Number of bytes must be 1 ~ 999,999,999,999B (~1TB)\n");
        return 0;
    }     
    if (atoll(numBytes_string) <= 0) {
        printf("Number of bytes must be 1 ~ 999,999,999,999B (~1TB)\n");
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

int isValidBandWidth(double bandwidth)
{
    if (bandwidth<=0)
    {
        printf("Invalid bandwidth.\n");
        return 0;
    }
    return 1;
}


