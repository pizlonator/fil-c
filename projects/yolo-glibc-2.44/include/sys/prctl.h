#ifndef _SYS_PRCTL_H
#include_next <sys/prctl.h>

# ifndef _ISOMAC

#  ifndef PR_SET_VMA
#   define PR_SET_VMA            0x53564d41
#   define PR_SET_VMA_ANON_NAME  0
#  endif

extern int __prctl (int __option, ...);
libc_hidden_proto (__prctl)

# endif /* !_ISOMAC */
#endif
