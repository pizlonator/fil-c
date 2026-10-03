// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: pthread create/join + mutex.
#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static int counter = 0;

static void* worker(void* arg) {
    (void)arg;
    for (int i = 0; i < 10; i++) {
        pthread_mutex_lock(&m);
        counter++;
        pthread_mutex_unlock(&m);
    }
    return NULL;
}

int main(void) {
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) != 0) {
        printf("create failed\n");
        return 1;
    }
    if (pthread_join(t, NULL) != 0) {
        printf("join failed\n");
        return 1;
    }
    for (int i = 0; i < 10; i++) {
        pthread_mutex_lock(&m);
        counter++;
        pthread_mutex_unlock(&m);
    }
    printf("counter=%d\n", counter);
    return counter == 20 ? 0 : 1;
}
