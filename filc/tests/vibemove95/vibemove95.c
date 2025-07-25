#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test larger mid-word copy with detailed capability checking
    void** array1 = malloc(sizeof(void*) * 10);  
    void** array2 = malloc(sizeof(void*) * 10);
    
    // Set up array1 with valid pointers
    for (int i = 0; i < 10; i++) {
        array1[i] = malloc(100 + i);
    }
    
    // Set up array2 with different pointers
    for (int i = 0; i < 10; i++) {
        array2[i] = malloc(200 + i);
    }
    
    // Remember original array1 pointers for verification
    void* orig_array1[10];
    for (int i = 0; i < 10; i++) {
        orig_array1[i] = array1[i];
        ZASSERT(zhasvalidcap(array1[i]));  // All start with valid caps
    }
    
    // Copy 64 bytes (8 pointers) starting at offset 4
    // This affects:
    // - array1[0]: last 4 bytes clobbered
    // - array1[1-7]: fully overwritten with shifted data
    // - array1[8]: first 4 bytes clobbered
    // - array1[9]: untouched
    zmemmove((char*)array1 + 4, (char*)array2 + 4, 64);
    
    // Verify capabilities:
    // array1[0] - partially clobbered, should lose capability
    ZASSERT(!zhasvalidcap(array1[0]));
    
    // array1[1-7] - these contain aligned pointer data from array2
    // Capabilities should be preserved
    for (int i = 1; i <= 7; i++) {
        ZASSERT(zhasvalidcap(array1[i]));
    }
    
    // array1[8] - partially clobbered, should lose capability
    ZASSERT(!zhasvalidcap(array1[8]));
    
    // array1[9] - untouched, should maintain capability
    ZASSERT(zhasvalidcap(array1[9]));
    ZASSERT(array1[9] == orig_array1[9]);
    
    zprintf("Large mid-word copy capability verification succeeded\n");
    
    // Clean up
    for (int i = 0; i < 10; i++) {
        free(array2[i]);
    }
    free(orig_array1[9]);  // Only one that's still valid
    free(array1);
    free(array2);
    return 0;
}