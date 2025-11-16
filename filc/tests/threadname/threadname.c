#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void *thread_func(void *arg) {
  const char *new_name = "New Name";
  if (pthread_setname_np(pthread_self(), new_name) != 0) {
    perror("pthread_setname_np"); return NULL;
  }
  char buf[16];
  if (pthread_getname_np(pthread_self(), buf, sizeof(buf)) != 0) {
     perror("pthread_getname_np"); return NULL;
  }
  printf("Thread name is: \"%s\"\n", buf);
  return NULL;
}

int main(void) {
  pthread_t tid;
  if (pthread_create(&tid, NULL, thread_func, NULL) != 0) {
    perror("pthread_create"); return EXIT_FAILURE;
  }
  pthread_join(tid, NULL); return EXIT_SUCCESS;
}
