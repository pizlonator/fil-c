#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test copying uninitialized memory (should succeed)
    char* src = malloc(32);
    char* dst = malloc(32);
    
    // Don't initialize src - it contains uninitialized data
    
    // This should succeed - copying uninitialized data is allowed
    zmemmove_union(dst, src, 32);
    
    zprintf("Copy of uninitialized memory succeeded\n");
    
    free(src);
    free(dst);
    return 0;
}