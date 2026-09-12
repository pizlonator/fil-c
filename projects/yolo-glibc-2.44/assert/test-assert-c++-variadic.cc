/* Test assert as a variadic macro for C++ code snippets.
   Copyright The GNU Toolchain Authors.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* This test requires C++26, and is compiled with -std=c++26
   if GCC version supports that, and no additional options
   otherwise. */
#if defined __cplusplus && __cplusplus > 202302L

#undef NDEBUG
#include <assert.h>

template <typename T1, typename T2>
bool
foo ()
{ return true; }

struct C
{
  C (int p, int r) : x (p + r) {}

  int x;
};

int
func ()
{
  return 1;
}

static void
test_enabled ()
{
  {
    assert (foo <int, float> ());
  }

  {
    assert (C {1, 2}.x > 0);
  }

  {
    int x = 10, y = 20;
    assert ([x, y] { return x < y; } ());
  }

  {
    /* Ill-formed, not an assigment expression. */
    // assert (func (), func ());
    assert ((func (), func ()));
  }
}

/* GCC PR118629 fixed the handling of assert inside requires clause.  */
#if __GNUC_PREREQ (14, 3) || defined __clang__

template <typename Ts>
constexpr bool
assert_works ()
{
  return requires (Ts ts) {
    assert (ts);
  };
}

enum OE { oe };
enum TE : int { te };
enum class SE : int { se };

static_assert ( assert_works <OE> ());
static_assert ( assert_works <TE> ());
static_assert (!assert_works <SE> ());

#endif /* __GNUC_PREREQ (14, 3) || defined __clang__ */

#define NDEBUG
#include <assert.h>

static void
test_disabled ()
{
  /* Assert is variadic, but ignores arguments */
  assert(1, 2);
  assert(+, 1, -, 2, *, 30);
}

static int
do_test ()
{
  test_enabled ();
  test_disabled ();
  return 0;
}

#else
#include <support/test-driver.h>

static int
do_test ()
{
  return EXIT_UNSUPPORTED;
}

#endif

#include <support/test-driver.c>
