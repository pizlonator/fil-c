#define __sincos __sincos_fma
#define SECTION __attribute__ ((section (".text.fma")))
#include <sysdeps/ieee754/dbl-64/s_sincos.c>
