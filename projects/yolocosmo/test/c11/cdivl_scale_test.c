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

// Long-double complex division (__divtc3) uses the same logb/scalbn
// rescaling. On x86-64 long double is 80-bit; the generic __compiler_rt_logbl
// only handles ieee754 binary types and otherwise defers to libm logbl, so
// this guards both paths. 1 / 0.5L must be 2 + 0i.

int main() {
  volatile long double re = 0.5L, im = 0.0L;
  _Complex long double q = 1.0L / (re + im * 1.0il);
  long double qr = __real__ q;
  long double qi = __imag__ q;
  if (!isfinite(qr) || !isfinite(qi))
    exit(1);
  if (qr != 2.0L)
    exit(2);
  if (qi != 0.0L)
    exit(3);
}
