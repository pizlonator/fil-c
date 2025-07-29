#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Test sub-word copy (6 bytes) from NULL
    char* dst = malloc(16);
    
    // Try to copy 6 bytes from NULL
    zmemmove_union(dst, NULL, 6);
    
    return 0;
}