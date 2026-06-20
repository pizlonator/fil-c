#include <stdfil.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"

int main()
{
    char* p = malloc(16);
    strcpy(p, "hello");
    free(p);
    opaque(p);
    zsetproctitle(p);
    return 0;
}
