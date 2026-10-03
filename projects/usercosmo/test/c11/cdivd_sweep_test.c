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

// Compute z/z for a wide range of magnitudes, spanning denominators far
// below and far above 1. z/z must be ~1+0i for every finite nonzero z. The
// bug lived entirely in the max(|c|,|d|) < 1 regime, so this sweep pins down
// that the exponent-bias fix holds across scales. We allow a few ULP of
// slack because the imaginary part is a near-perfect cancellation that FMA
// contraction (e.g. on aarch64) leaves as a tiny rounding residual rather
// than exact zero.

static double values[] = {
    1e-300, 1e-200, 1e-100, 1e-30, 1e-10, 1e-3, 0.1, 0.25,
    0.5,    0.75,   0.999,  1.0,   1.001, 2.0,  10.0, 1e3,
    1e10,   1e30,   1e100,  1e200, 1e300,
};

int main() {
  int n = sizeof(values) / sizeof(values[0]);
  for (int i = 0; i < n; ++i) {
    volatile double re = values[i];
    volatile double im = values[i] * 0.5;
    _Complex double z = re + im * 1.0i;
    _Complex double q = z / z;
    double qr = __real__ q;
    double qi = __imag__ q;
    if (!isfinite(qr) || !isfinite(qi))
      exit(1 + i);
    if (fabs(qr - 1.0) > 1e-12 || fabs(qi) > 1e-12)
      exit(100 + i);
  }
}
