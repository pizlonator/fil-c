#include <stdio.h>

int main()
{
    setvbuf(stdout, 0, _IONBF, 0);
    __builtin_cpu_init();
    if (__builtin_cpu_supports("f16c"))
        printf("have f16c\n");
    else
        printf("do not have f16c\n");
    if (__builtin_cpu_supports("avx512f"))
        printf("have avx512f\n");
    else
        printf("do not have avx512f\n");
    return 0;
}

