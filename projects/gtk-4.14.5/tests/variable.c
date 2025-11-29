/* -*- mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include <math.h>
#include "variable.h"

void
variable_init (Variable *variable)
{
  variable->weight = 0.0;
  variable->sum = 0.0;
  variable->sum2 = 0.0;
}

void
variable_add_weighted (Variable *variable,
                       double    value,
                       double    weight)
{
  variable->weight += weight;
  variable->sum += weight * value;
  variable->sum2 += weight * value * value;
}

void
variable_add (Variable *variable,
              double    value)
{
  variable_add_weighted (variable, value, 1.);
}

double
variable_mean (Variable *variable)
{
  return variable->sum / variable->weight;
}

double
variable_standard_deviation (Variable *variable)
{
  double mean = variable_mean (variable);
  return sqrt (variable->sum2 / variable->weight - mean * mean);
}
