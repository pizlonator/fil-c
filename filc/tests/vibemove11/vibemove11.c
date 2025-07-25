#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Zero-byte copy to freed memory should be legal
    char* dst = malloc(16);
    free(dst);
    
    char* src = malloc(16);
    memmove(dst, src, 0);
    
    zprintf("Zero-byte memmove to freed memory succeeded\n");
    
    free(src);
    return 0;
}