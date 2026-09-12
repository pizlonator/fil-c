#include "nldbl-compat.h"

int
attribute_hidden
__isoc23_fscanf (FILE *stream, const char *fmt, ...)
{
  va_list arg;
  int done;

  va_start (arg, fmt);
  done = __nldbl___isoc23_vfscanf (stream, fmt, arg);
  va_end (arg);

  return done;
}
