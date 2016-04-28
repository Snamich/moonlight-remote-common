#include "common.h"
#include "client.h"

#define MAXCMD 100

int
broadcastfd_setup()
{
    int sockfd, yes = 1;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("server (broadcastfd_setup socket)");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int)) == -1) {
        perror("server (broadcastfd_setup setsockopt)");
    }

    return sockfd;
}

int
broadcast(int sockfd, moonlight_server *server)
{
    int numbytes, rv = 0;
    struct sockaddr_in their_addr;
    socklen_t addr_len = sizeof(their_addr);
    struct hostent *host_entry;

    /* TODO: figure out address using subnet mask instead of spamming? */
    if ((host_entry = gethostbyname("255.255.255.255")) == NULL) {
        perror("server (broadcast gethostbyname)");
    }

    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(atoi(DISCOVER_PORT));
    their_addr.sin_addr = *((struct in_addr *)host_entry->h_addr);
    memset(their_addr.sin_zero, '\0', sizeof(their_addr.sin_zero));

    u32 msg = htonl(MSG_BROADCAST);

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

    if ((numbytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&their_addr, sizeof(their_addr))) == -1) {
        perror("server (broadcast sendto)");
    } else {
        if ((numbytes = recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("client (discover recvfrom)");
        } else {
            msg = ntohl(msg);
            if (msg == MSG_HANDSHAKE) {
                // check if duplicate server
                if (0) {

                } else {
                    // extract the IP of the sender
                    printf("handshake received\n");
                    their_addr.sin_port = htons(atoi(LISTEN_PORT));
                    memcpy(&server->addr, &their_addr, addr_len);

                    int hostfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    connect(hostfd, &their_addr, sizeof(their_addr));
                    hostname(hostfd, &server->name);

                    close(hostfd);

                    rv = 1;
                }

            }
        }
    }

    return rv;
}

int
pair(int sockfd)
{
    u32 pair_code = 0;
    u32 msg = htonl(MSG_PAIR);

    send(sockfd, &msg, sizeof(msg), 0);
    if (recv(sockfd, &pair_code, sizeof(pair_code), 0) == -1) {
        perror("client recv msg_pair");
    }

    pair_code = ntohl(pair_code);
    return pair_code;
}

int
unpair(int sockfd)
{
    u32 msg = htonl(MSG_UNPAIR);
    send(sockfd, &msg, sizeof(msg), 0);

    return 0;
}

int
list(int sockfd)
{
    u32 msg = htonl(MSG_LIST), nitems = 0;
    send(sockfd, &msg, sizeof(msg), 0);

    if (recv(sockfd, &nitems, sizeof(nitems), 0) == -1) {
        perror("client recv msg_list");
    }

    nitems = ntohl(nitems);

    return 0;
}

int
launch(int sockfd)
{
    u32 msg = htonl(MSG_LAUNCH);
    send(sockfd, &msg, sizeof(msg), 0);

    return 0;
}

int
quit(int sockfd)
{
    u32 msg = htonl(MSG_QUIT);
    send(sockfd, &msg, sizeof(msg), 0);

    return 0;
}

int
hostname(int sockfd, char **str)
{
    int rv = 0;
    u32 msg = htonl(MSG_HOSTNAME);
    send(sockfd, &msg, sizeof(msg), 0);

    *str = malloc(MAXHOSTLENGTH);
    if (recstr(sockfd, *str, MAXHOSTLENGTH)) {
        printf("rec hostname: %s\n", *str);
        rv = 1;
    }

    return rv;
}

/* bool */
/* send_config(int sockfd, host_config *config) */
/* { */
/*     int fps, bitrate, packetsize, width, height; */
/*     bool rv = false; */
/*     char *nsops, *localaudio; */

/*     if (config) { */
/*         fps = config->fps; */
/*         bitrate = config->bitrate; */
/*         packetsize = config->packetsize; */
/*         width = config->width; */
/*         height = config->height; */
/*         nsops = config->nsops ? "-nsops" : ""; */
/*         localaudio = config->localaudio ? "-localaudio" : ""; */

/*         char cmd[MAXCMD]; */

/*         snprintf(cmd, MAXCMD, "-fps %d -bitrate %d -packetsize %d %s %s", fps, bitrate, packetsize, nsops, localaudio); */

/*         if (sendstr(sockfd, cmd, MAXCMD)) { */
/*             rv = true; */
/*         } */
/*     } */

/*     return rv; */
/* } */
