#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Non-zero copy from special pointer should fail
    zweak_map* map = zweak_map_new();
    char* dst = malloc(16);
    
    zmemmove(dst, map, 10);
    
    return 0;
}