#ifndef CLIENT_H
#define CLIENT_H

int broadcastfd_setup();
int discover(int sockfd, struct sockaddr_in *addr);
int servfd_setup(moonlight_server *server);

long save_server(moonlight_server *server, char *path);
int load_server(moonlight_server *server, char *path);
int add_server(moonlight_server *server, struct sockaddr_in *addr, char *name, char *path);
int update_server_count(moonlight_server *server, char *path);
int is_duplicate_server(moonlight_server *servers, int server_count, struct sockaddr_in *addr);

long save_host(host *host, FILE *fd);
int load_host(host *host, FILE *fd);
int add_host(host *host, char *name, char *ip, char *path);
int update_host_config(host *host, char *path);
int is_duplicate_host(host *hosts, int host_count, char *ip);

int pair(int sockfd);
int unpair(int sockfd);
int list(int sockfd);
int launch(int sockfd, host *host, char *app);
int quit(int sockfd);
int hostname(int sockfd, char **str);

int hash_name(char *name);
char *get_server_name(struct sockaddr_in *addr);
int get_config(host_config *config, char *cfg, int size);
int get_host_ip(host *host, char *str, int size);

#endif // CLIENT_H
