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

// Single-precision complex division shares the templated compiler_rt
// rescaling logic (__divsc3), so it has the same sub-unit denominator
// failure mode. 1 / 0.5f must be 2 + 0i.

int main() {
  volatile float re = 0.5f, im = 0.0f;
  _Complex float q = 1.0f / (re + im * 1.0if);
  float qr = __real__ q;
  float qi = __imag__ q;
  if (!isfinite(qr) || !isfinite(qi))
    exit(1);
  if (qr != 2.0f)
    exit(2);
  if (qi != 0.0f)
    exit(3);
}
