#include <stdfil.h>

struct O {
    void blah();
};

int f(void (O::*g)())
{
    if (g)
        return 42;
    return 666;
}

int main()
{
    ZASSERT(f(&O::blah) == 42);
    return 0;
}
