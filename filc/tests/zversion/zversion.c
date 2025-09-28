#include <stdfil.h>

int main()
{
    ZASSERT(zversion() == FILC_VERSION);
    ZASSERT(FILC_VERSION);
    return 0;
}
