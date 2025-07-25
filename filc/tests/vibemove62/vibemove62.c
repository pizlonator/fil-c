#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Zero-byte copy from special pointer should be legal
    zweak_map* map = zweak_map_new();
    char* dst = malloc(16);
    
    zmemmove(dst, map, 0);
    
    zprintf("Zero-byte memmove from special pointer succeeded\n");
    
    free(dst);
    return 0;
}