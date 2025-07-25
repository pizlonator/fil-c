#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    char* src = malloc(16);
    char* dst = malloc(16);
    
    // Initialize source
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    // Try to zmemmove from above bounds - copy 20 bytes from a 16-byte allocation
    zmemmove(dst, src, 20);
    
    return 0;
}