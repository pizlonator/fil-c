#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // zmemmove version: Test sub-word copy (7 bytes) from freed memory
    char* src = malloc(16);
    char* dst = malloc(16);
    
    // Initialize
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    free(src);
    
    // Try to copy 7 bytes from freed memory
    zmemmove_union(dst, src, 7);
    
    return 0;
}