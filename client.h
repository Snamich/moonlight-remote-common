#ifndef CLIENT_H
#define CLIENT_H

int broadcastfd_setup();
int broadcast(int sockfd, struct moonlight_server *server);
int pair(int sockfd);
int unpair(int sockfd);
int list(int sockfd);
int launch(int sockfd);
int quit(int sockfd);
int hostname(int sockfd, char **str);

#endif // CLIENT_H
