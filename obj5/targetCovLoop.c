#include <stdio.h>
#include <string.h>

/* Order the permitted characters from 0 through 61. */
static int alnum_index(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + c - 'A';
    if (c >= 'a' && c <= 'z') return 36 + c - 'a';
    return -1;
}

int eval(const char *input) {
    if (strlen(input) < 3) return 1;

    int first = alnum_index((unsigned char)input[0]);
    int middle = alnum_index((unsigned char)input[1]);
    int last = alnum_index((unsigned char)input[2]);

    if (input[0] == 'S') {
        /* '0' takes 61 trips; '1' takes 60; ...; 'z' takes none. */
        unsigned int limit = 61u - (unsigned int)middle;
        volatile unsigned int trips = 0;
        for (unsigned int i = 0; i < limit; ++i) {
            ++trips;
        }

        if (trips == 61u && input[2] == 's') {
            __builtin_trap();
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("ERROR: must call the target with exactly one string command line argument.\n");
        return 1;
    }

    return eval(argv[1]);
}
