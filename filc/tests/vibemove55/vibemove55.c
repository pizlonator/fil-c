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
    
    // Try to zmemmove to above bounds in destination
    zmemmove(dst + 10, src, 10);  // Writing 10 bytes starting at offset 10 in a 16-byte allocation
    
    return 0;
}