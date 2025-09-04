/* Macros for using symbol sets for running lists of functions.
   Copyright (C) 1994-2024 Free Software Foundation, Inc.
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

#ifndef _SET_HOOKS_H
#define _SET_HOOKS_H 1

#define __need_size_t
#include <stddef.h>
#include <sys/cdefs.h>
#include <libc-symbols.h>

#include "set-hooks-arch.h"

#ifdef symbol_set_define
/* Define a hook variable called NAME.  Functions put on this hook take
   arguments described by PROTO.  Use `text_set_element (NAME, FUNCTION)'
   from include/libc-symbols.h to add a function to the hook.  */

# define DEFINE_HOOK(NAME, PROTO)		\
  typedef void __##NAME##_hook_function_t PROTO; \
  symbol_set_define (NAME)

# define DECLARE_HOOK(NAME, PROTO)		\
  typedef void __##NAME##_hook_function_t PROTO;\
  symbol_set_declare (NAME)

/* Run all the functions hooked on the set called NAME.
   Each function is called like this: `function ARGS'.  */

# define RUN_HOOK(NAME, ARGS)						      \
do {									      \
  void *const *ptr;						      \
  for (ptr = (void *const *) symbol_set_first_element (NAME);		      \
       ! symbol_set_end_p (NAME, ptr); ++ptr)				      \
    (*(__##NAME##_hook_function_t *) *ptr) ARGS;			      \
} while (0)

/* Define a hook variable with NAME and PROTO, and a function called RUNNER
   which calls each function on the hook in turn, with ARGS.  */

# define DEFINE_HOOK_RUNNER(name, runner, proto, args) \
DEFINE_HOOK (name, proto); \
extern void runner proto; void runner proto { RUN_HOOK (name, args); }

# ifdef SET_RELHOOK
/* This is similar to RUN_RELHOOK, but the hooks were registered with
 * SET_RELHOOK so that a relative offset was computed by the linker
 * rather than an absolute address by the dynamic linker. */
#  define RUN_RELHOOK(NAME, ARGS)				      \
do {								      \
  void *const *ptr;						      \
  for (ptr = (void *const *) symbol_set_first_element (NAME);	      \
       ! symbol_set_end_p (NAME, ptr); ++ptr) {			      \
    __##NAME##_hook_function_t *f =				      \
	(void*) ((uintptr_t) ptr + (ptrdiff_t) *ptr);		      \
    (*f) ARGS;							      \
  } \
} while (0)
# else
#  define SET_RELHOOK(NAME, HOOK) text_set_element (NAME, HOOK)
#  define RUN_RELHOOK(NAME, ARGS) RUN_HOOK(NAME, ARGS)
# endif

#else

/* The system does not provide necessary support for this.  */
# define DEFINE_HOOK(NAME, PROTO)

# define DECLARE_HOOK(NAME, PROTO)

# define RUN_HOOK(NAME, ARGS)

# define DEFINE_HOOK_RUNNER(name, runner, proto, args)

# define SET_RELHOOK(NAME, HOOK)

# define RUN_RELHOOK(NAME, ARGS)

#endif

#endif /* set-hooks.h */
