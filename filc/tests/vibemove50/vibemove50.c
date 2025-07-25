#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test mid-word copy where destination has NO pointers initially
    char* array1_buf = malloc(sizeof(void*) * 5);  // Allocate as char* buffer
    void** array2 = malloc(sizeof(void*) * 5);
    
    // Initialize array1 with non-pointer data
    for (size_t i = 0; i < sizeof(void*) * 5; i++) {
        array1_buf[i] = (char)(i + 100);
    }
    
    // Set up array2 with valid pointers
    for (int i = 0; i < 5; i++) {
        array2[i] = malloc(20 + i);
    }
    
    // Copy 32 bytes starting at offset 4 (mid-first-pointer)
    // This copies misaligned pointer data into array1
    memmove(array1_buf + 4, (char*)array2 + 4, 32);
    
    // Cast to pointer array to check capabilities
    void** array1 = (void**)array1_buf;
    
    // array1[0] has first 4 bytes of original data, last 4 bytes from copy - no valid cap
    ZASSERT(!zhasvalidcap(array1[0]));
    
    // array1[1-3] have valid capabilities preserved
    ZASSERT(zhasvalidcap(array1[1]));
    ZASSERT(zhasvalidcap(array1[2]));
    ZASSERT(zhasvalidcap(array1[3]));
    
    // array1[4] has first 4 bytes from copy, last 4 bytes of original data - no valid cap
    ZASSERT(!zhasvalidcap(array1[4]));
    
    zprintf("Mid-word copy to non-pointer buffer succeeded\n");
    
    // No cleanup needed - program is exiting
    return 0;
}