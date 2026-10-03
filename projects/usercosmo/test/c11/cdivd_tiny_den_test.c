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

// (1 + i) / (1e-300 + 1e-300i) = 1e300 + 0i exactly. The whole point of
// __divdc3's logb/scalbn rescaling is to keep c*c+d*d from underflowing to
// zero when the denominator is tiny. A broken rescale lets the denominator
// underflow and returns (inf, inf).

int main() {
  volatile double ar = 1.0, ai = 1.0, cr = 1e-300, ci = 1e-300;
  _Complex double a = ar + ai * 1.0i;
  _Complex double c = cr + ci * 1.0i;
  _Complex double q = a / c;
  double qr = __real__ q;
  double qi = __imag__ q;
  if (!isfinite(qr) || !isfinite(qi))
    exit(1);
  // a/c = (1+i)/((1+i)*1e-300) = 1/1e-300 = 1e300
  if (!(qr > 0.9e300 && qr < 1.1e300))
    exit(2);
  if (qi != 0.0)
    exit(3);
}
