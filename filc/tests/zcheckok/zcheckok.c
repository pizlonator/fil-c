#include <stdfil.h>
#include <stdlib.h>

int main()
{
    zcheck_readonly("hello", 5);
    int x;
    zcheck(&x, 4);
    zcheck(zweak_new(NULL), 0);
    zcheck(NULL, 0);
    zcheck((void*)666, 0);
    zcheck_readonly(zweak_new(NULL), 0);
    zcheck_readonly(NULL, 0);
    zcheck_readonly((void*)666, 0);
    ZASSERT(zis_readonly("hello"));
    ZASSERT(!zis_readonly(&x));
    ZASSERT(!zis_readonly(NULL));
    ZASSERT(!zis_readonly((void*)666));
    ZASSERT(!zis_readonly((void*)zweak_new(NULL)));
    return 0;
}

