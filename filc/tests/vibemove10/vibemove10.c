#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Zero-byte copy from freed memory should be legal
    char* buf = malloc(16);
    free(buf);
    
    char* dst = malloc(16);
    memmove(dst, buf, 0);
    
    zprintf("Zero-byte memmove from freed memory succeeded\n");
    
    free(dst);
    return 0;
}