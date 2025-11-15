#include <strings.h>
#include <stdlib.h>
char *foo(const char *p,int n) { char *x = malloc(n); bzero(x, n); bcopy(p,x,n); return x; }
int main() { foo("hello",6); return 0; }
