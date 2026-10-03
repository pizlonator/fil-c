#ifndef COSMOPOLITAN_LIBC_SYSV_CONSTS_MADV_H_
#define COSMOPOLITAN_LIBC_SYSV_CONSTS_MADV_H_

#define MADV_NORMAL     0
#define MADV_RANDOM     1
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED   3
#define MADV_DONTNEED   4

/* Fil-C additions: the rest of the Linux madvise() advice values (mirroring
   musl's sys/mman.h), which the Fil-C runtime passes through. */
#define MADV_FREE          8
#define MADV_REMOVE        9
#define MADV_DONTFORK      10
#define MADV_DOFORK        11
#define MADV_MERGEABLE     12
#define MADV_UNMERGEABLE   13
#define MADV_HUGEPAGE      14
#define MADV_NOHUGEPAGE    15
#define MADV_DONTDUMP      16
#define MADV_DODUMP        17
#define MADV_WIPEONFORK    18
#define MADV_KEEPONFORK    19
#define MADV_COLD          20
#define MADV_PAGEOUT       21
#define MADV_POPULATE_READ 22
#define MADV_POPULATE_WRITE 23
#define MADV_DONTNEED_LOCKED 24
#define MADV_SOFT_OFFLINE  101
#define MADV_HWPOISON      100

#endif /* COSMOPOLITAN_LIBC_SYSV_CONSTS_MADV_H_ */
