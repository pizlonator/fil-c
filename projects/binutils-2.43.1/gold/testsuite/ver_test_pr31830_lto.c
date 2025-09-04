extern __inline __attribute__((__gnu_inline__)) void foo(void) {}
extern void __collector_foo_2_2(void);
__attribute__((__symver__("foo@GLIBC_2.2.5")))
void __collector_foo_2_2(void) {}
void foo(void) {}
