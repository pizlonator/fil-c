#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Test sub-word copy (5 bytes) writing past end of buffer
    char* src = malloc(16);
    char* dst = malloc(16);
    
    // Initialize
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    // Try to copy 5 bytes to offset 13 (only 3 bytes available)
    zmemmove(dst + 13, src, 5);
    
    return 0;
}