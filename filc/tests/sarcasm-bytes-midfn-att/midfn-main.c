#include <stdio.h>

long midfn_through(long x);
long midfn_around(long x);

int main(void)
{
    /* fallthrough executes the decoded run: x + 1 + 5 + 2 */
    if (midfn_through(34) != 42) {
        printf("midfn_through=%ld\n", midfn_through(34));
        return 1;
    }
    /* the spelled jmp skips the run: x + 1 */
    if (midfn_around(41) != 42) {
        printf("midfn_around=%ld\n", midfn_around(41));
        return 1;
    }
    printf("bytes midfn att ok\n");
    return 0;
}
