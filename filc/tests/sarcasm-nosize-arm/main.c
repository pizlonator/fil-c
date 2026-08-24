#include <stdio.h>

int first(int x);
int second(int x);

int main()
{
    printf("%d %d\n", first(10), second(10));   /* 11 12 */
    return 0;
}
