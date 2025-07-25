#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Zero-byte copy to/from NULL should be legal
    zmemmove(NULL, NULL, 0);
    zprintf("Zero-byte zmemmove to/from NULL succeeded\n");
    
    char* buf = malloc(16);
    zmemmove(buf, NULL, 0);
    zprintf("Zero-byte zmemmove from NULL succeeded\n");
    
    zmemmove(NULL, buf, 0);
    zprintf("Zero-byte zmemmove to NULL succeeded\n");
    
    free(buf);
    
    return 0;
}