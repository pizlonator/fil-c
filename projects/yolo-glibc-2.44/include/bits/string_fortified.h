#ifndef _ISOMAC
# if IS_IN(libc) && defined SHARED
/* Redirect calls from __builtin_stpcpy_chk to internal __stpcpy when building
   with fortify enable.  */
__asm__ ("stpcpy = __GI___stpcpy");
# endif

/* Add the internal aliass attribute to symbol before they first usage on the
   fortify wrappers.  */
libc_hidden_builtin_proto (memcpy)
libc_hidden_builtin_proto (mempcpy)
libc_hidden_builtin_proto (memmove)
libc_hidden_builtin_proto (memset)
#endif

#include <string/bits/string_fortified.h>
