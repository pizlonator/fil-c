#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test copying pointers from src to dst that previously had pointers
    void** src_ptrs = malloc(sizeof(void*) * 4);
    void** dst_ptrs = malloc(sizeof(void*) * 4);
    
    // Set up source pointers
    src_ptrs[0] = malloc(10);
    src_ptrs[1] = malloc(20);
    src_ptrs[2] = malloc(30);
    src_ptrs[3] = malloc(40);
    
    // Set up destination pointers (different allocations)
    dst_ptrs[0] = malloc(50);
    dst_ptrs[1] = malloc(60);
    dst_ptrs[2] = malloc(70);
    dst_ptrs[3] = malloc(80);
    
    // Remember old dst pointers to free later
    void* old_dst[4];
    for (int i = 0; i < 4; i++)
        old_dst[i] = dst_ptrs[i];
    
    // Copy pointers - destination previously had valid pointers
    zmemmove_union(dst_ptrs, src_ptrs, sizeof(void*) * 4);
    
    // Verify pointers copied correctly and maintain capabilities
    for (int i = 0; i < 4; i++) {
        ZASSERT(dst_ptrs[i] == src_ptrs[i]);
        ZASSERT(zhasvalidcap(dst_ptrs[i]));
    }
    
    zprintf("Pointer copy to buffer with existing pointers succeeded\n");
    
    // Clean up
    for (int i = 0; i < 4; i++) {
        free(src_ptrs[i]);
        free(old_dst[i]);
    }
    free(src_ptrs);
    free(dst_ptrs);
    return 0;
}