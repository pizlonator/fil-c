#include <stdfil.h>

int main(void)
{
    unsigned long id1, id2;
    asm volatile("rdpid %0" : "=r"(id1));
    asm volatile("rdpid %0" : "=r"(id2));

    ZASSERT(id1 == id2);
    zprintf("rdpid = %lu\n", id1);

    return 0;
}
