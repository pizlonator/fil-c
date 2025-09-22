// gcc -g -O2 -o entry_value entry_value.c
int __attribute__((noinline, noclone)) foo (int x, int y)
{
  return x + y;
}

int __attribute__((noinline, noclone)) bar (int x, int y)
{
  int z;
  z = foo (x, y);
  z += foo (y, x);
  return z;
}

int
main (int argc, char **argv)
{
  return bar (argc + 1, argc - 1);
}
