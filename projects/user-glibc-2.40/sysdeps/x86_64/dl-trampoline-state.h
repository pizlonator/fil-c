/* x86-64 PLT dl-trampoline state macros.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

#if (STATE_SAVE_ALIGNMENT % 16) != 0
# error STATE_SAVE_ALIGNMENT must be multiple of 16
#endif

#if (STATE_SAVE_OFFSET % STATE_SAVE_ALIGNMENT) != 0
# error STATE_SAVE_OFFSET must be multiple of STATE_SAVE_ALIGNMENT
#endif

#if DL_RUNTIME_RESOLVE_REALIGN_STACK
/* Local stack area before jumping to function address: RBX.  */
# define LOCAL_STORAGE_AREA	8
# define BASE			rbx
# ifdef USE_FXSAVE
/* Use fxsave to save XMM registers.  */
#  define REGISTER_SAVE_AREA	(512 + STATE_SAVE_OFFSET)
#  if (REGISTER_SAVE_AREA % 16) != 0
#   error REGISTER_SAVE_AREA must be multiple of 16
#  endif
# endif
#else
# ifndef USE_FXSAVE
#  error USE_FXSAVE must be defined
# endif
/* Use fxsave to save XMM registers.  */
# define REGISTER_SAVE_AREA	(512 + STATE_SAVE_OFFSET + 8)
/* Local stack area before jumping to function address:  All saved
   registers.  */
# define LOCAL_STORAGE_AREA	REGISTER_SAVE_AREA
# define BASE			rsp
# if (REGISTER_SAVE_AREA % 16) != 8
#  error REGISTER_SAVE_AREA must be odd multiple of 8
# endif
#endif
