#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test copying pointers from src to dst that previously had non-pointer data
    void** src_ptrs = malloc(sizeof(void*) * 4);
    char* dst_buf = malloc(sizeof(void*) * 4);
    
    // Set up source pointers
    src_ptrs[0] = malloc(10);
    src_ptrs[1] = malloc(20);
    src_ptrs[2] = malloc(30);
    src_ptrs[3] = malloc(40);
    
    // Initialize destination with non-pointer data
    for (size_t i = 0; i < sizeof(void*) * 4; i++)
        dst_buf[i] = (char)(i + 100);
    
    // Copy pointers to buffer that had non-pointer data
    zmemmove_union(dst_buf, src_ptrs, sizeof(void*) * 4);
    
    // Cast destination to pointer array
    void** dst_ptrs = (void**)dst_buf;
    
    // Verify pointers copied correctly and maintain capabilities
    for (int i = 0; i < 4; i++) {
        ZASSERT(dst_ptrs[i] == src_ptrs[i]);
        ZASSERT(zhasvalidcap(dst_ptrs[i]));
    }
    
    zprintf("Pointer copy to buffer with non-pointer data succeeded\n");
    
    // Clean up
    for (int i = 0; i < 4; i++) {
        free(src_ptrs[i]);
    }
    free(src_ptrs);
    free(dst_buf);
    return 0;
}