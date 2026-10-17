#include <stdfil.h>

struct C { int value; };

union Nonzero { int C::*member; int *pointer; };

union Nonzero2 {
    struct {
        int C::*member;
        int C::*member2;
    } s;
    int *pointer;
    int *pointer2;
};

union Nonzero2b {
    struct {
        int C::*member;
        int C::*member2;
    } s;
    struct {
        int *pointer;
        int *pointer2;
    } s2;
};

union Nonzero3 {
    struct {
        int C::*member;
        int C::*member2;
        int C::*member3;
        int C::*member4;
        int C::*member5;
    } s;
    int *pointer;
    int *pointer2;
    int *pointer3;
    int *pointer4;
    int *pointer5;
};

union Nonzero3b {
    struct {
        int C::*member;
        int C::*member2;
        int C::*member3;
        int C::*member4;
        int C::*member5;
    } s;
    struct {
        int *pointer;
        int *pointer2;
        int *pointer3;
        int *pointer4;
        int *pointer5;
    } s2;
};

union Nonzero3c {
    struct {
        int a;
        int b;
        int C::*member2;
        int c;
        int d;
        int C::*member4;
    } s;
    struct {
        int *pointer;
        int *pointer2;
        int *pointer3;
        int *pointer4;
        int *pointer5;
    } s2;
};

union Nonzero4 {
    struct {
        int *pointer;
        int *pointer2;
        int *pointer3;
        int *pointer4;
        int *pointer5;
    } s2;
    struct {
        int C::*member;
        int C::*member2;
        int C::*member3;
        int C::*member4;
        int C::*member5;
    } s;
};

int main()
{
    Nonzero& nz = *new Nonzero();
    Nonzero2& nz2 = *new Nonzero2();
    Nonzero2b& nz2b = *new Nonzero2b();
    Nonzero3& nz3 = *new Nonzero3();
    Nonzero3b& nz3b = *new Nonzero3b();
    Nonzero3c& nz3c = *new Nonzero3c();
    Nonzero4& nz4 = *new Nonzero4();

    // An all-ones numeric null representation must not invent a capability.
    ZASSERT(!zhasvalidcap(nz.pointer));
    ZASSERT(!zhasvalidcap(nz3c.s2.pointer2));
    ZASSERT(nz.pointer == (int*)-1);
    ZASSERT(nz2.pointer == (int*)-1);
    ZASSERT(nz2.pointer2 == (int*)-1);
    ZASSERT(nz2b.s2.pointer == (int*)-1);
    ZASSERT(nz2b.s2.pointer2 == (int*)-1);
    ZASSERT(nz3.pointer == (int*)-1);
    ZASSERT(nz3.pointer2 == (int*)-1);
    ZASSERT(nz3.pointer3 == (int*)-1);
    ZASSERT(nz3.pointer4 == (int*)-1);
    ZASSERT(nz3.pointer5 == (int*)-1);
    ZASSERT(nz3b.s2.pointer == (int*)-1);
    ZASSERT(nz3b.s2.pointer2 == (int*)-1);
    ZASSERT(nz3b.s2.pointer3 == (int*)-1);
    ZASSERT(nz3b.s2.pointer4 == (int*)-1);
    ZASSERT(nz3b.s2.pointer5 == (int*)-1);
    ZASSERT(!nz3c.s2.pointer);
    ZASSERT(nz3c.s2.pointer2 == (int*)-1);
    ZASSERT(!nz3c.s2.pointer3);
    ZASSERT(nz3c.s2.pointer4 == (int*)-1);
    ZASSERT(!nz3c.s2.pointer5);
    ZASSERT(!nz4.s2.pointer);
    ZASSERT(!nz4.s2.pointer2);
    ZASSERT(!nz4.s2.pointer3);
    ZASSERT(!nz4.s2.pointer4);
    ZASSERT(!nz4.s2.pointer5);
    return 0;
}
