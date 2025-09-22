/* Provide a sys/times.h header file.
   Copyright (C) 2008-2021 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Simon Josefsson <simon@josefsson.org>, 2008.  */

/* This file is supposed to be used on platforms where <sys/times.h>
   is missing.  */

#ifndef _@GUARD_PREFIX@_SYS_TIMES_H

# if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
# endif
@PRAGMA_COLUMNS@

# if @HAVE_SYS_TIMES_H@
#  @INCLUDE_NEXT@ @NEXT_SYS_TIMES_H@
# endif

# define _@GUARD_PREFIX@_SYS_TIMES_H

/* Get clock_t.
   But avoid namespace pollution on glibc systems.  */
# ifndef __GLIBC__
#  include <time.h>
# endif

/* The definition of _GL_ARG_NONNULL is copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */

# ifdef __cplusplus
extern "C" {
# endif

# if !@HAVE_STRUCT_TMS@
#  if !GNULIB_defined_struct_tms
  /* Structure describing CPU time used by a process and its children.  */
  struct tms
  {
    clock_t tms_utime;          /* User CPU time.  */
    clock_t tms_stime;          /* System CPU time.  */

    clock_t tms_cutime;         /* User CPU time of dead children.  */
    clock_t tms_cstime;         /* System CPU time of dead children.  */
  };
#   define GNULIB_defined_struct_tms 1
#  endif
# endif

# if @GNULIB_TIMES@
#  if !@HAVE_TIMES@
  extern clock_t times (struct tms *buffer) _GL_ARG_NONNULL ((1));
#  endif
# elif defined GNULIB_POSIXCHECK
#  undef times
#  if HAVE_RAW_DECL_TIMES
_GL_WARN_ON_USE (times, "times is unportable - "
                 "use gnulib module times for portability");
#  endif
# endif

# ifdef __cplusplus
}
# endif

#endif                          /* _@GUARD_PREFIX@_SYS_TIMES_H */
