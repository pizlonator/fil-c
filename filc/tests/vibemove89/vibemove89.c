#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test single word (8 bytes) pointer copy - dst never had pointers
    void** src = malloc(sizeof(void*));
    char* dst_buf = malloc(sizeof(void*));
    
    // Set up source pointer
    src[0] = malloc(100);
    strcpy(src[0], "test data");
    
    // Initialize dst with non-pointer data
    for (size_t i = 0; i < sizeof(void*); i++)
        dst_buf[i] = (char)(i + 42);
    
    // Copy single pointer
    zmemmove(dst_buf, src, sizeof(void*));
    
    void** dst = (void**)dst_buf;
    
    // Verify
    ZASSERT(dst[0] == src[0]);
    ZASSERT(zhasvalidcap(dst[0]));
    ZASSERT(strcmp(dst[0], "test data") == 0);
    
    zprintf("Single word pointer copy (dst never had pointers) succeeded\n");
    
    free(src[0]);
    free(src);
    free(dst_buf);
    return 0;
}