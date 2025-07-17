#include <stdfil.h>
#include <wctype.h>

int main()
{
    ZASSERT(iswctype('x', wctype("alnum")));
    ZASSERT(iswctype('0', wctype("alnum")));
    ZASSERT(!iswctype('-', wctype("alnum")));
    ZASSERT(iswctype('x', wctype("alpha")));
    ZASSERT(!iswctype('0', wctype("alpha")));
    ZASSERT(!iswctype('-', wctype("alpha")));
    ZASSERT(iswctype(' ', wctype("blank")));
    ZASSERT(!iswctype('x', wctype("blank")));
    ZASSERT(!iswctype('0', wctype("blank")));
    ZASSERT(!iswctype('x', wctype("digit")));
    ZASSERT(iswctype('0', wctype("digit")));
    ZASSERT(!iswctype('-', wctype("digit")));
    ZASSERT(!iswctype(' ', wctype("graph")));
    ZASSERT(iswctype('x', wctype("graph")));
    ZASSERT(iswctype('0', wctype("graph")));
    ZASSERT(!iswctype(' ', wctype("lower")));
    ZASSERT(iswctype('x', wctype("lower")));
    ZASSERT(!iswctype('X', wctype("lower")));
    ZASSERT(!iswctype('0', wctype("lower")));
    ZASSERT(iswctype(' ', wctype("print")));
    ZASSERT(iswctype('x', wctype("print")));
    ZASSERT(iswctype('0', wctype("print")));
    ZASSERT(iswctype('.', wctype("punct")));
    ZASSERT(!iswctype('x', wctype("punct")));
    ZASSERT(!iswctype('0', wctype("punct")));
    ZASSERT(iswctype(' ', wctype("space")));
    ZASSERT(!iswctype('x', wctype("space")));
    ZASSERT(!iswctype('0', wctype("space")));
    ZASSERT(!iswctype(' ', wctype("upper")));
    ZASSERT(!iswctype('x', wctype("upper")));
    ZASSERT(iswctype('X', wctype("upper")));
    ZASSERT(!iswctype('0', wctype("upper")));
    ZASSERT(!iswctype('x', wctype("xdigit")));
    ZASSERT(iswctype('f', wctype("xdigit")));
    ZASSERT(iswctype('0', wctype("xdigit")));
    ZASSERT(!iswctype('-', wctype("xdigit")));
    return 0;
}

