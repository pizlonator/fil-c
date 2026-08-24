#include <stdio.h>
unsigned long mk(unsigned long *a);
int main(){ unsigned long a[1]={1000000}; printf("%lu\n", mk(a)); return 0; }  /* 1000000+197503143=198503143 */
