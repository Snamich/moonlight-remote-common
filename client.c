#include "common.h"
#include "client.h"

#include <errno.h>

static int
game_cmp(const void *a, const void *b)
{
    const char *g1 = *(const char **)a;
    const char *g2 = *(const char **)b;

    // don't include "The" in comparison
    if (g1 && g2) {
        if (strncmp(g1, "The", 3) == 0) {
            g1 += 4;
        }

        if (strncmp(g2, "The", 3) == 0) {
            g2 += 4;
        }
    }

    return strcmp(g1, g2);
}

long
save_host(host *host, FILE *fd)
{
    long rv = 0;

    if (!host || !fd) {
        printf("error, NULL host or FILE passed to save_host\n");
        goto exit;
    }

    size_t namelen = strlen(host->name) + 1;
    size_t iplen = strlen(host->ip) + 1;

    // write out the host name
    if (!fwrite(&namelen, sizeof(namelen), 1, fd)) {
        printf("error writing host name length\n");
        goto exit;
    }

    if (!fwrite(host->name, namelen, 1, fd)) {
        printf("error writing host name\n");
        goto exit;
    }

    // write out the host ip address
    if (!fwrite(&iplen, sizeof(iplen), 1, fd)) {
        printf("error writing host ip length\n");
        goto exit;
    }

    if (!fwrite(host->ip, iplen, 1, fd)) {
        printf("error writing host ip\n");
        goto exit;
    }

    // write out the host config
    long config_offset = ftell(fd);
    if (!fwrite(&host->config, sizeof(host->config), 1, fd)) {
        printf("error writing host config\n");
        goto exit;
    }

    rv = config_offset;

exit:
    return rv;
}

int
load_host(host *host, FILE *fd)
{
    int rv = 0;

    if (!host || !fd) {
        printf("error, NULL host or FILE passed to load_host\n");
        goto exit;
    }

    // read in the host name
    size_t namelen;
    if (!fread(&namelen, sizeof(namelen), 1, fd))  {
        printf("error reading host name length\n");
    }

    host->name = malloc(namelen);
    if (!host->name) {
        perror("client (load_host malloc name)");
        goto exit;
    }

    if (!fread(host->name, namelen, 1, fd)) {
        printf("error reading host name\n");
        goto memory_cleanup_name;
    }

    // read in the host ip address
    size_t iplen;
    if (!fread(&iplen, sizeof(iplen), 1, fd)) {
        printf("error reading host ip length\n");
        goto memory_cleanup_name;
    }

    host->ip = malloc(iplen);
    if (!host->ip) {
        perror("client (load_host malloc ip)");
        goto memory_cleanup_name;
    }

    if (!fread(host->ip, iplen, 1, fd)) {
        printf("error reading host ip\n");
        goto memory_cleanup_ip;
    }

    // read in the host config
    host->config_offset = ftell(fd);
    if (!fread(&host->config, sizeof(host->config), 1, fd)) {
        printf("error reading host config\n");
        goto memory_cleanup_ip;
    }

    // TODO: fix pairing
    host->is_paired = 1;
    rv = 1;
    goto exit;

memory_cleanup_ip:
    free(host->ip);
memory_cleanup_name:
    free(host->name);
exit:
    return rv;
}

long
save_server(moonlight_server *server, char *path)
{
    long rv = 0;

    if (!server || !path) {
        printf("error, NULL server or path passed to save_server\n");
    }

    FILE *savefd = fopen(path, "wb");
    if (!savefd) {
        perror("client (save_server fopen)");
        goto exit;
    }

    // write the server name
    size_t namelen;
    namelen = strlen(server->name) + 1;
    if (!fwrite(&namelen, sizeof(namelen), 1, savefd)) {
        printf("error writing server name length\n");
        goto file_delete;
    }

    if (!fwrite(server->name, namelen, 1, savefd)) {
        printf("error writing server name\n");
        goto file_delete;
    }

    // write the server address
    if (!fwrite(&server->addr, sizeof(server->addr), 1, savefd)) {
        printf("error writing server address\n");
        goto file_delete;
    }

    // write the server's hosts
    long count_offset = ftell(savefd);
    if (!fwrite(&server->host_count, sizeof(server->host_count), 1,  savefd)) {
        printf("error writing server host count\n");
        goto file_delete;
    }

    host *h;
    for (int i = 0; i < server->host_count; ++i) {
        h = server->hosts + i;
        if (!save_host(h, savefd)) {
            goto file_delete;
        }
    }

    rv = count_offset;
    goto file_cleanup;

file_delete:
    remove(path);
file_cleanup:
    fclose(savefd);
exit:
    return rv;
}

long
load_server(moonlight_server *server, char *path)
{
    long rv = 0;

    if (!server || !path) {
        printf("error, NULL server or path passed to load_server\n");
    }

    FILE *loadfd = fopen(path, "rb");
    if (!loadfd) {
        perror("client (load_server fopen)");
        goto exit;
    }

    // read the server name
    size_t namelen;
    if (!fread(&namelen, sizeof(namelen), 1, loadfd)) {
        int err = errno;
        //perror("load_server (fread)");
        printf("error: %s\n", strerror(err));
        printf("namelen: %lu\n", namelen);
        printf("error reading server name length\n");
        goto file_cleanup;
    }

    server->name = malloc(namelen);
    if (!server->name) {
        perror("client (load_server malloc)");
        goto file_cleanup;
    }

    if (!fread(server->name, namelen, 1, loadfd)) {
        printf("error reading server name\n");
        goto memory_cleanup;
    }

    // read the server address
    if (!fread(&server->addr, sizeof(server->addr), 1, loadfd)) {
        printf("error reading server address\n");
        goto memory_cleanup;
    }

    // read the server's hosts
    long count_offset = ftell(loadfd);
    if (!fread(&server->host_count, sizeof(server->host_count), 1, loadfd)) {
        printf("error reading server host count\n");
        goto memory_cleanup;
    }

    host *h;
    for (int i = 0; i < server->host_count; ++i) {
        h = server->hosts + i;
        if (!load_host(h, loadfd)) {
            goto memory_cleanup;
        }

        h->server = server;
        printf("host %s loaded\n", h->name);
    }

    rv = count_offset;
    goto file_cleanup;

memory_cleanup:
    free(server->name);
file_cleanup:
    fclose(loadfd);
exit:
    return rv;
}

int
update_host_config(host *host, char *path)
{
    if (path) {
        FILE *savefd = fopen(path, "rb+");
        if (savefd) {
            fseek(savefd, host->config_offset, SEEK_SET);
            printf("update_host->config offset: %ld\n", host->config_offset);
            printf("update_host_config before position: %ld\n", ftell(savefd));
            fwrite(&host->config, sizeof(host->config), 1, savefd);
            printf("update_host_config after position: %ld\n", ftell(savefd));

            // put the file back at the end
            fseek(savefd, 0, SEEK_END);

            fclose(savefd);
        }
    }

    return 1;
}

int
update_server_count(moonlight_server *server, char *path)
{
    if (path) {
        FILE *savefd = fopen(path, "rb+");
        if (savefd) {
            fseek(savefd, server->count_offset, SEEK_SET);
            printf("update_server_count offset: %ld\n", server->count_offset);
            printf("update_server_count before position: %ld\n", ftell(savefd));
            fwrite(&server->host_count, sizeof(server->host_count), 1, savefd);
            printf("update_server_count after position: %ld\n", ftell(savefd));

            // put the file back at the end
            fseek(savefd, 0, SEEK_END);

            fclose(savefd);
        }
    }

    return 1;
}

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
tcp_client_setup(moonlight_server *server)
{
    int servfd;
    struct sockaddr_in *server_addr = &server->addr;
    server_addr->sin_port = htons(atoi(LISTEN_PORT));

    if ((servfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("client (tcp_client_setup socket)");
    }

    if (connect(servfd, server_addr, sizeof(*server_addr)) == -1) {
        perror("client (tcp_client_setup connect)");
    }

    return servfd;
}

int
discover(int sockfd, struct sockaddr_in *addr)
{
    int rv = 0;

    /* TODO: figure out address using subnet mask instead of spamming? */
    struct hostent *host_entry;
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

    ssize_t numbytes;
    socklen_t addr_len = sizeof(*addr);
    if ((numbytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)addr, addr_len)) == -1) {
        perror("client (broadcast sendto)");
    } else {
        if ((numbytes = recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)addr, &addr_len)) == -1) {
            perror("client (discover recvfrom)");
        } else {
            msg = ntohl(msg);
            if (msg == MSG_HANDSHAKE) {
                printf("client (discover): received handshake\n");
                rv = 1;
                addr->sin_port = htons(atoi(LISTEN_PORT));
            }
        }
    }

    return rv;
}

int
is_duplicate_server(moonlight_server *servers, int server_count, struct sockaddr_in *addr)
{
    int rv = 0;
    if (!servers || !addr) {
        goto exit;
    }

    moonlight_server *s;
    for (int i = 0; i < server_count; ++i) {
        // check server sockaddr_in
        s = servers + i;
        if (addr->sin_addr.s_addr == s->addr.sin_addr.s_addr) {
            rv = 1;
        }
    }

exit:
    return rv;
}

char *
get_server_name(struct sockaddr_in *addr)
{
    int hostfd;
    if ((hostfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("client (get_server_name socket)");
    }

    if (connect(hostfd, addr, sizeof(*addr)) == -1) {
        perror("client (get_server_name connect)");
    }

    char *name;
    hostname(hostfd, &name);

    close(hostfd);

    return name;
}

char *
get_server_file(moonlight_server *server)
{
    char *file = NULL;

    size_t namelen = strlen(server->name);

    char *ip = inet_ntoa(server->addr.sin_addr);
    size_t iplen = strlen(ip);

    size_t filelen = namelen + iplen + 2;
    file = malloc(filelen);
    if (file) {
        snprintf(file, filelen, "%s-%s", server->name, ip);
    }

    return file;
}

int
add_server(moonlight_server *server, struct sockaddr_in *addr, char *name, char *path)
{
    if (access(path, F_OK) != -1) {
        // file exists
        printf("found server file\n");
        server->count_offset = load_server(server, path);

        printf("server host count: %d\n", server->host_count);
    } else {
        // file doesn't exist
        printf("server file not found, creating one at: %s\n", path);
        struct sockaddr_in *server_addr = &server->addr;
        memcpy(server_addr, addr, sizeof(*server_addr));

        server->name = name;
        server->host_count = 0;
        server->count_offset = save_server(server, path);
    }

    return 0;
}

int
is_duplicate_host(host *hosts, int host_count, char *ip)
{
    int rv = 0;
    if (!hosts || !ip) {
        goto exit;
    }

    host *h;
    for (int i = 0; i < host_count; ++i) {
        h = hosts + i;
        if (strcmp(h->ip, ip) == 0) {
            rv = 1;
        }
    }

exit:
    return rv;
}

int
add_host(host *host, char *name, char *ip, moonlight_server *server, char *path)
{
    size_t namelen = strlen(name) + 1;
    size_t iplen = strlen(ip) + 1;

    host->name = malloc(namelen);
    host->ip = malloc(iplen);
    host->server = server;

    memcpy(host->name, name, namelen);
    memcpy(host->ip, ip, iplen);
    host->is_paired = 1;
    host->config = DEFAULT_CONFIG;

    if (path) {
        FILE *savefd = fopen(path, "ab");
        if (savefd) {
            printf("add_host beg file position: %ld of %s\n", ftell(savefd), path);
            host->config_offset = save_host(host, savefd);
            printf("add_host end file position: %ld of %s\n", ftell(savefd), path);
            fclose(savefd);
        } else {
            perror("add_host (fopen)");
        }
    }

    return 1;
}

int
pair(host *host, int *pair_code)
{
    int rv = 0;

    int sockfd = tcp_client_setup(host->server);
    if (sockfd < 0) {
        goto exit;
    }

    u32 msg = htonl(MSG_PAIR);
    send(sockfd, &msg, sizeof(msg), 0);

    char ip[MAXIPLEN];
    get_host_ip(host, ip);
    sendstr(sockfd, ip);

    recv(sockfd, &msg, sizeof(msg), 0);
    if (ntohl(msg) == MSG_OK) {
        if (recv(sockfd, pair_code, sizeof(*pair_code), 0) == -1) {
            perror("client recv msg_pair");
            goto sock_close;
        }
    } else {
        goto sock_close;
    }

    *pair_code = ntohl(*pair_code);
    rv = sockfd;
    goto exit;

sock_close:
    close(sockfd);
exit:
    return rv;
}

// TODO: implement this
int
pair_cancel(int sockfd)
{
    int rv = 0;

    if (sockfd < 0) {
        goto exit;
    }

    u32 msg = htonl(MSG_OK);
    if (send(sockfd, &msg, sizeof(msg), 0) == -1) {
        perror("client send pair_cancel");
        goto sock_close;
    }

    rv = 1;

sock_close:
    close(sockfd);
exit:
    return rv;
}

int
pair_response(int sockfd)
{
    int rv = 0;

    if (sockfd < 0) {
        goto exit;
    }

    u32 msg;
    if (recv(sockfd, &msg, sizeof(msg), 0) == -1) {
        perror("client recv pair_response");
        goto sock_close;
    }

    rv = ntohl(msg) == MSG_OK ? 1 : 0;

sock_close:
    close(sockfd);
exit:
    return rv;
}

int
unpair(host *host)
{
    int rv = 0;

    int sockfd = tcp_client_setup(host->server);
    if (sockfd < 0) {
        goto exit;
    }

    u32 msg = htonl(MSG_UNPAIR);
    send(sockfd, &msg, sizeof(msg), 0);

    char ip[MAXIPLEN];
    get_host_ip(host, ip);
    sendstr(sockfd, ip);

    recv(sockfd, &msg, sizeof(msg), 0);
    if (ntohl(msg) == MSG_OK) {
        recv(sockfd, &msg, sizeof(msg), 0);
        if (ntohl(msg) == MSG_OK) {
            rv = 1;
        }
    }

    close(sockfd);

exit:
    return rv;
}

int
list(host *host, gamelist *glist)
{
    int rv = 0;

    int sockfd = tcp_client_setup(host->server);
    if (sockfd < 0) {
        goto exit;
    }

    u32 msg = htonl(MSG_LIST);
    send(sockfd, &msg, sizeof(msg), 0);

    char ip[MAXIPLEN];
    get_host_ip(host, ip);
    sendstr(sockfd, ip);

    char **list;
    u32 i;
    recv(sockfd, &msg, sizeof(msg), 0);
    if (ntohl(msg) == MSG_OK) {
        // check if there's a new list to receive
        recv(sockfd, &msg, sizeof(msg), 0);
        if (ntohl(msg) == MSG_NO) {
            goto sock_close;
        }

        // get the number of items
        u32 nitems;
        if (recv(sockfd, &nitems, sizeof(nitems), 0) == -1) {
            perror("client recv msg_list");
            goto sock_close;
        }

        nitems = ntohl(nitems);
        list = malloc(nitems * sizeof(char *));
        if (!list) {
            goto sock_close;
        }

        for (i = 0; i < nitems; ++i) {
            if (!recstr(sockfd, &list[i])) {
                printf("client (list): error receiving string\n");
                goto list_cleanup;
            }

            printf("client (list) received line: %s\n", list[i]);
        }

        if (glist) {
            free_gamelist(glist);
        }

        glist->list = list;
        glist->count = nitems;

        qsort(glist->list, glist->count, sizeof(*glist->list), game_cmp);

        rv = 1;
        goto sock_close;
    } else {
        goto sock_close;
    }

list_cleanup:
    for (u32 j = 0; j < i; ++j) {
        free(list[j]);
    }
    free(list);
sock_close:
    close(sockfd);
exit:
    return rv;
}

int
launch(host *host, char *game)
{
    int rv = 0;

    int sockfd = tcp_client_setup(host->server);
    if (sockfd < 0) {
        goto exit;
    }

    printf("client - launching game: %s", game);
    u32 msg = htonl(MSG_LAUNCH | host->config << CFG_SHIFT);
    send(sockfd, &msg, sizeof(msg), 0);

    char cmd[MAXCMDLEN];
    snprintf(cmd, MAXCMDLEN, "-app \"%s\" ", game);
    sendstr(sockfd, game);

    char ip[MAXIPLEN];
    get_host_ip(host, ip);
    sendstr(sockfd, ip);

    recv(sockfd, &msg, sizeof(msg), 0);
    if (ntohl(msg) == MSG_OK) {
        recv(sockfd, &msg, sizeof(msg), 0);
        if (ntohl(msg) == MSG_OK) {
            rv = 1;
        }
    }

    close(sockfd);

exit:
    return rv;
}

int
quit(host *host)
{
    int rv = 0;

    int sockfd = tcp_client_setup(host->server);
    if (sockfd < 0) {
        goto exit;
    }

    u32 msg = htonl(MSG_QUIT);
    send(sockfd, &msg, sizeof(msg), 0);

    char ip[MAXIPLEN];
    get_host_ip(host, ip);
    sendstr(sockfd, ip);

    recv(sockfd, &msg, sizeof(msg), 0);
    if (ntohl(msg) == MSG_OK) {
        recv(sockfd, &msg, sizeof(msg), 0);
        if (ntohl(msg) == MSG_OK) {
            rv = 1;
        }
    }

    close(sockfd);

exit:
    return rv;
}

int
hostname(int sockfd, char **str)
{
    int rv = 0;

    u32 msg = htonl(MSG_HOSTNAME);
    send(sockfd, &msg, sizeof(msg), 0);

    if (recstr(sockfd, str)) {
        printf("rec hostname: %s\n", *str);
        rv = 1;
    }

    return rv;
}

int
get_host_ip(host *host, char *str)
{
    int rv = snprintf(str, MAXIPLEN, "%s", host->ip);

    return rv;
}

int
free_gamelist(gamelist *glist)
{
    for (u32 i = 0; i < glist->count; ++i) {
        free(glist->list[i]);
    }

    free(glist->list);
    return 1;
}
