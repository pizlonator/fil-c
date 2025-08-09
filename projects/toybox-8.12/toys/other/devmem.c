/* devmem.c - Access physical addresses
 *
 * Copyright 2019 The Android Open Source Project

USE_DEVMEM(NEWTOY(devmem, "<1(no-sync)(no-mmap)f:", TOYFLAG_USR|TOYFLAG_SBIN))

config DEVMEM
  bool "devmem"
  default y
  help
    usage: devmem [-f FILE] ADDR [WIDTH [DATA...]]

    Read/write physical addresses. WIDTH is 1, 2, 4, or 8 bytes (default 4).
    Prefix ADDR with 0x for hexadecimal, output is in same base as address.

    -f FILE		File to operate on (default /dev/mem)
    --no-sync	Don't open the file with O_SYNC (for cached access)
    --no-mmap	Don't mmap the file
*/

#define FOR_devmem
#include "toys.h"

GLOBALS(
  char *f;
)

unsigned long xatolu(char *str, int bytes)
{
  char *end = str;
  unsigned long lu;

  errno = 0;
  lu = strtoul(str, &end, 0);
  // Report out of range values as errors rather than truncating.
  if (errno == ERANGE || lu > (~0UL)>>(sizeof(long)-bytes)*8)
    error_exit("%s>%d bytes", str, bytes);
  if (*end || errno) perror_exit("bad %s", str);

  return lu;
}

void devmem_main(void)
{
  int ii, writing = toys.optc > 2, bytes = 4, fd;
  unsigned long data QUIET, map_len QUIET,
    addr = xatolu(*toys.optargs, sizeof(long));
  void *map QUIET, *p QUIET;

  // WIDTH?
  if (toys.optc>1) {
    char *sizes = sizeof(long)==8 ? "1248" : "124";

    if ((ii = stridx(sizes, *toys.optargs[1]))==-1 || toys.optargs[1][1])
      error_exit("bad width: %s", toys.optargs[1]);
    bytes = 1<<ii;
  }

  // Map in just enough.
  if (CFG_TOYBOX_FORK) {
    fd = xopen(TT.f ? : "/dev/mem", O_RDWR*writing+O_SYNC*!FLAG(no_sync));
    if (FLAG(no_mmap)) xlseek(fd, addr, SEEK_SET);
    else {
      unsigned long long page_size = sysconf(_SC_PAGESIZE)-1, map_off;

      map_off = addr & ~page_size;
      map_len = addr + (writing ? (toys.optc - 2) * bytes : bytes) - map_off;
      map = xmmap(0, map_len, writing ? PROT_WRITE : PROT_READ, MAP_SHARED, fd,
          map_off);
      p = map+(addr&page_size);
      close(fd);
    }
  } else p = (void *)addr;

  // Not using peek()/poke() because registers care about size of read/write.
  if (writing) {
    for (ii = 2; ii<toys.optc; ii++) {
      data = xatolu(toys.optargs[ii], bytes);
      if (FLAG(no_mmap)) xwrite(fd, &data, bytes);
      else {
        if (bytes==1) *(char *)p = data;
        else if (bytes==2) *(unsigned short *)p = data;
        else if (bytes==4) *(unsigned int *)p = data;
        else if (sizeof(long)==8 && bytes==8) *(unsigned long *)p = data;
        p += bytes;
      }
    }
  } else {
    if (FLAG(no_mmap)) xread(fd, &data, bytes);
    else {
      if (bytes==1) data = *(char *)p;
      else if (bytes==2) data = *(unsigned short *)p;
      else if (bytes==4) data = *(unsigned int *)p;
      else if (sizeof(long)==8 && bytes==8) data = *(unsigned long *)p;
    }
    printf((!strchr(*toys.optargs, 'x')) ? "%0*ld\n" : "0x%0*lx\n",
      bytes*2, data);
  }

  if (CFG_TOYBOX_FORK) {
    if (FLAG(no_mmap)) close(fd);
    else munmap(map, map_len);
  }
}
