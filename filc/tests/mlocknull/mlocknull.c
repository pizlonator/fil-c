#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
int main(void) {
  if (mlock(NULL, 4096) == -1)
    perror("mlock");
  return 0;
}
