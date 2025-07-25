#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test copying pointers at aligned offset (multiple of 8)
    void** src_ptrs = malloc(sizeof(void*) * 4);
    void** dst_ptrs = malloc(sizeof(void*) * 4);
    
    // Set up source pointers
    src_ptrs[0] = malloc(10);
    src_ptrs[1] = malloc(20);
    src_ptrs[2] = malloc(30);
    src_ptrs[3] = malloc(40);
    
    // Copy at aligned offset (0 is multiple of 8)
    memmove(dst_ptrs, src_ptrs, sizeof(void*) * 4);
    
    // Pointers should maintain capabilities
    ZASSERT(zhasvalidcap(dst_ptrs[0]));
    ZASSERT(zhasvalidcap(dst_ptrs[1]));
    ZASSERT(zhasvalidcap(dst_ptrs[2]));
    ZASSERT(zhasvalidcap(dst_ptrs[3]));
    
    zprintf("Aligned memmove preserved pointer capabilities\n");
    
    // Clean up
    free(src_ptrs[0]);
    free(src_ptrs[1]);
    free(src_ptrs[2]);
    free(src_ptrs[3]);
    free(src_ptrs);
    free(dst_ptrs);
    return 0;
}