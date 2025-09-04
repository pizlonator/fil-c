/* Test of strtold() and c_strtold() in the "C" locale.
   Copyright (C) 2008-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Eric Blake.  */

static void
test_function (long double (*my_strtold) (const char *, char **))
{
  /* Subject sequence empty or invalid.  */
  {
    const char input[] = "";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " ";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " +";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " .";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " .e0";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);              /* IRIX 6.5 */
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " +.e-0";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);              /* IRIX 6.5 */
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " in";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " na";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }

  /* Simple floating point values.  */
  {
    const char input[] = "1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1.";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = ".5";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.5L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = " 1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "+1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == -1.0L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e0";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e+0";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e-0";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 10.0L);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "5e-1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.5L);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }

  /* Zero.  */
  {
    const char input[] = "0";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = ".0";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0e0";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0e+9999999";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 10);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0e-9999999";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 10);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-0";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!!signbit (result) == !!signbit (minus_zerol)); /* IRIX 6.5 */
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }

  /* Suffixes.  */
  {
    const char input[] = "1f";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1.f";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e+";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e-";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1E 2";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);            /* HP-UX 11.31/ia64, IRIX 6.5 */
    ASSERT (ptr == input + 1);          /* HP-UX 11.31/ia64, IRIX 6.5 */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);          /* glibc-2.3.2, Mac OS X 10.5, Haiku */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "00x1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-0x";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!!signbit (result) == !!signbit (minus_zerol)); /* Mac OS X 10.5, IRIX 6.5 */
    ASSERT (ptr == input + 2);          /* glibc-2.3.2, Mac OS X 10.5, Haiku */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0xg";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);          /* glibc-2.3.2, Mac OS X 10.5, Haiku */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0xp";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);          /* glibc-2.3.2, Mac OS X 10.5, Haiku */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0XP";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);          /* glibc-2.3.2, Mac OS X 10.5, Haiku */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x.";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);          /* glibc-2.3.2, Mac OS X 10.5, Haiku */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0xp+";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);          /* glibc-2.3.2, Mac OS X 10.5, Haiku */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0xp+1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);          /* glibc-2.3.2, Mac OS X 10.5, Haiku */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x.p+1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);          /* glibc-2.3.2, Mac OS X 10.5, Haiku */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1p+1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1P+1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }

  /* Overflow/underflow.  */
  {
    const char input[] = "1E1000000";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == HUGE_VALL);
    ASSERT (ptr == input + 9);
    ASSERT (errno == ERANGE);
  }
  {
    const char input[] = "-1E1000000";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == -HUGE_VALL);
    ASSERT (ptr == input + 10);
    ASSERT (errno == ERANGE);
  }
  {
    const char input[] = "1E-100000";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (0.0L <= result && result <= LDBL_MIN);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 9);
    ASSERT (errno == ERANGE);
  }
  {
    const char input[] = "-1E-100000";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (-LDBL_MIN <= result && result <= 0.0L);
#if 0
    /* FIXME - this is glibc bug 5995; POSIX allows returning positive
       0 on negative underflow, even though quality of implementation
       demands preserving the sign.  Disable this test until fixed
       glibc is more prevalent.  */
    ASSERT (!!signbit (result) == !!signbit (minus_zerol)); /* glibc-2.3.2, Haiku */
#endif
    ASSERT (ptr == input + 10);
    ASSERT (errno == ERANGE);
  }
  {
    const char input[] = "1E 1000000";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);            /* HP-UX 11.31/ia64, IRIX 6.5 */
    ASSERT (ptr == input + 1);          /* HP-UX 11.31/ia64, IRIX 6.5 */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64, IRIX 6.5 */
  }
  {
    const char input[] = "0x1P 1000000";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);            /* HP-UX 11.31/ia64, IRIX 6.5, mingw */
    ASSERT (ptr == input + 3);          /* HP-UX 11.31/ia64, IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }

  /* Infinity.  */
  {
    const char input[] = "iNf";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == HUGE_VALL);       /* IRIX 6.5 */
    ASSERT (ptr == input + 3);          /* IRIX 6.5 */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64 */
  }
  {
    const char input[] = "-InF";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == -HUGE_VALL);      /* IRIX 6.5 */
    ASSERT (ptr == input + 4);          /* IRIX 6.5 */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64 */
  }
  {
    const char input[] = "infinite";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == HUGE_VALL);       /* IRIX 6.5 */
    ASSERT (ptr == input + 3);          /* IRIX 6.5 */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64 */
  }
  {
    const char input[] = "infinitY";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == HUGE_VALL);       /* IRIX 6.5 */
    ASSERT (ptr == input + 8);          /* IRIX 6.5 */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64 */
  }
  {
    const char input[] = "infinitY.";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == HUGE_VALL);       /* IRIX 6.5 */
    ASSERT (ptr == input + 8);          /* IRIX 6.5 */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64 */
  }

  /* NaN.  Some processors set the sign bit of the default NaN, so all
     we check is that using a sign changes the result.  */
  {
    const char input[] = "-nan";
    char *ptr1;
    char *ptr2;
    long double result1;
    long double result2;
    errno = 0;
    result1 = my_strtold (input, &ptr1);
    result2 = my_strtold (input + 1, &ptr2);
#if 1 /* All known CPUs support NaNs.  */
    ASSERT (isnanl (result1));          /* IRIX 6.5 */
    ASSERT (isnanl (result2));          /* IRIX 6.5 */
# if 0
    /* Sign bits of NaN is a portability sticking point, not worth
       worrying about.  */
    ASSERT (!!signbit (result1) != !!signbit (result2));
# endif
    ASSERT (ptr1 == input + 4);         /* IRIX 6.5 */
    ASSERT (ptr2 == input + 4);         /* IRIX 6.5 */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64 */
#else
    ASSERT (result1 == 0.0L);
    ASSERT (result2 == 0.0L);
    ASSERT (!signbit (result1));
    ASSERT (!signbit (result2));
    ASSERT (ptr1 == input);
    ASSERT (ptr2 == input + 1);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }
  {
    const char input[] = "+nan(";
    char *ptr1;
    char *ptr2;
    long double result1;
    long double result2;
    errno = 0;
    result1 = my_strtold (input, &ptr1);
    result2 = my_strtold (input + 1, &ptr2);
#if 1 /* All known CPUs support NaNs.  */
    ASSERT (isnanl (result1));          /* IRIX 6.5 */
    ASSERT (isnanl (result2));          /* IRIX 6.5 */
    ASSERT (!!signbit (result1) == !!signbit (result2));
    ASSERT (ptr1 == input + 4);         /* Mac OS X 10.5, IRIX 6.5 */
    ASSERT (ptr2 == input + 4);         /* Mac OS X 10.5, IRIX 6.5 */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64 */
#else
    ASSERT (result1 == 0.0L);
    ASSERT (result2 == 0.0L);
    ASSERT (!signbit (result1));
    ASSERT (!signbit (result2));
    ASSERT (ptr1 == input);
    ASSERT (ptr2 == input + 1);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }
  {
    const char input[] = "-nan()";
    char *ptr1;
    char *ptr2;
    long double result1;
    long double result2;
    errno = 0;
    result1 = my_strtold (input, &ptr1);
    result2 = my_strtold (input + 1, &ptr2);
#if 1 /* All known CPUs support NaNs.  */
    ASSERT (isnanl (result1));          /* IRIX 6.5 */
    ASSERT (isnanl (result2));          /* IRIX 6.5 */
# if 0
    /* Sign bits of NaN is a portability sticking point, not worth
       worrying about.  */
    ASSERT (!!signbit (result1) != !!signbit (result2)); /* glibc-2.3.2, musl libc, OpenBSD 6.0, IRIX 6.5, mingw, Haiku */
# endif
    ASSERT (ptr1 == input + 6);         /* glibc-2.3.2, IRIX 6.5, mingw, Haiku */
    ASSERT (ptr2 == input + 6);         /* glibc-2.3.2, IRIX 6.5, mingw, Haiku */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64, Haiku */
#else
    ASSERT (result1 == 0.0L);
    ASSERT (result2 == 0.0L);
    ASSERT (!signbit (result1));
    ASSERT (!signbit (result2));
    ASSERT (ptr1 == input);
    ASSERT (ptr2 == input + 1);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }
  {
    const char input[] = " nan().";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
#if 1 /* All known CPUs support NaNs.  */
    ASSERT (isnanl (result));           /* IRIX 6.5 */
    ASSERT (ptr == input + 6);          /* glibc-2.3.2, IRIX 6.5, mingw, Haiku */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64, Haiku */
#else
    ASSERT (result == 0.0L);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }
  {
    /* The behavior of nan(0) is implementation-defined, but all
       implementations we know of which handle optional
       n-char-sequences handle nan(0) the same as nan().  */
    const char input[] = "-nan(0).";
    char *ptr1;
    char *ptr2;
    long double result1;
    long double result2;
    errno = 0;
    result1 = my_strtold (input, &ptr1);
    result2 = my_strtold (input + 1, &ptr2);
#if 1 /* All known CPUs support NaNs.  */
    ASSERT (isnanl (result1));          /* IRIX 6.5 */
    ASSERT (isnanl (result2));          /* IRIX 6.5 */
# if 0
    /* Sign bits of NaN is a portability sticking point, not worth
       worrying about.  */
    ASSERT (!!signbit (result1) != !!signbit (result2));
# endif
    ASSERT (ptr1 == input + 7);         /* glibc-2.3.2, IRIX 6.5, mingw, Haiku */
    ASSERT (ptr2 == input + 7);         /* glibc-2.3.2, IRIX 6.5, mingw, Haiku */
    ASSERT (errno == 0);                /* HP-UX 11.31/ia64 */
#else
    ASSERT (result1 == 0.0L);
    ASSERT (result2 == 0.0L);
    ASSERT (!signbit (result1));
    ASSERT (!signbit (result2));
    ASSERT (ptr1 == input);
    ASSERT (ptr2 == input + 1);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }

  /* Hex.  */
  {
    const char input[] = "0xa";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 10.0L);           /* IRIX 6.5, mingw */
    ASSERT (ptr == input + 3);          /* IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0XA";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 10.0L);           /* IRIX 6.5, mingw */
    ASSERT (ptr == input + 3);          /* IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);            /* IRIX 6.5, mingw */
    ASSERT (ptr == input + 3);          /* IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p+";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);            /* IRIX 6.5, mingw */
    ASSERT (ptr == input + 3);          /* IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1P+";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);            /* IRIX 6.5, mingw */
    ASSERT (ptr == input + 3);          /* IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p+1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 2.0L);            /* IRIX 6.5, mingw */
    ASSERT (ptr == input + 6);          /* IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0X1P+1";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 2.0L);            /* IRIX 6.5, mingw */
    ASSERT (ptr == input + 6);          /* IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p+1a";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 2.0L);            /* IRIX 6.5, mingw */
    ASSERT (ptr == input + 6);          /* IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p 2";
    char *ptr;
    long double result;
    errno = 0;
    result = my_strtold (input, &ptr);
    ASSERT (result == 1.0L);            /* HP-UX 11.31/ia64, IRIX 6.5, mingw */
    ASSERT (ptr == input + 3);          /* HP-UX 11.31/ia64, IRIX 6.5, mingw */
    ASSERT (errno == 0);
  }

  /* Large buffers.  */
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        long double result;
        memset (input, '\t', m - 1);
        input[m - 1] = '1';
        input[m] = '\0';
        errno = 0;
        result = my_strtold (input, &ptr);
        ASSERT (result == 1.0L);
        ASSERT (ptr == input + m);
        ASSERT (errno == 0);
      }
    free (input);
  }
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        long double result;
        memset (input, '0', m - 1);
        input[m - 1] = '1';
        input[m] = '\0';
        errno = 0;
        result = my_strtold (input, &ptr);
        ASSERT (result == 1.0L);
        ASSERT (ptr == input + m);
        ASSERT (errno == 0);
      }
    free (input);
  }
#if 0
  /* Newlib has an artificial limit of 20000 for the exponent.  TODO -
     gnulib should fix this.  */
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        long double result;
        input[0] = '.';
        memset (input + 1, '0', m - 10);
        input[m - 9] = '1';
        input[m - 8] = 'e';
        input[m - 7] = '+';
        input[m - 6] = '9';
        input[m - 5] = '9';
        input[m - 4] = '9';
        input[m - 3] = '9';
        input[m - 2] = '9';
        input[m - 1] = '1';
        input[m] = '\0';
        errno = 0;
        result = my_strtold (input, &ptr);
        ASSERT (result == 1.0L);        /* Mac OS X 10.5, FreeBSD 11.0, NetBSD 7.1, OpenBSD 6.0, Minix 3.3, IRIX 6.5, mingw */
        ASSERT (ptr == input + m);
        ASSERT (errno == 0);            /* Mac OS X 10.5, FreeBSD 11.0, NetBSD 7.1, OpenBSD 6.0, Minix 3.3, IRIX 6.5, mingw */
      }
    free (input);
  }
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        long double result;
        input[0] = '1';
        memset (input + 1, '0', m - 9);
        input[m - 8] = 'e';
        input[m - 7] = '-';
        input[m - 6] = '9';
        input[m - 5] = '9';
        input[m - 4] = '9';
        input[m - 3] = '9';
        input[m - 2] = '9';
        input[m - 1] = '1';
        input[m] = '\0';
        errno = 0;
        result = my_strtold (input, &ptr);
        ASSERT (result == 1.0L);        /* Mac OS X 10.5, FreeBSD 11.0, NetBSD 7.1, OpenBSD 6.0, Minix 3.3, IRIX 6.5, mingw */
        ASSERT (ptr == input + m);
        ASSERT (errno == 0);            /* Mac OS X 10.5, FreeBSD 11.0, NetBSD 7.1, OpenBSD 6.0, Minix 3.3, IRIX 6.5, mingw */
      }
    free (input);
  }
#endif
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        long double result;
        input[0] = '-';
        input[1] = '0';
        input[2] = 'e';
        input[3] = '1';
        memset (input + 4, '0', m - 3);
        input[m] = '\0';
        errno = 0;
        result = my_strtold (input, &ptr);
        ASSERT (result == 0.0L);
        ASSERT (!!signbit (result) == !!signbit (minus_zerol)); /* IRIX 6.5 */
        ASSERT (ptr == input + m);
        ASSERT (errno == 0);
      }
    free (input);
  }

  /* Rounding.  */
  /* TODO - is it worth some tests of rounding for typical IEEE corner
     cases, such as .5 ULP rounding up to the smallest denormal and
     not causing underflow, or LDBL_MIN - .5 ULP not causing an
     infinite loop?  */
}
