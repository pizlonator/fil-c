#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test overlapping copy (this is what memmove is designed for)
    char* buf = malloc(32);
    
    // Initialize
    for (int i = 0; i < 32; i++)
        buf[i] = (char)(i + 100);
    
    // Overlapping copy - forward overlap
    zmemmove(buf + 5, buf, 10);
    
    // Verify the copy worked correctly
    for (int i = 0; i < 10; i++) {
        ZASSERT(buf[i + 5] == (char)(i + 100));
    }
    
    zprintf("Forward overlapping memmove succeeded\n");
    
    // Overlapping copy - backward overlap
    zmemmove(buf, buf + 3, 10);
    
    zprintf("Backward overlapping memmove succeeded\n");
    
    free(buf);
    return 0;
}