#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include <stdio.h>
#include "utils.h"

int main()
{
    // Test copying arrays of different integer types
    int* int_src = malloc(sizeof(int) * 10);
    int* int_dst = malloc(sizeof(int) * 10);
    
    // Initialize
    for (int i = 0; i < 10; i++)
        int_src[i] = i * 1000;
    
    // Copy int array
    zmemmove(int_dst, int_src, sizeof(int) * 10);
    
    // Verify
    for (int i = 0; i < 10; i++) {
        ZASSERT(int_dst[i] == i * 1000);
    }
    
    zprintf("Int array copy succeeded\n");
    
    // Test with doubles
    double* double_src = malloc(sizeof(double) * 5);
    double* double_dst = malloc(sizeof(double) * 5);
    
    for (int i = 0; i < 5; i++)
        double_src[i] = i * 3.14159;
    
    zmemmove(double_dst, double_src, sizeof(double) * 5);
    
    for (int i = 0; i < 5; i++) {
        ZASSERT(double_dst[i] == i * 3.14159);
    }
    
    zprintf("Double array copy succeeded\n");
    
    free(int_src);
    free(int_dst);
    free(double_src);
    free(double_dst);
    return 0;
}