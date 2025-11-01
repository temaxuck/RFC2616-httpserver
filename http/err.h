#ifndef HTTP_ERR_H
#define HTTP_ERR_H

#include "common.h"

#define HTTP_ERR_MAP(XX)                                                \
    /* General errors: */                                               \
    XX(0, OK,          "OK")                                            \
    XX(1, BAD_SOCK,    "Bad socket")                                    \
    XX(2, FAILED_SOCK, "Failed to create socket")                       \
    XX(3, BAD_ADDR,    "Bad address")                                   \
    XX(4, ADDR_IN_USE, "Address already in use")                        \
    XX(5, OOM,         "Out of memory")                                 \
    XX(6, OOB,         "Out of bounds")                                 \
    /* IO errors */                                                     \
    XX(7, FAILED_READ, "Failed to read from socket")                    \
    XX(8, EOF,         "Tried to read from consumed connection")        \
    /* Parser errors: */                                                \
    XX(9, FAILED_PARSE, "Failed to parse HTTP Message")                 \
    XX(10, URL_TOO_LONG, "Encountered too long URL")                    \
    XX(11, WRONG_STAGE, "Tried to parse message with parser being at wrong stage") \
    /* Implementation errors */                                         \
    XX(12, NOT_IMPLEMENTED, "Feature not implemented yet")

typedef enum {
#define XX(num, name, ...) HTTP_ERR_##name = num,
    HTTP_ERR_MAP(XX)
#undef XX
} HTTP_Err;

char *http_err_to_cstr(HTTP_Err err) {
#define XX(num, name, repr) if (err == num) return repr;
    HTTP_ERR_MAP(XX)
#undef XX
    HTTP_ASSERT(0 && "Unreachable");
}

#endif // HTTP_ERR_H

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
