/* Return backtrace of current program state.
   Copyright (C) 2003-2026 Free Software Foundation, Inc.
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

#include <execinfo.h>
#include <stdlib.h>
#include <unwind.h>
#include <unwind-link.h>
#if ENABLE_SFRAME
#include <sframe.h>
#endif

struct trace_arg
{
  void **array;
  struct unwind_link *unwind_link;
  _Unwind_Word cfa;
  int cnt;
  int size;
};

#if ENABLE_SFRAME
/* Initialize the SFrame backtrace routine and attempt to backtrace
   the current stack using SFrame information.  For the SFrame
   backtrace to be considered valid, the tracer must return more than
   one frame.  If it doesn't, this could indicate a mixed
   environment for example, glibc may have been compiled with SFrame
   support, while the application was not.  An even more complex
   scenario arises when the application uses shared objects compiled
   with differing configurations.

   Additionally, glibc includes certain callback paths that must be
   considered.  For example: an application calls shared object A,
   which calls shared object B, which in turn calls qsort() in
   glibc.  qsort() then invokes a helper in shared object C, which
   raises a SIGFPE signal, handled by object D, which requests a
   backtrace.  Any of these components may or may not include SFrame
   encoding.

   In cases where a stack frame lacks SFrame information, the SFrame
   backtracer can fall back to using the DWARF unwinder.

   This function must be always inline.  Otherwise the
   __builtin_frame_address and the __getXX helper functions will not
   return the right addresses.  */

static inline int __attribute__ ((always_inline))
do_sframe_backtrace (void **array, int size)
{
  frame frame;
  frame.pc = __getPC ();
  frame.sp = __getSP ();
  frame.fp = (_Unwind_Ptr) __builtin_frame_address (0);
  return __stacktrace_sframe (array, size, &frame);
}
#endif

static _Unwind_Reason_Code
backtrace_helper (struct _Unwind_Context *ctx, void *a)
{
  struct trace_arg *arg = a;

  /* We are first called with address in the __backtrace function.
     Skip it.  */
  if (arg->cnt != -1)
    {
      arg->array[arg->cnt]
	= (void *) UNWIND_LINK_PTR (arg->unwind_link, _Unwind_GetIP) (ctx);
      if (arg->cnt > 0)
	arg->array[arg->cnt]
	  = unwind_arch_adjustment (arg->array[arg->cnt - 1],
				    arg->array[arg->cnt]);

      /* Check whether we make any progress.  */
      _Unwind_Word cfa
	= UNWIND_LINK_PTR (arg->unwind_link, _Unwind_GetCFA) (ctx);

      if (arg->cnt > 0 && arg->array[arg->cnt - 1] == arg->array[arg->cnt]
	 && cfa == arg->cfa)
       return _URC_END_OF_STACK;
      arg->cfa = cfa;
    }
  if (++arg->cnt == arg->size)
    return _URC_END_OF_STACK;
  return _URC_NO_REASON;
}

int
__backtrace (void **array, int size)
{
  struct trace_arg arg =
    {
     .array = array,
     .unwind_link = __libc_unwind_link_get (),
     .cfa = 0,
     .size = size,
     .cnt = -1
    };

  if (size <= 0)
    return 0;

#if ENABLE_SFRAME
  /* Try first the SFrame backtracer.  */
  int cnt = do_sframe_backtrace (array, size);
  if (cnt > 1)
    return cnt;
#endif

  /* Try the dwarf unwinder.  */
  if (arg.unwind_link == NULL)
    return 0;

  UNWIND_LINK_PTR (arg.unwind_link, _Unwind_Backtrace)
    (backtrace_helper, &arg);

  /* _Unwind_Backtrace seems to put NULL address above
     _start.  Fix it up here.  */
  if (arg.cnt > 1 && arg.array[arg.cnt - 1] == NULL)
    --arg.cnt;
  return arg.cnt != -1 ? arg.cnt : 0;
}
weak_alias (__backtrace, backtrace)
libc_hidden_def (__backtrace)
