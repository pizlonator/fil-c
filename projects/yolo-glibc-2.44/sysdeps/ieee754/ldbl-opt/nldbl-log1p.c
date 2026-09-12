#include "nldbl-compat.h"

double
attribute_hidden
log1pl (double x)
{
  return log1p (x);
}
weak_alias (log1pl, logp1l)
