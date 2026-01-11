#define _GNU_SOURCE

#include <pthread.h>
#include <stdfil.h>
#include <sys/resource.h>
#include <stdlib.h>

int main()
{
    void* bound = NULL;
    size_t stackSize = 0;

    pthread_attr_t sattr;
    pthread_attr_init(&sattr);
    pthread_getattr_np(pthread_self(), &sattr);
    int rc = pthread_attr_getstack(&sattr, &bound, &stackSize);
    pthread_attr_destroy(&sattr);
    void* origin = (char*)bound + stackSize;
    zprintf("%p, %p\n", origin, bound);
    ZASSERT(zstack_pointer() > bound);
    ZASSERT(zstack_pointer() < origin);
    return 0;
}

