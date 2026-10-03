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

// __divdc3 scales the denominator by 2^-ilogbw where
// ilogbw = (int)logb(max(|c|,|d|)). For a denominator whose magnitude is
// below 1, ilogbw is negative and the scaling must enlarge the operands.
// A broken logb/scalbn instead underflows the denominator to zero, firing
// the nan-recovery branch and returning (inf, nan).
//
// Simplest case: 1 / 0.5 = 2 + 0i. The operands are volatile so the
// compiler emits a real runtime __divdc3 call instead of folding.

int main() {
  volatile double re = 0.5, im = 0.0;
  _Complex double q = 1.0 / (re + im * 1.0i);
  double qr = __real__ q;
  double qi = __imag__ q;
  if (!isfinite(qr) || !isfinite(qi))
    exit(1);
  if (qr != 2.0)
    exit(2);
  if (qi != 0.0)
    exit(3);
}
