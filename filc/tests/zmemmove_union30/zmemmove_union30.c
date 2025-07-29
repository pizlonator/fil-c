#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Zero-byte copy to read-only memory should be legal
    const char* read_only = "hello world";
    char* src = malloc(16);
    
    zmemmove_union((void*)read_only, src, 0);
    
    zprintf("Zero-byte memmove to read-only memory succeeded\n");
    
    free(src);
    return 0;
}