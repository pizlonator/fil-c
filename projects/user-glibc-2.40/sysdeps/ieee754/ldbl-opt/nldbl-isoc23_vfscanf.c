#include "nldbl-compat.h"

int
attribute_hidden
__isoc23_vfscanf (FILE *s, const char *fmt, va_list ap)
{
  return __nldbl___isoc23_vfscanf (s, fmt, ap);
}
