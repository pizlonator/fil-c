#define COSF __cosf_sse2
#define SECTION __attribute__ ((section (".text.sse2")))
#include <sysdeps/ieee754/flt-32/s_cosf.c>
