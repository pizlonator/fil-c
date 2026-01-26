#include <optional>
#include <stdfil.h>
#include "thingy.h"

using namespace std;

static Thingy make(int* ptr, int a, int b, int c, int d)
{
    return { ptr, a, b, c, d };
}

struct Blah {
    int x;
    optional<Thingy> thingy;
    int y;
};

int main()
{
    bool p = true;
    bool q = true;
    asm ("" : "+r"(p));
    asm ("" : "+r"(q));
    int* ptr = new int(42);
    Blah* blah = new Blah();
    auto stuff = [&] () -> optional<Thingy> {
        if (!p)
            return { };
        if (q)
            return make(ptr, 1, 2, 3, 4);
        return { };
    };
    blah->thingy = stuff();
    zprintf("blah->thingy->ptr = %P\n", blah->thingy->ptr);
    ZASSERT(*blah->thingy->ptr == 42);
    return 0;
}

