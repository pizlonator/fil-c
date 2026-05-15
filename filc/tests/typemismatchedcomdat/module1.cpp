extern "C" {

inline __attribute__((__noinline__)) int foo(int x) { return x + 1; }

void bar();

}

int main()
{
    foo(42);
    bar();
    return 0;
}

