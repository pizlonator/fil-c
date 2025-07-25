#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test copy with mixed alignment - some pointers preserved, some clobbered
    void** array1 = malloc(sizeof(void*) * 10);  
    void** array2 = malloc(sizeof(void*) * 10);
    
    // Set up both arrays with valid pointers
    for (int i = 0; i < 10; i++) {
        array1[i] = malloc(100 + i);
        array2[i] = malloc(200 + i);
        ZASSERT(zhasvalidcap(array1[i]));
        ZASSERT(zhasvalidcap(array2[i]));
    }
    
    // First: aligned copy that preserves capabilities
    // Copy array2[2-4] to array1[5-7] (24 bytes, aligned)
    memmove(&array1[5], &array2[2], sizeof(void*) * 3);
    
    // Verify aligned copy preserved capabilities
    ZASSERT(array1[5] == array2[2] && zhasvalidcap(array1[5]));
    ZASSERT(array1[6] == array2[3] && zhasvalidcap(array1[6]));
    ZASSERT(array1[7] == array2[4] && zhasvalidcap(array1[7]));
    
    // Second: misaligned copy that clobbers capabilities
    // Copy 16 bytes starting at offset 4 into array1[0]
    memmove((char*)array1 + 4, array2, 16);
    
    // array1[0] is partially clobbered (last 4 bytes)
    ZASSERT(!zhasvalidcap(array1[0]));
    
    // array1[1] is fully clobbered with misaligned data
    ZASSERT(!zhasvalidcap(array1[1]));
    
    // array1[2] is partially clobbered (first 4 bytes)
    ZASSERT(!zhasvalidcap(array1[2]));
    
    // array1[3-4] should be untouched
    ZASSERT(zhasvalidcap(array1[3]));
    ZASSERT(zhasvalidcap(array1[4]));
    
    // array1[5-7] still have capabilities from first copy
    ZASSERT(zhasvalidcap(array1[5]));
    ZASSERT(zhasvalidcap(array1[6]));
    ZASSERT(zhasvalidcap(array1[7]));
    
    zprintf("Mixed aligned/misaligned copy capability test succeeded\n");
    
    // No cleanup needed - program is exiting
    return 0;
}