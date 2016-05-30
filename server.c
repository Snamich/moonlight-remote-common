#include "common.h"

#include <ctype.h>

#define BACKLOG 5
#define MAXLIST 2

int
listenfd_setup()
{
    int rv = -1;

    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // don't care if it uses IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP stream
    hints.ai_flags = AI_PASSIVE;      // automatically fill with my IP

    if (getaddrinfo(NULL, LISTEN_PORT, &hints, &servinfo) != 0) {
        perror("server (listenfd_setup getaddrinfo)");
        goto exit;
    }

    int sockfd;
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
        perror("server (listenfd_setup socket)");
        goto addr_cleanup;
    }

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("server (listenfd_setup setsockopt)");
        goto addr_cleanup;
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("server (listenfd_setup bind)");
        goto addr_cleanup;
    }


    if (listen(sockfd, BACKLOG) == -1) {
        perror("server (listenfd_setup listen)");
        goto addr_cleanup;
    }

    rv = sockfd;

addr_cleanup:
    freeaddrinfo(servinfo);
exit:
    return rv;
}

int
broadcastfd_setup()
{
    int rv = -1;

    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if (getaddrinfo(NULL, DISCOVER_PORT, &hints, &servinfo) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        goto exit;
    }

    int sockfd;
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
        perror("server (broadcastfd_setup socket)");
        goto addr_cleanup;
    }

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("server (broadcastfd_setup setsockopt)");
        goto addr_cleanup;
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("server (broadcastfd_setup bind)");
        goto addr_cleanup;
    }

    rv = sockfd;

addr_cleanup:
    freeaddrinfo(servinfo);
exit:
    return rv;
}

void
discover(int sockfd)
{
    int numbytes;
    u32 msg;
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof(their_addr);

    while (1) {
        if ((numbytes = recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("server (discover recvfrom)");
            continue;
        } else {
            msg = ntohl(msg);

            if (msg == MSG_BROADCAST) {
                printf("received broadcast packet\n");
                msg = htonl(MSG_HANDSHAKE);

                struct sockaddr_in send_addr = *((struct sockaddr_in *)&their_addr);

                if ((numbytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&send_addr, sizeof(send_addr))) == -1) {
                    perror("server (broadcast sendto)");
                }
            }
        }
    }
}

int
main(int argc, char **argv)
{
    pid_t child_pid;

    if ((child_pid = fork()) < 0) {
        perror("fork");
        exit(1);
    }

    if (!child_pid) {
        /* child */
        int broadcastfd = broadcastfd_setup();;
        discover(broadcastfd);
    } else {
        /* parent */
        bool host_running = false;
        int listenfd = listenfd_setup();
        uint msg = 0;

        struct sockaddr_storage their_addr;
        socklen_t addr_size = sizeof(their_addr);

        while (1) {
            // get msg, only handles one client for now
            int connfd = accept(listenfd, (struct sockaddr *)&their_addr, &addr_size);
            if (recv(connfd, &msg, sizeof(msg), 0) == -1) {
                perror("server (recv msg)");
                continue;
            }

            msg = ntohl(msg);
            printf("server received msg %x from client\n", msg);

            switch (msg) {
                case MSG_LIST:
                {
                    /* FILE *fd = popen("moonlight list 192.168.0.182", "r"); */
                    /* if (!fd) { */
                    /*     perror("server (popen)"); */
                    /*     // send back some kind of error response */
                    /*     continue; */
                    /* } */

                    msg = htonl(MSG_OK);
                    send(connfd, &msg, sizeof(msg), 0);

                    FILE *fd = fopen("mlist.txt", "r");
                    if (!fd) {
                        perror("server (popen)");
                        // send back some kind of error response
                        continue;
                    }

                    // TODO: get a temp file here
                    FILE *listfd = fopen("list.txt", "wb+");
                    if (!listfd) {
                        perror("server (msg_list fopen)");
                        continue;
                    }

                    int nlines = 0;
                    size_t linelen = 0, maxline = 0;
                    char *line = NULL;
                    fseek(listfd, sizeof(linelen) * 2, SEEK_SET);
                    while ((linelen = getline(&line, &linelen, fd)) != -1) {
                        // skip the line if it doesn't start with a number
                        if (!isdigit(line[0])) {
                            continue;
                        }

                        // find the start of the name
                        size_t i;
                        for (i = 0; i < linelen && !isalpha(line[i]); ++i) {}

                        // write the size of the string followed by the string itself
                        line[linelen - 1] = '\0';
                        linelen = linelen - i;
                        if (maxline < linelen) {
                            maxline = linelen;
                        }

                        fwrite(&linelen, sizeof(linelen), 1, listfd);
                        fwrite(line + i, linelen, 1, listfd);

                        ++nlines;
                    }

                    // write the number of lines to the beginning of the file
                    fseek(listfd, 0, SEEK_SET);
                    fwrite(&nlines, sizeof(nlines), 1, listfd);
                    free(line);
                    fclose(fd);

                    // send the list
                    // TODO: compress this into the reading
                    //       would have to resize list on client side and figure out how to end transmission
                    nlines = htonl(nlines);
                    send(connfd, &nlines, sizeof(nlines), 0);
                    nlines = ntohl(nlines);

                    char *list = malloc(maxline);
                    if (!list) {
                        perror("server msg_list malloc list");
                        continue;
                    }

                    fseek(listfd, sizeof(nlines) * 2, SEEK_SET);
                    for (int i = 0; i < nlines; ++i) {
                        fread(&linelen, sizeof(linelen), 1, listfd);
                        printf("server msg_list: sending linelen: %zu\n", linelen);
                        fread(list, linelen, 1, listfd);
                        printf("server msg_list: sending line: %s\n", list);
                        sendstr(connfd, list);
                    }

                    free(list);
                    fclose(listfd);
                    printf("server - finished sending list\n");
                } break;

                case MSG_PAIR:
                {
                    /* run moonlight pair <host> and return code to enter on host */
                    char *ip;
                    recstr(connfd, &ip);

                    char cmd[MAXCMDLEN];
                    // force std buffers to flush output after every line
                    int total = snprintf(cmd, MAXCMDLEN, "stdbuf -oL moonlight pair ");
                    total += snprintf(cmd + total, MAXCMDLEN, "%s", ip);
                    // redirect stderr to stdout to capture pairing failure
                    total += snprintf(cmd + total, MAXCMDLEN, "%s", " 2>&1");

                    FILE *fd = popen(cmd, "r");
                    if (!fd) {
                        perror("server (pair)");
                    }

                    size_t linelen = 0;
                    char *line = NULL;
                    int pair_code;
                    while ((linelen = getline(&line, &linelen, fd)) != -1) {
                        // find the line the pin is on
                        if (6 < linelen && strncmp(line, "Please", 6) != 0) {
                            continue;
                        }

                        // find the start of the name
                        size_t i;
                        for (i = 0; i < linelen && !isdigit(line[i]); ++i) {}

                        pair_code = atoi(line + i);
                        break;
                    }

                    pair_code = htonl(pair_code);
                    if (send(connfd, &pair_code, sizeof(pair_code), 0) == -1) {
                        perror("server (send pair_code)");
                    }

                    // check for successful PIN entry or cancellation
                    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
                    setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
                    while ((linelen = getline(&line, &linelen, fd))) {
                        if (linelen == -1 && feof(fd)) {
                            clearerr(fd);
                            if (recv(connfd, &msg, sizeof(msg), 0) != -1) {
                                msg = ntohl(msg);
                                if (msg == MSG_OK) {
                                    msg = htonl(MSG_NO);
                                    break;
                                }
                            }
                        } else {
                            // something was read
                            if (strncmp(line, "Succesfully paired", 18) == 0) {
                                msg = htonl(MSG_OK);
                                break;
                            } else if (strncmp(line, "Failed", 6) == 0) {
                                msg = htonl(MSG_NO);
                                break;
                            } else {
                                continue;
                            }
                        }
                    }

                    if (send(connfd, &msg, sizeof(msg), 0) == -1) {
                        perror("server (send response)");
                    }

                    free(line);
                    pclose(fd);
                } break;

                case MSG_UNPAIR:
                {
                    /* run moonlight unpair <host> */
                    /* this doesn't appear to actually do anything right now */
                    char *ip;
                    recstr(connfd, &ip);

                    char cmd[MAXCMDLEN];
                    int total = snprintf(cmd, MAXCMDLEN, "moonlight unpair ");
                    snprintf(cmd + total, MAXCMDLEN, "%s", ip);

                    u32 msg = system(cmd) == 0 ? htonl(MSG_OK) : htonl(MSG_NO);
                    send(connfd, &msg, sizeof(msg), 0);

                    free(ip);
                } break;

                case MSG_LAUNCH:
                {
                    /* run moonlight stream -app <game> <config> <host> */
                    if (!host_running) {
                        char cmd[MAXCMDLEN];
                        int total = snprintf(cmd, MAXCMDLEN, "moonlight stream ");
                        printf("server cmd: %s\n", cmd);

                        char *game;
                        int game_size = recstr(connfd, &game);
                        total += snprintf(cmd + total, MAXCMDLEN, "%s", game);
                        printf("server cmd: %s\n", cmd);

                        host_running = !system(cmd);
                        u32 msg = host_running ? htonl(MSG_OK) : htonl(MSG_NO);
                        send(connfd, &msg, sizeof(msg), 0);

                        free(game);

                    } else {

                    }
                } break;

                case MSG_QUIT:
                {
                    /* run moonlight quit <host> and return code to enter on host */
                    if (host_running) {
                        char *ip;
                        recstr(connfd, &ip);

                        char cmd[MAXCMDLEN];
                        int total = snprintf(cmd, MAXCMDLEN, "echo moonlight quit ");
                        snprintf(cmd + total, MAXCMDLEN, "%s", ip);

                        host_running = system(cmd);
                        u32 msg = host_running == 0 ? htonl(MSG_OK) : htonl(MSG_NO);
                        send(connfd, &msg, sizeof(msg), 0);
                    } else {

                    }
                } break;

                case MSG_HOSTNAME:
                {
                    char hostname[MAXHOSTLEN];
                    if (gethostname(hostname, MAXHOSTLEN) == -1) {
                        perror("server (gethostname)");
                    } else {
                        int hostlen = strnlen(hostname, MAXHOSTLEN);
                        sendstr(connfd, hostname);
                    }
                } break;

                case MSG_PING:
                {
                    /* check if the host is available */
                    char *ip;
                    recstr(connfd, &ip);

                    char cmd[MAXCMDLEN];
                    int total = snprintf(cmd, MAXCMDLEN, "echo ping ");
                    snprintf(cmd + total, MAXCMDLEN, "%s", ip);

                    u32 msg = system(cmd) == 0 ? htonl(MSG_OK) : htonl(MSG_NO);
                    send(connfd, &msg, sizeof(msg), 0);
                } break;

                default:
                {
                    fprintf(stderr, "unknown msg sent: %x\n", msg);
                }
            }

            printf("server - finished handling request\n");
            close(connfd);
        }
    }
}
