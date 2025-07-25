#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

struct complex_struct {
    int a;
    char* ptr1;
    double b;
    void* ptr2;
    char buf[16];
};

int main()
{
    // Test copying structures with pointers
    struct complex_struct src, dst;
    
    src.a = 42;
    src.ptr1 = malloc(20);
    strcpy(src.ptr1, "hello");
    src.b = 3.14159;
    src.ptr2 = malloc(30);
    strcpy(src.buf, "world");
    
    // Copy entire structure
    memmove(&dst, &src, sizeof(struct complex_struct));
    
    // Verify data
    ZASSERT(dst.a == 42);
    ZASSERT(dst.b == 3.14159);
    ZASSERT(strcmp(dst.buf, "world") == 0);
    
    // Verify pointers maintained capabilities
    ZASSERT(zhasvalidcap(dst.ptr1));
    ZASSERT(zhasvalidcap(dst.ptr2));
    ZASSERT(strcmp(dst.ptr1, "hello") == 0);
    
    zprintf("Structure copy with pointers succeeded\n");
    
    free(src.ptr1);
    free(src.ptr2);
    return 0;
}