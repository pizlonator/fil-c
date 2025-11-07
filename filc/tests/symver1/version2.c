int foo_v1(void)
{
    return 1410;
}

int foo_v2(void)
{
    return 666;
}

asm(".symver foo_v1, foo@v1");
asm(".filc_symver foo_v2, foo@@v2");
