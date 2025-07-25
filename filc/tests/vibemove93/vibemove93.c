#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test 10K word pointer copy - dst never had pointers
    size_t count = 10000;
    void** src = malloc(sizeof(void*) * count);
    char* dst_buf = malloc(sizeof(void*) * count);
    
    // Set up source pointers (only set some to avoid too much allocation)
    for (size_t i = 0; i < count; i += 100) {
        src[i] = malloc(10);
    }
    
    // Initialize dst with non-pointer data
    memset(dst_buf, 42, sizeof(void*) * count);
    
    // Copy pointers
    zmemmove(dst_buf, src, sizeof(void*) * count);
    
    void** dst = (void**)dst_buf;
    
    // Verify a few spots
    for (size_t i = 0; i < count; i += 100) {
        ZASSERT(dst[i] == src[i]);
        if (src[i])
            ZASSERT(zhasvalidcap(dst[i]));
    }
    
    zprintf("10K word pointer copy (dst never had pointers) succeeded\n");
    
    // Clean up
    for (size_t i = 0; i < count; i += 100) {
        if (src[i])
            free(src[i]);
    }
    free(src);
    free(dst_buf);
    return 0;
}