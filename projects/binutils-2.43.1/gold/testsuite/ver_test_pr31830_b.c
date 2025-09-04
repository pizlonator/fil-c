extern void __collector_foo_2_2(void);
__attribute__((__symver__("foo@GLIBC_2.2.5")))
void __collector_foo_2_2(void) {}
