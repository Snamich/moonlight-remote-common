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
#define MSG_NO        0x000000010
#define MSG_OK        0x000000011

#define CFG_FPS              0x0001
#define CFG_RESOLUTION       0x0002
#define CFG_MODIFY_SETTINGS  0x0004
#define CFG_LOCAL_AUDIO      0x0008

#define CFG_SHIFT                 4

#define DEFAULT_CONFIG ((u32)(CFG_FPS | CFG_RESOLUTION))

#define MAXHOSTLEN 100
#define MAXCMDLEN  200
#define MAXIPLEN    20

static int
set_config_opt(u32 *config, u32 options, u32 value)
{
    u32 rv = 0;

    if (config) {
        if (value) {
            rv = *config | options;
        } else {
            rv = *config & ~options;
        }
    }

    return rv;
}

static int
get_config_opt(u32 config, u32 options)
{
    return config & options;
}

typedef struct host {
    char *name;
    char *ip;
    int is_paired;
    u32 config;
    long config_offset;
    struct moonlight_server *server;
} host;

typedef struct moonlight_server {
    host hosts[5];
    struct sockaddr_in addr;
    char *name;
    int host_count;
    long count_offset;
} moonlight_server;

static int
sendstr(int sockfd, char *str)
{
    ssize_t numbytes = 0;
    int total = 0, nsize;

    size_t size = strlen(str) + 1;
    nsize = htonl(size);
    send(sockfd, &nsize, sizeof(nsize), 0);

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
recstr(int sockfd, char **str)
{
    ssize_t numbytes = 0;
    int total = 0, rv = 0, size;

    recv(sockfd, &size, sizeof(size), 0);
    size = ntohl(size);
    printf("recstr received string size: %d\n", size);

    char *s = malloc(size);
    if (!s) {
        perror("recstr (malloc)");
        goto exit;
    }

    do {
        numbytes = recv(sockfd, s, size - total, 0);

        if (numbytes == -1) {
            perror("recstr");
            goto memory_cleanup;
        } else if (numbytes == 0) {
            break;
        }

        total += numbytes;
    } while (total < size);

    rv = total;
    *str = s;
    goto exit;

memory_cleanup:
    free(s);
exit:
    return rv;
}

static int
is_valid_ip(char *ip)
{
    struct sockaddr_in sa;
    int rv = inet_pton(AF_INET, ip, &(sa.sin_addr));

    return rv != 0;
}

#endif // COMMON_H
