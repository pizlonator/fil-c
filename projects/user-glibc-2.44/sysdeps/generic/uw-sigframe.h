/* Internal header file for handling signal frames.  Generic version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

/* Each architecture that supports SFrame may need to define several
   macros to handle exceptional cases during stack backtracing.

   MD_DECODE_SIGNAL_FRAME(frame) should recover frame information when
   a signal-related exception occurs.  The input frame must contain a
   valid program counter (PC) field.  On success, the macro should
   return _URC_NO_REASON.

   MD_DETECT_OUTERMOST_FRAME(frame) is used to detect the outermost
   stack frame.  It returns _URC_NO_REASON upon successful
   detection.

   The FRAME structure is defined in sysdeps/generic/sframe.h  */
