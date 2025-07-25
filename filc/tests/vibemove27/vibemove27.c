#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test copying between stack and heap
    char stack_buf[32];
    char* heap_buf = malloc(32);
    
    // Initialize stack buffer
    for (int i = 0; i < 32; i++)
        stack_buf[i] = (char)(i + 50);
    
    // Copy from stack to heap
    memmove(heap_buf, stack_buf, 32);
    
    // Verify
    for (int i = 0; i < 32; i++) {
        ZASSERT(heap_buf[i] == stack_buf[i]);
    }
    
    zprintf("Stack to heap memmove succeeded\n");
    
    // Modify heap and copy back to stack
    for (int i = 0; i < 32; i++)
        heap_buf[i] = (char)(i + 100);
    
    memmove(stack_buf, heap_buf, 32);
    
    // Verify
    for (int i = 0; i < 32; i++) {
        ZASSERT(stack_buf[i] == (char)(i + 100));
    }
    
    zprintf("Heap to stack memmove succeeded\n");
    
    free(heap_buf);
    return 0;
}