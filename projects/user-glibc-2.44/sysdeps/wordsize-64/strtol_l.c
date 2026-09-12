/* We have to irritate the compiler a bit.  */
#define ____strtoll_l_internal ____strtoll_l_internal_XXX
#define __strtoll_l __strtoll_l_XXX
#define strtoll_l strtoll_l_XXX
#define __isoc23_strtoll_l __isoc23_strtoll_l_XXX

#include <stdlib/strtol_l.c>

#undef ____strtoll_l_internal
#undef __strtoll_l
#undef strtoll_l
#undef __isoc23_strtoll_l
strong_alias (____strtol_l_internal, ____strtoll_l_internal)
libc_hidden_ver (____strtol_l_internal, ____strtoll_l_internal)
weak_alias (__strtol_l, __strtoll_l)
weak_alias (__strtol_l, strtoll_l)
weak_alias (__isoc23_strtol_l, __isoc23_strtoll_l)
libc_hidden_ver (__isoc23_strtol_l, __isoc23_strtoll_l)
