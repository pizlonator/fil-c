#define __sinh __sinh_fma

#define SECTION __attribute__ ((section (".text.fma")))

#include <sysdeps/ieee754/dbl-64/e_sinh.c>
