#include "nldbl-compat.h"

double
attribute_hidden
atan2pil (double x, double y)
{
  return atan2pi (x, y);
}
