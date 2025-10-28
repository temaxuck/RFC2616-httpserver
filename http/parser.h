/** parser.h - HTTP request/response parser.
 *
 * Parser reads message in a blocking way in several stages (see Table below):
 *
 * ---------+-----------------------------+-----------------------------------
 * Stage no |         Stage name          |         Stage result
 * ---------+-----------------------------+-----------------------------------
 *          |                             | Request: Method, URI, HTTP-Version
 *    0     |         Start-Line          | Response: HTTP-Version,
 *          |                             |           Status-Code,
 *          |                             |           Reason-Phrase
 * ---------+-----------------------------+-----------------------------------
 *    1     |          Headers            | Request: Headers
 *          |                             | Response: Headers
 * ---------+-----------------------------+-----------------------------------
 *          |                             | Request: Body (if Content-Length
 *          |                             |          is specified and is
 *    2     |            Body             |          greater than 0)
 *          |                             | Response: Body (same remark as
 *          |                             |           above)
 * ---------+-----------------------------+-----------------------------------
 *    3     |            Done             | Parsing is done
 * ---------+-----------------------------+-----------------------------------
 *
 * Usage:
 *
 * ```c
 * // open/accept connection
 * HTTP_ERR err;
 * HTTP_Parser p = {0};
 * http_parser_init(&p, HTTP_PK_REQ, connfd);
 * err = http_parser_start_line(&p);
 * // handle err
 * err = http_parser_headers(&p);
 * // handle err
 * printf("Received request (of size %ld bytes):\n", p.nread);
 * printf("Method:       %s\n", http_method_to_cstr(p.method));
 * printf("URI:          %s\n", p.uri.repr);
 * printf("HTTP Version: %hd.%hd\n", p.httpver.maj, p.httpver.min);
 * printf("Headers:\n");
 * for (size_t i = 0; i < p.headers.len; i++) {
 *     printf("%s: %s", p.headers.items[i].k, p.headers.items[i].v);
 * }
 *
 * // body
 * printf("Body:\n");
 * for (; !http_parser_finished(&p);) {
 *     char chunk[8] = {0};
 *     err = http_parser_stream_body(&p, chunk, 8);
 *     // handle err
 *     printf("%.*s", http_parser_last_read(&p), chunk);
 * }
 *
 * http_parser_free(&p);
 * ```
 *
 * NOTE: This HTTP Parser implementation doesn't support lots of features,
 *       that MUST, SHALL, SHOULD, MAY be implemented, or are REQUIRED,
 *       RECOMMENDED, OPTIONAL, according to RFC 2616. Such features include
 *       (the list may be incomplete):
 *
 *        1. Chunked message body (when Transfer-Encoding header is
 *           specified). Message body is parsed only when message has
 *           explicitly specified header 'Content-Length', otherwise it is
 *           omitted;
 *       2. Keep-alive connections;
 *       3. Upgrade connections;
 *       4. Multi-line header values;
 *
 */

#ifndef HTTP_PARSER_H
#  define HTTP_PARSER_H

#include <stdbool.h>

#include "common.h"
#ifdef HTTP_PARSER_IMPL
#  define IO_IMPL
#endif // HTTP_PARSER_IMPL
#include "io.h"

typedef enum { HTTP_PK_REQ, HTTP_PK_RESP } HTTP_ParserKind;
typedef enum {
    HTTP_PS_START_LINE,
    HTTP_PS_HEADERS,
    HTTP_PS_BODY,
    HTTP_PS_DONE,
} HTTP_ParserStage;

#ifndef HTTP_PARSER_URI_MAX_LEN
#  define HTTP_PARSER_URI_MAX_LEN 256
#endif // HTTP_PARSER_URI_MAX_LEN

typedef plex {
    char repr[HTTP_PARSER_URI_MAX_LEN];
} HTTP_URI;

typedef plex {
    // It is expected that the connection socket is opened and is ready
    // for reading
    int connfd;

    unsigned int kind  : 2;
    unsigned int stage : 4;

    unsigned int method : 4; // Kind: HTTP_PK_REQ
    unsigned int status : 8; // Kind: HTTP_PK_RESP
    HTTP_Version httpver;
    HTTP_URI uri;            // Kind: HTTP_PK_REQ

    HTTP_Headers headers;
    uint64_t content_length;

    IO_Buffer *_buffer;
    IO_Reader *_reader;
    size_t _last_reader_pos;
    ssize_t _body_start_pos;
    bool _ignore_lf;
} HTTP_Parser;

/**
 * Initializes parser instance `p` with kind `pk` and connection `connfd`.
 *
 * Usage:
 * ```c
 *     HTTP_Parser p = {0};
 *     http_parser_init(&p, HTTP_PK_REQ, connfd);
 *     // or http_parser_init(&p, HTTP_PK_RESP, connfd)
 * ```
 */
HTTP_Err http_parser_init(HTTP_Parser *p, HTTP_ParserKind pk, int connfd);

/**
 * Returns the number of bytes parser `p` read last time from socket.
 *
 * This might be useful, in case you want to know how many bytes was actually
 * read from stream, when you read body in chunks, using
 * http_parser_stream_body().
 */
size_t http_parser_last_read(HTTP_Parser *p);

/**
 * Returns the total number of bytes parser `p` read from socket.
 */
size_t http_parser_total_read(HTTP_Parser *p);

/**
 * Returns the number of bytes read from HTTP Message body, using parser `p`.
 */
size_t http_parser_body_size(HTTP_Parser *p);

/**
 * Parses Start-Line, using parser `p`.
 *
 * It is a generic function for advancing the first stage of the parser. It is
 * equivalent to calling http_parser_request_line() or
 * http_parser_status_line(), depending on whether parser's kind is
 * HTTP_PK_REQ and HTTP_PK_RESP accordingly.
 *
 * Parser's stage must be HTTP_PS_START_LINE, otherwise this function is
 * expected to return HTTP_ERR_FAILED_PARSE.
 *
 * NOTE: Calling this function automatically advances parser's stage.
 */
HTTP_Err http_parser_start_line(HTTP_Parser *p);

/**
 * Parses Request-Line, using parser `p`.
 *
 * If parser's kind is HTTP_PK_RESP, this function will return
 * HTTP_ERR_FAILED_PARSE.
 *
 * Parser's stage must be HTTP_PS_START_LINE, otherwise this function is
 * expected to return HTTP_ERR_FAILED_PARSE.
 *
 * NOTE: Calling this function automatically advances parser's stage.
 */
HTTP_Err http_parser_request_line(HTTP_Parser *p);

/**
 * Parses Status-Line, using parser `p`.
 *
 * If parser's kind is HTTP_PK_REQ, this function will return
 * HTTP_ERR_FAILED_PARSE.
 *
 * Parser's stage must be HTTP_PS_START_LINE, otherwise this function is
 * expected to return HTTP_ERR_FAILED_PARSE.
 *
 * NOTE: Calling this function automatically advances parser's stage.
 */
HTTP_Err http_parser_status_line(HTTP_Parser *p);

/**
 * Parses headers, using parser `p`.
 *
 * Parser's stage must be HTTP_PS_HEADERS, otherwise this function is expected
 * to return HTTP_ERR_FAILED_PARSE.
 *
 * NOTE: Calling this function automatically advances parser's stage.
 */
HTTP_Err http_parser_headers(HTTP_Parser *p);

/**
 * Reads body into chunk `chunk` of size up to `chunk_sz`, using parser `p`.
 *
 * Parser's stage must be HTTP_PS_BODY, otherwise this function is expected to
 * return HTTP_ERR_FAILED_PARSE.
 *
 * Since this function doesn't necessarily read `chunk_sz` bytes, you may want
 * to use http_parser_last_read() to get a number of bytes that was actually
 * read.
 *
 * NOTE: Calling this function automatically advances parser's stage.
 */
HTTP_Err http_parser_stream_body(HTTP_Parser *p, char *chunk, size_t chunk_sz);

/**
 * Checks whether parser `p` is done parsing.
 */
bool http_parser_is_finished(HTTP_Parser *p);

/**
 * Frees parser `p`.
 */
HTTP_Err http_parser_free(HTTP_Parser *p);

#endif // HTTP_PARSER_H

#ifdef HTTP_PARSER_IMPL
#  ifndef HTTP_PARSER_IMPL_GUARD
#    define HTTP_PARSER_IMPL_GUARD

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "da.h"
#include "log.h"

#ifndef HTTP_PARSER_BUF_SZ
/* #  define HTTP_PARSER_BUF_SZ 64*1<<10 */
#  define HTTP_PARSER_BUF_SZ 5
#endif // HTTP_PARSER_BUF_SZ

#ifndef http_return_defer
#  define http_return_defer(value) do { result = (value); goto defer; } while(0)
#endif // http_return_defer

#ifndef CRLF
#  define CRLF "\r\n"
static char CR = CRLF[0];
static char LF = CRLF[1];
#endif // HTTP_PARSER_TOKEN_MAX_LEN

typedef plex {
    // da
    size_t len, cap;
    char *items;
    // parser
    size_t pos;
} HTTP_Parser_Message;

typedef enum {
    HTTP_PTK_NUMBER,
    HTTP_PTK_GENTOK, // RFC2616 General Token
    HTTP_PTK_CRLF,   // RFC2616 CRLF
    HTTP_PTK_SEP,    // RFC2616 Separator
    HTTP_PTK_EOF,    // End of message
} HTTP_Parser_TokenKind;

const char HTTP_PARSER_SEP[] = {
    '(' , ')' , '<' , '>'  , '@' ,
    ',' , ';' , ':' , '\\' , '"' ,
    '/' , '[' , ']' , '?'  , '=' ,
    '{' , '}' , ' ' , '\t'
};

#undef HTTP_PARSER_SEP_COUNT
#define HTTP_PARSER_SEP_COUNT 19

typedef plex {
    const char *start;
    const char *end;

    HTTP_Parser_TokenKind tk;

    double numval;
} HTTP_Parser_Token;

static IO_Buffer *_new_buffer(size_t cap) {
    IO_Buffer *b = HTTP_REALLOC(NULL, sizeof(IO_Buffer));
    if (b == NULL) return NULL;
    if (io_buffer_init(b, cap) != IO_ERR_OK) {
        return NULL;
    };
    return b;
}

static IO_Reader *_new_reader(IO_Buffer *b, int fd) {
    IO_Reader *r = HTTP_REALLOC(NULL, sizeof(IO_Reader));
    if (r == NULL) return NULL;
    if (io_reader_init(r, b, fd) != IO_ERR_OK) {
        return NULL;
    };
    return r;
}

static void _advance_stage(HTTP_Parser *p) {
    p->stage = !http_parser_is_finished(p) ? p->stage + 1 : HTTP_PS_DONE;
}

HTTP_Err http_parser_init(HTTP_Parser *p, HTTP_ParserKind pk, int connfd) {
    p->kind = pk;
    p->stage = HTTP_PS_START_LINE;

    p->connfd = connfd;

    p->method = HTTP_Method_UNKNOWN;
    p->status = HTTP_Status_UNKNOWN;
    p->httpver.maj = 0;
    p->httpver.min = 0;

    p->headers = (HTTP_Headers) {0};
    p->content_length = 0;

    p->_buffer = _new_buffer(HTTP_PARSER_BUF_SZ);
    p->_reader = _new_reader(p->_buffer, p->connfd);
    p->_last_reader_pos = p->_reader->pos;
    p->_body_start_pos = -1;
    p->_ignore_lf = false;

    return HTTP_ERR_OK;
}

size_t http_parser_last_read(HTTP_Parser *p) {
    return p->_reader->pos - p->_last_reader_pos;
}

size_t http_parser_total_read(HTTP_Parser *p) {
    return p->_reader->nread;
}

size_t http_parser_body_size(HTTP_Parser *p) {
    if (p->_body_start_pos == -1) {
        return 0;
    }
    return p->_reader->nread - (size_t) p->_body_start_pos;
}

bool http_parser_is_finished(HTTP_Parser *p) {
    return p->stage >= HTTP_PS_DONE;
}

HTTP_Err http_parser_free(HTTP_Parser *p) {
    for (size_t i = 0; i < p->headers.len; i++) {
        free(p->headers.items[i].k);
        free(p->headers.items[i].v);
    }

    if (p->_buffer) {
        io_buffer_free(p->_buffer);
        free(p->_buffer);
    }

    if (p->_reader) free(p->_reader);

    return HTTP_ERR_OK;
}
//////////////////// BEGIN: Lexer ////////////////////
static inline bool _iscrlf(char c) {
    return c == CR || c == LF;
}

static inline bool _isws(char c) {
    return c == ' ' || c == '\t';
}

static inline bool _islws(char c) {
    return _iscrlf(c) || _isws(c);
}

static inline bool _isctl(char c) {
    int _c = c;
    return _c < 31 && _c != 127;
}

static inline bool _issep(char c) {
    size_t i = 0;
    for (; i < HTTP_PARSER_SEP_COUNT && HTTP_PARSER_SEP[i] != c; i++);
    return i < HTTP_PARSER_SEP_COUNT;
}

static inline bool _isgentok(char c) {
    return !_isctl(c) && !_issep(c);
}

static inline bool _adv_char(HTTP_Parser_Message *msg) {
    if (msg->pos >= msg->len) return false;
    msg->pos++; return true;
}

static inline bool _adv_n_chars(HTTP_Parser_Message *msg, size_t n) {
    while (n-->0 && _adv_char(msg));
    return n == 0;
}

static inline void _skipws(HTTP_Parser_Message *msg) {
    while (msg->pos < msg->len && _isws(msg->items[msg->pos]) && _adv_char(msg));
}

static bool _next_token(HTTP_Parser_Message *msg, HTTP_Parser_Token *t, bool *ignore_lf) {
    memset(t, 0, sizeof(*t));
    t->start = &msg->items[msg->pos];
    t->end = &msg->items[msg->pos];

    if (msg->pos >= msg->len) {
        t->tk = HTTP_PTK_EOF;
        return false;
    }

    if (*ignore_lf && msg->items[msg->pos] == LF) {
        _adv_char(msg);
        return _next_token(msg, t, ignore_lf);
    }
    *ignore_lf = false;

    if (msg->items[msg->pos] == LF) goto crlf_token;
    if (msg->items[msg->pos] == CR) {
        if (msg->pos+1 < msg->len && msg->items[msg->pos+1] == LF) {
            _adv_char(msg);
            t->end++;
        }
        *ignore_lf = true;
    crlf_token:
        t->tk = HTTP_PTK_CRLF;
        _adv_char(msg);
        t->end++;
        return true;
    }

    if (_issep(msg->items[msg->pos])) {
        t->tk = HTTP_PTK_SEP;
        t->end++;
        _adv_char(msg);
        return true;
    }

    if (isdigit(msg->items[msg->pos])) {
        t->tk = HTTP_PTK_NUMBER;
        char *end;
        t->numval = strtod(&msg->items[msg->pos], &end);
        t->end = end;
        _adv_n_chars(msg, t->end - t->start);

        return true;
    }

    if (_isgentok(msg->items[msg->pos])) {
        t->tk = HTTP_PTK_GENTOK;
        size_t n = 1;
        for (; msg->pos + n < msg->len && _isgentok(msg->items[msg->pos + n]); n++);
        t->end += n;
        _adv_n_chars(msg, n);
        return true;
    }

    return false;
}

static inline char *_token_to_cstr(HTTP_Parser_Token *t) {
    size_t len = t->end - t->start;
    char *buf = HTTP_REALLOC(NULL, len + 1);
    if (!buf) return NULL;

    memcpy(buf, t->start, len);
    buf[len] = '\0';

    return buf;
}

static inline bool _token_cmp_cstr(HTTP_Parser_Token *t, const char *cstr) {
    bool result = false;
    char *repr = _token_to_cstr(t);
    http_return_defer(strcmp(repr, cstr) == 0);

 defer:
    free(repr);
    return result;
}
//////////////////// END:   Lexer ////////////////////

//////////////////// BEGIN: Parser ////////////////////

// TODO: I know this is awful. Implement wrapper around io_* API.
static HTTP_Err _io_err_to_http_err(IO_Err err) {
    if (err == IO_ERR_OK) return HTTP_ERR_OK;
    if (err == IO_ERR_OOM) return HTTP_ERR_OOM;
    if (err == IO_ERR_OOB) return HTTP_ERR_OOB;
    if (err == IO_ERR_EOF) return HTTP_ERR_EOF;
    if (err == IO_ERR_PARTIAL) return HTTP_ERR_OK;
    if (err == IO_ERR_FAILED_READ) return HTTP_ERR_FAILED_READ;

    HTTP_ASSERT(false && "Unreachable");
}

static HTTP_Err _prefetch(HTTP_Parser *p) {
    IO_Err err = io_reader_prefetch(p->_reader, p->_buffer->cap);
    return _io_err_to_http_err(err);
}

static HTTP_Err _peek(HTTP_Parser *p, char *dest, size_t n) {
    IO_Err err = io_reader_npeek(p->_reader, dest, n);
    return _io_err_to_http_err(err);
}

static HTTP_Err _read(HTTP_Parser *p, char *dest, size_t n) {
    p->_last_reader_pos = p->_reader->pos;
    IO_Err err = io_reader_nread(p->_reader, dest, n);
    return _io_err_to_http_err(err);
}

/**
 * Reads a line of HTTP Message from socket into `msg`, using parser `p`.
 *
 * NOTE: This function is not intended for reading body.
 */
static HTTP_Err _receive_msg(HTTP_Parser *p, HTTP_Parser_Message *msg) {
    HTTP_Err result = HTTP_ERR_OK;
    http_da_reset(msg);
    msg->pos = 0;
    p->_last_reader_pos = p->_reader->pos;

    for (;;) {
        HTTP_Err err = _prefetch(p);
        if (err != HTTP_ERR_OK) http_return_defer(err);

        size_t buffered = io_buffer_len(p->_buffer);
        char *temp = HTTP_REALLOC(NULL, buffered);
        size_t i = 0;
        for (;i < buffered;) {
            char c = io_buffer_at(p->_buffer, i++);
            if (c == CR || c == LF) {
                io_reader_nconsume(p->_reader, temp, i);
                http_da_append_carr(msg, temp, i);
                if (c == CR) {
                    err = _peek(p, &c, 1);
                    if (err != HTTP_ERR_OK) http_return_defer(err);
                }
                if (c == LF) {
                    http_da_append(msg, c);
                    io_reader_nconsume(p->_reader, NULL, 1);
                }
                http_return_defer(HTTP_ERR_OK);
            }
        }
        io_reader_nconsume(p->_reader, temp, i);
        http_da_append_carr(msg, temp, i);
    }
 defer:
    return result;
}

/**
 * Parses HTTP version from input string `s` into `hv`.
 *
 * Returns number of characters read, or -1 if failed to parse.
 */
static int _parse_version(const char *s, HTTP_Version *hv) {
    int n;
    if (sscanf(s, "HTTP/%hd.%hd%n", &hv->maj, &hv->min, &n) == 2) return n;
    return -1;
}

HTTP_Err http_parser_start_line(HTTP_Parser *p) {
    if (p->stage != HTTP_PS_START_LINE) return HTTP_ERR_WRONG_STAGE;
    if (p->kind == HTTP_PK_REQ) return http_parser_request_line(p);
    if (p->kind == HTTP_PK_RESP) return http_parser_status_line(p);
    HTTP_ASSERT(false && "Unreachable");
}

HTTP_Err http_parser_request_line(HTTP_Parser *p) {
    if (p->stage != HTTP_PS_START_LINE) return HTTP_ERR_WRONG_STAGE;
    HTTP_Err result = HTTP_ERR_OK;

    HTTP_Parser_Message msg = {0};
    HTTP_Parser_Token t = {0};
    char *t_cstr = NULL;

    _receive_msg(p, &msg);

    // Method
    {
        if (!(_next_token(&msg, &t, &p->_ignore_lf) && t.tk == HTTP_PTK_GENTOK))
            http_return_defer(HTTP_ERR_FAILED_PARSE);
        t_cstr = _token_to_cstr(&t);
        p->method = http_method_from_cstr(t_cstr);
    }

    // Request URI
    // TODO: Parse URI properly
    {
        _skipws(&msg);
        size_t n = 0;
        for (;n < HTTP_PARSER_URI_MAX_LEN + 1 && msg.pos + n < msg.len; n++) {
            if (_iscrlf(msg.items[msg.pos + n]) || _islws(msg.items[msg.pos + n]))
                break;
        }

        if (_iscrlf(msg.items[msg.pos+n])) http_return_defer(HTTP_ERR_FAILED_PARSE);
        if (n == HTTP_PARSER_URI_MAX_LEN) http_return_defer(HTTP_ERR_URI_TOO_LONG);
        strncpy(p->uri.repr, &msg.items[msg.pos], n);
        _adv_n_chars(&msg, n);
    }

    // HTTP Version
    {
        _skipws(&msg);
        int n;
        if ((n = _parse_version(&msg.items[msg.pos], &p->httpver)) == -1)
            http_return_defer(HTTP_ERR_FAILED_PARSE);
        _adv_n_chars(&msg, n);
    }

    _skipws(&msg);
    if (!_iscrlf(msg.items[msg.pos])) http_return_defer(HTTP_ERR_FAILED_PARSE);

 defer:
    _advance_stage(p);
    free(msg.items);
    free(t_cstr);
    return result;
}

HTTP_Err http_parser_status_line(HTTP_Parser *p) {
    if (p->stage != HTTP_PS_START_LINE) return HTTP_ERR_WRONG_STAGE;
    HTTP_UNUSED(p);
    HTTP_TODO("http_parser_status_line");
    _advance_stage(p);
    return HTTP_ERR_NOT_IMPLEMENTED;
}

HTTP_Err http_parser_headers(HTTP_Parser *p) {
    if (p->stage != HTTP_PS_HEADERS) return HTTP_ERR_WRONG_STAGE;
    HTTP_Err result = HTTP_ERR_OK;

    HTTP_Parser_Message msg = {0};
    HTTP_Parser_Token t = {0};

    for (;;) {
        _receive_msg(p, &msg);
        if (_iscrlf(msg.items[msg.pos])) break;

        HTTP_Header h = {0};

        // Field name
        if (!(_next_token(&msg, &t, &p->_ignore_lf) && t.tk == HTTP_PTK_GENTOK))
            http_return_defer(HTTP_ERR_FAILED_PARSE);
        h.k = _token_to_cstr(&t);

        // Expect ":"
        if (!(_next_token(&msg, &t, &p->_ignore_lf) && t.tk == HTTP_PTK_SEP && _token_cmp_cstr(&t, ":")))
            http_return_defer(HTTP_ERR_FAILED_PARSE);

        // Field value
        // TODO: Support multi-line field values
        _skipws(&msg);
        size_t n = 0;
        for (;msg.pos + n < msg.len && !_iscrlf(msg.items[msg.pos + n]); n++);
        h.v = strndup(&msg.items[msg.pos], n);

        if (strcmp(h.k, "Content-Length") == 0 && sscanf(h.v, "%ld", &p->content_length) != 1) {
            HTTP_WARN("Failed to parse Content-Length");
        }

        http_da_append(&p->headers, h);
    }

 defer:
    _advance_stage(p);
    free(msg.items);
    return result;
}

HTTP_Err http_parser_stream_body(HTTP_Parser *p, char *chunk, size_t chunk_sz) {
    if (p->stage != HTTP_PS_BODY) return HTTP_ERR_WRONG_STAGE;
    if (p->content_length == 0) {
        _advance_stage(p);
        return HTTP_ERR_OK;
    }

    if (p->_body_start_pos == -1) p->_body_start_pos = p->_reader->pos;
    size_t body_sz = http_parser_body_size(p);

    HTTP_ASSERT(body_sz <= p->content_length && "Read more than Content-Length");
    size_t body_rest = p->content_length - body_sz;
    size_t to_read = (body_rest < chunk_sz) ? body_rest : chunk_sz;

    HTTP_Err err = _read(p, chunk, to_read);
    if (err != HTTP_ERR_OK) return err;

    body_sz = http_parser_body_size(p);
    HTTP_ASSERT(body_sz <= p->content_length && "Read more than Content-Length");
    if (body_sz == p->content_length) _advance_stage(p);
    return HTTP_ERR_OK;
}
//////////////////// END:   Parser ////////////////////

#  endif // HTTP_PARSER_IMPL_GUARD
#endif // HTTP_PARSER_IMPL

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
