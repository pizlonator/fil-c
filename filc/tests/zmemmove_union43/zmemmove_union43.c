#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdint.h>
#include "utils.h"

int main()
{
    // Test with extremely large size (should fail due to bounds)
    char* src = malloc(16);
    char* dst = malloc(16);
    
    // Try to copy SIZE_MAX bytes
    zmemmove_union(dst, src, SIZE_MAX);
    
    return 0;
}