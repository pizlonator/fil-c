#if defined SHARED && IS_IN (libc)
# define STRNCMP __strncmp_ppc
# undef libc_hidden_builtin_def
# define libc_hidden_builtin_def(name) \
    __hidden_ver1 (__strncmp_ppc, __GI_strncmp, __strncmp_ppc);
#endif
#include <string/strncmp.c>
