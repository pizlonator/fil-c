#include <stdio.h>
static int x = 42, y = 7;
int *g, *h, *k;
int main(void) {
  __sync_bool_compare_and_swap(&g, 0, &x);
  int *old = __sync_val_compare_and_swap(&h, 0, &y);
  __sync_lock_test_and_set(&k, &x);
  int *prev = __sync_lock_test_and_set(&k, &y);
  printf("%d %d %d %d %p\n", *g, *h, *k, *prev, (void *)old);
  return 0;
}
