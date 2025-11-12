#ifndef HTTP_PATH_H
#  define HTTP_PATH_H

#ifndef WILDCARD_CSTR
#  define WILDCARD_CSTR "*"
#endif // WILDCARD_CSTR

typedef struct http_pc_s HTTP_PathComponents;

struct http_pc_s {
    char *value;
    struct http_pc_s *next;

    ssize_t wc_idx;
};

typedef struct {
    /* stats */
    size_t wc_count;      // wildcards count
    size_t hc_count;      // hard components count

    struct http_pc_s *pc; // path components
} HTTP_PathPattern;

/**
 * Initializes path components `pc` from path string representation `path`.
 */
HTTP_Err http_pc_init(HTTP_PathComponents **pc, const char *path);

/**
 * Frees path components `pc`.
 */
HTTP_Err http_pc_free(HTTP_PathComponents *pc);

/**
 * Initializes `pattern` from it's string representation `s`.
 */
HTTP_Err http_pattern_init(HTTP_PathPattern *pattern, const char *s);

/**
 * Frees `pattern`.
 */
HTTP_Err http_pattern_free(HTTP_PathPattern *pattern);

/**
 * Matches `path` to the most relevant pattern in an array of `patterns` of
 * size `patterns_len`.
 *
 * Returns index to the most relevant of `patterns`, or -1 if none matched the
 * `path`.
 *
 * The most relevant pattern is selected by this criteria:
 * 1. `path` matches pattern;
 * 2. pattern has more "path components" than others;
 * 3. if two patterns are both matched by `path` and have equal amount of
 *    "path components", than the one with the highest number of "hard
 *    components" ("hard component" is a non-wildcard component) wins.
 */
ssize_t http_pc_match_patterns(HTTP_PathPattern *patterns, size_t patterns_len, HTTP_PathComponents *path);

/* const char *pathvar(HTTP_PathPattern pattern, char *path, size_t index); */

#endif // HTTP_PATH_H

#ifdef HTTP_PATH_IMPL
#  ifndef HTTP_PATH_IMPL_GUARD
#    define HTTP_PATH_IMPL_GUARD

static bool _pc_parse(const char *path, size_t path_len, HTTP_PathComponents **pc, bool is_root, ssize_t wc_idx) {
    if (path_len > 0 && *path == '/') {
        path++;
        path_len--;
    }

    if (path_len == 0) {
        if (is_root) {
            *pc = malloc(sizeof(HTTP_PathComponents));
            if (*pc == NULL) return false;
            (*pc)->value = strdup("");
            (*pc)->next = NULL;
            (*pc)->wc_idx = -1;
        } else {
            *pc = NULL;
        }
        return true;
    }

    *pc = malloc(sizeof(HTTP_PathComponents));
    if (*pc == NULL) return false;

    size_t pos = 0;
    while (pos < path_len && path[pos] != '/') pos++;

    (*pc)->value = strndup(path, pos);
    if (strcmp((*pc)->value, WILDCARD_CSTR) == 0) (*pc)->wc_idx = ++wc_idx;
    else (*pc)->wc_idx = -1;

    HTTP_ASSERT(path_len >= pos);
    bool res = _pc_parse(path + pos, path_len - pos, &((*pc)->next), false, wc_idx);
    if (!res) {
        free((*pc)->value);
        free(*pc);

        return false;
    }

    return true;
}

static bool _pc_match(HTTP_PathComponents *pattern, HTTP_PathComponents *path) {
    if (pattern == NULL && path == NULL) return true;
    if (pattern == NULL || path == NULL) return false;

    path->wc_idx = -1;
    if (strcmp(pattern->value, path->value) == 0)
        if (_pc_match(pattern->next, path->next)) return true;

    if (pattern->wc_idx >= 0) {
        if (_pc_match(pattern->next, path->next) || _pc_match(pattern, path->next)) {
            path->wc_idx = pattern->wc_idx;
            return true;
        }
    }

    return false;
}

HTTP_Err http_pc_init(HTTP_PathComponents **pc, const char *path) {
    if (!_pc_parse(path, strlen(path), pc, true, -1))
        return HTTP_ERR_FAILED_PARSE;
    return HTTP_ERR_OK;
}

HTTP_Err http_pc_free(HTTP_PathComponents *pc) {
    while (pc != NULL) {
        HTTP_PathComponents *next = pc->next;
        free(pc->value);
        free(pc);
        pc = next;
    }
    return HTTP_ERR_OK;
}

HTTP_Err http_pattern_init(HTTP_PathPattern *pattern, const char *s) {
    memset(pattern, 0, sizeof(HTTP_PathPattern));
    HTTP_Err err;
    if ((err = http_pc_init(&pattern->pc, s))) return err;

    HTTP_PathComponents *cur = pattern->pc;
    while (cur != NULL) {
        if (strcmp(cur->value, WILDCARD_CSTR) == 0) pattern->wc_count++;
        else pattern->hc_count++;

        cur = cur->next;
    }

    return HTTP_ERR_OK;
}

HTTP_Err http_pattern_free(HTTP_PathPattern *pattern) {
    http_pc_free(pattern->pc);
    return HTTP_ERR_OK;
}

ssize_t http_match_patterns(HTTP_PathPattern *patterns, size_t patterns_len, HTTP_PathComponents *path) {
    ssize_t res = -1;

    for (size_t i = 0; i < patterns_len; i++) {
        HTTP_PathPattern *cur = &patterns[i];
        if (_pc_match(cur->pc, path)) {
            if (res == -1) {
                res = i;
                continue;
            }
            size_t cur_pc_count = cur->wc_count + cur->hc_count;
            size_t res_pc_count = patterns[res].wc_count + patterns[res].hc_count;
            if (cur_pc_count > res_pc_count ||
                (cur_pc_count == res_pc_count && cur->wc_count < patterns[res].wc_count)) res = i;
        }
    }

    return res;
}

#  endif // HTTP_PATH_IMPL_GUARD
#endif // HTTP_PATH_IMPL

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
