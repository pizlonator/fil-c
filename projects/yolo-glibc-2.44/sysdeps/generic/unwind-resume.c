/* Copyright (C) 2003-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <gnu/lib-names.h>
#include <unwind-link.h>
#include <sysdep.h>
#include <unwind-resume.h>

static struct unwind_link *
link (void)
{
  struct unwind_link *unwind_link = __libc_unwind_link_get ();
  if (unwind_link == NULL)
    __libc_fatal (LIBGCC_S_SO " must be installed for unwinding to work\n");
  return unwind_link;
}

#if !HAVE_ARCH_UNWIND_RESUME
void
_Unwind_Resume (struct _Unwind_Exception *exc)
{
  UNWIND_LINK_PTR (link (), _Unwind_Resume) (exc);
}
#endif

#ifndef HAVE_CC_WITH_LIBUNWIND
_Unwind_Reason_Code
__gcc_personality_v0 PERSONALITY_PROTO
{
  return UNWIND_LINK_PTR (link (), personality) PERSONALITY_ARGS;
}
#else
void *
_Unwind_GetLanguageSpecificData (struct _Unwind_Context *context)
{
  return UNWIND_LINK_PTR (link (), _Unwind_GetLanguageSpecificData) (context);
}

void
_Unwind_SetGR (struct _Unwind_Context *context, int index, _Unwind_Word val)
{
  UNWIND_LINK_PTR (link (), _Unwind_SetGR) (context, index, val);
}

_Unwind_Ptr
_Unwind_GetIP (struct _Unwind_Context *context)
{
  return UNWIND_LINK_PTR (link (), _Unwind_GetIP) (context);
}

void
_Unwind_SetIP (struct _Unwind_Context *context, _Unwind_Ptr val)
{
  UNWIND_LINK_PTR (link (), _Unwind_SetIP) (context, val);
}

_Unwind_Ptr
_Unwind_GetRegionStart (struct _Unwind_Context *context)
{
  return UNWIND_LINK_PTR (link (), _Unwind_GetRegionStart) (context);
}
#endif

_Unwind_Reason_Code
_Unwind_ForcedUnwind (struct _Unwind_Exception *exc, _Unwind_Stop_Fn stop,
                      void *stop_argument)
{
  return UNWIND_LINK_PTR (link (), _Unwind_ForcedUnwind)
    (exc, stop, stop_argument);
}

_Unwind_Word
_Unwind_GetCFA (struct _Unwind_Context *context)
{
  return UNWIND_LINK_PTR (link (), _Unwind_GetCFA) (context);
}
