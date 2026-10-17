#include <stdfil.h>

void copy_byte(void *destination, unsigned char byte);
void *clear_byte(void *value);
void self_copy_byte(void *slot);

int main(void)
{
    int target = 42;
    /* A live capability with a known payload makes the distinction exact:
     * both byte operations leave the address bits unchanged, but must clear
     * its capability. A numeric-store rewrite would incorrectly retain it.
     * We inspect the capability; we never dereference this tagged address. */
    void *carrier = zmkptr(&target, 0x1000);
    ZASSERT(zhasvalidcap(carrier));
    copy_byte(&carrier, 0);
    ZASSERT((unsigned long)carrier == 0x1000);
    ZASSERT(!zhasvalidcap(carrier));

    carrier = zmkptr(&target, 0x1000);
    carrier = clear_byte(carrier);
    ZASSERT((unsigned long)carrier == 0x1000);
    ZASSERT(!zhasvalidcap(carrier));

    carrier = zmkptr(&target, 0x1000);
    self_copy_byte(&carrier);
    ZASSERT((unsigned long)carrier == 0x1000);
    ZASSERT(!zhasvalidcap(carrier));
    return 0;
}
