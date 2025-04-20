#include <stdfil.h>
#include <thread>

struct Foo {
    Foo()
    {
        zprintf("cons;");
    }
    ~Foo()
    {
        zprintf("dest:%d;", x);
    }
    int x;
};

thread_local Foo foo;

int main()
{
    std::thread t([] () { foo.x = 666; });
    t.join();
    zprintf("done\n");
    return 0;
}


