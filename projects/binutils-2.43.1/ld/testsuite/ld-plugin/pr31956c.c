#include <stdlib.h>

extern void *__real_malloc (size_t);

void *
__wrap_malloc (size_t n)
{
  if (n == 0)
    return NULL;
  else
    return __real_malloc (n);
};

int
main (void)
{
  void *ptr = malloc (30);
  return ptr == NULL ? 1 : 0;
}
