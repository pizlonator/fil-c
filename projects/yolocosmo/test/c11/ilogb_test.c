// Copyright 2026 Justine Alexandra Roberts Tunney
//
// Permission to use, copy, modify, and/or distribute this software for
// any purpose with or without fee is hereby granted, provided that the
// above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
// WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
// AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
// DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
// PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <limits.h>
#include <math.h>
#include <stdlib.h>

// ilogb returns the integer binary exponent and must be negative for
// arguments below 1 (the regime that broke complex division). C99 7.12.6.5:
// ilogb(0) = FP_ILOGB0, ilogb(inf) = INT_MAX, ilogb(nan) = FP_ILOGBNAN.

int main() {
  volatile double x;
  x = 0.5;
  if (ilogb(x) != -1)
    exit(1);
  x = 0.2;
  if (ilogb(x) != -3)
    exit(2);
  x = 1.0;
  if (ilogb(x) != 0)
    exit(3);
  x = 1024.0;
  if (ilogb(x) != 10)
    exit(4);
  x = 0x1p-1050;  // subnormal
  if (ilogb(x) != -1050)
    exit(5);
  x = 0.0;
  if (ilogb(x) != FP_ILOGB0)
    exit(6);
  x = INFINITY;
  if (ilogb(x) != INT_MAX)
    exit(7);
  x = NAN;
  if (ilogb(x) != FP_ILOGBNAN)
    exit(8);
}
