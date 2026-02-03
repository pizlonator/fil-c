/*
 * Copyright (c) 2004
 * Kevin Atkinson
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation. I make no representations about
 * the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * This code was originally adopted from the slist implementation
 * found in the SGI STL under the following copyright:
 *
 * Copyright (c) 1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef ACOMMON_LSORT__HPP
#define ACOMMON_LSORT__HPP

namespace acommon {

using std::swap;

template <class N>
struct Next {
  N * & operator() (N * n) const {return n->next;}
};
template <class N>
struct Less {
  bool operator() (N * x, N * y) const {return x->data < y->data;}
};


template <class N, class LT, class NX>
static inline N * merge(N * x, N * y, const LT & lt, const NX & nx)
{
  if (lt(y,x)) swap(x,y);
  N * first = x;
  while (nx(x) && y) {
    if (lt(y,nx(x))) {
      N * xn = nx(x);
      N * yn = nx(y);
      nx(x) = y;
      nx(y) = xn;
      y = yn;
    }
    x = nx(x);
  }
  if (y) {
    nx(x) = y;
  }
  return first;
}

// THIS is SLOWER!!!
//  and even slower when condational move is used!!!!
template <class N, class LT, class NX>
static inline N * merge1(N * x, N * y, const LT & lt, const NX & nx)
{
  N * * cur = lt(x,y) ? &x : &y;
  N * first = *cur;
  N * last = *cur;
  *cur = nx(*cur);
  while (x && y) {
    cur = lt(x,y) ? &x : &y;
    nx(last) = *cur;
    last = *cur;
    *cur = nx(*cur);
  }
  if (x) {nx(last) = x;}
  else if (y) {nx(last) = y;}
  return first;
}

template <class N, class LT>
static inline N * merge(N * x, N * y, const LT & lt)
{
  return sort(x, y, lt, Next<N>());
}

template <class N>
static inline N * merge(N * x, N * y)
{
  return sort(x, y, Less<N>(), Next<N>());
}

template <class N, class LT, class NX>
N * sort(N * first, const LT & lt, const NX & nx)
{
  if (!first) return first;

  N * carry = 0;
  N * counter[sizeof(void *)*8] = {0};
  int fill = 0;

  while (first) {

    N * tmp = nx(first);
    nx(first) = carry;
    carry = first;
    first = tmp;

    int i = 0;
    while (i < fill && counter[i]) {
      carry = merge(counter[i], carry, lt, nx);
      counter[i] = 0;
      ++i;
    }

    swap(carry, counter[i]);

    if (i == fill) {
      ++fill;
    }
  }

  for (int i = 1; i < fill; ++i) {
    if (!counter[i]) counter[i] = counter[i-1];
    else if (counter[i-1]) counter[i] = merge(counter[i], counter[i-1], lt, nx);
  }

  return counter[fill-1];
}

template <class N, class LT>
static inline N * sort(N * first, const LT & lt)
{
  return sort(first, lt, Next<N>());
}

template <class N>
static inline N * sort(N * first)
{
  return sort(first, Less<N>(), Next<N>());
}


template <class N>
static inline N * fix_links(N * cur)
{
  N * prev = 0;
  while (cur) {
    cur->prev = prev;
    prev = cur;
    cur = cur->next;
  }
}

}

#endif

