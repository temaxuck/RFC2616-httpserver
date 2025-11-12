#ifndef HTTP_REQRESP_H
#  define HTTP_REQRESP_H

#include <stdbool.h>

#include "err.h"
#include "common.h"

typedef plex {
    HTTP_Method          method;
    HTTP_Version         httpver;
    HTTP_URL             url;
    HTTP_PathComponents *pc;

    HTTP_Headers headers;
    uint64_t     content_length;

    int connfd;
    /* Server will use this to parse the incoming request */
    HTTP_Parser *_parser;
} HTTP_Request;

typedef plex {
    HTTP_Status  status;
    HTTP_Version httpver;

    HTTP_Headers headers;
    uint64_t     content_length;

    int connfd;
    /* Client will use this to parse the incoming response */
    HTTP_Parser *_parser;

    bool _was_sent;
} HTTP_Response;

HTTP_Err http_request_init(HTTP_Request *req, int connfd);
HTTP_Err http_response_init(HTTP_Response *resp, int connfd);
HTTP_Err http_request_free(HTTP_Request *req);
HTTP_Err http_response_free(HTTP_Response *resp);

HTTP_Err http_request_set_url(HTTP_Request *req, char *url);
HTTP_Err http_request_set_method(HTTP_Request *req, HTTP_Method m);
HTTP_Err http_request_add_header(HTTP_Request *req, const char *hname, const char *hval);
HTTP_Err http_request_remove_header(HTTP_Request *req, const char *hname);
HTTP_Err http_request_write_body_chunk(HTTP_Request *req, char *chunk, size_t chunk_sz);

void         http_request_addr(HTTP_Request *req, char *peer_addr_repr, size_t peer_addr_repr_len);
HTTP_Method  http_request_method(HTTP_Request *req);
HTTP_URL     http_request_url(HTTP_Request *req);
HTTP_Version http_request_httpver(HTTP_Request *req);
HTTP_Headers http_request_headers(HTTP_Request *req);
uint64_t     http_request_content_length(HTTP_Request *req);

/**
 * Stream body of HTTP Request `req` into buffer `chunk` of size `chunk_sz`.
 *
 * On successful read returns HTTP_ERR_CONT. Once consumed successfully,
 * returns HTTP_ERR_OK.
 */
HTTP_Err http_request_read_body_chunk(HTTP_Request *req, char *chunk, size_t chunk_sz);

/**
 * Returns request's (`req`) path variable that matches handler's pattern at
 * position `pos`.
 *
 * The returned value is a dynamically allocated list, that is safe to free,
 * using http_pc_free() function.
 *
 * If the pattern doesn't handle a path variable at position `pos`, NULL is
 * returned.
 */
HTTP_PathComponents *http_request_pathvar(HTTP_Request *req, size_t pos);
HTTP_PathComponents *http_request_path_components(HTTP_Request *req);
HTTP_Err             http_response_add_header(HTTP_Response *resp, const char *hname, const char *hval);
HTTP_Err             http_response_set_status_code(HTTP_Response *resp, uint16_t sc);
HTTP_Err             http_response_set_content_length(HTTP_Response *resp, uint64_t content_length);
HTTP_Err             http_response_send(HTTP_Response *resp, uint16_t sc);
#endif // HTTP_REQRESP_H

#ifdef HTTP_REQRESP_IMPL
#  ifndef HTTP_REQRESP_IMPL_GUARD
#    define HTTP_REQRESP_IMPL_GUARD

#include "io.h"
#include "parser.h"
#define HTTP_SOCK_IMPL
#include "socket.h"

HTTP_Err http_request_init(HTTP_Request *req, int connfd) {
    req->method  = HTTP_Method_GET;
    req->httpver = (HTTP_Version) {.maj = 1, .min = 1};
    http_url_free(&req->url);
    req->url = (HTTP_URL) {0};
    req->pc = NULL;

    req->headers = (HTTP_Headers) {0};
    req->content_length = 0;

    req->connfd = connfd;

    return HTTP_ERR_OK;
}

HTTP_Err http_request_set_method(HTTP_Request *req, HTTP_Method m) {
    req->method = m;
    return HTTP_ERR_OK;
}

HTTP_Err http_request_set_url(HTTP_Request *req, char *url) {
    HTTP_Err err;
    if ((err = http_url_parse(&req->url, url, strlen(url)))) return err;
    if ((err = http_pc_init(&req->pc, req->url.path))) return err;
    return HTTP_ERR_OK;
}

HTTP_PathComponents *http_request_pathvar(HTTP_Request *req, size_t pos) {
    HTTP_PathComponents *last = NULL, *root = NULL, *cur = req->pc;

    while (cur != NULL) {
        if (cur->wc_idx == (ssize_t)pos) {
            HTTP_PathComponents **pv = (last == NULL) ? &root : &last->next;
            *pv = HTTP_REALLOC(NULL, sizeof(HTTP_PathComponents));
            (*pv)->value = strdup(cur->value);
            (*pv)->next = NULL;
            (*pv)->wc_idx = cur->wc_idx;

            last = *pv;
        }
        cur = cur->next;
    }

    return root;
}

HTTP_Err http_request_read_body_chunk(HTTP_Request *req, char *chunk, size_t chunk_sz) {
    if (req->_parser->stage == HTTP_PS_DONE) return HTTP_ERR_OK;

    HTTP_Err err = http_parser_stream_body(req->_parser, chunk, chunk_sz);

    if (err == HTTP_ERR_OK) return HTTP_ERR_CONT;
    return err;
}

HTTP_Err http_request_free(HTTP_Request *req) {
    http_url_free(&req->url);
    http_headers_free(&req->headers);
    http_pc_free(req->pc);

    return HTTP_ERR_OK;
}

HTTP_Err http_request_add_header(HTTP_Request *req, const char *hname, const char *hval) {
    char *hn = strdup(hname);
    if (hn == NULL) return HTTP_ERR_OOM;
    char *hv = strdup(hval);
    if (hv == NULL) return HTTP_ERR_OOM;

    http_da_append(&req->headers, ((HTTP_Header){ .k = hn, .v = hv }));
    return HTTP_ERR_OK;
}

HTTP_Err http_request_set_content_length(HTTP_Request *req, uint64_t cl) {
    req->content_length = cl;
    return HTTP_ERR_OK;
}

HTTP_Err http_response_init(HTTP_Response *resp, int connfd) {
    resp->status  = HTTP_Status_OK;
    resp->httpver = (HTTP_Version) {.maj = 1, .min = 1};

    resp->headers = (HTTP_Headers) {0};
    resp->content_length = 0;

    resp->connfd = connfd;

    return HTTP_ERR_OK;
}

HTTP_Err http_response_set_status_code(HTTP_Response *resp, uint16_t sc) {
    resp->status = sc;
    return HTTP_ERR_OK;
}

HTTP_Err http_response_set_content_length(HTTP_Response *resp, uint64_t content_length) {
    resp->content_length = content_length;
    return HTTP_ERR_OK;
}

HTTP_Err http_response_add_header(HTTP_Response *resp, const char *hname, const char *hval) {
    char *hn = strdup(hname);
    if (hn == NULL) return HTTP_ERR_OOM;
    char *hv = strdup(hval);
    if (hv == NULL) return HTTP_ERR_OOM;

    http_da_append(&resp->headers, ((HTTP_Header){ .k = hn, .v = hv }));
    return HTTP_ERR_OK;
}

HTTP_Err http_response_send(HTTP_Response *resp, uint16_t sc) {
    if (resp->_was_sent) {
        char peer_addr[HTTP_ADDR_REPR_MAX_LEN] = {0};
        http_sock_get_repr(resp->connfd, peer_addr, HTTP_ADDR_REPR_MAX_LEN, true);
        HTTP_WARN("Duplicate call to http_response_send(). The response was headed to \"%s\". Ignoring this call...", peer_addr);
        return HTTP_ERR_OK;
    }

    HTTP_StringBuilder sb = {0};
    http_sb_append_format(&sb, "HTTP/%hu.%hu %u %s\r\n",
                          resp->httpver.maj, resp->httpver.min, sc, http_reason_phrase(sc));

    http_sb_append_format(&sb, "Content-Length: %zu\r\n", resp->content_length);
    // TODO: Compose a list of values, if there are more than one values that
    //       correspond to the header key
    // TODO: Convert header key to canonical form for header's field name
    for (size_t i = 0; i < resp->headers.len; i++) {
        if (strcmp(resp->headers.items[i].k, "Content-Length") == 0) continue;
        http_sb_append_format(&sb, "%s: %s\r\n",
                              resp->headers.items[i].k, resp->headers.items[i].v);
    }
    http_sb_append_cstr(&sb, "\r\n");
    http_sb_finalize(&sb);
    if (write(resp->connfd, sb.items, sb.len) == -1) return HTTP_ERR_FAILED_WRITE;
    http_sb_free(&sb);
    resp->_was_sent = true;

    return HTTP_ERR_OK;
}

HTTP_Err http_response_write_body_chunk(HTTP_Response *resp, char *chunk, size_t chunk_sz) {
    if (!resp->_was_sent) {
        char peer_addr[HTTP_ADDR_REPR_MAX_LEN] = {0};
        http_sock_get_repr(resp->connfd, peer_addr, HTTP_ADDR_REPR_MAX_LEN, true);
        HTTP_WARN(
                  "Trying to write body chunk before headers were sent. The response was headed"
                  " to \"%s\". Ignoring this call... (call http_response_send() first!)", peer_addr);
        return HTTP_ERR_OK;
    }

    if (write(resp->connfd, chunk, chunk_sz) == -1) return HTTP_ERR_FAILED_WRITE;
    return HTTP_ERR_OK;
}

HTTP_Err http_response_free(HTTP_Response *resp) {
    http_headers_free(&resp->headers);
    return HTTP_ERR_OK;
}

#  endif // HTTP_REQRESP_IMPL_GUARD
#endif // HTTP_REQRESP_IMPL


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
