#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <math.h>

#include "common.h"


static char max_write_buf[TG_MAX_WRITE] = {0};




/*
 * This function attemps to read exactly count bytes from file descriptor fd
 * into buffer starting at buf. It repeatedly calls read() until either:
 * 1. count bytes have been read
 * 2. end of file is reached, or for a network socket, the connection is closed
 * 3. read() produces an error
 * Each internal call to read() is for at most max_per_read bytes. The return
 * value gives the number of bytes successfully read.
 * The dummy_buf flag can be set by the caller to indicate that the contents
 * of buf are irrelevant. In this case, all read() calls put their data at
 * location starting at buf, overwriting previous reads.
 * To avoid buffer overflow, the length of buf should be at least count when
 * dummy_buf = false, and at least min{count, max_per_read} when
 * dummy_buf = true.
 */
unsigned int read_exact(int fd, char *buf, size_t count, size_t max_per_read, bool dummy_buf)
{
    unsigned int bytes_total_read = 0;  /* total number of bytes that have been read */
    unsigned int bytes_to_read = 0; /* maximum number of bytes to read in next read() call */
    char *cur_buf = NULL;   /* current location */
    int n;  /* number of bytes read in current read() call */

    if (!buf)
        return 0;

    while (count > 0)
    {
        bytes_to_read = (count > max_per_read) ? max_per_read : count;
        cur_buf = (dummy_buf) ? buf : (buf + bytes_total_read);
        n = read(fd, cur_buf, bytes_to_read);

        if (n <= 0)
        {
            if (n < 0)
                printf("Error: read() in read_exact()");
            break;
        }
        else
        {
            bytes_total_read += n;
            count -= n;
        }
    }

    return bytes_total_read;
}

/*
 * This function attemps to write exactly count bytes from the buffer starting
 * at buf to file referred to by file descriptor fd. It repeatedly calls
 * write() until either:
 * 1. count bytes have been written
 * 2. write() produces an error
 * Each internal call to write() is for at most max_per_write bytes. The return
 * value gives the number of bytes successfully written.
 * The dummy_buf flag can be set by the caller to indicate that the contents
 * of buf are irrelevant. In this case, all write() calls get their data from
 * starting location buf.
 * To avoid buffer overflow, the length of buf should be at least count when
 * dummy_buf = false, and at least min{count, max_per_write} when
 * dummy_buf = true.
 * Users can rate-limit the sending of traffic. If rate_mbps is equal to 0, it indicates no rate-limiting.
 * Users can also set ToS value for traffic.
 */
unsigned int write_exact(int fd, char *buf, size_t count, size_t max_per_write, bool dummy_buf)
{
    unsigned int bytes_total_write = 0; /* total number of bytes that have been written */
    unsigned int bytes_to_write = 0;    /* maximum number of bytes to write in next send() call */
    char *cur_buf = NULL;   /* current location */
    int n;  /* number of bytes read in current read() call */
    
    while (count > 0)
    {
        bytes_to_write = (count > max_per_write) ? max_per_write : count;
        cur_buf = (dummy_buf) ? buf : (buf + bytes_total_write);        
        n = write(fd, cur_buf, bytes_to_write);       
        

        if (n <= 0)
        {
            if (n < 0)
                printf("Error: write() in write_exact()");
            break;
        }
        else
        {
            bytes_total_write += n;
            count -= n;
            
        }
    }

    return bytes_total_write;
}

/* read the metadata of a flow and return true if it succeeds. */
bool read_flow_metadata(int fd, struct flow_metadata *f)
{
    char buf[TG_METADATA_SIZE] = {0};

    if (!f)
        return false;

    if (read_exact(fd, buf, TG_METADATA_SIZE, TG_METADATA_SIZE, false) != TG_METADATA_SIZE)
        return false;

    /* extract metadata */
    memcpy(&(f->id), buf + offsetof(struct flow_metadata, id), sizeof(f->id));
    memcpy(&(f->size), buf + offsetof(struct flow_metadata, size), sizeof(f->size));
	memcpy(&(f->ddl), buf + offsetof(struct flow_metadata, ddl), sizeof(f->ddl));
    return true;
}

/* write a flow request into a socket and return true if it succeeds */
bool write_flow_req(int fd, struct flow_metadata *f)
{
    char buf[TG_METADATA_SIZE] = {0};   /* buffer to hold metadata */

    if (!f)
        return false;

    /* fill in metadata */
    memcpy(buf + offsetof(struct flow_metadata, id), &(f->id), sizeof(f->id));
    memcpy(buf + offsetof(struct flow_metadata, size), &(f->size), sizeof(f->size));
	memcpy(buf + offsetof(struct flow_metadata, ddl), &(f->ddl), sizeof(f->ddl));
    /* write the request into the socket */
    if (write_exact(fd, buf, TG_METADATA_SIZE, TG_METADATA_SIZE, false) == TG_METADATA_SIZE)
        return true;
    else
        return false;
}

/* write a flow (response) into a socket and return true if it succeeds */
bool write_flow(int fd, struct flow_metadata *f)
{
    char *write_buf = NULL;  /* buffer to hold the real content of the flow */
    unsigned int max_per_write = 0;
    unsigned int result = 0;

    if (!f)
        return false;

    /* echo back metadata */
    if (!write_flow_req(fd, f))
    {
        printf("Error: write_flow_req() in write_flow()\n");
        return false;
    }
	
    write_buf = max_write_buf;
    max_per_write = TG_MAX_WRITE;
    
    /* generate the flow response */
    result = write_exact(fd, write_buf, f->size, max_per_write, true);
    if (result == f->size)
        return true;
    else
    {
        printf("Error: write_exact() in write_flow() only successfully writes %u of %u bytes.\n", result, f->size);
        return false;
    }
}

/* print error information */
void error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/* generate poission process arrival interval */
double poission_gen_interval(double avg_rate)
{
    if (avg_rate > 0)
        return -logf(1.0 - (double)(rand() % RAND_MAX) / RAND_MAX) / avg_rate;
    else
        return 0;
}

/* calculate usleep overhead */
unsigned int get_usleep_overhead(int iter_num)
{
    int i=0;
    unsigned int tot_sleep_us = 0;
    struct timeval tv_start, tv_end;

    if (iter_num <= 0)
        return 0;

    gettimeofday(&tv_start, NULL);
    for(i = 0; i < iter_num; i ++)
        usleep(0);
    gettimeofday(&tv_end, NULL);
    tot_sleep_us = (tv_end.tv_sec - tv_start.tv_sec) * 1000000 + tv_end.tv_usec - tv_start.tv_usec;

    return tot_sleep_us/iter_num;
}


/* display progress */
void display_progress(unsigned int num_finished, unsigned int num_total)
{
    if (num_total == 0)
        return;

    printf("Generate %u / %u (%.1f%%) requests\r", num_finished, num_total, (num_finished * 100.0) / num_total);
    fflush(stdout);
}


/* netlink send d2tcp control message*/
int nl_send_d2tcp_ctrl_msg(uint32_t saddr, uint16_t sport, uint32_t daddr,
	uint16_t dport, uint32_t size_in_bytes, uint32_t microsecs_to_ddl) {

	struct ctrl_msg request;
	struct ctrl_msg* echo;
	struct sockaddr_nl src_addr, dest_addr;
	struct nlmsghdr* nlh = NULL;
	struct iovec iov;
	int sock_fd;
	struct msghdr msg;

	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
	if (sock_fd < 0) {
		return sock_fd;
	}

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();
	bind(sock_fd, (struct sockaddr*) &src_addr, sizeof(src_addr));

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;
	dest_addr.nl_groups = 0;

	request.saddr = saddr;
	request.sport = sport;
	request.daddr = daddr;
	request.dport = dport;
	request.size = size_in_bytes;
	request.time_to_ddl = microsecs_to_ddl;

	nlh = (struct nlmsghdr*) malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	memcpy(NLMSG_DATA(nlh), &request, sizeof(request));

	iov.iov_base = (void*)nlh;
	iov.iov_len = nlh->nlmsg_len;
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void*)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	sendmsg(sock_fd, &msg, 0);
	recvmsg(sock_fd, &msg, 0);
	//printf("Check syslog for result.\n");
	close(sock_fd);
	return 0;
}
