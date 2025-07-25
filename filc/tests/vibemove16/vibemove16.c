#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test memmove to read-only memory (compiler may optimize)
    const char* read_only = "hello world";
    char* src = malloc(16);
    
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    memmove((void*)read_only, src, 5);
    
    zprintf("read_only = %s\n", read_only);
    
    return 0;
}