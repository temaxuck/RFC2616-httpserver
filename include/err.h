#ifndef HTTP_ERR_H
#define HTTP_ERR_H

#include "common.h"

#define HTTP_ERR_MAP(XX)                                                \
    /* General errors: */                                               \
    XX(0,  OK,          "OK")                                           \
    XX(-1, BAD_SOCK,    "Bad socket")                                   \
    XX(-2, FAILED_SOCK, "Failed to create socket")                      \
    XX(-3, BAD_ADDR,    "Bad address")                                  \
    XX(-4, ADDR_IN_USE, "Address already in use")                       \
    XX(-5, OOM,         "Out of memory")                                \
    XX(-6, OOB,         "Out of bounds")                                \
    /* IO errors */                                                     \
    XX(-7,  FAILED_READ,  "Failed to read from a socket")               \
    XX(-8,  FAILED_WRITE, "Failed to write to a socket")                \
    XX(-9,  EOF,          "Tried to read from consumed connection")     \
    XX(-10, CONT,         "Continue reading/writing")                   \
    /* Parser errors: */                                                \
    XX(-11, URL_TOO_LONG, "Encountered too long URL")                   \
    XX(-12, WRONG_STAGE,  "Tried to parse message with parser being at wrong stage") \
    XX(-13, FAILED_PARSE, "Failed to parse HTTP Message")               \
    /* Other errors */                                                  \
    XX(-14, NOT_IMPLEMENTED, "Feature not implemented yet")

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

#include "io.h"

// TODO: I know this is awful. Implement wrapper around io_* API.
HTTP_Err io_err_to_http_err(IO_Err err) {
    if (err == IO_ERR_OK) return HTTP_ERR_OK;
    if (err == IO_ERR_OOM) return HTTP_ERR_OOM;
    if (err == IO_ERR_OOB) return HTTP_ERR_OOB;
    if (err == IO_ERR_EOF) return HTTP_ERR_EOF;
    if (err == IO_ERR_PARTIAL) return HTTP_ERR_OK;
    if (err == IO_ERR_FAILED_READ) return HTTP_ERR_FAILED_READ;

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
