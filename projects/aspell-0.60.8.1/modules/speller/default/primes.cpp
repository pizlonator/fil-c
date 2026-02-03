// Copyright (c) 2000
// Kevin Atkinson
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without
// fee, provided that the above copyright notice appear in all copies
// and that both that copyright notice and this permission notice
// appear in supporting documentation. Kevin Atkinson makes no
// representations about the suitability of this software for any
// purpose.  It is provided "as is" without express or implied
// warranty.

#include "primes.hpp"
#include <cmath>
#include <cassert>

namespace aspeller {

  using namespace std;

  void Primes::resize(size_type s) {
    size_type i, j = 2;
    data.resize(s);
    for(i = 0; i < s; ++i) 
      data[i] = true;
    if (s > 0)
      data[0] = false;
    if (s > 1)
      data[1] = false;
    size_type sqrt_s = static_cast<size_type>(sqrt(static_cast<double>(s)));
    while (j < sqrt_s) {
      for (i = 2*j; i < s; i += j) {
	data[i] = false;
      }
      ++j;
      for (;j < sqrt_s && !data[j]; ++j);
    }
  }

  bool Primes::is_prime(size_type n) const {
    if (n < size()) {
      return data[n];
    } else {
      size_type e = static_cast<size_type>(sqrt(static_cast<double>(n)));
      assert(e < size());
      for (const_iterator i = begin(); *i <= e; ++i) 
	if (!(n % *i)) return false;
      return true;
    }
  }
}
