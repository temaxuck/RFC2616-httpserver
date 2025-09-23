#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "vendor/nob.h"

#ifndef CC
#  define CC "gcc"
#endif // CC

#ifndef EXAMPLE_INFILE
#  define EXAMPLE_INFILE "example.c"
#endif // EXAMPLE_INFILE

#ifndef EXAMPLE_OUTFILE
#  define EXAMPLE_OUTFILE "example"
#endif // EXAMPLE_OUTFILE


int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Cmd cmd = {0};
    cmd_append(&cmd, CC);
    cmd_append(&cmd, "-o", EXAMPLE_OUTFILE);
    cmd_append(&cmd, "-Wall");
    cmd_append(&cmd, EXAMPLE_INFILE);

    if (!cmd_run(&cmd)) return 1;

    return 0;
}
