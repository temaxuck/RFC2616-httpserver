#ifndef HTTP_ERR_H
#define HTTP_ERR_H

#define HTTP_ERR_MAP(XX)                                            \
    /* General errors: */                                           \
    XX(0, OK,          "OK")                                        \
    XX(1, BAD_SOCK,    "Bad socket (probably closed)")              \
    XX(2, FAILED_SOCK, "Failed to create socket")                   \
    XX(3, ADDR_IN_USE, "Address already in use")                    \
    XX(4, OOM,         "Out of memory")                             \
    XX(5, OOB,         "Out of bounds")                             \
    /* IO errors */                                                 \
    XX(6, FAILED_READ, "Failed to read from socket")                \
    XX(7, EOF,         "Tried to read from consumed connection")    \
    /* Parser errors: */                                            \
    XX(8, FAILED_PARSE, "Failed to parse HTTP Message")             \
    XX(9, URI_TOO_LONG, "Encountered too long URI")                 \
    XX(10, WRONG_STAGE, "Tried to parse message with parser being at wrong stage")\
    /* Implementation errors */                                     \
    XX(10, NOT_IMPLEMENTED, "Feature not implemented yet")

typedef enum {
#define XX(num, name, ...) HTTP_ERR_##name = num,
    HTTP_ERR_MAP(XX)
#undef XX
} HTTP_Err;

#ifndef HTTP_ASSERT
#  include <assert.h>
#  define HTTP_ASSERT assert
#endif

char *http_err_to_cstr(HTTP_Err err) {
#define XX(num, name, repr) if (err == num) return repr;
    HTTP_ERR_MAP(XX)
#undef XX
    HTTP_ASSERT(0 && "Unreachable");
}

#endif // HTTP_ERR_H
