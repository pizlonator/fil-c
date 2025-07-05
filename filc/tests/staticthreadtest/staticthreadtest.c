#include <pthread.h>
#include <stdfil.h>

static void* thread_main(void* arg)
{
    return arg;
}

int main()
{
    unsigned i;
    for (i = 10000; i--;) {
        pthread_t thread;
        pthread_create(&thread, NULL, thread_main, (void*)i);
        void* result;
        pthread_join(thread, &result);
        ZASSERT((unsigned)result == i);
    }
    return 0;
}

