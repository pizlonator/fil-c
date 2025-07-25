#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Test sub-word copy (4 bytes) reading past end of buffer
    char* src = malloc(16);
    char* dst = malloc(16);
    
    // Initialize
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    // Try to copy 4 bytes starting at offset 14 (only 2 bytes available)
    memmove(dst, src + 14, 4);
    
    return 0;
}