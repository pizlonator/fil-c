#include "nldbl-compat.h"

int
attribute_hidden
__isoc23_sscanf (const char *s, const char *fmt, ...)
{
  va_list arg;
  int done;

  va_start (arg, fmt);
  done = __nldbl___isoc23_vsscanf (s, fmt, arg);
  va_end (arg);

  return done;
}
