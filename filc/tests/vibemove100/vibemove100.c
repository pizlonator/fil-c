#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

struct nested_ptrs {
    void* ptr1;
    struct {
        void* ptr2;
        char data[8];
        void* ptr3;
    } inner;
    void* ptr4;
};

int main()
{
    // Test copying nested structures with pointers using memmove
    struct nested_ptrs src, dst;
    
    // Initialize source
    src.ptr1 = malloc(10);
    src.inner.ptr2 = malloc(20);
    strcpy(src.inner.data, "hello");
    src.inner.ptr3 = malloc(30);
    src.ptr4 = malloc(40);
    
    // Copy structure
    memmove(&dst, &src, sizeof(struct nested_ptrs));
    
    // Verify structure members
    ZASSERT(dst.ptr1 == src.ptr1 && zhasvalidcap(dst.ptr1));
    ZASSERT(dst.inner.ptr2 == src.inner.ptr2 && zhasvalidcap(dst.inner.ptr2));
    ZASSERT(strcmp(dst.inner.data, "hello") == 0);
    ZASSERT(dst.inner.ptr3 == src.inner.ptr3 && zhasvalidcap(dst.inner.ptr3));
    ZASSERT(dst.ptr4 == src.ptr4 && zhasvalidcap(dst.ptr4));
    
    zprintf("Nested structure memmove succeeded\n");
    
    free(src.ptr1);
    free(src.inner.ptr2);
    free(src.inner.ptr3);  
    free(src.ptr4);
    return 0;
}