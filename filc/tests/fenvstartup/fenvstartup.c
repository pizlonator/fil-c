#include <fenv.h>
#include <stdio.h>
int main(void) {
  printf("flags %d\n", fetestexcept(FE_ALL_EXCEPT));
  return 0;
}
