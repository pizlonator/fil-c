#include <sysdeps/unix/sysv/linux/bits/mman-shared.h>

#ifndef _ISOMAC

extern __typeof (pkey_alloc) __pkey_alloc;
libc_hidden_proto (__pkey_alloc)
extern __typeof (pkey_free) __pkey_free;
libc_hidden_proto (__pkey_free)
extern __typeof (pkey_mprotect) __pkey_mprotect;
libc_hidden_proto (__pkey_mprotect)
extern __typeof (pkey_get) __pkey_get;
libc_hidden_proto (__pkey_get)
extern __typeof (pkey_set) __pkey_set;
libc_hidden_proto (__pkey_set)

#endif
