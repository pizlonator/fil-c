#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Zero-byte copy to special pointer should be legal
    zweak_map* map = zweak_map_new();
    char* src = malloc(16);
    
    zmemmove(map, src, 0);
    
    zprintf("Zero-byte memmove to special pointer succeeded\n");
    
    free(src);
    return 0;
}