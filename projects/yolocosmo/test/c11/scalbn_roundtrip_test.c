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

#include <math.h>
#include <stdlib.h>

// scalbn(x,n) = x * 2^n exactly for in-range results. __divdc3 uses it to
// undo the logb-derived scaling, so scalbn must agree with logb: for any
// finite nonzero x, scalbn(x, -(int)logb(x)) lands in [1,2).

int main() {
  volatile double x;

  // exact powers of two
  x = 0.5;
  if (scalbn(x, 1) != 1.0)
    exit(1);
  if (scalbn(x, -1) != 0.25)
    exit(2);
  x = 1.0;
  if (scalbn(x, 10) != 1024.0)
    exit(3);
  if (scalbn(x, -10) != 0x1p-10)
    exit(4);

  // round trip against logb across magnitudes
  static double vals[] = {1e-300, 1e-30, 0.1, 0.2, 0.5, 0.75,
                          1.0,    3.0,   7.0, 1e30, 1e300};
  int n = sizeof(vals) / sizeof(vals[0]);
  for (int i = 0; i < n; ++i) {
    x = vals[i];
    int e = (int)logb(x);
    double m = scalbn(x, -e);  // normalize mantissa
    if (!(fabs(m) >= 1.0 && fabs(m) < 2.0))
      exit(10 + i);
    if (scalbn(m, e) != x)  // exact inverse
      exit(30 + i);
  }
}
