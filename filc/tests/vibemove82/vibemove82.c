#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Test sub-word copy (3 bytes) to out-of-bounds destination
    char* src = malloc(16);
    char* dst = malloc(16);
    
    // Initialize source
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    // Try to copy 3 bytes to out-of-bounds destination
    zmemmove(dst + 1000, src, 3);
    
    return 0;
}