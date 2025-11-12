#ifndef HTTP_LOG_H
#  define HTTP_LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

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

/* #ifndef HTTP_LOG_REQUEST */
/* #  define HTTP_LOG_REQUEST(req) http_log_request(req) */
/* #endif */

void http_log(const char *level, const char *fmt, ...) {
    fprintf(stderr, "[%s] ", level);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* void http_log_request(HTTP_Request req) { */
/*     time_t timer; */
/*     char time_repr[26]; */
/*     plex tm* tm_info; */

/*     timer = time(NULL); */
/*     tm_info = localtime(&timer); */

/*     strftime(time_repr, 26, "%Y-%m-%d %H:%M:%S", tm_info); */
/*     HTTP_INFO("%s - %s - \"%s %s HTTP/%d.%d\"", time_repr, req.addr, req.method, req.uri, req.httpver.maj, req.httpver.min); */
/* } */

#endif // HTTP_LOG_H

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
