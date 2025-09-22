// gcc -g -O2 -o parameter_ref parameter_ref.c

volatile int vv;

/* Don't inline, but do allow clone to create specialized versions.  */
static __attribute__((noinline)) int
foo (int x, int y, int z)
{
  int a = x * 2;
  int b = y * 2;
  int c = z * 2;
  vv++;
  return x + z;
}

int
main (int x, char **argv)
{
  return foo (x, 2, 3) + foo (x, 4, 3) + foo (x + 6, x, 3) + x;
}
