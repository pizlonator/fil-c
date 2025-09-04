int
foo_v1 (int a)
{
  return 1;
}
asm (".symver foo_v1, foo@v1");

int
foo_v2 (int a)
{
  return 2;
}
asm (".symver foo_v2, foo@v2");

int
foo (int a)
{
  return 3;
}
asm (".symver foo, foo@@@v3");
