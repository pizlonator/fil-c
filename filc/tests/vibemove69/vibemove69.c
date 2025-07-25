#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test 1-byte copy
    char* src = malloc(16);
    char* dst = malloc(16);
    
    src[0] = 42;
    dst[0] = 0;
    
    zmemmove(dst, src, 1);
    
    ZASSERT(dst[0] == 42);
    zprintf("1-byte memmove succeeded\n");
    
    free(src);
    free(dst);
    return 0;
}