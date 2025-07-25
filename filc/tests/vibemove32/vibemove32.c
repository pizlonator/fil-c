#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Test sub-word copy (1 byte) out-of-bounds high
    char* src = malloc(16);
    char* dst = malloc(16);
    
    // Initialize
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    // Try to copy 1 byte from way out-of-bounds
    memmove(dst, src + 1000, 1);
    
    return 0;
}