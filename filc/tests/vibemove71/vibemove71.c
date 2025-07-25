#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test large copy (10KB)
    size_t size = 10 * 1024;
    char* src = malloc(size);
    char* dst = malloc(size);
    
    // Initialize source
    for (size_t i = 0; i < size; i++)
        src[i] = (char)(i % 256);
    
    // Do the large copy
    zmemmove(dst, src, size);
    
    // Verify a few spots
    ZASSERT(dst[0] == src[0]);
    ZASSERT(dst[size/2] == src[size/2]);
    ZASSERT(dst[size-1] == src[size-1]);
    
    zprintf("Large 10KB memmove succeeded\n");
    
    free(src);
    free(dst);
    return 0;
}