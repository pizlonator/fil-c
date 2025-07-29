#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test sub-word copies (2-7 bytes) and word copy (8 bytes)
    char* src = malloc(32);
    char* dst = malloc(32);
    
    // Initialize source
    for (int i = 0; i < 32; i++)
        src[i] = (char)(i + 100);
    
    // Test different sizes
    for (int size = 2; size <= 8; size++) {
        // Clear destination
        memset(dst, 0, 32);
        
        // Do the copy
        zmemmove_union(dst, src, size);
        
        // Verify
        for (int i = 0; i < size; i++) {
            ZASSERT(dst[i] == src[i]);
        }
        
        zprintf("%d-byte memmove succeeded\n", size);
    }
    
    free(src);
    free(dst);
    return 0;
}