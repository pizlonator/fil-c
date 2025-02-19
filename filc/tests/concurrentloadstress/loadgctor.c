#include <stdfil.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

#define N 1000

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
    unsigned i;
    for (i = N / 2; i--;) {
        do_stuff(
            "thread",
            zasprintf("filc/test-output/concurrentloadstress/gctor%u.so", i + N / 2),
            "bar");
    }
    return NULL;
}

int main()
{
    pthread_t t;
    pthread_create(&t, NULL, thread_main, NULL);

    unsigned i;
    for (i = N / 2; i--;)
        do_stuff("main", zasprintf("filc/test-output/concurrentloadstress/gctor%u.so", i), "bar");

    pthread_join(t, NULL);
    return 0;
}

