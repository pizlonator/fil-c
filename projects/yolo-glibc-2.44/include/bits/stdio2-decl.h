#include <libio/bits/stdio2-decl.h>

#ifndef _ISOMAC
libc_hidden_proto (__fgets_unlocked_chk)
libc_hidden_ldbl_proto (vfprintf)
extern int __vasprintf_chk (char **, int, const char *, __gnuc_va_list) __THROW;
libc_hidden_ldbl_proto (__vasprintf_chk)
extern int __vfprintf_chk (FILE *, int, const char *, __gnuc_va_list);
libc_hidden_ldbl_proto (__vfprintf_chk)
#endif
