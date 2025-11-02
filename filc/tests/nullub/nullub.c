#include <stdio.h>
#include <stdfil.h>

int main() {
  void *p = zandptr((void *)1, ~1ULL);
  if (p)
    puts("p is not null");
  else
    puts("p is null");
  return 0;
}
