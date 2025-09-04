#include "nldbl-compat.h"

int
attribute_hidden
__isoc23_vscanf (const char *fmt, va_list ap)
{
  return __nldbl___isoc23_vfscanf (stdin, fmt, ap);
}
