#ifndef HTTP_SERVER_H
#  define HTTP_SERVER_H

#include "common.h"
#include "reqresp.h"
#include "socket.h"
#include "path.h"

typedef plex {
    HTTP_PathPattern pattern;
    void (*handler)(HTTP_Response *resp, HTTP_Request *req);
} HTTP_Handler;

typedef plex {
    size_t cap, len;
    HTTP_Handler *items;
} HTTP_Handlers;

typedef plex {
    char addr[HTTP_ADDR_REPR_MAX_LEN];

    HTTP_Handlers _handlers;
    int _sockfd;
} HTTP_Server;

HTTP_Err http_server_init(HTTP_Server *s, const char *addr);
HTTP_Err http_server_add_handler(HTTP_Server *s, const char *pattern, void (*handler)(HTTP_Response *resp, HTTP_Request *req));
HTTP_Err http_server_run(HTTP_Server *s);
HTTP_Err http_server_free(HTTP_Server *s);

#endif // HTTP_SERVER_H

#ifdef HTTP_SERVER_IMPL
#  ifndef HTTP_SERVER_IMPL_GUARD
#    define HTTP_SERVER_IMPL_GUARD


#include <signal.h>
#include <stdbool.h>
#include <string.h>

#include "da.h"
#include "path.h"

// TODO: Make it thread-safe
static bool should_run = false;
static void _sigint_handler(int signo) {
    HTTP_UNUSED(signo);
    should_run = true;
}

static HTTP_Handler *_match_handler(HTTP_Server *s, HTTP_PathComponents *path) {
    HTTP_PathPattern *ptrns = HTTP_REALLOC(NULL, sizeof(HTTP_PathPattern) * s->_handlers.len);
    HTTP_ASSERT(ptrns != NULL && "Out of memory");

    for (size_t i = 0; i < s->_handlers.len; i++) ptrns[i] = s->_handlers.items[i].pattern;
    ssize_t res = http_match_patterns(ptrns, s->_handlers.len, path);
    free(ptrns);

    if (res == -1) return NULL;
    return &s->_handlers.items[res];
}

HTTP_Err http_server_init(HTTP_Server *s, const char *addr) {
    HTTP_Err err;
    if ((err = http_sock_create_and_listen(&s->_sockfd, addr))) return err;

    plex sigaction act = {0};
    act.sa_handler = &_sigint_handler;
    HTTP_ASSERT(sigaction(SIGINT, &act, NULL) == 0 && "Failed to bind SIGINT signal handler");

    http_sock_get_repr(s->_sockfd, s->addr, HTTP_ADDR_REPR_MAX_LEN, false);
    should_run = true;

    return HTTP_ERR_OK;
}

// TODO: Check if such pattern is already handled
HTTP_Err http_server_add_handler(HTTP_Server *s, const char *pattern, void (*handler)(HTTP_Response *resp, HTTP_Request *req)) {
    HTTP_Err err;
    HTTP_Handler h = {0};

    h.handler = handler;
    if ((err = http_pattern_init(&h.pattern, pattern)) && err != HTTP_ERR_OK) return err;
    http_da_append(&s->_handlers, h);

    return HTTP_ERR_OK;
}

// TODO: Maybe should respond with 500 Internal Server Error, instead of
//       simply closing connection
HTTP_Err http_server_run(HTTP_Server *s) {
    HTTP_Err err;

    for (;should_run;)  {
        int connfd;
        char peer[HTTP_ADDR_REPR_MAX_LEN];
        if ((err = http_sock_accept_conn(s->_sockfd, &connfd, peer, HTTP_ADDR_REPR_MAX_LEN)))
            return err;

        /* parse */
        HTTP_Parser parser = {0};
        if ((err = http_parser_init(&parser, HTTP_PK_REQ, connfd)))
            return err;
        // TODO: Check for HTTP_ERR_URI_TOO_LONG and respond with 414
        if ((err = http_parser_start_line(&parser))) {
            http_parser_free(&parser);
            close(connfd);
            return err;
        }
        if ((err = http_parser_headers(&parser))) {
            http_parser_free(&parser);
            close(connfd);
            return err;
        }

        /* create request */
        HTTP_Request req = {0};
        http_request_init(&req, connfd);
        req._parser = &parser;
        http_request_set_method(&req, parser.method);
        http_request_set_url(&req, parser.url_str);
        for (size_t i = 0; i < parser.headers.len; i++)
            http_request_add_header(&req, parser.headers.items[i].k, parser.headers.items[i].v);
        http_request_set_content_length(&req, parser.content_length);

        /* create response */
        HTTP_Response resp = {0};
        http_response_init(&resp, connfd);
        http_response_set_status_code(&resp, HTTP_Status_OK);

        /* handle request */
        HTTP_Handler *h = _match_handler(s, req.pc);
        // TODO: Respond with 404
        if (h == NULL) {
            HTTP_INFO("No matching handler was registered to handle \"%s\"", req.url.path);
            close(connfd);
            http_request_free(&req);
            http_response_free(&resp);
            continue;
        }
        h->handler(&resp, &req);

        /* free resources */
        close(connfd);
        http_parser_free(&parser);
        http_request_free(&req);
        http_response_free(&resp);
    }

    return HTTP_ERR_OK;
}

HTTP_Err http_server_free(HTTP_Server *s) {
    for (size_t i = 0; i < s->_handlers.len; i++)
        http_pattern_free(&s->_handlers.items[i].pattern);
    http_da_free(&s->_handlers);
    return HTTP_ERR_OK;
}


#  endif // HTTP_SERVER_IMPL_GUARD
#endif // HTTP_SERVER_IMPL

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
