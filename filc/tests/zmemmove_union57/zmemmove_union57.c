#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test ~20 word pointer copy - dst never had pointers
    size_t count = 20;
    void** src = malloc(sizeof(void*) * count);
    char* dst_buf = malloc(sizeof(void*) * count);
    
    // Set up source pointers
    for (size_t i = 0; i < count; i++) {
        src[i] = malloc(10 + i);
    }
    
    // Initialize dst with non-pointer data
    for (size_t i = 0; i < sizeof(void*) * count; i++)
        dst_buf[i] = (char)(i % 256);
    
    // Copy pointers
    zmemmove_union(dst_buf, src, sizeof(void*) * count);
    
    void** dst = (void**)dst_buf;
    
    // Verify
    for (size_t i = 0; i < count; i++) {
        ZASSERT(dst[i] == src[i]);
        ZASSERT(zhasvalidcap(dst[i]));
    }
    
    zprintf("20 word pointer copy (dst never had pointers) succeeded\n");
    
    // Clean up
    for (size_t i = 0; i < count; i++) {
        free(src[i]);
    }
    free(src);
    free(dst_buf);
    return 0;
}