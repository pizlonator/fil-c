#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test single word (8 bytes) pointer copy - dst previously had pointers
    void** src = malloc(sizeof(void*));
    void** dst = malloc(sizeof(void*));
    
    // Set up source pointer
    src[0] = malloc(100);
    strcpy(src[0], "test data");
    
    // Set up destination pointer (will be overwritten)
    dst[0] = malloc(200);
    void* old_dst = dst[0];
    
    // Copy single pointer
    zmemmove_union(dst, src, sizeof(void*));
    
    // Verify
    ZASSERT(dst[0] == src[0]);
    ZASSERT(zhasvalidcap(dst[0]));
    ZASSERT(strcmp(dst[0], "test data") == 0);
    
    zprintf("Single word pointer copy (dst had pointers) succeeded\n");
    
    free(src[0]);
    free(old_dst);
    free(src);
    free(dst);
    return 0;
}