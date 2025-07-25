#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test ~20 word pointer copy - dst previously had pointers
    size_t count = 20;
    void** src = malloc(sizeof(void*) * count);
    void** dst = malloc(sizeof(void*) * count);
    
    // Set up source pointers
    for (size_t i = 0; i < count; i++) {
        src[i] = malloc(10 + i);
    }
    
    // Set up destination pointers
    void* old_dst[20];
    for (size_t i = 0; i < count; i++) {
        dst[i] = malloc(50 + i);
        old_dst[i] = dst[i];
    }
    
    // Copy pointers
    memmove(dst, src, sizeof(void*) * count);
    
    // Verify
    for (size_t i = 0; i < count; i++) {
        ZASSERT(dst[i] == src[i]);
        ZASSERT(zhasvalidcap(dst[i]));
    }
    
    zprintf("20 word pointer copy (dst had pointers) succeeded\n");
    
    // Clean up
    for (size_t i = 0; i < count; i++) {
        free(src[i]);
        free(old_dst[i]);
    }
    free(src);
    free(dst);
    return 0;
}