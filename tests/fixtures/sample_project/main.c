#include "util.h"
#include <stdio.h>

static void local_func(void) {
    helper();
}

int main(int argc, char **argv) {
    int x = add(1, 2);
    local_func();
    printf("%d\n", x);
    return 0;
}
