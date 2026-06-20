#include <stdfil.h>
#include <string.h>
#include <stdlib.h>

int main()
{
    char* p = malloc(16);
    strcpy(p, "hello");
    zsetproctitle(p + 1000);
    return 0;
}
