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
    
    // Try to zmemmove from below bounds
    zmemmove(dst, src - 1000, 10);
    
    return 0;
}