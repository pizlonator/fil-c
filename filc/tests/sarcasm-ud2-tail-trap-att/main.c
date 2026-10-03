#include <stdio.h>

void ud2_tail_trap(void);

int main(void)
{
    ud2_tail_trap();
    printf("FAIL survived ud2\n");
    return 1;
}
