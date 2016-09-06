#include "common.h"

#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>

#define BACKLOG 5

static int host_running = 0;

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
get_config_str(u32 config, char *str, int size)
{
    int fps, rv = 0;
    char *resolution, *modify_settings, *localaudio;

    if (str && 0 < size) {
        fps = get_config_opt(config, CFG_FPS) ? 60 : 30;
        resolution = get_config_opt(config, CFG_RESOLUTION) ? "-1080" : "-720";
        modify_settings = get_config_opt(config, CFG_MODIFY_SETTINGS) ? "-nsops" : "";
        localaudio = get_config_opt(config, CFG_LOCAL_AUDIO) ? "-localaudio" : "";

        rv = snprintf(str, size, "-fps %d %s %s %s ", fps, resolution, modify_settings, localaudio);
    }

    return rv;
}

void
sigchld_handler(int s)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
    host_running = 0;
}

int
main(int argc, char **argv)
{
    pid_t broadcast_pid;

    if ((broadcast_pid = fork()) < 0) {
        perror("fork");
        exit(1);
    }

    if (!broadcast_pid) {
        /* child */
        int broadcastfd = broadcastfd_setup();;
        discover(broadcastfd);
    } else {
        /* parent */
        int listenfd = listenfd_setup();
        uint msg_packed = 0, msg = 0;

        char **gamelist = NULL;
        u32 gamelist_count = 0;

        struct sockaddr_storage their_addr;
        socklen_t addr_size = sizeof(their_addr);

        // set up signal handler for launch process
        struct sigaction sa;
        sa.sa_handler = sigchld_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        if (sigaction(SIGCHLD, &sa, NULL) == -1) {
            perror("sigaction");
            exit(1);
        }

        while (1) {
            // get msg, only handles one client for now
            int connfd = accept(listenfd, (struct sockaddr *)&their_addr, &addr_size);
            if (recv(connfd, &msg_packed, sizeof(msg_packed), 0) == -1) {
                perror("server (recv msg)");
                continue;
            }

            msg_packed = ntohl(msg_packed);
            msg = msg_packed & 0x000F;
            printf("server received msg %x from client\n", msg);

            switch (msg) {
                case MSG_LIST:
                {
                    u32 msg;

                    char *ip;
                    recstr(connfd, &ip);
                    if (is_valid_ip(ip)) {
                        msg = htonl(MSG_OK);
                        send(connfd, &msg, sizeof(msg), 0);

                        char cmd[MAXCMDLEN];
                        snprintf(cmd, MAXCMDLEN, "moonlight list %s", ip);

                        FILE *pd = popen(cmd, "r");
                        if (!pd) {
                            perror("server (popen)");
                            // send back some kind of error response
                            continue;
                        }

                        char tmplist[] = "list.XXXXXX";
                        FILE *new_listfd = fdopen(mkstemp(tmplist), "wb+");
                        if (!new_listfd) {
                            perror("server (msg_list fopen)");
                            continue;
                        }

                        u32 nlines = 0;
                        size_t linelen = 0;
                        char *line = NULL;
                        ssize_t bytes_read = 0;
                        fseek(new_listfd, sizeof(linelen), SEEK_SET);
                        while ((bytes_read = getline(&line, &linelen, pd)) != -1) {
                            // skip the line if it doesn't start with a number
                            if (!isdigit(line[0])) {
                                continue;
                            }

                            // find the start of the name
                            ssize_t i;
                            for (i = 0; i < bytes_read && !isalpha(line[i]); ++i) {}

                            // remove newline and write the size of the string followed by the string itself
                            line[bytes_read - 1] = '\0';
                            bytes_read = bytes_read - i;

                            fwrite(&bytes_read, sizeof(bytes_read), 1, new_listfd);
                            fwrite(line + i, bytes_read, 1, new_listfd);

                            ++nlines;
                        }

                        // write the number of lines to the beginning of the file
                        fseek(new_listfd, 0, SEEK_SET);
                        fwrite(&nlines, sizeof(nlines), 1, new_listfd);

                        fflush(new_listfd);
                        free(line);
                        pclose(pd);

                        // compare new file to old file to see if list has changed
                        // TODO: make the list file based on host?
                        int force_list = (msg_packed >> FORCE_LIST_SHIFT) & 0x000F;
                        snprintf(cmd, MAXCMDLEN, "cmp %s list.txt", tmplist);
                        if (!gamelist || force_list || system(cmd) != 0) {
                            printf("server msg_list: list has changed or was forced, sending a new copy\n");
                            msg = htonl(MSG_OK);
                            send(connfd, &msg, sizeof(msg), 0);

                            int save_list = 0;
                            FILE *old_listfd = fopen("list.txt", "wb");
                            if (old_listfd) {
                                save_list = 1;
                                fwrite(&nlines, sizeof(nlines), 1, old_listfd);
                            } else {
                                printf("server msg_list: unable to open list for saving\n");
                            }

                            // free the previous gamelist
                            // TODO: maybe only free items that have changed? is it worth it?
                            if (gamelist) {
                                for (u32 i = 0; i < gamelist_count; ++i) {
                                    free(gamelist[i]);
                                }

                                free(gamelist);
                            }

                            gamelist_count = nlines;
                            gamelist = malloc(gamelist_count * sizeof(*gamelist));
                            if (!gamelist) {
                                perror("server msg_list malloc gamelist");
                            }

                            // send the list
                            nlines = htonl(nlines);
                            send(connfd, &nlines, sizeof(nlines), 0);
                            nlines = ntohl(nlines);

                            fseek(new_listfd, sizeof(nlines), SEEK_SET);
                            for (u32 i = 0; i < nlines; ++i) {
                                fread(&linelen, sizeof(linelen), 1, new_listfd);
                                printf("server msg_list: sending linelen: %zu\n", linelen);
                                gamelist[i] = malloc(linelen);

                                fread(gamelist[i], linelen, 1, new_listfd);
                                printf("server msg_list: sending game: %s at spot %d\n", gamelist[i], i);
                                sendstr(connfd, gamelist[i]);

                                if (save_list) {
                                    fwrite(&linelen, sizeof(linelen), 1, old_listfd);
                                    fwrite(gamelist[i], linelen, 1, old_listfd);
                                }
                            }

                            if (save_list) {
                                fclose(old_listfd);
                            }
                        } else {
                            printf("server msg_list: list hasn't changed, not sending\n");
                            msg = htonl(MSG_NO);
                            send(connfd, &msg, sizeof(msg), 0);
                        }

                        fclose(new_listfd);
                        remove(tmplist);
                        printf("server msg_list: finished sending list\n");
                    } else {

                    }

                    free(ip);
                } break;

                case MSG_PAIR:
                {
                    /* run moonlight pair <host> and return code to enter on host */
                    u32 msg;

                    char *ip;
                    recstr(connfd, &ip);
                    if (is_valid_ip(ip)) {
                        msg = htonl(MSG_OK);
                        send(connfd, &msg, sizeof(msg), 0);

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
                        int pair_code = -1;
                        ssize_t bytes_read = 0;
                        while ((bytes_read = getline(&line, &linelen, fd)) != -1) {
                            // find the line the pin is on
                            if (6 < bytes_read && strncmp(line, "Please", 6) != 0) {
                                continue;
                            }

                            // find the start of the name
                            ssize_t i;
                            for (i = 0; i < bytes_read && !isdigit(line[i]); ++i) {}

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
                        while ((bytes_read = getline(&line, &linelen, fd))) {
                            if (bytes_read == -1 && feof(fd)) {
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
                    } else {
                        msg = htonl(MSG_NO);
                        send(connfd, &msg, sizeof(msg), 0);
                    }

                    free(ip);
                } break;

                case MSG_UNPAIR:
                {
                    /* run moonlight unpair <host> */
                    /* this doesn't appear to actually do anything right now */
                    u32 msg;

                    char *ip;
                    recstr(connfd, &ip);
                    if (is_valid_ip(ip)) {
                        msg = htonl(MSG_OK);
                        send(connfd, &msg, sizeof(msg), 0);

                        char cmd[MAXCMDLEN];
                        int total = snprintf(cmd, MAXCMDLEN, "moonlight unpair ");
                        snprintf(cmd + total, MAXCMDLEN, "%s", ip);

                        u32 msg = system(cmd) == 0 ? htonl(MSG_OK) : htonl(MSG_NO);
                        send(connfd, &msg, sizeof(msg), 0);
                    } else {
                        msg = htonl(MSG_NO);
                        send(connfd, &msg, sizeof(msg), 0);
                    }

                    free(ip);
                } break;

                case MSG_LAUNCH:
                {
                    /* run moonlight stream -app <game> <config> <host> */
                    u32 msg;

                    if (!host_running) {
                        u32 config = (msg_packed >> CFG_SHIFT) & 0x000F;
                        u32 game_id = (msg_packed >> GAME_SHIFT) & 0x00FF;

                        printf("server msg_launch - launching game_id %d\n", game_id);

                        if (gamelist && game_id < gamelist_count) {
                            msg = htonl(MSG_OK);
                            send(connfd, &msg, sizeof(msg), 0);

                            char *ip;
                            recstr(connfd, &ip);
                            if (is_valid_ip(ip)) {
                                msg = htonl(MSG_OK);
                                send(connfd, &msg, sizeof(msg), 0);

                                pid_t launch_pid;
                                if (!(launch_pid = fork())) {
                                    // child
                                    char *fps, *resolution, *modify_settings, *localaudio;

                                    fps = get_config_opt(config, CFG_FPS) ? "60" : "30";
                                    resolution = get_config_opt(config, CFG_RESOLUTION) ? "-1080" : "-720";
                                    modify_settings = get_config_opt(config, CFG_MODIFY_SETTINGS) ? "-nsops" : NULL;
                                    localaudio = get_config_opt(config, CFG_LOCAL_AUDIO) ? "-localaudio" : NULL;

                                    char *arglist[] = { "/usr/bin/moonlight", "stream", "-app", gamelist[game_id],
                                                        "-fps", fps, resolution, modify_settings, localaudio, ip, NULL };

                                    if (execv(arglist[0], &arglist[0]) == -1) {
                                        perror("server (msg_launch execv)");
                                    }

                                    exit(1);
                                } else if (launch_pid > 0) {
                                    // parent
                                    host_running = 1;
                                    msg = htonl(MSG_OK);
                                } else {
                                    // error forking
                                    msg = htonl(MSG_NO);
                                }

                                send(connfd, &msg, sizeof(msg), 0);
                            } else {
                                msg = htonl(MSG_NO);
                                send(connfd, &msg, sizeof(msg), 0);
                            }

                            free(ip);
                        } else {
                            printf("server msg_launch: invalid game_id sent\n");
                            msg = htonl(MSG_NO);
                            send(connfd, &msg, sizeof(msg), 0);
                        }
                    } else {

                    }
                } break;

                case MSG_QUIT:
                {
                    /* run moonlight quit <host> and return code to enter on host */
                    u32 msg;
                    if (host_running) {
                        char *ip;
                        recstr(connfd, &ip);
                        if (is_valid_ip(ip)) {
                            msg = htonl(MSG_OK);
                            send(connfd, &msg, sizeof(msg), 0);

                            char cmd[MAXCMDLEN];
                            int total = snprintf(cmd, MAXCMDLEN, "echo moonlight quit ");
                            snprintf(cmd + total, MAXCMDLEN, "%s", ip);

                            host_running = system(cmd);
                            msg = host_running == 0 ? htonl(MSG_OK) : htonl(MSG_NO);
                            send(connfd, &msg, sizeof(msg), 0);
                        } else {
                            msg = htonl(MSG_NO);
                            send(connfd, &msg, sizeof(msg), 0);
                        }

                        free(ip);
                    } else {

                    }
                } break;

                case MSG_HOSTNAME:
                {
                    char hostname[MAXHOSTLEN];
                    if (gethostname(hostname, MAXHOSTLEN) == -1) {
                        perror("server (gethostname)");
                    } else {
                        sendstr(connfd, hostname);
                    }
                } break;

                case MSG_PING:
                {
                    /* check if the host is running */
                    if (host_running) {
                        msg = htonl(MSG_OK);
                    } else {
                        msg = htonl(MSG_NO);
                    }

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
