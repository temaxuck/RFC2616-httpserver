#ifndef HTTP_DA_H
#  define HTTP_DA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

#ifndef HTTP_REALLOC
#  define HTTP_REALLOC realloc
#endif // HTTP_REALLOC

#ifndef HTTP_ASSERT
#  include <assert.h>
#  define HTTP_ASSERT assert
#endif // HTTP_ASSERT

#define HTTP_DA_INIT_CAP 16

#define http_da_reserve(da, expected_cap) do {                          \
        if ((da)->cap >= (expected_cap)) break;                         \
        if ((da)->cap == 0) (da)->cap = HTTP_DA_INIT_CAP;               \
        while ((da)->cap < (expected_cap)) (da)->cap *= 2;              \
        if ((da)->cap == 0) (da)->cap = HTTP_DA_INIT_CAP;               \
        (da)->items = HTTP_REALLOC((da)->items, (da)->cap * sizeof(*(da)->items)); \
        HTTP_ASSERT((da)->items != NULL && "Failed to allocate memory for a dynamic array"); \
    } while (0)

#define http_da_append(da, item) do {           \
        http_da_reserve((da), (da)->len+1);          \
        (da)->items[(da)->len++] = item;        \
    } while (0)

#define http_da_append_carr(da, carr, carr_len) do {                    \
        if (carr_len <= 0) break;                                       \
        http_da_reserve((da), (da)->len + (carr_len));                  \
        memcpy((da)->items + (da)->len, (carr), sizeof(*(carr)) * (carr_len)); \
        (da)->len += (carr_len);                                        \
    } while (0)
#define http_da_reset(da) ({(da)->len = 0;})
#define http_da_free(da) ({free((da)->items);})

#define http_sb_append_cstr(sb, cstr) http_da_append_carr((sb), (cstr), strlen(cstr))
#define http_sb_append_substr(sb, cstr, lb, rb) do {                    \
        HTTP_ASSERT((rb) < strlen(cstr) && "Trying to append out of bounds part of string"); \
        http_da_append_carr((sb), (cstr) + (lb), ((rb) - (lb)));    \
    } while (0)
#define http_sb_append_char(sb, ch) http_da_append((sb), (ch))
#define http_sb_free(sb) http_da_free(sb)

typedef plex {
    size_t len, cap;
    char *items;
} HTTP_StringBuilder;

#endif // HTTP_DA_H
