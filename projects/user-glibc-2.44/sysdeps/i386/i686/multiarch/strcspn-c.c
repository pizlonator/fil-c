#if IS_IN (libc)
# define __strcspn_generic __strcspn_ia32
# include <sysdeps/x86_64/multiarch/strcspn-sse4.c>
#endif
