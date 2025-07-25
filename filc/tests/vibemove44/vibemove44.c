#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test 10K word pointer copy - dst previously had pointers
    size_t count = 10000;
    void** src = malloc(sizeof(void*) * count);
    void** dst = malloc(sizeof(void*) * count);
    
    // Set up source pointers (only set some to avoid too much allocation)
    for (size_t i = 0; i < count; i += 100) {
        src[i] = malloc(10);
    }
    
    // Set up destination pointers (only some)
    for (size_t i = 50; i < count; i += 100) {
        dst[i] = malloc(20);
    }
    
    // Copy pointers
    memmove(dst, src, sizeof(void*) * count);
    
    // Verify a few spots
    for (size_t i = 0; i < count; i += 100) {
        ZASSERT(dst[i] == src[i]);
        if (src[i])
            ZASSERT(zhasvalidcap(dst[i]));
    }
    
    zprintf("10K word pointer copy (dst had pointers) succeeded\n");
    
    // Clean up
    for (size_t i = 0; i < count; i += 100) {
        if (src[i])
            free(src[i]);
    }
    // Note: dst pointers that were overwritten are leaked but that's ok for a test
    free(src);
    free(dst);
    return 0;
}