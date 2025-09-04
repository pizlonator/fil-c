/* Flush wrapper for struct __*printf_buffer.  Generic version.
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

#include <printf_buffer.h>

#include <assert.h>
#include <stdint.h>

/* Xprintf (buffer_do_flush) (BUF) performs the flush operation.  The
   actual implementation is specific to the multibyte and wide
   variants.

   If the flush fails, Xprintf_buffer_mark_failed (BUF) must be
   called, and BUF->write_ptr and BUF->write_end can be left
   unchanged.

   The function must not do anything if failure has already occurred,
   that is, if BUF->mode == Xprintf (buffer_mode_failed).

   The framework implicitly invokes flush with BUF->write_ptr ==
   BUF->write_end only.  (This is particularly relevant to the
   __sprintf_chk flush, which just calls __chk_fail.)  But in some
   cases, Xprintf_buffer_flush may be called explicitly (when
   BUF->mode/the backing function is known).  In that case, it is
   possible that BUF->write_ptr < BUF->write_end is true.

   If the flush succeeds, the pointers are changed so that
   BUF->write_ptr < BUF->write_end.  It is possible to switch to a
   completely different buffer here.  If the buffer is moved, it may
   be necessary to updated BUF->write_base and BUF->written from the
   flush function as well.

   Note that when chaining buffers, in the flush function for the
   outer buffer (to which data is written first), it is necessary to
   check for BUF->next->failed (for the inner buffer) and set
   BUF->base.failed to true (for the outer buffer).  This should come
   towards the end of the outer flush function.  Usually, there is
   also some unwrapping step afterwards; it has to check the outer
   buffer (BUF->base.failed) and propagate any error to the inner
   buffer (BUF->next->failed), so essentially in the other
   direction.  */
static void Xprintf (buffer_do_flush) (struct Xprintf_buffer *buf);

bool
Xprintf_buffer_flush (struct Xprintf_buffer *buf)
{
  if (__glibc_unlikely (Xprintf_buffer_has_failed (buf)))
    return false;

  Xprintf (buffer_do_flush) (buf);
  if (Xprintf_buffer_has_failed (buf))
    return false;

  /* Ensure that the flush has made available some bytes.  */
  assert (buf->write_ptr != buf->write_end);
  return true;
}
