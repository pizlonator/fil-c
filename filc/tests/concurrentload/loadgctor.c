#include <stdfil.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>

static void do_stuff(const char* name, const char* so, const char* sym)
{
    zprintf("%s: loading %s\n", name, so);
    void* lib = dlopen(so, RTLD_LAZY | RTLD_LOCAL);
    zprintf("%s: lib = %p\n", name, lib);
    if (!lib)
        zprintf("%s: error: %s\n", name, dlerror());
    zprintf("%s: dlsymming.\n", name);
    void (*bar)(void) = dlsym(lib, sym);
    zprintf("%s: %s = %p\n", name, sym, bar);
    if (!bar)
        zprintf("%s: error: %s\n", name, dlerror());
    zprintf("%s: calling %s.\n", name, sym);
    bar();
}

static void* thread_main(void* arg)
{
    do_stuff("thread", "filc/test-output/concurrentload/gctor.so", "bar");
    return NULL;
}

int main()
{
    pthread_t t;
    pthread_create(&t, NULL, thread_main, NULL);

    do_stuff("main", "filc/test-output/concurrentload/gctor2.so", "bar2");

    pthread_join(t, NULL);
    return 0;
}

