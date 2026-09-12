#define __log1p __log1p_fma
#define SECTION __attribute__ ((section (".text.fma")))

#include <sysdeps/ieee754/dbl-64/s_log1p.c>
