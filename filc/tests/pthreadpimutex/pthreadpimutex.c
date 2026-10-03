#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/* pthread_mutex_init probes for PI futex support with FUTEX_UNLOCK_PI, which
   Linux answers with EPERM. The unlock result's sign used to be wrong, so this
   probe aborted the process. Also exercise a contended PI mutex. */

static pthread_mutex_t mutex;
static long counter;

static void* thread_main(void* arg)
{
    for (int i = 0; i < 100000; i++) {
        if (pthread_mutex_lock(&mutex))
            abort();
        counter++;
        if (pthread_mutex_unlock(&mutex))
            abort();
    }
    return arg;
}

int main(void)
{
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) ||
        pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT))
        return 1;
    int err = pthread_mutex_init(&mutex, &attr);
    if (err) {
        printf("pthread_mutex_init: %d\n", err);
        return 1;
    }
    pthread_t threads[4];
    for (int i = 0; i < 4; i++)
        pthread_create(threads + i, NULL, thread_main, NULL);
    for (int i = 0; i < 4; i++)
        pthread_join(threads[i], NULL);
    if (counter != 400000 || pthread_mutex_destroy(&mutex))
        return 1;
    puts("priority-inheritance mutex ok");
    return 0;
}
