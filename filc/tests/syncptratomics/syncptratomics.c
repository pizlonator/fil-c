#include <assert.h>
#include <stdio.h>
static int x = 42, y = 7;
int *g, *h, *k;
int main(void) {
  assert(__sync_bool_compare_and_swap(&g, 0, &x));
  assert(!__sync_bool_compare_and_swap(&g, 0, &y));
  assert(g == &x && *g == 42);
  int *old = __sync_val_compare_and_swap(&h, 0, &y);
  assert(old == 0);
  int *failed = __sync_val_compare_and_swap(&h, &x, &x);
  assert(failed == &y && *failed == 7 && h == &y);
  __sync_lock_test_and_set(&k, &x);
  int *prev = __sync_lock_test_and_set(&k, &y);
  printf("%d %d %d %d %p\n", *g, *h, *k, *prev, (void *)old);
  return 0;
}
