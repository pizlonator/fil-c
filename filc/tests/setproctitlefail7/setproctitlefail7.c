#include <stdfil.h>
#include <string.h>
#include <stdlib.h>

int main()
{
    char* p = malloc(16);
    memset(p, 'x', 16);
    zsetproctitle(p);
    return 0;
}
