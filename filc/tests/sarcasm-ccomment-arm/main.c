#include <stdio.h>

int loadadd(int **p);

int main()
{
    int v[2] = { 30, 12 };
    int *slot[1];
    slot[0] = v;
    printf("%d\n", loadadd(slot));   /* 30 + 12 = 42 */
    return 0;
}
