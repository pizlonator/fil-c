#include <immintrin.h>

struct Foo
{
    __m256d a;
    double b[5];
};

__attribute__((noinline)) void foo(const Foo v)
{
    volatile double z = _mm256_cvtsd_f64(v.a);
    (void)z;
}

int main()
{
    foo(Foo{});
    return 0;
}
