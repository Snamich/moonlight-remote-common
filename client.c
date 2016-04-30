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
discover(int sockfd, struct sockaddr_in *addr)
{
    int numbytes, rv = 0;
    socklen_t addr_len = sizeof(*addr);
    struct hostent *host_entry;

    /* TODO: figure out address using subnet mask instead of spamming? */
    if ((host_entry = gethostbyname("255.255.255.255")) == NULL) {
        perror("server (broadcast gethostbyname)");
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons(atoi(DISCOVER_PORT));
    addr->sin_addr = *((struct in_addr *)host_entry->h_addr);
    memset(addr->sin_zero, '\0', sizeof(addr->sin_zero));

    u32 msg = htonl(MSG_BROADCAST);

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

    if ((numbytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)addr, addr_len)) == -1) {
        perror("client (broadcast sendto)");
    } else {
        if ((numbytes = recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)addr, &addr_len)) == -1) {
            perror("client (discover recvfrom)");
        } else {
            msg = ntohl(msg);
            if (msg == MSG_HANDSHAKE) {
                rv = 1;
            }
        }
    }

    return rv;
}

int
is_duplicate_server(moonlight_server *servers, int server_count, struct sockaddr_in *addr)
{
    if (!servers || !addr) {
        return 0;
    }

    moonlight_server *s;
    for (int i = 0; i < server_count; ++i) {
        // check server sockaddr_in
        s = servers + i;
        if (addr->sin_addr.s_addr == s->addr.sin_addr.s_addr) {
            return 1;
        }
    }

    return 0;
}

int
add_server(moonlight_server *server, struct sockaddr_in *addr)
{
    int hostfd;
    struct sockaddr_in *server_addr = &server->addr;
    memcpy(server_addr, addr, sizeof(*server_addr));
    server_addr->sin_port = htons(atoi(LISTEN_PORT));

    if ((hostfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("client (add_server socket)");
    }

    if (connect(hostfd, server_addr, sizeof(*server_addr)) == -1) {
        perror("client (add_server connect)");
    }

    hostname(hostfd, &server->name);
    server->host_count = 0;

    close(hostfd);

    return 0;
}

int
is_duplicate_host(host *hosts, int host_count, char *ip)
{
    if (!hosts || !ip) {
        return 0;
    }

    host *h;
    for (int i = 0; i < host_count; ++i) {
        h = hosts + i;
        if (strcmp(h->ip, ip) == 0) {
            return 1;
        }
    }

    return 0;
}

int
add_host(host *host, char *name, char *ip)
{
    int namelen = strlen(name) + 1;
    int iplen = strlen(ip) + 1;

    host->name = malloc(namelen);
    host->ip = malloc(iplen);

    memcpy(host->name, name, namelen);
    memcpy(host->ip, ip, iplen);
    host->is_paired = 0;
    memcpy(&host->config, &default_config, sizeof(default_config));

    return 1;
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
