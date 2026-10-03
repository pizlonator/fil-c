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

#include <complex.h>
#include <math.h>
#include <stdlib.h>

// (0 + 0.2i) / (0 - 0.2i) = -1 + 0i. The denominator has a zero real part
// and magnitude below 1, so __divdc3 must scale up by 2^-ilogbw with a
// negative ilogbw. A broken logb returns the wrong exponent and yields a
// non-finite result instead of the exact -1.

int main() {
  volatile double ar = 0.0, ai = 0.2, cr = 0.0, ci = -0.2;
  _Complex double a = ar + ai * 1.0i;
  _Complex double c = cr + ci * 1.0i;
  _Complex double q = a / c;
  double qr = __real__ q;
  double qi = __imag__ q;
  if (!isfinite(qr) || !isfinite(qi))
    exit(1);
  if (qr != -1.0)
    exit(2);
  if (qi != 0.0)
    exit(3);
}
