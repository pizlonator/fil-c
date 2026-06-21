#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* base = opaque(malloc(16));
    size_t a = (argc > 1) ? 0 : 15;   /* runtime value is 15; compiler cannot fold */
    size_t b = (argc > 1) ? 1 : 16;   /* runtime value is 16; compiler cannot fold */
    char* p = base + a;
    char* q = base + b;
    *p = 'x';  /* in bounds */
    *q = 'y';  /* one-past-end: should be caught */
    return 0;
}
