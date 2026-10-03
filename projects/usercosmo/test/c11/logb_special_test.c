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

// C99 7.12.6.11: logb(0) = -inf, logb(+-inf) = +inf, logb(nan) = nan,
// and logb(-x) = logb(x). Sign of the argument is ignored.

int main() {
  volatile double x;
  x = 0.0;
  if (!isinf(logb(x)) || logb(x) > 0)  // -inf
    exit(1);
  x = -0.0;
  if (!isinf(logb(x)) || logb(x) > 0)  // -inf
    exit(2);
  x = INFINITY;
  if (!isinf(logb(x)) || logb(x) < 0)  // +inf
    exit(3);
  x = -INFINITY;
  if (!isinf(logb(x)) || logb(x) < 0)  // +inf
    exit(4);
  x = NAN;
  if (!isnan(logb(x)))
    exit(5);
  x = -8.0;
  if (logb(x) != 3.0)  // sign ignored
    exit(6);
  x = -0.5;
  if (logb(x) != -1.0)
    exit(7);
}
