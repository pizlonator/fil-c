# bison-cxx-std.m4 serial 1

# Copyright (C) 2018-2021 Free Software Foundation, Inc.

# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

m4_define([_BISON_CXXSTD_98_snippet],
[[]])

m4_define([_BISON_CXXSTD_03_snippet],
[[]])

m4_define([_BISON_CXXSTD_11_snippet],
[[  // C++11
#include <algorithm>
#include <cassert>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility> // std::swap
#include <vector>

  template <typename T>
  struct check
  {
    static_assert(sizeof(int) <= sizeof(T), "not big enough");
  };

  using right_angle_brackets = check<check<bool>>;

  auto f = std::make_shared<std::string>("shared_ptr");

  int a;
  decltype(a) b;

  typedef check<int> check_type;
  check_type c;
  check_type&& cr = static_cast<check_type&&>(c);

  auto d = a;

  // Some versions of libstdc++ do not support std::set::emplace.
  void foo()
  {
    std::set<int> is;
    is.emplace(42);
  }

  // Clang++ 3.5, for a while, was unable to process properly
  // the for-loop because its variable, r, is a typedef...
  // It failed as follows:
  //
  // error: unexpected ':' in nested name specifier; did you mean '::'?
  //    for (auto r: std::set<int>{1, 2})
  //               ^
  //               ::
  using r = std::set<int>;
  void bar()
  {
    for (int r: std::set<int>{1, 2})
      continue;
  }

  // GCC 4.8.2 on Solaris 11.3 does not support to_string.
  auto e = std::to_string(42);

  // Needed by glr2.cc.
  void cxx11_vector_data ()
  {
    std::vector<int> ints;
    ints.emplace_back (42);
    assert (ints.data () == &ints[0]);
  }

  // Needed by glr2.cc.
  void cxx11_array_swap ()
  {
    int i0[4] = { 1, 2, 3, 4 };
    int i1[4] = { 5, 6, 7, 8 };
    std::swap (i0, i1);
  }
]])

m4_define([_BISON_CXXSTD_14_snippet],
[[  // C++14
  void mismatch()
  {
    using ints = std::vector<int>;
    auto v1 = ints{1, 2, 3};
    auto v2 = ints{1, 2};
    std::mismatch(std::begin(v1), std::end(v1),
                  std::begin(v2), std::end(v2));
  }
]])

m4_define([_BISON_CXXSTD_17_snippet],
[[  // C++17
  namespace ns1::ns2::ns3 {}

#include <optional>
  auto opt_string = std::optional<std::string>{};
  auto out = std::ostringstream{};
]])

m4_define([_BISON_CXXSTD_20_snippet],
[[  // C++20
]])

m4_define([_BISON_CXXSTD_2B_snippet],
[[  // C++2B
]])


m4_define([_BISON_CXXSTD_testbody(98)],
[AC_LANG_PROGRAM([
_BISON_CXXSTD_98_snippet
])])

m4_define([_BISON_CXXSTD_testbody(03)],
[AC_LANG_PROGRAM([
_BISON_CXXSTD_98_snippet
_BISON_CXXSTD_03_snippet
])])

m4_define([_BISON_CXXSTD_testbody(11)],
[AC_LANG_PROGRAM([
_BISON_CXXSTD_98_snippet
_BISON_CXXSTD_03_snippet
_BISON_CXXSTD_11_snippet
])])

m4_define([_BISON_CXXSTD_testbody(14)],
[AC_LANG_PROGRAM([
_BISON_CXXSTD_98_snippet
_BISON_CXXSTD_03_snippet
_BISON_CXXSTD_11_snippet
_BISON_CXXSTD_14_snippet
])])

m4_define([_BISON_CXXSTD_testbody(17)],
[AC_LANG_PROGRAM([
_BISON_CXXSTD_98_snippet
_BISON_CXXSTD_03_snippet
_BISON_CXXSTD_11_snippet
_BISON_CXXSTD_14_snippet
_BISON_CXXSTD_17_snippet
])])

m4_define([_BISON_CXXSTD_testbody(20)],
[AC_LANG_PROGRAM([
_BISON_CXXSTD_98_snippet
_BISON_CXXSTD_03_snippet
_BISON_CXXSTD_11_snippet
_BISON_CXXSTD_14_snippet
_BISON_CXXSTD_17_snippet
_BISON_CXXSTD_20_snippet
])])

m4_define([_BISON_CXXSTD_testbody(2b)],
[AC_LANG_PROGRAM([
_BISON_CXXSTD_98_snippet
_BISON_CXXSTD_03_snippet
_BISON_CXXSTD_11_snippet
_BISON_CXXSTD_14_snippet
_BISON_CXXSTD_17_snippet
_BISON_CXXSTD_20_snippet
_BISON_CXXSTD_2B_snippet
])])


m4_define([_BISON_CXXSTD_testbody],
[m4_ifdef([$0($1)],
          [m4_indir([$0($1)], m4_shift2($@))],
          [m4_fatal([$0: unknown C++ standard: $1])])])


# BISON_CXXSTD(STD)
# -----------------
# Check whether the C++ compiler supports STD (11, 98, 2b, etc.).
# If it does, AC_SUBST 'CXX<STD>_CXXFLAGS' to the corresponding flags.
AC_DEFUN([BISON_CXXSTD],
[AC_REQUIRE([AC_PROG_CXX])
AC_LANG_PUSH([C++])
for f in '-std=c++$1' '-std=c++$1 -stdlib=libc++'
do
  BISON_CHECK_COMPILER_FLAG([$f],
                     [AC_SUBST(m4_toupper([CXX$1_CXXFLAGS]), [$f]) break],
                     [], [],
                     [_BISON_CXXSTD_testbody($1)])
done
AC_LANG_POP([C++])
])
