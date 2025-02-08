static int foo(void)
{
    return 1410;
}

void* baz(void) { return foo; }

int bar(void) { return 42; }

asm(".filc_globl foo");
asm(".filc_weak bar");
