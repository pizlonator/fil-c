/* We have to irritate the compiler a bit.  */
#define ____wcstoll_l_internal ____wcstoll_l_internal_XXX
#define __wcstoll_l ___wcstoll_l_XXX
#define wcstoll_l __wcstoll_l_XX
#define __isoc23_wcstoll_l __isoc23_wcstoll_l_XXX

#include <wcsmbs/wcstol_l.c>

#undef ____wcstoll_l_internal
#undef __wcstoll_l
#undef wcstoll_l
#undef __isoc23_wcstoll_l
strong_alias (____wcstol_l_internal, ____wcstoll_l_internal)
weak_alias (__wcstol_l, __wcstoll_l)
weak_alias (__wcstol_l, wcstoll_l)
weak_alias (__isoc23_wcstol_l, __isoc23_wcstoll_l)
libc_hidden_ver (__isoc23_wcstol_l, __isoc23_wcstoll_l)
