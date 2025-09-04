/* We have to irritate the compiler a bit.  */
#define __wcstoll_internal __wcstoll_internal_XXX
#define wcstoll wcstoll_XXX
#define wcstoq wcstoq_XXX
#define __isoc23_wcstoll __isoc23_wcstoll_XXX

#include <wcsmbs/wcstol.c>

#undef __wcstoll_internal
#undef wcstoll
#undef wcstoq
#undef __isoc23_wcstoll
strong_alias (__wcstol_internal, __wcstoll_internal)
libc_hidden_ver (__wcstol_internal, __wcstoll_internal)
weak_alias (wcstol, wcstoll)
weak_alias (wcstol, wcstoq)
weak_alias (wcstol, wcstoimax)
weak_alias (__isoc23_wcstol, __isoc23_wcstoll)
libc_hidden_ver (__isoc23_wcstol, __isoc23_wcstoll)
weak_alias (__isoc23_wcstol, __isoc23_wcstoimax)
