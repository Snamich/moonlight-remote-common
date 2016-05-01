#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "stdtypes.h"

#define LISTEN_PORT   "7777"
#define DISCOVER_PORT "7778"

#define MSG_LIST     0x00000001
#define MSG_PAIR     0x00000002
#define MSG_UNPAIR   0x00000003
#define MSG_LAUNCH   0x00000004
#define MSG_QUIT     0x00000005
#define MSG_HOSTNAME 0x00000006
#define MSG_PING     0x00000007

#define MSG_BROADCAST 0x000000008
#define MSG_HANDSHAKE 0x000000009

#define MAXHOSTLEN 100
#define MAXCMDLEN 200

typedef struct host_config {
    int fps;
    int bitrate;
    int packetsize;
    int resolution;
    int nsops;
    int localaudio;
} host_config;

static const host_config default_config = {
    .fps = 60,
    .bitrate = 0,
    .packetsize = 0,
    .resolution = 720,
    .nsops = 0,
    .localaudio = 0
};

typedef struct host {
    char *name;
    char *ip;
    int is_paired;
    int servfd;
    host_config config;
} host;

typedef struct moonlight_server {
    host hosts[5];
    struct sockaddr_in addr;
    char *name;
    int host_count;
} moonlight_server;

static int
sendstr(int sockfd, char *str, int size)
{
    int total = 0, numbytes = 0;

    send(sockfd, &size, sizeof(size), 0);

    while (total < size) {
        if ((numbytes = send(sockfd, str + total, size - total, 0)) == -1) {
            perror("sendstr");
            return 0;
        }

        total += numbytes;
    }

    return total;
}

static int
recstr(int sockfd, char *str, int size)
{
    int total = 0, numbytes = 0, strlen;

    recv(sockfd, &strlen, sizeof(strlen), 0);

    do {
        numbytes = recv(sockfd, str, size - total, 0);

        if (numbytes == -1) {
            perror("recstr");
            return 0;
        } else if (numbytes == 0) {
            break;
        }

        total += numbytes;
    } while (total < size && total < strlen);

    return total;
}

#endif // COMMON_H
