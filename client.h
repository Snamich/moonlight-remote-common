#ifndef CLIENT_H
#define CLIENT_H

int broadcastfd_setup();
int discover(int sockfd, struct sockaddr_in *addr);

int is_duplicate_server(struct moonlight_server *servers, int server_count, struct sockaddr_in *addr);
int add_server(struct moonlight_server *servers, struct sockaddr_in *addr);

int is_duplicate_host(host *hosts, int host_count, char *ip);
int add_host(host *host, char *name, char *ip);

int pair(int sockfd);
int unpair(int sockfd);
int list(int sockfd);
int launch(int sockfd);
int quit(int sockfd);
int hostname(int sockfd, char **str);

#endif // CLIENT_H
