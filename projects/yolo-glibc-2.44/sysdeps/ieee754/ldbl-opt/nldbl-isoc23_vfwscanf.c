#include "nldbl-compat.h"

int
attribute_hidden
__isoc23_vfwscanf (FILE *s, const wchar_t *fmt, va_list ap)
{
  return __nldbl___isoc23_vfwscanf (s, fmt, ap);
}
