#include <stdfil.h>

void foo();

int main()
{
    try {
        foo();
        ZASSERT(!"foo returned");
    } catch (...) {
        ZASSERT(!"exception caught");
    }
    return 0;
}
