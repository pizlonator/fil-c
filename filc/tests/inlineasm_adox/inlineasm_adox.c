#include <stdfil.h>

int main(void)
{
    unsigned long x;
    unsigned long y;
    unsigned long dummy;

    /* dummy == 0: OF = 0, so adox y, x with x = 10, y = 5 gives 15. */
    x = 10;
    y = 5;
    dummy = 0;
    asm volatile("cmp $1, %2\n\t"
                 "adox %1, %0"
                 : "+r"(x)
                 : "r"(y), "r"(dummy)
                 : "cc");
    ZASSERT(x == 15);

    /* dummy == 0x8000000000000000: OF = 1, so adox y, x with x = 10, y = 5 gives 16. */
    x = 10;
    y = 5;
    dummy = 0x8000000000000000UL;
    asm volatile("cmp $1, %2\n\t"
                 "adox %1, %0"
                 : "+r"(x)
                 : "r"(y), "r"(dummy)
                 : "cc");
    ZASSERT(x == 16);

    return 0;
}
