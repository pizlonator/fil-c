int __thread tls1[100]; /* Size > glibc.rtld.optional_static_tls / 2.  */
int *f1(void) { return tls1; }
