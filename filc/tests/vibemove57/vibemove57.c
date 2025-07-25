#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    char* dst = malloc(16);
    
    // Try to zmemmove from NULL source
    zmemmove(dst, NULL, 10);
    
    return 0;
}