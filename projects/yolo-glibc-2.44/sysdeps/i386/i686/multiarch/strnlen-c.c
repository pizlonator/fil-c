#define STRNLEN  __strnlen_ia32
#include <string/strnlen.c>

#ifdef SHARED
/* Alias for internal symbol to avoid PLT generation, it redirects the
   libc_hidden_def (__strnlen/strlen) to default implementation.  */
__hidden_ver1 (__strnlen_ia32, __GI_strnlen, __strnlen_ia32);
strong_alias (__strnlen_ia32, __strnlen_ia32_1);
__hidden_ver1 (__strnlen_ia32_1, __GI___strnlen, __strnlen_ia32_1);
#endif
