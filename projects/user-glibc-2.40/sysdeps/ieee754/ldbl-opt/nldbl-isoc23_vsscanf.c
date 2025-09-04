#include "nldbl-compat.h"

int
attribute_hidden
__isoc23_vsscanf (const char *string, const char *fmt, va_list ap)
{
  return __nldbl___isoc23_vsscanf (string, fmt, ap);
}
