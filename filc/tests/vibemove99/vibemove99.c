#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test very large copy (100KB) with memmove
    size_t size = 100 * 1024;
    char* src = malloc(size);
    char* dst = malloc(size);
    
    // Initialize some spots
    src[0] = 42;
    src[size/2] = 43;
    src[size-1] = 44;
    
    // Large copy with memmove
    memmove(dst, src, size);
    
    // Verify
    ZASSERT(dst[0] == 42);
    ZASSERT(dst[size/2] == 43);
    ZASSERT(dst[size-1] == 44);
    
    zprintf("Very large 100KB memmove succeeded\n");
    
    free(src);
    free(dst);
    return 0;
}