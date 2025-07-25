#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test copying to misaligned offset (not multiple of 8) where destination contains pointers
    void** ptr_array = malloc(sizeof(void*) * 4);
    char* src = malloc(32);
    
    // Set up some pointers
    ptr_array[0] = malloc(10);
    ptr_array[1] = malloc(20);
    ptr_array[2] = malloc(30);
    ptr_array[3] = malloc(40);
    
    // Initialize source
    for (int i = 0; i < 32; i++)
        src[i] = (char)(i + 42);
    
    // Copy to misaligned offset (offset 3 is not a multiple of 8)
    // This will partially overwrite pointers
    zmemmove((char*)ptr_array + 3, src, 10);
    
    // The pointers should lose their capabilities
    ZASSERT(!zhasvalidcap(ptr_array[0]));
    ZASSERT(!zhasvalidcap(ptr_array[1]));
    
    zprintf("Misaligned memmove clobbered pointer capabilities as expected\n");
    
    free(src);
    free(ptr_array);
    return 0;
}