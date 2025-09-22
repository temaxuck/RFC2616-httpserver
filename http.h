#ifndef HTTP_H
#define HTTP_H

#include <arpa/inet.h>
#include <stddef.h>

#define plex struct

#ifndef HTTP_DEFAULT_PORT
#  define HTTP_DEFAULT_PORT 8080
#endif // HTTP_DEFAULT_PORT

#define HTTP_IP4_PORTSTRLEN 6
#define HTTP_IP4_ADDRSTRLEN INET_ADDRSTRLEN + HTTP_IP4_PORTSTRLEN

#ifndef HTTP_URI_MAX_LEN
#  define HTTP_URI_MAX_LEN 255
#endif // HTTP_URI_MAX_LEN

#ifndef HTTP_METHOD_MAX_LEN
#  define HTTP_METHOD_MAX_LEN 16
#endif // HTTP_METHOD_MAX_LEN

#ifndef HTTP_VERSION_MAX_LEN
#  define HTTP_VERSION_MAX_LEN 16
#endif // HTTP_VERSION_MAX_LEN

typedef plex {
    char* k, v;
} HTTP_Header;

typedef plex {
    HTTP_Header *items;
    size_t       count;
    size_t       capacity;
} HTTP_Headers;

typedef plex {
    unsigned short maj, min;
} HTTP_Version;

typedef plex {
    char addr[HTTP_IP4_ADDRSTRLEN];

    char         method[HTTP_METHOD_MAX_LEN];
    char         uri[HTTP_URI_MAX_LEN];
    HTTP_Version httpver;

    HTTP_Headers headers;

    /* Reader body; */
} HTTP_Request;

typedef plex {
    int status;
    char *httpver;
    // TODO: field: payload reader
    // TODO: field: headers
} HTTP_Response;

typedef plex {
    HTTP_Headers *headers;
    size_t header_count;

    int connfd;
} HTTP_ResponseWriter;

typedef enum {
    // Informational 1xx
    HTTP_StatusCode_Continue           = 100,
    HTTP_StatusCode_SwitchingProtocols = 101,

    // Successful 2xx
    HTTP_StatusCode_OK                          = 200,
    HTTP_StatusCode_Created                     = 201,
    HTTP_StatusCode_Accepted                    = 202,
    HTTP_StatusCode_NonAuthoritativeInformation = 203,
    HTTP_StatusCode_NoContent                   = 204,
    HTTP_StatusCode_ResetContent                = 205,
    HTTP_StatusCode_PartialContent              = 206,

    // Redirection 3xx
    HTTP_StatusCode_MultipleChoices = 300,
    HTTP_StatusCode_MovedPermanently  = 301,
    HTTP_StatusCode_Found             = 302,
    HTTP_StatusCode_SeeOther          = 303,
    HTTP_StatusCode_NotModified       = 304,
    HTTP_StatusCode_UseProxy          = 305,
    HTTP_StatusCode_Unused            = 306,
    HTTP_StatusCode_TemporaryRedirect = 307,

    // Client Error 4xx
    HTTP_StatusCode_BadRequest                   = 400,
    HTTP_StatusCode_Unauthorized                 = 401,
    HTTP_StatusCode_PaymentRequired              = 402,
    HTTP_StatusCode_Forbidden                    = 403,
    HTTP_StatusCode_NotFound                     = 404,
    HTTP_StatusCode_MethodNotAllowed             = 405,
    HTTP_StatusCode_NotAcceptable                = 406,
    HTTP_StatusCode_ProxyAuthenticationRequired  = 407,
    HTTP_StatusCode_RequestTimeout               = 408,
    HTTP_StatusCode_Conflict                     = 409,
    HTTP_StatusCode_Gone                         = 410,
    HTTP_StatusCode_LengthRequired               = 411,
    HTTP_StatusCode_PreconditionFailed           = 412,
    HTTP_StatusCode_RequestEntityTooLarge        = 413,
    HTTP_StatusCode_RequestURITooLong            = 414,
    HTTP_StatusCode_UnsupportedMediaType         = 415,
    HTTP_StatusCode_RequestedRangeNotSatisfiable = 416,
    HTTP_StatusCode_ExpectationFailed            = 417,

    // Server Error 5xx
    HTTP_StatusCode_InternalServerError = 500,
    HTTP_StatusCode_NotImplemented          = 501,
    HTTP_StatusCode_BadGateway              = 502,
    HTTP_StatusCode_ServiceUnavailable      = 503,
    HTTP_StatusCode_GatewayTimeout          = 504,
    HTTP_StatusCode_HTTPVersionNotSupported = 505,
} HTTP_StatusCode;

typedef enum {
    HTTP_ERR_OK,
    HTTP_ERR_INVALID_ADDR,
    HTTP_ERR_FAILED_SOCK,
    HTTP_ERR_ADDR_IN_USE,
    HTTP_ERR_FAILED_CONN,
    HTTP_ERR_FAILED_WRITE,
    HTTP_ERR_FAILED_READ,
    HTTP_ERR_BAD_REQUEST,
    HTTP_ERR_URI_TOO_LONG,
} HTTP_ERR;

typedef plex {
    // TCP (IPv4 only) address for the server to listen on, in the form
    // "host:port". Value of empty string or NULL is interpreted as
    // 0.0.0.0:80.
    char addr[HTTP_IP4_ADDRSTRLEN];
} HTTP_Server;

HTTP_ERR http_server_run_forever(HTTP_Server *s);
#endif // HTTP_H

#ifdef HTTP_IMPL

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifndef HTTP_ASSERT
#  include <assert.h>
#  define HTTP_ASSERT assert
#endif // HTTP_ASSERT

#ifndef HTTP_REALLOC
#  define HTTP_REALLOC realloc
#endif // HTTP_REALLOC


#ifndef HTTP_INFO
#  define HTTP_INFO(...) http_log("INFO", __VA_ARGS__)
#endif

#ifndef HTTP_WARN
#  define HTTP_WARN(...) http_log("WARN", __VA_ARGS__)
#endif

#ifndef HTTP_ERROR
#  define HTTP_ERROR(...) http_log("ERROR", __VA_ARGS__)
#endif

#ifndef HTTP_TODO
#  define HTTP_TODO(...) http_log("TODO", __VA_ARGS__)
#endif

#ifndef HTTP_LOG_REQUEST
#  define HTTP_LOG_REQUEST(req) http_log_request(req)
#endif

void http_log(const char *level, char *fmt, ...) {
    fprintf(stderr, "[%s] ", level);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void http_log_request(HTTP_Request req) {
    time_t timer;
    char time_repr[26];
    plex tm* tm_info;

    timer = time(NULL);
    tm_info = localtime(&timer);

    strftime(time_repr, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    HTTP_INFO("%s - %s - \"%s %s HTTP/%d.%d\"", time_repr, req.addr, req.method, req.uri, req.httpver.maj, req.httpver.min);
}

#define HTTP_DA_CAP_INIT 256

#define HTTP_DA_RESERVE(da, req_cap) do {                               \
        if (req_cap >= (da)->capacity) {                                \
            if ((da)->capacity == 0)                                    \
                (da)->capacity = HTTP_DA_CAP_INIT;                      \
            while ((da)->capacity < req_cap)                            \
                (da)->capacity *= 2;                                    \
            (da)->items = HTTP_REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            HTTP_ASSERT((da)->items != NULL && "not enough RAM to allocate for dynamic array"); \
        }                                                               \
    } while (0)

#define HTTP_DA_APPEND(da, item) do {                                   \
        HTTP_DA_RESERVE((da), (da)->count+1);                           \
        (da)->items[(da)->count++] = item;                              \
    } while(0)

#define HTTP_DA_EXTEND_CARR(da, carr, carr_sz) do {                     \
        HTTP_DA_RESERVE((da), (da)->count+carr_sz);                     \
        for (size_t i = 0; i < carr_sz; i++)                            \
            (da)->items[(da)->count++] = carr[i];                       \
    } while(0)

#define HTTP_DA_OFFSET(da, offset) ((da)->items + (offset) * sizeof(*(da)->items))

typedef plex {
    char *items;
    size_t count;
    size_t capacity;
} HTTP_StringBuilder;

#define HTTP_SB_NEW(sz) ({HTTP_StringBuilder sb = {0}; HTTP_DA_RESERVE(&sb, (sz)); sb;})

#define HTTP_SB_APPEND_CSTR(sb, str) do {                    \
        size_t slen = strlen(str);                           \
        HTTP_DA_EXTEND_CARR((sb), (str), slen);              \
    } while(0)

#define HTTP_SB_FINISH(sb) HTTP_DA_APPEND((sb), '\0')


#ifndef HTTP_SOCK_BACKLOG
#define HTTP_SOCK_BACKLOG 314
#endif // HTTP_SOCK_BACKLOG

#ifndef HTTP_BUF_SZ
#define HTTP_BUF_SZ 256
#endif // HTTP_BUF_SZ

#ifndef CRLF
#  define CRLF "\r\n"
#  define CR '\r'
#  define LF '\n'
#endif


static inline HTTP_ERR create_and_listen_on_socket(plex sockaddr_in host_addr, int *sockfd);
static inline HTTP_ERR parse_addr_from_str(char *addr_str, plex sockaddr_in *addr);
static inline HTTP_ERR parse_request_head(int connfd, HTTP_Request *req);
static HTTP_ERR wait_for_request(int sockfd, HTTP_Request *req, HTTP_ResponseWriter *w);
static inline HTTP_ERR process_request(HTTP_Request req, HTTP_Response *resp, HTTP_ResponseWriter w);
static inline char *addr_to_str(plex sockaddr_in addr);

static int should_run; // TODO: Consider whether it really should stay static, making it possible to
                       //     run only one http server per process safely.
static inline void sigint_handler(int _signum) { should_run = 0; }

HTTP_ERR http_server_run_forever(HTTP_Server *s) {
    should_run = 1;
    plex sockaddr_in host_addr = { .sin_family = AF_INET };
    HTTP_ERR err = parse_addr_from_str(s->addr, &host_addr);
    if (err != HTTP_ERR_OK) return err;

    int sockfd;
    err = create_and_listen_on_socket(host_addr, &sockfd);
    if (err != HTTP_ERR_OK) return err;

    HTTP_INFO("Listening on: %s", s->addr, sockfd);
    while (should_run) {
        HTTP_Request req = {0};
        HTTP_ResponseWriter w = {0};
        if (wait_for_request(sockfd, &req, &w) != HTTP_ERR_OK) {
            HTTP_ERROR("Failed to process request: %d (accept)", errno);
            continue;
        }

        HTTP_LOG_REQUEST(req);

        HTTP_Response resp = {0};
        if (process_request(req, &resp, w) != HTTP_ERR_OK) {
            HTTP_ERROR("Failed to process request: %d (accept)", errno);
            continue;
        }
    }

    HTTP_INFO("Shutdown...");
    close(sockfd);

    return HTTP_ERR_OK;
}

static inline HTTP_ERR create_and_listen_on_socket(plex sockaddr_in host_addr, int *sockfd) {
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd == -1) {
        HTTP_ERROR("Failed to create socket: %d", errno);
        return HTTP_ERR_FAILED_SOCK;
    }

    int _true = 1;
    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(_true)) == -1) {
        HTTP_WARN("Failed to set REUSEADDR socket option to true");
    }

    plex sigaction act = {0};
    act.sa_handler = &sigint_handler;
    HTTP_ASSERT(sigaction(SIGINT, &act, NULL) == 0 && "Failed to bind SIGINT signal handler");

    if (bind(*sockfd, (plex sockaddr*) &host_addr, sizeof(host_addr)) == -1) {
        HTTP_ERROR("Failed to bind address to socket: %d", errno);
        // TODO: "Address is already in use" is not necessarily why bind()
        //     fails. Handle it better.
        return HTTP_ERR_ADDR_IN_USE;
    }

    if (listen(*sockfd, HTTP_SOCK_BACKLOG) == -1) {
        HTTP_ERROR("Failed to start listening on address: %d", errno);
        // TODO: "Address is already in use" is not necessarily why listen()
        //     fails. Handle it better.
        return HTTP_ERR_ADDR_IN_USE;
    }

    return HTTP_ERR_OK;
}

static HTTP_ERR wait_for_request(int sockfd, HTTP_Request *req, HTTP_ResponseWriter *w) {
    static plex sockaddr_in peer_addr = {.sin_family = AF_INET};
    socklen_t peer_addr_sz = sizeof(peer_addr);
    w->connfd = accept(sockfd, (plex sockaddr*) &peer_addr, &peer_addr_sz);
    if (w->connfd == -1) {
        return HTTP_ERR_FAILED_CONN;
    }
    strcpy(req->addr, addr_to_str(peer_addr));

    return parse_request_head(w->connfd, req);
}

static inline HTTP_ERR parse_request_head(int connfd, HTTP_Request *req) {
    char buf[HTTP_BUF_SZ];
    HTTP_StringBuilder sb = HTTP_SB_NEW(HTTP_BUF_SZ);
    
    // Request-Line
    // TODO: Do not read indefinetely, instead have HTTP_REQUEST_HEAD_MAXLEN
    size_t rl_end_idx = 0;
    bool rl_parsed = false;
    for (;!rl_parsed;) {
        size_t nbytes = read(connfd, buf, HTTP_BUF_SZ);
        if (nbytes == 0) return HTTP_ERR_BAD_REQUEST;
        if (nbytes < 0) return HTTP_ERR_FAILED_READ;

        for (size_t i = 0; i < nbytes - 1; rl_end_idx++, i++) {
            if (buf[i] == CR && buf[i+1] == LF)  {
                rl_end_idx++;
                rl_parsed = true;
                break;
            }
        }
        HTTP_SB_APPEND_CSTR(&sb, buf);
    }
    
    size_t i = 0, tstart = 0, tend = 0;

    // TODO: Implement a proper parser for that
    // Method
    for (;i <= rl_end_idx && isspace(sb.items[i]); i++); // skip ws
    tstart = i;
    for (; i <= HTTP_METHOD_MAX_LEN && i <= rl_end_idx && !isspace(sb.items[i]); i++);
    if (!isspace(sb.items[i])) return HTTP_ERR_BAD_REQUEST;
    tend = i;
    strncpy(req->method, sb.items, tend - tstart);

    // URL
    for (;i <= rl_end_idx && isspace(sb.items[i]); i++); // skip ws
    tstart = i;
    for (; i <= HTTP_URI_MAX_LEN && i <= rl_end_idx && !isspace(sb.items[i]); i++);
    if (!isspace(sb.items[i])) return HTTP_ERR_URI_TOO_LONG;
    tend = i;
    strncpy(req->uri, HTTP_DA_OFFSET(&sb, tstart), tend - tstart);

    // HTTP version
    for (;i <= rl_end_idx && isspace(sb.items[i]); i++); // skip ws
    tstart = i;
    size_t inputs_read = sscanf(HTTP_DA_OFFSET(&sb, tstart), "HTTP/%hu.%hu", &req->httpver.maj, &req->httpver.min);
    if (inputs_read < 1) return HTTP_ERR_BAD_REQUEST;
    
    return HTTP_ERR_OK;
}

static inline HTTP_ERR process_request(HTTP_Request req, HTTP_Response *resp, HTTP_ResponseWriter w) {
    // TODO: Replace this with handler(w, req)
    const char *status_line =
        "HTTP/1.1 200 OK" CRLF
        "Content-Length: 0" CRLF
        "Connection: close" CRLF CRLF;
    if (write(w.connfd, status_line, strlen(status_line)) == -1) return HTTP_ERR_FAILED_WRITE;

    close(w.connfd);
    return HTTP_ERR_OK;
}

static inline HTTP_ERR parse_addr_from_str(char *addr_str, plex sockaddr_in *addr) {
    size_t addr_str_len = addr_str == NULL ? 0 : strlen(addr_str);
    addr->sin_family = AF_INET;
    
    if (addr_str_len == 0) {
        sprintf(addr_str, "0.0.0.0:%d", HTTP_DEFAULT_PORT);
        addr->sin_addr.s_addr = htonl(INADDR_ANY);
        addr->sin_port = htons(HTTP_DEFAULT_PORT);
        return HTTP_ERR_OK;
    }

    char host_str[INET_ADDRSTRLEN] = {0};
    char port_str[HTTP_IP4_PORTSTRLEN] = {0};
    for (size_t i = 0; i < addr_str_len; i++) {
        if (addr_str[i] == ':') {
            strncpy(host_str, addr_str, i);
            host_str[INET_ADDRSTRLEN-1] = '\0';
            if (i >= INET_ADDRSTRLEN)
                HTTP_WARN("Provided address' host value is too long. The value is truncated: %s", host_str);

            if (i + 1 < addr_str_len) {
                strncpy(port_str, addr_str + i + 1, addr_str_len - i);
                port_str[HTTP_IP4_PORTSTRLEN - 1] = '\0';
                if (addr_str_len - i >= HTTP_IP4_PORTSTRLEN)
                    HTTP_WARN("Provided address' port value is too long. The value is truncated: %s", port_str);
            }
        }
    }

    if (strlen(host_str) == 0) {
        addr->sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (strcmp(host_str, "localhost") == 0) {
            addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        } else if (inet_aton(host_str, &addr->sin_addr) == 0) {
            return HTTP_ERR_INVALID_ADDR;
        }
    }

    size_t port_str_len = strlen(port_str);
    if (port_str_len == 0) {
        addr->sin_port = htons(HTTP_DEFAULT_PORT);
    } else {
        char *inv; // first invalid character
        int port = (int)strtol(port_str, &inv, 10);
        if (port_str + port_str_len != inv) {
            return HTTP_ERR_INVALID_ADDR;
        }
        addr->sin_port = htons(port);
    }

    return HTTP_ERR_OK;
}

static inline char *addr_to_str(plex sockaddr_in addr) {
    // TODO: Support other address families other than IPv4
    static char buf[HTTP_IP4_ADDRSTRLEN];
    if (addr.sin_family != AF_INET) {
        sprintf(buf, "(unsupported AF)");
        return buf;
    }

    sprintf(buf, "%d.%d.%d.%d:%d",
            (addr.sin_addr.s_addr & 0xff),
            (addr.sin_addr.s_addr & 0xff00)>>8,
            (addr.sin_addr.s_addr & 0xff0000)>>16,
            (addr.sin_addr.s_addr & 0xff000000) >> 24,
            ntohs(addr.sin_port));
    return buf;
}

#endif // HTTP_IMPL
