#include "nldbl-compat.h"

int
attribute_hidden
__isoc23_swscanf (const wchar_t *s, const wchar_t *fmt, ...)
{
  va_list arg;
  int done;

  va_start (arg, fmt);
  done = __nldbl___isoc23_vswscanf (s, fmt, arg);
  va_end (arg);

  return done;
}
