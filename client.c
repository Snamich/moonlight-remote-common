#include "common.h"
#include "client.h"

#include <errno.h>

int
hash_name(char *name)
{
    int hash = 0;

    if (!name) {
        return 0;
    }

    for(int i = 0; name[i] != '\0'; ++i) {
        hash += name[i] * (i + 1);
    }

    return hash;
}

long
save_host(host *host, FILE *fd)
{
    long config_offset, rv = 0;
    size_t namelen, iplen;

    if (!host || !fd) {
        printf("error, NULL host or FILE passed to save_host\n");
        goto exit;
    }

    namelen = strlen(host->name) + 1;
    iplen = strlen(host->ip) + 1;

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
    config_offset = ftell(fd);
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
    size_t namelen, iplen;

    if (!host || !fd) {
        printf("error, NULL host or FILE passed to load_host\n");
        goto exit;
    }

    // read in the host name
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
    long count_offset, rv = 0;
    size_t namelen;

    if (!server || !path) {
        printf("error, NULL server or path passed to save_server\n");
    }

    FILE *savefd = fopen(path, "wb");
    if (!savefd) {
        perror("client (save_server fopen)");
        goto exit;
    }

    // write the server name
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
    count_offset = ftell(savefd);
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

int
load_server(moonlight_server *server, char *path)
{
    int rv = 0;
    long count_offset;
    size_t namelen;

    if (!server || !path) {
        printf("error, NULL server or path passed to load_server\n");
    }

    FILE *loadfd = fopen(path, "rb");
    if (!loadfd) {
        perror("client (load_server fopen)");
        goto exit;
    }

    // read the server name
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
    count_offset = ftell(loadfd);
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
    FILE *savefd;

    if (path) {
        savefd = fopen(path, "rb+");
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
    FILE *savefd;

    if (path) {
        savefd = fopen(path, "rb+");
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
servfd_setup(moonlight_server *server)
{
    int servfd;
    struct sockaddr_in *server_addr = &server->addr;
    server_addr->sin_port = htons(atoi(LISTEN_PORT));

    if ((servfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("client (servfd_setup socket)");
    }

    if (connect(servfd, server_addr, sizeof(*server_addr)) == -1) {
        perror("client (servfd_setup connect)");
    }

    return servfd;
}

int
discover(int sockfd, struct sockaddr_in *addr)
{
    int rv = 0;
    ssize_t numbytes;
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
                addr->sin_port = htons(atoi(LISTEN_PORT));
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

char *
get_server_name(struct sockaddr_in *addr)
{
    int hostfd;
    char *name;

    if ((hostfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("client (get_server_name socket)");
    }

    if (connect(hostfd, addr, sizeof(*addr)) == -1) {
        perror("client (get_server_name connect)");
    }

    hostname(hostfd, &name);

    close(hostfd);

    return name;
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
        printf("server file not found, creating one\n");
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
add_host(host *host, char *name, char *ip, char *path)
{
    FILE *savefd;

    size_t namelen = strlen(name) + 1;
    size_t iplen = strlen(ip) + 1;

    host->name = malloc(namelen);
    host->ip = malloc(iplen);
    host->servfd = -1;

    memcpy(host->name, name, namelen);
    memcpy(host->ip, ip, iplen);
    host->is_paired = 1;
    memcpy(&host->config, &default_config, sizeof(default_config));

    if (path) {
        savefd = fopen(path, "ab");
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
launch(int sockfd, host *host, char *app)
{
    u32 msg = htonl(MSG_LAUNCH);
    send(sockfd, &msg, sizeof(msg), 0);

    char cmd[MAXCMDLEN];
    int total = snprintf(cmd, MAXCMDLEN, "-app %s ", app);
    total += get_config(&host->config, cmd + total, MAXCMDLEN - total);
    total += get_host_ip(host, cmd + total, MAXCMDLEN - total);

    sendstr(sockfd, cmd, total + 1);

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

    *str = malloc(MAXHOSTLEN);
    if (recstr(sockfd, *str, MAXHOSTLEN)) {
        printf("rec hostname: %s\n", *str);
        rv = 1;
    }

    return rv;
}

int
get_config(host_config *config, char *str, int size)
{
    int fps, bitrate, packetsize, rv = 0;
    char *resolution, *modify_settings, *localaudio;

    if (config && str) {
        fps = config->fps;
        bitrate = config->bitrate;
        packetsize = config->packetsize;
        resolution = (config->resolution == 720) ? "-720" : "-1080";
        modify_settings = config->modify_settings ? "-nsops" : "";
        localaudio = config->localaudio ? "-localaudio" : "";

        rv = snprintf(str, size, "-fps %d %s %s %s ", fps, resolution, modify_settings, localaudio);
    }

    return rv;
}

int
get_host_ip(host *host, char *str, int size)
{
    int rv = snprintf(str, size, "%s", host->ip);

    return rv;
}
