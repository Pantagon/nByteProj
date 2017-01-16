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

#define BUFFER 1024

int main(int argc, char** argv) {
    int opt;
    char* servIP = NULL;
    int port = -1;
    int numBytes = 0;
    while ((opt = getopt(argc, argv, "s:n:p:")) != -1) {
        switch (opt) {
            case 's':
                servIP = optarg;
                break;
            case 'n':
                numBytes = atoi(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                usage();
        }
    }

    if (servIP == NULL || validIP(servIP) == 0) {
        perror("Invalid IP.\n");
        usage();
    }
    if (numBytes <= 0) {
        perror("Number of bytes must be positive.\n");
        usage();
    }
    if (port < 0 || port > 65535) {
        perror("Invalid port number.\n");
        usage();
    }

    printf("Requesting server IP: %s port: %d to send %d bytes of data.\n", servIP, port, numBytes);
    int sockfd;
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
    int32_t nBytes = htonl(numBytes);
    char* data = (char*) &nBytes;
    int left = sizeof(nBytes);
    ssize_t written;
    while (left > 0) {
        if ((written = write(sockfd, data, left)) < 0) {
            if (errno == EINTR) {
                written = 0;
            } else {
                perror("Error writing number of bytes to server.\n");
                exit(-1);
            }
        }
        left -= written;
        data += written;
    }
    printf("Finished sending the number of bytes to the server.\n");

    printf("Now ready for receiving data from the server.\n");
    int dataLeft = numBytes;
    int total = 0;
    ssize_t dataLength = 0;
    char buffer[BUFFER];
    while (dataLeft > 0) {
        if ((dataLength = read(sockfd, buffer, BUFFER)) < 0) {
            if (errno == EINTR) {
                dataLength = 0;
            } else {
                perror("Error receiving data.\n");
                exit(-1);
            }
        }
        printf("Received %d bytes.\n", dataLength);
        dataLeft -= dataLength;
        total += dataLength;
    }
    printf("Finised receiving. Totally received: %d bytes.\n", total);
    close(sockfd);
    return 0;
}

int validIP(char* ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr));
}

void usage() {
    printf("Usage: client -s <servIP> -n <numBytesToRcv> -p <port>\n");
    exit(-1);
}
