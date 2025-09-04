/* Helper for getting proper name of overflow fallback function for
   {wc|st}{p|r|s}n{cat|cpy}

   All versions must be listed in ifunc-impl-list.c.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#ifndef _STRNCPY_OR_CAT_OVERFLOW_DEF_H_
#define _STRNCPY_OR_CAT_OVERFLOW_DEF_H_ 1

#if defined USE_MULTIARCH && IS_IN(libc)
#  define UNDERSCORES __
#  ifdef USE_WITH_SSE2
#    define ISA_EXT _sse2
#  elif defined USE_WITH_AVX2
#    ifdef USE_WITH_RTM
#      define ISA_EXT _avx2_rtm
#    else
#      define ISA_EXT _avx2
#    endif

#  elif defined USE_WITH_EVEX256
#    define ISA_EXT _evex
#  elif defined USE_WITH_EVEX512
#    define ISA_EXT _evex512
#  endif
#else
#  define UNDERSCORES
#  define ISA_EXT
#endif

#ifdef USE_AS_WCSCPY
#  define STRCPY_PREFIX wc
#  define STRCAT_PREFIX wcs
#  ifdef USE_AS_STPCPY
#    define STRCPY_POSTFIX pcpy
#  else
#    define STRCPY_POSTFIX scpy
#  endif
#else
#  define STRCPY_PREFIX st
#  define STRCAT_PREFIX str
#  ifdef USE_AS_STPCPY
#    define STRCPY_POSTFIX pcpy
#  else
#    define STRCPY_POSTFIX rcpy
#  endif
#endif
#define STRCAT_POSTFIX cat

#define PRIMITIVE_OF_NAMER(underscores, prefix, postfix, ext)                 \
  underscores##prefix##postfix##ext

#define OF_NAMER(...) PRIMITIVE_OF_NAMER (__VA_ARGS__)

#ifndef OVERFLOW_STRCPY
#  define OVERFLOW_STRCPY                                                     \
    OF_NAMER (UNDERSCORES, STRCPY_PREFIX, STRCPY_POSTFIX, ISA_EXT)
#endif

#ifndef OVERFLOW_STRCAT
#  define OVERFLOW_STRCAT                                                     \
    OF_NAMER (UNDERSCORES, STRCAT_PREFIX, STRCAT_POSTFIX, ISA_EXT)
#endif

#endif
