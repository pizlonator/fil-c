#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test edge case: copy that preserves some pointers in the middle
    // We'll copy aligned data but with edges that partially overlap pointers
    void** array1 = malloc(sizeof(void*) * 10);  
    void** array2 = malloc(sizeof(void*) * 10);
    
    // Set up both arrays with valid pointers
    for (int i = 0; i < 10; i++) {
        array1[i] = malloc(100 + i);
        array2[i] = malloc(200 + i);
    }
    
    // Copy 36 bytes starting at offset 4
    // This will:
    // - Partially clobber array1[0] (last 4 bytes)
    // - The next 32 bytes contain:
    //   - last 4 bytes of array2[0]
    //   - full array2[1], array2[2], array2[3] 
    //   - first 4 bytes of array2[4]
    // - This gets written to array1 starting at offset 4, so:
    //   - array1[1] gets last 4 bytes of array2[0] + first 4 bytes of array2[1] = no valid cap
    //   - array1[2] gets last 4 bytes of array2[1] + first 4 bytes of array2[2] = no valid cap
    //   - array1[3] gets last 4 bytes of array2[2] + first 4 bytes of array2[3] = no valid cap
    //   - array1[4] gets last 4 bytes of array2[3] + first 4 bytes of array2[4] = no valid cap
    //   - first 4 bytes of array1[5] get clobbered
    memmove((char*)array1 + 4, (char*)array2 + 4, 36);
    
    // Check capabilities
    ZASSERT(!zhasvalidcap(array1[0]));  // Partially clobbered
    ZASSERT(zhasvalidcap(array1[1]));   // Valid capability preserved
    ZASSERT(zhasvalidcap(array1[2]));   // Valid capability preserved
    ZASSERT(zhasvalidcap(array1[3]));   // Valid capability preserved
    ZASSERT(zhasvalidcap(array1[4]));   // Valid capability preserved
    ZASSERT(zhasvalidcap(array1[5]));   // Valid capability preserved
    ZASSERT(zhasvalidcap(array1[6]));   // Untouched
    ZASSERT(zhasvalidcap(array1[7]));   // Untouched
    
    zprintf("Edge case misaligned copy test succeeded\n");
    
    // No cleanup needed - program is exiting
    return 0;
}