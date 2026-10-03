#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
  int a = fetestexcept(FE_ALL_EXCEPT);
  feclearexcept(FE_ALL_EXCEPT);
  void *v[1000]; for (int i = 0; i < 1000; i++) v[i] = malloc(16 + i * 64);
  int b = fetestexcept(FE_ALL_EXCEPT);
  for (int i = 0; i < 1000; i++) free(v[i]);
  int c = fetestexcept(FE_ALL_EXCEPT);
  printf("start %d after-malloc %d after-free %d\n", a, b, c);
  return 0;
}
