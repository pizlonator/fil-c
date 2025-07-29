#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Non-zero copy to read-only memory should fail
    const char* read_only = "hello world";
    char* src = malloc(16);
    
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    zmemmove_union((void*)read_only, src, 5);
    
    return 0;
}