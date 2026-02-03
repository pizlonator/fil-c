// This file is part of The New Aspell
// Copyright (C) 2002 by Melvin Hadasht and Kevin Atkinson under the
// GNU LGPL license version 2.0 or 2.1.  You should have received a
// copy of the LGPL license along with this library if you did not you
// can find it at http://www.gnu.org/.

#include <stdlib.h>

#include "strtonum.hpp"
#include "asc_ctype.hpp"

namespace acommon {

  static double strtodbl_c(const char * nptr, const char ** endptr)
  {
    double x = 0.0;
    double y = 0.0;
    double decimal = 1.0;
    int negative = 0;
    const char * str = nptr;

    while (asc_isspace(*str))
      str++;
    if (!*str)
      goto END_STRTODBL_C;
    if (*str == '-') {
      negative = 1;
      str++;
    } else if (*str == '+')
      str++;
    if (!*str)
      goto END_STRTODBL_C;
    while (*str >= '0' && *str <= '9') {
      x = x * 10.0 + (*str - '0');
      str++;
    }
    if (!*str || *str != '.')
      goto END_STRTODBL_C;
    str++;  
    decimal = 1.0;
    while (*str >= '0' && *str <= '9') {
      decimal *= 0.1;
      y = y + (*str - '0')*decimal;
      str++;
    }
  END_STRTODBL_C:
    if (endptr)
      *endptr = (char *) str;
    return negative ? -(x + y) : (x + y);
  }

  double strtod_c(const char * nptr, const char ** endptr)
  {
    double x;
    const char * eptr;
    x = strtodbl_c(nptr, &eptr);
    if (*eptr == 'E' || *eptr == 'e') {
      const char *nptr2 = eptr;
      long int y, i;
      double e = 1.0;
      nptr2++;
      y = strtol(nptr2, (char **)&eptr, 10);
      if (y) {
        for (i=0; i < ( y < 0 ? -y : y); i++)
          e *= 10.0;
        x =  (y < 0) ? x / e : x * e;
      }
    }
    if (endptr)
      *endptr = eptr;
    return x;
  }

  long strtoi_c(const char * npter, const char ** endptr) {

    char * str = (char*)npter;
    long num = 0;
    long sign = 1;
 
    *endptr = str;
    while (asc_isspace(*str)) {
      str++;
    }
    if (*str == '-' || *str == '+') {
      sign = *(str++) == '-' ? -1 : 1;
    }
    while (*str >= '0' && *str <= '9' ) {
      num = num * 10 + (long)(*str - '0');
      str++;
    }
    *endptr = str;
    return num;
  }
}

