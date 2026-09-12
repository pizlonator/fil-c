#define __sincos __sincos_avx
#define SECTION __attribute__ ((section (".text.avx")))
#include <sysdeps/ieee754/dbl-64/s_sincos.c>
