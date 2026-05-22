#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: cgraph <command> [options]\n");
        return 1;
    }
    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    return 1;
}
