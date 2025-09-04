#ifndef _LIBC_SYS_SYSLOG_H
#define _LIBC_SYS_SYSLOG_H 1
#include <misc/sys/syslog.h>
#ifndef _ISOMAC

#include <bits/syslog-decl.h>

libc_hidden_ldbl_proto (syslog)

/* __vsyslog_internal uses the same mode_flags bits as
   __v*printf_internal; see libio/libioP.h.  */
extern void __vsyslog_internal (int pri, const char *fmt, __gnuc_va_list ap,
				unsigned int mode_flags)
     attribute_hidden
     __attribute__ ((__format__ (__printf__, 2, 0)));

libc_hidden_ldbl_proto (__syslog_chk)

#endif /* _ISOMAC */
#endif /* syslog.h */
