// TCP Client test program (IPV6)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstdlib>

#include "sdk.h"

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in6 serv_addr;
    struct hostent *server;
    char buffer[256] = "This is a string from client!";

    if(argc < 3) {
        printf("usage: client <addr> <port> <netpath> <nwid>\n");
        return 1;
    }

    /* Starts ZeroTier core service in separate thread, loads user-space TCP/IP stack
    and sets up a private AF_UNIX socket between ZeroTier library and your app. Any 
    subsequent zts_* socket API calls (shown below) are mediated over this hidden AF_UNIX 
    socket and are spoofed to appear as AF_INET sockets. The implementation of this API
    is in src/sockets.c */
    zts_init_rpc(argv[3],argv[4]);

    portno = atoi(argv[2]);

    printf("\nIPv6 TCP Client Started...\n");
    
    //Sockets Layer Call: socket()
    sockfd = zts_socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    //Sockets Layer Call: gethostbyname2()
    server = gethostbyname2(argv[1],AF_INET6);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin6_flowinfo = 0;
    serv_addr.sin6_family = AF_INET6;
    memmove((char *) &serv_addr.sin6_addr.s6_addr, (char *) server->h_addr, server->h_length);
    serv_addr.sin6_port = htons(portno);

    //Sockets Layer Call: connect()
    if (zts_connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    
    //Sockets Layer Call: send()
    n = send(sockfd,buffer, strlen(buffer)+1, 0);
    if (n < 0)
        error("ERROR writing to socket");

    printf("sent %d bytes\n", n);
    memset(buffer, 0, 256);
    
    //Sockets Layer Call: recv()
    printf("reading...\n");
    n = recv(sockfd, buffer, 255, 0);
    if (n < 0)
        error("ERROR reading from socket");
    printf("Message from server: %s\n", buffer);

    //Sockets Layer Call: close()
    close(sockfd);
        
    return 0;
}