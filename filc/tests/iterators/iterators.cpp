#include <vector>
#include <memory>
#include <stdfil.h>

using namespace std;

struct Foo {
    Foo()
        : x(666)
    {
    }

    Foo(int x)
        : x(x)
    {
    }

    ~Foo()
    {
    }

    int x;
};

using FooVector = vector<unique_ptr<Foo>>;

static __attribute__((noinline)) int doStuff(vector<FooVector>& fooVector,
                                             vector<FooVector>::const_iterator it)
{
    int result = 0;
    const auto end = fooVector.end();
    for (; it != end; ++it) {
        for (const auto& foo : *it)
            result += foo->x;
    }
    return result;
}

static int doDoStuff(vector<FooVector>& fooVector)
{
    auto it = fooVector.begin();
    ++it;
    return doStuff(fooVector, it);
}

static FooVector create(int x)
{
    FooVector result;
    result.push_back(make_unique<Foo>(42 + x));
    result.push_back(make_unique<Foo>(666 + x));
    result.push_back(make_unique<Foo>(1410 + x));
    return result;
}

int main()
{
    vector<FooVector> fooVector;
    fooVector.push_back(create(0));
    fooVector.push_back(create(1));
    fooVector.push_back(create(2));
    fooVector.push_back(create(3));
    ZASSERT(doDoStuff(fooVector) == 43 + 667 + 1411 + 44 + 668 + 1412 + 45 + 669 + 1413);
    return 0;
}

