#include <stdfil.h>

int punk(void);
int sybir_punk(void);
int funk(void);

int main()
{
    ZASSERT(punk() == 13);
    ZASSERT(sybir_punk() == 13);
    ZASSERT(funk() == 13);
    return 0;
}

