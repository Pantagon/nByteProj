#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
/* structure of flow metadata */

/* structure of control message */
struct ctrl_msg {
	uint32_t saddr;
	uint32_t daddr;
	uint16_t sport;
	uint16_t dport;
	uint32_t size;
	uint32_t time_to_ddl;
};

/* structure of flow metadata */
struct flow_metadata
{
    	unsigned int id;    /* ID */
    	unsigned long int size;  /* flow size (bytes) */
	unsigned long int ddl; /*flow deadline (microsec)*/
};

struct Thread_arg
{	
	uint32_t saddr;
	uint32_t daddr;
	uint16_t sport;
	uint16_t dport;
	int sockfd;	
};

/* flow meta data size */
#define TG_METADATA_SIZE (sizeof(struct flow_metadata))
/* default server port */
#define TG_SERVER_PORT 5001
/* default number of backlogged connections for listen() */
#define TG_SERVER_BACKLOG_CONN SOMAXCONN
/* maximum number of bytes to write in a 'send' system call */
#define TG_MAX_WRITE (1 << 20)
/* minimum number of bytes to write in a 'send' system call (used with rate limiting) */
#define TG_MIN_WRITE (1 << 16)
/* maximum amount of data to read in a 'recv' system call */
#define TG_MAX_READ (1 << 20)
/* default initial number of TCP connections per pair */
#define TG_PAIR_INIT_CONN 5
/* default goodput / link capacity ratio */
#define TG_GOODPUT_RATIO (1448.0 / (1500 + 14 + 4 + 8 + 12))

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

/*
 * I borrow read_exact() and write_exact() from https://github.com/datacenter/empirical-traffic-gen.
 * Thanks Mohammad Alizadeh. I really learn a lot from your code and papers!
 */

/* read exactly 'count' bytes from a socket 'fd' */
unsigned int read_exact(int fd, char *buf, size_t count, size_t max_per_read, bool dummy_buf);

/* write exactly 'count' bytes into a socket 'fd' */
unsigned int write_exact(int fd, char *buf, size_t count, size_t max_per_write, bool dummy_buf);

/* read the metadata of a flow from a socket and return true if it succeeds. */
bool read_flow_metadata(int fd, struct flow_metadata *f);

/* write a flow request into a socket and return true if it succeeds */
bool write_flow_req(int fd, struct flow_metadata *f);

/* write a flow (response) into a socket and return true if it succeeds */
bool write_flow(int fd, struct flow_metadata *f);

/* print error information and terminate the program */
void error(char *msg);

/* generate poission process arrival interval */
double poission_gen_interval(double avg_rate);

/* calculate usleep overhead */
unsigned int get_usleep_overhead(int iter_num);

/* display progress */
void display_progress(unsigned int num_finished, unsigned int num_total);

#endif
