#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#define ASSERT(exp) do { \
        if ((exp)) \
            break; \
        fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
        abort(); \
    } while (false)

static bool signaled = false;

static void handler(int signo)
{
    signaled = true;
}

static bool functioned = false;

static void function(union sigval sigval)
{
    functioned = true;
}

int main()
{
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_NONE;
    timer_t timer;
    ASSERT(!timer_create(CLOCK_REALTIME, &sev, &timer));
    struct itimerspec it;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_nsec = 0;
    it.it_value.tv_sec = 0;
    it.it_value.tv_nsec = 1000000;
    ASSERT(!timer_settime(timer, 0, &it, NULL));
    for (;;) {
        ASSERT(!timer_gettime(timer, &it));
        ASSERT(!it.it_interval.tv_sec);
        ASSERT(!it.it_interval.tv_nsec);
        if (!it.it_value.tv_sec && !it.it_value.tv_nsec)
            break;
    }
    timer_getoverrun(timer);
    ASSERT(!timer_delete(timer));

    ASSERT(!signaled);
    sigset_t blockset;
    sigset_t oldset;
    ASSERT(!sigemptyset(&blockset));
    ASSERT(!sigaddset(&blockset, SIGUSR1));
    ASSERT(!pthread_sigmask(SIG_BLOCK, &blockset, &oldset));
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;
    ASSERT(!timer_create(CLOCK_REALTIME, &sev, &timer));
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_nsec = 0;
    it.it_value.tv_sec = 0;
    it.it_value.tv_nsec = 1000000;
    ASSERT(!timer_settime(timer, 0, &it, NULL));
    ASSERT(!signal(SIGUSR1, handler));
    ASSERT(!signaled);
    ASSERT(!pthread_sigmask(SIG_SETMASK, &oldset, NULL));
    while (!signaled) asm volatile("" : : : "memory");
    timer_getoverrun(timer);
    ASSERT(!timer_delete(timer));

    ASSERT(!functioned);
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = function;
    ASSERT(!timer_create(CLOCK_REALTIME, &sev, &timer));
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_nsec = 0;
    it.it_value.tv_sec = 0;
    it.it_value.tv_nsec = 1000000;
    ASSERT(!timer_settime(timer, 0, &it, NULL));
    while (!functioned) asm volatile("" : : : "memory");
    timer_getoverrun(timer);
    ASSERT(!timer_delete(timer));
    
    return 0;
}

