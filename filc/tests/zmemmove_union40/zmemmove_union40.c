#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test partial overwrite of pointer (misaligned size)
    void** ptr_array = malloc(sizeof(void*) * 2);
    char* src = malloc(16);
    
    // Set up pointers
    ptr_array[0] = malloc(10);
    ptr_array[1] = malloc(20);
    
    // Initialize source
    for (int i = 0; i < 16; i++)
        src[i] = (char)(i + 42);
    
    // Copy 5 bytes starting at aligned offset - this partially overwrites first pointer
    zmemmove_union(ptr_array, src, 5);
    
    // First pointer should lose capability due to partial overwrite
    ZASSERT(!zhasvalidcap(ptr_array[0]));
    
    // Second pointer should still be valid
    ZASSERT(zhasvalidcap(ptr_array[1]));
    
    zprintf("Partial pointer overwrite handled correctly\n");
    
    free(ptr_array[1]);
    free(src);
    free(ptr_array);
    return 0;
}