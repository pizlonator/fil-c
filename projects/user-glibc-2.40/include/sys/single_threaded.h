#include <misc/sys/single_threaded.h>

#ifndef _ISOMAC

libc_hidden_proto_alias (__libc_single_threaded,
			 __libc_single_threaded_internal);

#if !defined SHARED || !IS_IN(libc)
# define __libc_single_threaded_internal __libc_single_threaded
#endif

#endif
