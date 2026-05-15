extern "C" {

inline __attribute__((__noinline__)) void foo() { }

void bar()
{
    foo();
}

}
