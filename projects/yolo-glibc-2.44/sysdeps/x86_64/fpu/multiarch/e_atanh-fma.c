#define __ieee754_atanh __ieee754_atanh_fma
#define __log1p __log1p_fma

#define SECTION __attribute__ ((section (".text.fma")))

#include <sysdeps/ieee754/dbl-64/e_atanh.c>
