#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "vendor/nob.h"

#ifndef SRCDIR
#  define SRCDIR "./src/"
#endif // SRCDIR

#ifndef RESULT_OUT
#  define RESULT_OUT "http.test.h"
#endif // RESULT_OUT

typedef struct {
    FILE *fhead;
    FILE *fimpl;
} Bundler;

void create_safe_copy();
void bundler_init(Bundler *b);
void bundler_begin(Bundler *b);
void bundler_end(Bundler *b);
void bundler_append_entry(Bundler *b, const char *header_path, const char *impl_path);
void bundler_bundle(Bundler *b);
void bundler_free(Bundler *b);

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    create_safe_copy();
    
    Bundler b = {0};
    bundler_init(&b);

    bundler_begin(&b);
    
    bundler_end(&b);

    bundler_bundle(&b);
    bundler_free(&b);
    
    return 0;
}

void create_safe_copy() {
    String_Builder src_sb = {0};
    sb_append_cstr(&src_sb, RESULT_OUT);
    sb_append_null(&src_sb);
    
    String_Builder dest_sb = {0};
    sb_append_cstr(&dest_sb, RESULT_OUT);
    sb_append_cstr(&dest_sb, ".old");
    sb_append_null(&dest_sb);

    copy_file(src_sb.items, dest_sb.items);
}

#ifdef __linux__
#  include <unistd.h>
#  include <linux/limits.h>
char *get_filename_from_fp(FILE *f) {
    static char path[PATH_MAX];
    
    char proclnk[PATH_MAX];
    sprintf(proclnk, "/proc/self/fd/%d", fileno(f));
    ssize_t r = readlink(proclnk, path, PATH_MAX);
    if (r < 0)
        return "[Unknown path]";
    
    path[r] = '\0';
    return path;
}
#endif // __linux__
    
void bundler_init(Bundler *b) {
    b->fhead = tmpfile();
    if (b->fhead == NULL) {
        nob_log(ERROR, "Failed to create temporary file: %d (tmpfile)", errno);
        abort();
    }
#ifdef __linux__
    nob_log(INFO, "Created temporary file (head): %s", get_filename_from_fp(b->fhead));
#endif // __linux__

    b->fimpl = tmpfile();
    if (b->fimpl == NULL) {
        nob_log(ERROR, "Failed to create temporary file: %d (tmpfile)", errno);
        abort();
    }
#ifdef __linux__
    nob_log(INFO, "Created temporary file (impl): %s", get_filename_from_fp(b->fimpl));
#endif // __linux__
}

void bundler_begin(Bundler *b) {
    String_View sv = sv_from_cstr(
                                  "#ifndef HTTP_H\n"
                                  "#  define HTTP_H\n"
                                  );
    fwrite(temp_sv_to_cstr(sv), sizeof(char), sv.count, b->fhead);
    sv = sv_from_cstr(
                      "#ifdef HTTP_IMPL\n"
                      "#ifndef HTTP_IMPL_GUARD\n"
                      "#  define HTTP_IMPL_GUARD\n"
                      "#endif // HTTP_IMPL_GUARD\n"
                      );
    fwrite(temp_sv_to_cstr(sv), sizeof(char), sv.count, b->fimpl);
}

void bundler_end(Bundler *b) {
    String_View sv = sv_from_cstr("#endif // HTTP_H\n\n\n");
    fwrite(temp_sv_to_cstr(sv), sizeof(char), sv.count, b->fhead);
    sv = sv_from_cstr("#endif // HTTP_IMPL\n");
    fwrite(temp_sv_to_cstr(sv), sizeof(char), sv.count, b->fimpl);
}

void bundler_bundle(Bundler *b) {
    fflush(b->fhead);
    fflush(b->fimpl);

    FILE *fbundle = fopen(RESULT_OUT, "w+");
    if (fbundle == NULL) {
        nob_log(ERROR, "Failed to create bundled single header file %s: %d (fopen)", RESULT_OUT, errno);
        bundler_free(b);
        abort();
    }
    
    struct stat sthead, stimpl;
    if (fstat(fileno(b->fhead), &sthead) || fstat(fileno(b->fimpl), &stimpl)) {
        nob_log(ERROR, "Failed to read stats: %d (fstat)", errno);
        bundler_free(b);
        abort();
    }

    off_t fbundle_sz = sthead.st_size + stimpl.st_size;
    if (ftruncate(fileno(fbundle), fbundle_sz) == -1) {
        nob_log(ERROR, "Failed to truncate bundle file: %d (ftruncate)", errno);
        fclose(fbundle);
        bundler_free(b);
        abort();
    }
    void *mbundle = mmap(NULL, fbundle_sz, PROT_WRITE, MAP_SHARED, fileno(fbundle), 0);
    if (mbundle == MAP_FAILED) {
        nob_log(ERROR, "Failed to map bundle file into memory: %d (mmap)", errno);
        fclose(fbundle);
        bundler_free(b);
        abort();
    }

    void *mhead = mmap(NULL, sthead.st_size, PROT_READ, MAP_PRIVATE, fileno(b->fhead), 0);
    if (mhead == MAP_FAILED) {
        nob_log(ERROR, "Failed to map temporary header file into memory: %d (mmap)", errno);
        munmap(mbundle, fbundle_sz);
        fclose(fbundle);
        bundler_free(b);
        abort();
    }

    void *mimpl = mmap(NULL, stimpl.st_size, PROT_READ, MAP_PRIVATE, fileno(b->fimpl), 0);
    if (mhead == MAP_FAILED) {
        nob_log(ERROR, "Failed to map temporary header file into memory: %d (mmap)", errno);
        munmap(mbundle, fbundle_sz);
        munmap(mhead, sthead.st_size);
        fclose(fbundle);
        bundler_free(b);
        abort();
    }

    memcpy(mbundle, mhead, sthead.st_size);
    memcpy(mbundle + sizeof(char)*sthead.st_size, mimpl, stimpl.st_size);
    msync(mbundle, fbundle_sz, MS_SYNC);


    // cleanup
    munmap(mbundle, fbundle_sz);
    munmap(mhead, sthead.st_size);
    munmap(mimpl, stimpl.st_size);
    fclose(fbundle);
}

void bundler_free(Bundler *b) {
    fclose(b->fhead);
    fclose(b->fimpl);
}
