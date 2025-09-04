/* We have to irritate the compiler a bit.  */
#define __wcstoull_internal __wcstoull_internal_XXX
#define wcstoull wcstoull_XXX
#define wcstouq wcstouq_XXX
#define __isoc23_wcstoull __isoc23_wcstoull_XXX

#include <wcsmbs/wcstoul.c>

#undef __wcstoull_internal
#undef wcstoull
#undef wcstouq
#undef __isoc23_wcstoull
strong_alias (__wcstoul_internal, __wcstoull_internal)
libc_hidden_ver (__wcstoul_internal, __wcstoull_internal)
weak_alias (wcstoul, wcstoull)
weak_alias (wcstoul, wcstouq)
weak_alias (wcstoul, wcstoumax)
weak_alias (__isoc23_wcstoul, __isoc23_wcstoull)
libc_hidden_ver (__isoc23_wcstoul, __isoc23_wcstoull)
weak_alias (__isoc23_wcstoul, __isoc23_wcstoumax)
