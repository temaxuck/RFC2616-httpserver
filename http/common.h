#ifndef HTTP_COMMON_H
#  define HTTP_COMMON_H

#include <string.h>

#define plex struct

#ifndef HTTP_REALLOC
#  define HTTP_REALLOC realloc
#endif // HTTP_REALLOC

#ifndef HTTP_ASSERT
#  include <assert.h>
#  define HTTP_ASSERT assert
#endif // HTTP_ASSERT

#ifndef HTTP_UNUSED
#  define HTTP_UNUSED(...) do { (void)__VA_ARGS__; } while(0)
#endif // HTTP_UNUSED

#ifndef HTTP_METHOD_MAX_LEN
#  define HTTP_METHOD_MAX_LEN 16
#endif // HTTP_METHOD_MAX_LEN

#ifndef HTTP_VERSION_MAX_LEN
#  define HTTP_VERSION_MAX_LEN 16
#endif // HTTP_VERSION_MAX_LEN

typedef plex {
    char *k, *v;
} HTTP_Header;

typedef plex {
    size_t       len, cap;
    HTTP_Header *items;
} HTTP_Headers;

typedef plex {
    unsigned short maj, min;
} HTTP_Version;

// NOTE: For now, only default methods (as specified by RFC 2616) are allowed
#define HTTP_METHOD_MAP(XX)                     \
    XX(0, UNKNOWN, "UNKNOWN")                   \
    XX(1, OPTIONS, "OPTIONS")                   \
    XX(2, GET,     "GET"    )                   \
    XX(3, HEAD,    "HEAD"   )                   \
    XX(4, POST,    "POST"   )                   \
    XX(5, PUT,     "PUT"    )                   \
    XX(6, DELETE,  "DELETE" )                   \
    XX(7, TRACE,   "TRACE"  )                   \
    XX(8, CONNECT, "CONNECT")

typedef enum {
#define XX(num, name, ...) HTTP_Method_##name = num,
    HTTP_METHOD_MAP(XX)
#undef XX
} HTTP_Method;

HTTP_Method http_method_from_cstr(char *text) {
#define XX(num, name, repr) if (strcmp(text, repr) == 0) return num;
    HTTP_METHOD_MAP(XX)
#undef XX
    return HTTP_Method_UNKNOWN;
}

char *http_method_to_cstr(HTTP_Method method) {
#define XX(num, name, repr) if (num == method) return repr;
    HTTP_METHOD_MAP(XX)
#undef XX
    return "UNKNOWN";
}

// NOTE: For now, only default statuses (as specified by RFC 2616) are allowed
#define HTTP_STATUS_MAP(XX)                                             \
    XX(0,   UNKNOWN,                          Unrecognized Status)       \
    XX(100, CONTINUE,                        Continue)                  \
    XX(101, SWITCHING_PROTOCOLS,             Switching Protocols)       \
    XX(200, OK,                              OK)                        \
    XX(201, CREATED,                         Created)                   \
    XX(202, ACCEPTED,                        Accepted)                  \
    XX(203, NON_AUTHORITATIVE_INFORMATION,   Non-Authoritative Information) \
    XX(204, NO_CONTENT,                      No Content)                \
    XX(205, RESET_CONTENT,                   Reset Content)             \
    XX(206, PARTIAL_CONTENT,                 Partial Content)           \
    XX(300, MULTIPLE_CHOICES,                Multiple Choices)          \
    XX(301, MOVED_PERMANENTLY,               Moved Permanently)         \
    XX(302, FOUND,                           Found)                     \
    XX(303, SEE_OTHER,                       See Other)                 \
    XX(304, NOT_MODIFIED,                    Not Modified)              \
    XX(305, USE_PROXY,                       Use Proxy)                 \
    XX(307, TEMPORARY_REDIRECT,              Temporary Redirect)        \
    XX(400, BAD_REQUEST,                     Bad Request)               \
    XX(401, UNAUTHORIZED,                    Unauthorized)              \
    XX(402, PAYMENT_REQUIRED,                Payment Required)          \
    XX(403, FORBIDDEN,                       Forbidden)                 \
    XX(404, NOT_FOUND,                       Not Found)                 \
    XX(405, METHOD_NOT_ALLOWED,              Method Not Allowed)        \
    XX(406, NOT_ACCEPTABLE,                  Not Acceptable)            \
    XX(407, PROXY_AUTHENTICATION_REQUIRED,   Proxy Authentication Required) \
    XX(408, REQUEST_TIMEOUT,                 Request Timeout)           \
    XX(409, CONFLICT,                        Conflict)                  \
    XX(410, GONE,                            Gone)                      \
    XX(411, LENGTH_REQUIRED,                 Length Required)           \
    XX(412, PRECONDITION_FAILED,             Precondition Failed)       \
    XX(413, PAYLOAD_TOO_LARGE,               Payload Too Large)         \
    XX(414, URI_TOO_LONG,                    URI Too Long)              \
    XX(415, UNSUPPORTED_MEDIA_TYPE,          Unsupported Media Type)    \
    XX(416, RANGE_NOT_SATISFIABLE,           Range Not Satisfiable)     \
    XX(417, EXPECTATION_FAILED,              Expectation Failed)        \
    XX(500, INTERNAL_SERVER_ERROR,           Internal Server Error)     \
    XX(501, NOT_IMPLEMENTED,                 Not Implemented)           \
    XX(502, BAD_GATEWAY,                     Bad Gateway)               \
    XX(503, SERVICE_UNAVAILABLE,             Service Unavailable)       \
    XX(504, GATEWAY_TIMEOUT,                 Gateway Timeout)           \
    XX(505, HTTP_VERSION_NOT_SUPPORTED,      HTTP Version Not Supported)

typedef enum {
#define XX(num, name, ...) HTTP_Status_##name = num,
    HTTP_STATUS_MAP(XX)
#undef XX
} HTTP_Status;
#endif // HTTP_COMMON_H

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
