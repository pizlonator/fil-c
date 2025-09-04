#define __sincos __sincos_fma4
#define SECTION __attribute__ ((section (".text.fma4")))
#include <sysdeps/ieee754/dbl-64/s_sincos.c>
