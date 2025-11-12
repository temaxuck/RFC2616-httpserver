/*
 * url.h - URL parser.
 *
 * TODO: Do not store each url component as a string, instead, use fields that
 *       represent each component's offset and length relative to original URL
 *       string.
 */
#ifndef HTTP_URL_H
#  define HTTP_URL_H

#include "common.h"
#include "err.h"

typedef plex {
    char *scheme;
    char *userinfo;
    char *host;
    char *port;
    char *path;
    char *query;
    char *fragment;
} HTTP_URL;

HTTP_Err http_url_parse(HTTP_URL *url, char *s, size_t slen);
HTTP_Err http_url_free(HTTP_URL *url);

#endif // HTTP_URL_H
#ifdef HTTP_URL_IMPL
#  ifndef HTTP_URL_IMPL_GUARD

#include <ctype.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <string.h>

typedef enum {
    HTTP_UPS_SCHEME,
    HTTP_UPS_HIERPART,
    HTTP_UPS_HOST,
    HTTP_UPS_PORT,
    HTTP_UPS_PATH,
    HTTP_UPS_QUERY,
    HTTP_UPS_FRAGMENT,
    HTTP_UPS_MAX,
} HTTP_URL_PARSE_STAGE;

static bool _ishexdig(char c) {
    return isdigit(c) || (c >= 65 && c <= 70) || (c >= 97 && c <= 102);
}

static bool _issubdelim(char c) {
    return c == '!' || c == '$' || c == '&' || c == '\'' ||
           c == '(' || c == ')' || c == '*' || c == '+'  ||
           c == ',' || c == ';' || c== '=';
}

static bool _parse_scheme(char *s, size_t slen, size_t *off, size_t *len) {
    *off = 0;
    *len = 0; 
    if (slen == 0) return false;
    size_t pos = 0;

    if (isalpha(*s)) {
        pos++;
        while (pos < slen && (isalnum(s[pos]) || s[pos] == '+' || s[pos] == '-'   || s[pos] == '.')) pos++;
    }

    if (pos < slen && s[pos] == ':') {
        *len = pos;
        return true;
    }

    return false;
}

static bool _parse_userinfo(char *s, size_t slen, size_t *off, size_t *len) {
    *off = 0;
    *len = 0; 
    if (slen == 0) return false;
    size_t pos = 0;

    while (pos < slen) {
        // unreserved
        if (isalnum(s[pos]) || s[pos] == '-' ||
            s[pos] == '.'   || s[pos] == '_' || s[pos] == '~') pos++;
        // pct-encoded
        else if (s[pos] == '%' && pos + 2 < slen &&
                 _ishexdig(s[pos+1]) && _ishexdig(s[pos+2])) pos += 3;
        // sub-delims
        else if (_issubdelim(s[pos])) pos++;
        else if (_issubdelim(s[pos]) || s[pos] == ':') pos++;
        else break;
    }

    if (pos < slen && s[pos] == '@') {
        *len = pos;
        return true;
    }
    
    return false;
}

static bool _is_ipv4(char *s, size_t slen) {
    char buf[INET_ADDRSTRLEN];
    slen = (slen < INET_ADDRSTRLEN) ? slen : INET_ADDRSTRLEN;
    memcpy(buf, s, slen);
    buf[slen-1] = '\0';

    struct in_addr tmp4;
    return inet_pton(AF_INET, buf, &tmp4) != -1;
}

static bool _is_ipv6(char *s, size_t slen) {
    char buf[INET6_ADDRSTRLEN];
    slen = (slen < INET6_ADDRSTRLEN) ? slen : INET6_ADDRSTRLEN;
    memcpy(buf, s, slen);
    buf[slen-1] = '\0';

    struct in6_addr tmp6;
    return inet_pton(AF_INET6, buf, &tmp6) != -1;
}

static bool _parse_ipv4(char *s, size_t slen, size_t *off, size_t *len) {
    *off = 0;
    *len = 0; 
    if (slen == 0) return false;
    size_t pos = 0;

    while (pos < slen && (isdigit(s[pos]) || s[pos] == '.')) pos++;

    if (pos == 0) return false;
    if (!_is_ipv4(s, pos)) return false;
    
    *len = pos;
    return true;

}

static bool _parse_ipv6(char *s, size_t slen, size_t *off, size_t *len) {
    *off = 0;
    *len = 0; 
    if (slen == 0) return false;
    size_t pos = 0;

    while (pos < slen && s[pos] != ']') pos++;

    if (pos == 0 || pos >= slen) return false;
    if (!_is_ipv6(s, pos)) return false;
    
    *len = pos;
    return true;
}

static bool _parse_regname(char *s, size_t slen, size_t *off, size_t *len) {
    *off = 0;
    *len = 0; 
    size_t pos = 0;

    while (pos < slen) {
        // unreserved
        if (isalnum(s[pos]) || s[pos] == '-' ||
            s[pos] == '.'   || s[pos] == '_' || s[pos] == '~') pos++;
        // pct-encoded
        else if (s[pos] == '%' && pos + 2 < slen &&
                 _ishexdig(s[pos+1]) && _ishexdig(s[pos+2])) pos += 3;
        // sub-delims
        else if (_issubdelim(s[pos])) pos++;
        else break;
    }

    *len = pos;
    return true;
}

// TODO: Validate port
static bool _parse_port(char *s, size_t slen, size_t *off, size_t *len) {
    if (slen == 0 || *s != ':') return false;
    *off = 1;
    *len = 0; 
    s++; slen--;
    
    size_t pos = 0;
    while (pos < slen && isdigit(s[pos])) pos++;

    *len = pos;
    return true;
}

// TODO: Maybe accept `path_kind` which can be path-abempty, path-absolute,
//       path-noscheme, path-rootless or path-empty
static bool _parse_path(char *s, size_t slen, size_t *off, size_t *len) {
    *off = 0;
    *len = 0;
    
    size_t pos = 0;
    while (pos < slen) {
        // unreserved
        if (isalnum(s[pos]) || s[pos] == '-' ||
            s[pos] == '.'   || s[pos] == '_' || s[pos] == '~') pos++;
        // pct-encoded
        else if (s[pos] == '%' && pos + 2 < slen &&
                 _ishexdig(s[pos+1]) && _ishexdig(s[pos+2])) pos += 3;
        // sub-delims
        else if (_issubdelim(s[pos])) pos++;
        else if (s[pos] == ':' || s[pos] == '@') pos++;
        else if (s[pos] == '/') pos++;
        else break;
    }

    *len = pos;
    return true;
}

static bool _parse_query_fragment(char *s, size_t slen, size_t *off, size_t *len) {
    if (slen == 0 || (*s != '?' && *s != '#')) return false;
    *off = 1;
    *len = 0; 
    s++; slen--;
    
    size_t pos = 0;
    while (pos < slen) {
        // unreserved
        if (isalnum(s[pos]) || s[pos] == '-' ||
            s[pos] == '.'   || s[pos] == '_' || s[pos] == '~') pos++;
        // pct-encoded
        else if (s[pos] == '%' && pos + 2 < slen &&
                 _ishexdig(s[pos+1]) && _ishexdig(s[pos+2])) pos += 3;
        // sub-delims
        else if (_issubdelim(s[pos])) pos++;
        else if (s[pos] == ':' || s[pos] == '@') pos++;
        else if (s[pos] == '/' || s[pos] == '?') pos++;
        else break;
    }
    *len = pos;
    return true;
}

static HTTP_Err _parse_url(HTTP_URL *url, char *s, size_t slen, HTTP_URL_PARSE_STAGE ups) {
    if (slen == 0) return HTTP_ERR_OK;
    size_t off = 0, len = 0;

    switch (ups) {
    case HTTP_UPS_SCHEME: {
        // absolute-URI
        if (_parse_scheme(s, slen, &off, &len)) {
            size_t pos = off + len;
            HTTP_ASSERT(pos < slen && s[pos] == ':' && "_parse_url - _parse_scheme");
            url->scheme = strndup(s + off, len);
            if (url->scheme == NULL) return HTTP_ERR_OOM;
            return _parse_url(url, s + pos + 1, slen - pos - 1, HTTP_UPS_HIERPART);
        }
        // relative
        return _parse_url(url, s, slen,  HTTP_UPS_HIERPART);
    }
    case HTTP_UPS_HIERPART: {
        // authority
        if (slen > 2 && s[0] == '/' && s[1] == '/') {
            s = &s[2]; slen -= 2;
            if (_parse_userinfo(s, slen, &off, &len)) {
                    size_t pos = off + len;
                    HTTP_ASSERT(pos < slen && s[pos] == '@' && "_parse_url - _parse_userinfo");
                    url->userinfo = strndup(s + off, len);
                    if (url->userinfo == NULL) return HTTP_ERR_OOM;
                    return _parse_url(url, s + pos + 1, slen - pos - 1, HTTP_UPS_HOST);
                }
                return _parse_url(url, s, slen, HTTP_UPS_HOST);
        }
        // path-absolute or path-noscheme
        return _parse_url(url, s, slen, HTTP_UPS_PATH);
    }
    // TODO: IPvFuture is not handled
    case HTTP_UPS_HOST: {
        // IPv6
        if (*s == '[') {
            s++; slen--;
            if (!_parse_ipv6(s, slen, &off, &len)) return HTTP_ERR_FAILED_PARSE;
            size_t pos = off + len;
            HTTP_ASSERT(pos < slen && s[pos] == ']' && "_parse_url - _parse_ipv6");
            url->host = strndup(s + off, len);
            if (url->host == NULL) return HTTP_ERR_OOM;
            return _parse_url(url, s + pos + 1, slen - pos - 1, HTTP_UPS_PORT);
        }
        if (_parse_ipv4(s, slen, &off, &len)) {
            url->host = strndup(s + off, len);
            if (url->host == NULL) return HTTP_ERR_OOM;
            return _parse_url(url, s + off + len, slen - off - len, HTTP_UPS_PORT);
        }
        if (_parse_regname(s, slen, &off, &len)) {
            url->host = strndup(s + off, len);
            if (url->host == NULL) return HTTP_ERR_OOM;
            return _parse_url(url, s + off + len, slen - off - len, HTTP_UPS_PORT);
        }
        return _parse_url(url, s, slen, HTTP_UPS_PATH);
    }
    case HTTP_UPS_PORT: {
        if (_parse_port(s, slen, &off, &len)) {
            url->port = strndup(s + off, len);
            if (url->port == NULL) return HTTP_ERR_OOM;
        }
        return _parse_url(url, s + off + len, slen - off - len, HTTP_UPS_PATH);
    }
    // TODO: Check if we should parse path-abempty, path-absolute,
    //       path-noscheme, path-rootless or path-empty
    case HTTP_UPS_PATH: {
        if (_parse_path(s, slen, &off, &len)) {
            url->path = strndup(s + off, len);
            if (url->path == NULL) return HTTP_ERR_OOM;
        }
        return _parse_url(url, s + off + len, slen - off - len, HTTP_UPS_QUERY);
    }
    case HTTP_UPS_QUERY: {
        if (*s == '?' && _parse_query_fragment(s, slen, &off, &len)) {
            url->query = strndup(s + off, len);
            if (url->query == NULL) return HTTP_ERR_OOM;
        }
        return _parse_url(url, s + off + len, slen - off - len, HTTP_UPS_FRAGMENT);
    }
    case HTTP_UPS_FRAGMENT: {
        if (_parse_query_fragment(s, slen, &off, &len)) {
            url->fragment = strndup(s + off, len);
            if (url->fragment == NULL) return HTTP_ERR_OOM;
        }
        return _parse_url(url, s + off + len, slen - off - len, HTTP_UPS_MAX);
    }
    default: {
        HTTP_WARN("Unparsed url part: %.*s", slen, s);
        return HTTP_ERR_OK;
    }}

    HTTP_ASSERT(false && "Unreachable");
}

HTTP_Err http_url_parse(HTTP_URL *url, char *s, size_t slen) {
    return _parse_url(url, s, slen, HTTP_UPS_SCHEME);
}

HTTP_Err http_url_free(HTTP_URL *url) {
    if(url->scheme) free(url->scheme);
    if(url->host) free(url->host);
    if(url->port) free(url->port);
    if(url->path) free(url->path);
    if(url->query) free(url->query);
    if(url->fragment) free(url->fragment);
    if(url->userinfo) free(url->userinfo);

    return HTTP_ERR_OK;
}
#    define HTTP_URL_IMPL_GUARD
#  endif // HTTP_URL_IMPL_GUARD
#endif // HTTP_URL_IMPL

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
