int __thread tls2;
int *f2(void) { return &tls2; }
