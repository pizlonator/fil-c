#include <pthread.h>
#include <stdfil.h>

static int x;

static void cleanup(void* arg)
{
    ZASSERT(arg == &x);
    zprintf("did run\n");
}

int main()
{
    pthread_cleanup_push(cleanup, &x);
    pthread_cleanup_pop(1);
    return 0;
}

