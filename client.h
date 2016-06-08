#ifndef CLIENT_H
#define CLIENT_H

typedef struct gamelist_item {
    char *name;
    u32 i;
} gamelist_item;

typedef struct gamelist {
    gamelist_item **list;
    u32 count;
} gamelist;

int broadcastfd_setup();
int discover(int sockfd, struct sockaddr_in *addr);
int tcp_client_setup(moonlight_server *server);

long save_server(moonlight_server *server, char *path);
long load_server(moonlight_server *server, char *path);
int add_server(moonlight_server *server, struct sockaddr_in *addr, char *name, char *path);
int update_server_count(moonlight_server *server, char *path);
int is_duplicate_server(moonlight_server *servers, int server_count, struct sockaddr_in *addr);

long save_host(host *host, FILE *fd);
int load_host(host *host, FILE *fd);
int add_host(host *host, char *name, char *ip, moonlight_server *server, char *path);
int update_host_config(host *host, char *path);
int is_duplicate_host(host *hosts, int host_count, char *ip);

int pair(host *host, int *pair_code);
int pair_cancel(int sockfd);
int pair_response(int sockfd);
int unpair(host *host);
int list(host *host, gamelist *glist);
int launch(host *host, u32 game_id);
int quit(host *host);
int hostname(int sockfd, char **str);

int get_host_ip(host *host, char *str);
char *get_server_file(moonlight_server *server);
char *get_server_name(struct sockaddr_in *addr);
int free_gamelist(gamelist *glist);

#endif // CLIENT_H
