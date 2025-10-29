/*
 * socket.h - Socket management.
 *
 * The maximum number of pending connections is defined by
 * `HTTP_SOCK_BACKLOG`. By default, it is set to 420, but you may re-define it
 * to custom value, like this:
 *
 * ```c
 * #define HTTP_SOCK_BACKLOG 9999
 * #include "http/socket.h"
 * ```
 *
 * NOTE: So far, this library has only synchronous implementation.
 */
#ifndef HTTP_SOCK_H
#  define HTTP_SOCK_H

#include <stddef.h>

#include "err.h"

// NOTE: INET6_ADDRSTRLEN + 2 (from "[]") + 6 (from ":" and port representation)
#define HTTP_ADDR_REPR_MAX_LEN (INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN) + 8

#ifndef HTTP_SOCK_BACKLOG
#  define HTTP_SOCK_BACKLOG 420
#endif // HTTP_SOCK_BACKLOG

/**
 * Creates a socket (saving the file descriptor into `sockfd`) and starts
 * listening on address `addr_repr`.
 *
 * The argument `addr_repr` is an address representation, that uses
 * "host:port" notation. If `addr_repr` is NULL or an empty string, ":http"
 * (all interfaces, port 80) is used. The "host" part may be omitted, which is
 * interpreted as if the socket should bind to all network interfaces. The
 * "host" part also may be a valid IPv4/IPv6 address. The "port" part must be
 * a valid port number or a service name (see services(5)).
 *
 * NOTE: The behavior of this function prefers IPv4 over IPv6, meaning if
 *       `addr_repr` is an empty string or NULL, or "host" part is omitted or
 *       equals to "localhost", the socket will bind to IPv4 address.
 */
HTTP_Err http_sock_create_and_listen(int *sockfd, const char *addr_repr);

/**
 * Accepts connection (saving the file descriptor into `connfd`) on socket
 * `sockfd`, saving peer's address representation into `peer_addr_repr` of
 * max length `peer_addr_repr_len`.
 */
HTTP_Err http_sock_accept_conn(int sockfd, int *connfd,
                               char *peer_addr_repr, size_t peer_addr_repr_len);
/**
 * Closes opened socket `sockfd`.
 */
HTTP_Err http_sock_close(int sockfd);

#endif // HTTP_SOCK_H

#ifdef HTTP_SOCK_IMPL
#  ifndef HTTP_SOCK_IMPL_GUARD
#    define HTTP_SOCK_IMPL_GUARD

#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "log.h"

/**
 * Parses address representation `addr_repr` into separate `host` and `port`.
 *
 * This function also partially validates address representation, but you
 * still should not fully rely on the result. For example, this function would
 * still return `HTTP_ERR_OK` for `addr_repr` equals to
 * "gibberish_host:gibberish_port", which is not, really, a valid address
 * representation.
 *
 * But it would recognize these values of `addr_repr` as invalid:
 * - ":" - (port was not given);
 * - "[]:9000" - (IPv6 address host was not given);
 * - "[::1:9000" - (IPv6 address host was not properly enclosed in braces);
 * - "localhost" - (nor ":" neither port was provided);
 *
 * However, case when `addr_repr` equals to NULL or "" is considered a valid
 * input. In this case, it is treated as if it's intended to bind to all
 * interfaces at default HTTP port (80);
 */
static HTTP_Err _parse_addr_repr(const char *addr_repr, char **host, char **port) {
    if (addr_repr == NULL) goto default_addr;

    int addr_repr_len = strlen(addr_repr);
    if (addr_repr_len == 0) {
    default_addr:
        *host = NULL;
        *port = strdup("http");
        return HTTP_ERR_OK;
    }

    if (addr_repr_len < 2) {
        // Valid address representation consists of at least 2 symbols.
        // For example: ":1" - all interfaces, port "1".
        return HTTP_ERR_BAD_ADDR;
    }

    if (addr_repr[0] == '[') {
        // IPv6
        int cb_pos = -1;
        for (int pos = 0; pos < addr_repr_len; pos++) {
            if (addr_repr[pos] == ']') {
                cb_pos = pos;
                break;
            }
        }
        if (
            cb_pos == -1
            || addr_repr_len <= cb_pos + 2
            || addr_repr[cb_pos + 1] != ':'
            ) return HTTP_ERR_BAD_ADDR;
        if (cb_pos <= 1) return HTTP_ERR_BAD_ADDR;
        *host = strndup(addr_repr + 1, cb_pos - 1);
        *port = strndup(addr_repr + cb_pos + 2, addr_repr_len - cb_pos - 2);
    } else {
        // IPv4 or ":8080"
        int colon_pos = -1;
        for (int pos = 0; pos < addr_repr_len; pos++) {
            if (addr_repr[pos] == ':') {
                colon_pos = pos;
                break;
            }
        }
        if (colon_pos == -1 || addr_repr_len <= colon_pos + 1) return HTTP_ERR_BAD_ADDR;
        if (colon_pos == 0) *host = NULL;
        else *host = strndup(addr_repr, colon_pos);
        *port = strndup(addr_repr + colon_pos + 1, addr_repr_len - colon_pos - 1);
    }

    return HTTP_ERR_OK;
}

/**
 * Converts provided `hostname` and `port` into struct sockaddr (`sa`) of size
 * `sa_sz`.
 */
static HTTP_Err _hp_to_sa(const char *hostname, const char *port,
                       plex sockaddr **sa, socklen_t *sa_sz) {
    plex addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    if (hostname == NULL || strlen(hostname) == 0 || strcmp(hostname, "localhost") == 0)
        hints.ai_family = AF_INET;
    else
        hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(hostname, port, &hints, &res);
    if (err != 0) {
        // TODO: I'm not sure about what error should we return, when err is
        //       EAI_SYSTEM or EAI_AGAIN. In other cases, it seems, like it's
        //       a bad address.
        return HTTP_ERR_BAD_ADDR;
    }

    *sa = HTTP_REALLOC(NULL, res->ai_addrlen);
    if (!*sa) {
        freeaddrinfo(res);
        return HTTP_ERR_OOM;
    }
    memcpy(*sa, res->ai_addr, res->ai_addrlen);
    *sa_sz = res->ai_addrlen;

    freeaddrinfo(res);
    
    return HTTP_ERR_OK;
}

static char *_sa_to_addr_repr(plex sockaddr sa) {
    static char addr_repr[HTTP_ADDR_REPR_MAX_LEN] = {0};

    if (sa.sa_family == AF_INET) {
        inet_ntop(AF_INET, &((plex sockaddr_in *)(&sa))->sin_addr, addr_repr, INET_ADDRSTRLEN);
        size_t pos = 0;
        for (; pos < HTTP_ADDR_REPR_MAX_LEN && addr_repr[pos] != '\0'; pos++);
        HTTP_ASSERT(pos != HTTP_ADDR_REPR_MAX_LEN && "Unreachable");
        sprintf(addr_repr + pos, ":%d", ntohs(((plex sockaddr_in *)(&sa))->sin_port));
        return addr_repr;
    }
    if (sa.sa_family == AF_INET6) {
        addr_repr[0] = '[';
        inet_ntop(AF_INET6, &((plex sockaddr_in6 *)(&sa))->sin6_addr, addr_repr+1, INET6_ADDRSTRLEN);
        size_t pos = 0;
        for (; pos < HTTP_ADDR_REPR_MAX_LEN && addr_repr[pos] != '\0'; pos++);
        HTTP_ASSERT(pos != HTTP_ADDR_REPR_MAX_LEN && "Unreachable");
        sprintf(addr_repr + pos, "]:%d", ntohs(((plex sockaddr_in *)(&sa))->sin_port));
        return addr_repr;
    }

    return NULL;
}

/* TODO: put this into Server
  plex sigaction act = {0};
  act.sa_handler = &sigint_handler;
  HTTP_ASSERT(sigaction(SIGINT, &act, NULL) == 0 && "Failed to bind SIGINT signal handler");
*/
HTTP_Err http_sock_create_and_listen(int *sockfd, const char *addr_repr) {
    char *host, *port;
    plex sockaddr *addr;
    socklen_t addr_len;
    HTTP_Err err;

    if ((err = _parse_addr_repr(addr_repr, &host, &port))) return err;
    if ((err = _hp_to_sa(host, port, &addr, &addr_len))) return err;

    free(host);
    free(port);

    *sockfd = socket(addr->sa_family, SOCK_STREAM, 0);
    if (*sockfd == -1) return HTTP_ERR_FAILED_SOCK;

    int _true = 1;
    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(_true)) == -1) {
        HTTP_WARN("Failed to set REUSEADDR socket option to true");
    }

    if (bind(*sockfd, addr, addr_len) == -1) {
        if (errno == EADDRINUSE) return HTTP_ERR_ADDR_IN_USE;
        printf("%s\n", strerror(errno));
        return HTTP_ERR_BAD_SOCK;
    }

    if (listen(*sockfd, HTTP_SOCK_BACKLOG) == -1) {
        if (errno == EADDRINUSE) return HTTP_ERR_ADDR_IN_USE;
        return HTTP_ERR_BAD_SOCK;
    }

    return HTTP_ERR_OK;
}

HTTP_Err http_sock_accept_conn(int sockfd, int *connfd,
                               char *peer_addr_repr, size_t peer_addr_repr_len) {
    plex sockaddr_storage peer_addr;
    socklen_t peer_addr_len;

    *connfd = accept(sockfd, (plex sockaddr *)&peer_addr, &peer_addr_len);
    if (*connfd == -1) return HTTP_ERR_FAILED_SOCK;

    strncpy(peer_addr_repr, _sa_to_addr_repr(*(plex sockaddr *)&peer_addr), peer_addr_repr_len);
    return HTTP_ERR_OK;
}

HTTP_Err http_sock_close(int sockfd) {
    if (!close(sockfd)) return HTTP_ERR_BAD_SOCK;
    return HTTP_ERR_OK;
}

#  endif // HTTP_SOCK_IMPL_GUARD
#endif // HTTP_SOCK_IMPL

/*
 * Copyright (c) 2025 Artem Darizhapov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
