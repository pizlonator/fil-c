// gcc -g -O2 -o implicit_pointer implicit_pointer.c

static __attribute__((noinline, noclone)) int foo (int i)
{
  int *p = &i;
  return *p;
}

int main (void)
{
  return foo (23) - 23;
}
