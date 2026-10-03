#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/* On x86, glibc defines pthread_spin_init as an alias in its assembly
   pthread_spin_unlock, which the user glibc replaces with C. */

static pthread_spinlock_t lock;
static long counter;

static void* thread_main(void* arg)
{
    for (int i = 0; i < 100000; i++) {
        pthread_spin_lock(&lock);
        counter++;
        pthread_spin_unlock(&lock);
    }
    return arg;
}

int main(void)
{
    if (pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE))
        return 1;
    pthread_t threads[4];
    for (int i = 0; i < 4; i++)
        pthread_create(threads + i, NULL, thread_main, NULL);
    for (int i = 0; i < 4; i++)
        pthread_join(threads[i], NULL);
    if (counter != 400000 || pthread_spin_destroy(&lock))
        return 1;
    puts("spinlock ok");
    return 0;
}
