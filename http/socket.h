#ifndef HTTP_SOCK_H
#  define HTTP_SOCK_H

#include <arpa/inet.h>
#include "common.h"
#include "err.h"

#ifndef HTTP_SOCK_BACKLOG
#  define HTTP_SOCK_BACKLOG 420
#endif // HTTP_SOCK_BACKLOG

HTTP_Err http_sock_create(int *sockfd);
HTTP_Err http_sock_listen(int sockfd, plex sockaddr_in host_addr);
HTTP_Err http_sock_close(int sockfd);

#endif // HTTP_SOCK_H

#ifdef HTTP_SOCK_IMPL
#  ifndef HTTP_SOCK_IMPL_GUARD
#    define HTTP_SOCK_IMPL_GUARD

#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"

HTTP_Err http_sock_create(int *sockfd) {
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd == -1) {
        return HTTP_ERR_FAILED_SOCK;
    }

    int _true = 1;
    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(_true)) == -1) {
        HTTP_WARN("Failed to set REUSEADDR socket option to true");
    }
    
    return HTTP_ERR_OK;
}

/* TODO: put this into Server
  plex sigaction act = {0};
  act.sa_handler = &sigint_handler;
  HTTP_ASSERT(sigaction(SIGINT, &act, NULL) == 0 && "Failed to bind SIGINT signal handler");
*/

// TODO: Abstract out an IPv4 address into either a struct HTTP_Addr or a string
HTTP_Err http_sock_bind(int sockfd, plex sockaddr_in host_addr) {
    if (bind(sockfd, (plex sockaddr*) &host_addr, sizeof(host_addr)) == -1) {
        if (errno == EADDRINUSE) return HTTP_ERR_ADDR_IN_USE;
        return HTTP_ERR_BAD_SOCK;
    }

    if (listen(sockfd, HTTP_SOCK_BACKLOG) == -1) {
        if (errno == EADDRINUSE) return HTTP_ERR_ADDR_IN_USE;
        return HTTP_ERR_BAD_SOCK;
    }

    return HTTP_ERR_OK;
}

// TODO: Abstract out an IPv4 address into either a struct HTTP_Addr or a string
HTTP_Err http_sock_accept_conn(int sockfd, int *connfd) {
    plex sockaddr_in peer_addr;
    socklen_t peer_addr_len;

    *connfd = accept(sockfd, (plex sockaddr*) &peer_addr, &peer_addr_len);
    if (*connfd == -1) return HTTP_ERR_FAILED_SOCK;
    
    return HTTP_ERR_OK;
}

HTTP_Err http_sock_close(int sockfd) {
    if (!close(sockfd)) return HTTP_ERR_BAD_SOCK;
    return HTTP_ERR_OK;
}

#  endif // HTTP_SOCK_IMPL_GUARD
#endif // HTTP_SOCK_IMPL
