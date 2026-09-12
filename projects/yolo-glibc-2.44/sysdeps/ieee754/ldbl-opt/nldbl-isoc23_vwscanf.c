#include "nldbl-compat.h"

int
attribute_hidden
__isoc23_vwscanf (const wchar_t *fmt, va_list ap)
{
  return __nldbl___isoc23_vfwscanf (stdin, fmt, ap);
}
