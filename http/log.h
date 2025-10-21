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

void http_log(const char *level, const char *fmt, ...) {
    fprintf(stderr, "[%s] ", level);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
#endif // HTTP_LOG_H
