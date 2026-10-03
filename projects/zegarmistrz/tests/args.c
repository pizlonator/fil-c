// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: argc/argv passing and exit code.
#include <stdio.h>

int main(int argc, char** argv) {
    for (int i = 0; i < argc; i++)
        printf("argv[%d]=%s\n", i, argv[i]);
    return 42;
}
