#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Test sub-word copy (2 bytes) out-of-bounds low
    char* src = malloc(16);
    char* dst = malloc(16);
    
    // Try to copy 2 bytes from below bounds
    zmemmove_union(dst, src - 1000, 2);
    
    return 0;
}