#include <filc_test_support.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test self-copy (src == dst)
    char* buf = malloc(32);
    
    // Initialize
    for (int i = 0; i < 32; i++)
        buf[i] = (char)(i + 42);
    
    // Self copy
    zmemmove_union(buf, buf, 32);
    
    // Verify nothing changed
    for (int i = 0; i < 32; i++) {
        ZASSERT(buf[i] == (char)(i + 42));
    }
    
    zprintf("Self-copy memmove succeeded\n");
    
    free(buf);
    return 0;
}