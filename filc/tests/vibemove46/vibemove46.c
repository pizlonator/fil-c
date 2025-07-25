#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test mid-word copy that partially clobbers pointers
    void** array1 = malloc(sizeof(void*) * 5);  
    void** array2 = malloc(sizeof(void*) * 5);
    
    // Set up array1 with valid pointers
    for (int i = 0; i < 5; i++) {
        array1[i] = malloc(10 + i);
    }
    
    // Set up array2 with different pointers
    for (int i = 0; i < 5; i++) {
        array2[i] = malloc(20 + i);
    }
    
    // Copy 32 bytes starting at offset 4 (mid-first-pointer)
    // This will:
    // - Partially overwrite array1[0] (last 4 bytes)
    // - Fully overwrite array1[1], array1[2], array1[3] with misaligned data
    // - Partially overwrite array1[4] (first 4 bytes)
    memmove((char*)array1 + 4, (char*)array2 + 4, 32);
    
    // array1[0] should lose capability (partially clobbered)
    ZASSERT(!zhasvalidcap(array1[0]));
    
    // array1[1], array1[2], array1[3] have valid capabilities preserved
    ZASSERT(zhasvalidcap(array1[1]));
    ZASSERT(zhasvalidcap(array1[2]));
    ZASSERT(zhasvalidcap(array1[3]));
    
    // array1[4] should lose capability (partially clobbered)
    ZASSERT(!zhasvalidcap(array1[4]));
    
    zprintf("Mid-word copy with partial pointer clobbering succeeded\n");
    
    // No cleanup needed - program is exiting
    return 0;
}