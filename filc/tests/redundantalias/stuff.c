int foo(void) { return 666; }

extern int bar(void) __attribute__((alias("foo")));

asm(".filc_alias foo, bar");

int stuff(void) { return 1410; }

extern int thingy(void) __attribute__((weak, alias("stuff")));

asm(".filc_weak_alias stuff, thingy");
