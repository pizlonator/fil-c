/* x86-64 feature tuning.
   This file is part of the GNU C Library.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.

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

#ifndef _X86_64_FEATURE_CONTROL_H
#define _X86_64_FEATURE_CONTROL_H

/* For each CET feature, IBT and SHSTK, valid control values.  */
enum dl_x86_cet_control
{
  /* Enable CET features based on ELF property note.  */
  cet_elf_property = 0,
  /* Always enable CET features.  */
  cet_always_on,
  /* Always disable CET features.  */
  cet_always_off,
  /* Enable CET features permissively.  */
  cet_permissive
};

/* PLT rewrite control.  */
enum dl_plt_rewrite_control
{
  /* No PLT rewrite.  */
  plt_rewrite_none,
  /* Rewrite PLT with JMP at run-time.  */
  plt_rewrite_jmp,
  /* Rewrite PLT with JMP and JMPABS at run-time.  */
  plt_rewrite_jmpabs
};

struct dl_x86_feature_control
{
  enum dl_x86_cet_control ibt : 2;
  enum dl_x86_cet_control shstk : 2;
  enum dl_plt_rewrite_control plt_rewrite : 2;
};

#endif /* feature-control.h */
