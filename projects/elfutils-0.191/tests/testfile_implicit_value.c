// gcc -m32 -g -O2 -o implicit_value implicit_value.c

static __attribute__((noinline, noclone)) int foo ()
{
  unsigned long long a[] = { 2, 21 };
  return a[0] * a[1];
}

int main (void)
{
  return foo () - 42;
}
