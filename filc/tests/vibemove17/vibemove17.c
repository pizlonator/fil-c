#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Non-zero copy from freed memory should fail
    char* src = malloc(16);
    char* dst = malloc(16);
    
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    free(src);
    
    memmove(dst, src, 10);
    
    return 0;
}