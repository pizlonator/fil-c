#include <stdfil.h>

int punk(void)
{
    return 666;
}

int sybir_punk(void);
int funk(void);

int main()
{
    ZASSERT(punk() == 666);
    ZASSERT(sybir_punk() == 13);
    ZASSERT(funk() == 13);
    return 0;
}

