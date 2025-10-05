#include <pthread.h>
#include <stdlib.h>

#define NTHREADS 10
#define COUNT 100000

int*_Atomic*_Atomic ptr;

static void* thread_main(void* arg)
{
    unsigned n;
    for (n = COUNT; n--;) {
        if ((n & 1)) {
            ptr = malloc(sizeof(int*_Atomic));
            continue;
        }
        for (;;) {
            int*_Atomic* my_ptr = ptr;
            if (!my_ptr) {
                int*_Atomic* new_ptr = malloc(sizeof(int*_Atomic));
                if (__c11_atomic_compare_exchange_weak(ptr, &my_ptr, new_ptr,
                                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                    break;
                continue;
            }
            int*_Atomic inner_ptr = *my_ptr;
            if (!inner_ptr) {
                int* new_ptr = malloc(sizeof(int));
                if (__c11_atomic_compare_exchange_weak(ptr, &inner_ptr, new_ptr,
                                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                    break;
                continue;
            }
            *inner_ptr = 666;
            break;
        }
    }
    
    return NULL;
}

int main()
{
    pthread_t* threads = malloc(sizeof(pthread_t) * NTHREADS);
    unsigned n;
    for (n = NTHREADS; n--;)
        pthread_create(threads + n, NULL, thread_main, NULL);
    for (n = NTHREADS; n--;)
        pthread_join(threads[n], NULL);
    return 0;
}

