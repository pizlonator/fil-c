#include <stdio.h>
#include <stdlib.h>

long kill_then_load(long *p);

int main(void)
{
    long *p = malloc(64);
    if (!p)
        return 1;
    p[0] = 42;
    printf("expect trap:\n");
    printf("%ld SHOULD NOT PRINT\n", kill_then_load(p));
    return 0;
}
