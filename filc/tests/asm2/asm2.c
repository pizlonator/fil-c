#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ASSERT(exp) do { \
        if ((exp)) \
            break; \
        fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, \
                #exp); \
        abort(); \
    } while (false)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("Yesyes\n");
    int x = 42;
    int y = 666;
    asm volatile("mov %1, %2" : "+m"(x) : "r"(y) : "memory");
    ASSERT(x == 666);
    printf("Nono\n");
    return 0;
}

