#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Zero-byte copy to/from NULL should be legal
    memmove(NULL, NULL, 0);
    zprintf("Zero-byte memmove to/from NULL succeeded\n");
    
    char* buf = malloc(16);
    memmove(buf, NULL, 0);
    zprintf("Zero-byte memmove from NULL succeeded\n");
    
    memmove(NULL, buf, 0);
    zprintf("Zero-byte memmove to NULL succeeded\n");
    
    free(buf);
    
    return 0;
}