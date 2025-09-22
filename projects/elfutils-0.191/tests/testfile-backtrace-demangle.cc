/* Test program for C++ demangled unwinding.
   Copyright (C) 2014 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define NOINLINE_NOCLONE __attribute__ ((noinline, noclone))
#else
#define NOINLINE_NOCLONE __attribute__ ((noinline))
#endif

void NOINLINE_NOCLONE
cxxfunc (int i)
{
  *(volatile int *)0=0;
  // Avoid tail call optimization.
  asm volatile ("");
}

extern "C"
{
  void NOINLINE_NOCLONE
  f (void)
  {
    cxxfunc(1);
    // Avoid tail call optimization.
    asm volatile ("");
  }
}

int
main()
{
  f();
}
