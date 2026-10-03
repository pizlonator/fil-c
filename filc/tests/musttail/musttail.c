#include <stdio.h>
#include <stdlib.h>
static long odd(long n, long acc);
static long even(long n, long acc) { if (n == 0) return acc; __attribute__((musttail)) return odd(n - 1, acc + 1); }
static long odd(long n, long acc) { if (n == 0) return -acc; __attribute__((musttail)) return even(n - 1, acc + 2); }
typedef char *(*fn)(char *, long);
static char *step(char *p, long n);
fn volatile next = step;
static char *step(char *p, long n) { if (n == 0) return p; p[n % 16] = 'a' + n % 26; __attribute__((musttail)) return next(p, n - 1); }
int main(void) {
  printf("%ld\n", even(10000001, 0));
  char *buf = calloc(17, 1);
  char *r = step(buf, 5000000);
  printf("%s %d\n", r, r == buf);
  return 0;
}
