#include "nldbl-compat.h"

double
attribute_hidden
pownl (double x, long long int y)
{
  return pown (x, y);
}
