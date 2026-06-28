#include <stdfil.h>
#include <ucontext.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

static ucontext_t ctx_main;
static ucontext_t ctx_func1;
static pthread_mutex_t lock;
static pthread_cond_t cond;
static bool ready;
static pthread_t main_thread;
static pthread_t started_thread;
static bool ok;

static void func1(void)
{
    zprintf("in func1\n");
    ZASSERT(pthread_self() == started_thread);
    swapcontext(&ctx_func1, &ctx_main);
    zprintf("in func1 again!\n");
    ZASSERT(pthread_self() == main_thread);
    ok = true;
}

static void* thread_main(void* arg)
{
    zprintf("in thread\n");
    getcontext(&ctx_func1);
    ctx_func1.uc_stack.ss_size = 16384;
    makecontext(&ctx_func1, func1, 0);
    zprintf("switching to func1\n");
    swapcontext(&ctx_main, &ctx_func1);
    zprintf("back from func1\n");
    pthread_mutex_lock(&lock);
    ready = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
    return NULL;
}

static void exit_handler(void)
{
    zprintf("in exit_handler\n");
    ZASSERT(ok);
}

int main()
{
    atexit(exit_handler);
    
    main_thread = pthread_self();
    
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    
    pthread_create(&started_thread, NULL, thread_main, NULL);

    pthread_mutex_lock(&lock);
    while (!ready)
        pthread_cond_wait(&cond, &lock);
    pthread_mutex_unlock(&lock);

    setcontext(&ctx_func1);
    ZASSERT(!"Should not be reached");

    return 0;
}

