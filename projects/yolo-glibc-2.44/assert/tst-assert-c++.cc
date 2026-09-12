/* Tests for interactions between C++ and assert.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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

/* Undefine NDEBUG to ensure the build system e.g. CFLAGS/CXXFLAGS
   does not disable the asserts we want to test.  */
#undef NDEBUG
#include <assert.h>

#if __GNUC_PREREQ (5, 0)
template <typename> struct is_void { static const bool value = false; };
template <> struct is_void <void> { static const bool value = true; };

static_assert(is_void <decltype (assert (""))>::value, "type is void");

/* The C++ standard requires that if the assert argument is a constant
   subexpression, then the assert itself is one, too.  */
constexpr int
check_constexpr ()
{
  return (assert (true), 1);
}

/* Objects of this class can be contextually converted to bool, but
   cannot be compared to int.  */
struct no_int
{
  no_int () = default;
  no_int (const no_int &) = delete;

  explicit operator bool () const
  {
    return true;
  }

  bool operator! () const; /* No definition.  */
  template <class T> bool operator== (T) const; /* No definition.  */
  template <class T> bool operator!= (T) const; /* No definition.  */
};

/* This class tests that operator== is not used by assert.  */
struct bool_and_int
{
  bool_and_int () = default;
  bool_and_int (const no_int &) = delete;

  explicit operator bool () const
  {
    return true;
  }

  bool operator! () const; /* No definition.  */
  template <class T> bool operator== (T) const; /* No definition.  */
  template <class T> bool operator!= (T) const; /* No definition.  */
};

/* Scoped enumerations are not contextually convertible to bool. */
enum class E { e1 = 1 };

int&
preincrement (int& i)
{
  return ++i;
}

static int
do_test ()
{
  {
    no_int value;
    assert (value);
  }

  {
    bool_and_int value;
    assert (value);
  }

  {
    assert ([] { return true; } ());
  }

  {
    assert (bool (E::e1));
    /* Ill-formed, E::e1 is not contextually convertible to bool. */
    // assert (E::e1);
  }

  {
    int i = 0;
    assert (preincrement (i) > 0);
    if (i != 1)
      return 1;
  }

  return 0;
}
#define NDEBUG
#include <assert.h>

static_assert(is_void <decltype (assert (""))>::value, "type is void with NDEBUG");

#else
#include <support/test-driver.h>

static int
do_test ()
{
  return EXIT_UNSUPPORTED;
}
#endif

#include <support/test-driver.c>
