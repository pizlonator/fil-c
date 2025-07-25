#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    char* src = malloc(16);
    char* dst = malloc(16);
    
    // Initialize
    for (int i = 0; i < 16; i++) {
        src[i] = (char)(i + 42);
        dst[i] = 0;
    }
    
    // Zero-byte copy should succeed
    zmemmove(dst, src, 0);
    
    // Verify nothing was copied
    for (int i = 0; i < 16; i++) {
        ZASSERT(dst[i] == 0);
    }
    
    zprintf("Zero-byte zmemmove succeeded\n");
    
    return 0;
}