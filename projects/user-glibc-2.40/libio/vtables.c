/* libio vtable validation.
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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

#include <dlfcn.h>
#include <libioP.h>
#include <stdio.h>
#include <ldsodefs.h>
#include <array_length.h>
#include <pointer_guard.h>
#include <libio-macros.h>

/* Both _IO_str_* and _IO_new_file functions are pulled into every link (from
   stdio initialization).  */
#ifndef SHARED
/* NB: the following directives do add pragma weak for _IO_default _* and
   _IO_wdefault_* symbols to potentially avoid link failures, since they
   are always used when the __io_vtables is used.  */
# pragma weak _IO_wstr_finish
# pragma weak _IO_wstr_overflow
# pragma weak _IO_wstr_pbackfail
# pragma weak _IO_wstr_seekoff
# pragma weak _IO_wstr_underflow

# pragma weak _IO_file_close
# pragma weak _IO_file_close_mmap
# pragma weak _IO_file_doallocate
# pragma weak _IO_file_finish
# pragma weak _IO_file_overflow
# pragma weak _IO_file_read
# pragma weak _IO_file_seek
# pragma weak _IO_file_seekoff_maybe_mmap
# pragma weak _IO_file_seekoff_mmap
# pragma weak _IO_file_setbuf
# pragma weak _IO_file_setbuf_mmap
# pragma weak _IO_file_setbuf_mmap
# pragma weak _IO_file_stat
# pragma weak _IO_file_sync
# pragma weak _IO_file_sync_mmap
# pragma weak _IO_file_underflow
# pragma weak _IO_file_underflow_maybe_mmap
# pragma weak _IO_file_underflow_mmap
# pragma weak _IO_file_xsgetn
# pragma weak _IO_file_xsgetn_maybe_mmap
# pragma weak _IO_file_xsgetn_mmap
# pragma weak _IO_file_xsputn

# pragma weak _IO_wfile_overflow
# pragma weak _IO_wfile_sync
# pragma weak _IO_wfile_underflow
# pragma weak _IO_wfile_underflow_maybe_mmap
# pragma weak _IO_wfile_underflow_mmap
# pragma weak _IO_wfile_doallocate
# pragma weak _IO_wfile_seekoff
# pragma weak _IO_wfile_xsputn

# pragma weak _IO_new_proc_close

# pragma weak _IO_cookie_close
# pragma weak _IO_cookie_read
# pragma weak _IO_cookie_seek
# pragma weak _IO_cookie_seekoff
# pragma weak _IO_cookie_write

# pragma weak _IO_mem_finish
# pragma weak _IO_mem_sync

# pragma weak _IO_wmem_finish
# pragma weak _IO_wmem_sync

# pragma weak __printf_buffer_as_file_overflow
# pragma weak __printf_buffer_as_file_xsputn

# pragma weak __wprintf_buffer_as_file_overflow
# pragma weak __wprintf_buffer_as_file_xsputn
#endif

const struct _IO_jump_t __io_vtables[] attribute_relro =
{
  /* _IO_str_jumps  */
  [IO_STR_JUMPS] =
  {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_str_finish),
    JUMP_INIT (overflow, _IO_str_overflow),
    JUMP_INIT (underflow, _IO_str_underflow),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_str_pbackfail),
    JUMP_INIT (xsputn, _IO_default_xsputn),
    JUMP_INIT (xsgetn, _IO_default_xsgetn),
    JUMP_INIT (seekoff, _IO_str_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_default_setbuf),
    JUMP_INIT (sync, _IO_default_sync),
    JUMP_INIT (doallocate, _IO_default_doallocate),
    JUMP_INIT (read, _IO_default_read),
    JUMP_INIT (write, _IO_default_write),
    JUMP_INIT (seek, _IO_default_seek),
    JUMP_INIT (close, _IO_default_close),
    JUMP_INIT (stat, _IO_default_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_wstr_jumps  */
  [IO_WSTR_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_wstr_finish),
    JUMP_INIT (overflow, (_IO_overflow_t) _IO_wstr_overflow),
    JUMP_INIT (underflow, (_IO_underflow_t) _IO_wstr_underflow),
    JUMP_INIT (uflow, (_IO_underflow_t) _IO_wdefault_uflow),
    JUMP_INIT (pbackfail, (_IO_pbackfail_t) _IO_wstr_pbackfail),
    JUMP_INIT (xsputn, _IO_wdefault_xsputn),
    JUMP_INIT (xsgetn, _IO_wdefault_xsgetn),
    JUMP_INIT (seekoff, _IO_wstr_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_default_setbuf),
    JUMP_INIT (sync, _IO_default_sync),
    JUMP_INIT (doallocate, _IO_wdefault_doallocate),
    JUMP_INIT (read, _IO_default_read),
    JUMP_INIT (write, _IO_default_write),
    JUMP_INIT (seek, _IO_default_seek),
    JUMP_INIT (close, _IO_default_close),
    JUMP_INIT (stat, _IO_default_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_file_jumps  */
  [IO_FILE_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_file_finish),
    JUMP_INIT (overflow, _IO_file_overflow),
    JUMP_INIT (underflow, _IO_file_underflow),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_default_pbackfail),
    JUMP_INIT (xsputn, _IO_file_xsputn),
    JUMP_INIT (xsgetn, _IO_file_xsgetn),
    JUMP_INIT (seekoff, _IO_new_file_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_new_file_setbuf),
    JUMP_INIT (sync, _IO_new_file_sync),
    JUMP_INIT (doallocate, _IO_file_doallocate),
    JUMP_INIT (read, _IO_file_read),
    JUMP_INIT (write, _IO_new_file_write),
    JUMP_INIT (seek, _IO_file_seek),
    JUMP_INIT (close, _IO_file_close),
    JUMP_INIT (stat, _IO_file_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_file_jumps_mmap  */
  [IO_FILE_JUMPS_MMAP] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_file_finish),
    JUMP_INIT (overflow, _IO_file_overflow),
    JUMP_INIT (underflow, _IO_file_underflow_mmap),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_default_pbackfail),
    JUMP_INIT (xsputn, _IO_new_file_xsputn),
    JUMP_INIT (xsgetn, _IO_file_xsgetn_mmap),
    JUMP_INIT (seekoff, _IO_file_seekoff_mmap),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, (_IO_setbuf_t) _IO_file_setbuf_mmap),
    JUMP_INIT (sync, _IO_file_sync_mmap),
    JUMP_INIT (doallocate, _IO_file_doallocate),
    JUMP_INIT (read, _IO_file_read),
    JUMP_INIT (write, _IO_new_file_write),
    JUMP_INIT (seek, _IO_file_seek),
    JUMP_INIT (close, _IO_file_close_mmap),
    JUMP_INIT (stat, _IO_file_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_file_jumps_maybe_mmap  */
  [IO_FILE_JUMPS_MAYBE_MMAP] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_file_finish),
    JUMP_INIT (overflow, _IO_file_overflow),
    JUMP_INIT (underflow, _IO_file_underflow_maybe_mmap),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_default_pbackfail),
    JUMP_INIT (xsputn, _IO_new_file_xsputn),
    JUMP_INIT (xsgetn, _IO_file_xsgetn_maybe_mmap),
    JUMP_INIT (seekoff, _IO_file_seekoff_maybe_mmap),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, (_IO_setbuf_t) _IO_file_setbuf_mmap),
    JUMP_INIT (sync, _IO_new_file_sync),
    JUMP_INIT (doallocate, _IO_file_doallocate),
    JUMP_INIT (read, _IO_file_read),
    JUMP_INIT (write, _IO_new_file_write),
    JUMP_INIT (seek, _IO_file_seek),
    JUMP_INIT (close, _IO_file_close),
    JUMP_INIT (stat, _IO_file_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_wfile_jumps  */
  [IO_WFILE_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_new_file_finish),
    JUMP_INIT (overflow, (_IO_overflow_t) _IO_wfile_overflow),
    JUMP_INIT (underflow, (_IO_underflow_t) _IO_wfile_underflow),
    JUMP_INIT (uflow, (_IO_underflow_t) _IO_wdefault_uflow),
    JUMP_INIT (pbackfail, (_IO_pbackfail_t) _IO_wdefault_pbackfail),
    JUMP_INIT (xsputn, _IO_wfile_xsputn),
    JUMP_INIT (xsgetn, _IO_file_xsgetn),
    JUMP_INIT (seekoff, _IO_wfile_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_new_file_setbuf),
    JUMP_INIT (sync, (_IO_sync_t) _IO_wfile_sync),
    JUMP_INIT (doallocate, _IO_wfile_doallocate),
    JUMP_INIT (read, _IO_file_read),
    JUMP_INIT (write, _IO_new_file_write),
    JUMP_INIT (seek, _IO_file_seek),
    JUMP_INIT (close, _IO_file_close),
    JUMP_INIT (stat, _IO_file_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_wfile_jumps_mmap  */
  [IO_WFILE_JUMPS_MMAP] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_new_file_finish),
    JUMP_INIT (overflow, (_IO_overflow_t) _IO_wfile_overflow),
    JUMP_INIT (underflow, (_IO_underflow_t) _IO_wfile_underflow_mmap),
    JUMP_INIT (uflow, (_IO_underflow_t) _IO_wdefault_uflow),
    JUMP_INIT (pbackfail, (_IO_pbackfail_t) _IO_wdefault_pbackfail),
    JUMP_INIT (xsputn, _IO_wfile_xsputn),
    JUMP_INIT (xsgetn, _IO_file_xsgetn),
    JUMP_INIT (seekoff, _IO_wfile_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_file_setbuf_mmap),
    JUMP_INIT (sync, (_IO_sync_t) _IO_wfile_sync),
    JUMP_INIT (doallocate, _IO_wfile_doallocate),
    JUMP_INIT (read, _IO_file_read),
    JUMP_INIT (write, _IO_new_file_write),
    JUMP_INIT (seek, _IO_file_seek),
    JUMP_INIT (close, _IO_file_close_mmap),
    JUMP_INIT (stat, _IO_file_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_wfile_jumps_maybe_mmap  */
  [IO_WFILE_JUMPS_MAYBE_MMAP] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_new_file_finish),
    JUMP_INIT (overflow, (_IO_overflow_t) _IO_wfile_overflow),
    JUMP_INIT (underflow, (_IO_underflow_t) _IO_wfile_underflow_maybe_mmap),
    JUMP_INIT (uflow, (_IO_underflow_t) _IO_wdefault_uflow),
    JUMP_INIT (pbackfail, (_IO_pbackfail_t) _IO_wdefault_pbackfail),
    JUMP_INIT (xsputn, _IO_wfile_xsputn),
    JUMP_INIT (xsgetn, _IO_file_xsgetn),
    JUMP_INIT (seekoff, _IO_wfile_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_file_setbuf_mmap),
    JUMP_INIT (sync, (_IO_sync_t) _IO_wfile_sync),
    JUMP_INIT (doallocate, _IO_wfile_doallocate),
    JUMP_INIT (read, _IO_file_read),
    JUMP_INIT (write, _IO_new_file_write),
    JUMP_INIT (seek, _IO_file_seek),
    JUMP_INIT (close, _IO_file_close),
    JUMP_INIT (stat, _IO_file_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_cookie_jumps  */
  [IO_COOKIE_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_file_finish),
    JUMP_INIT (overflow, _IO_file_overflow),
    JUMP_INIT (underflow, _IO_file_underflow),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_default_pbackfail),
    JUMP_INIT (xsputn, _IO_file_xsputn),
    JUMP_INIT (xsgetn, _IO_default_xsgetn),
    JUMP_INIT (seekoff, _IO_cookie_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_file_setbuf),
    JUMP_INIT (sync, _IO_file_sync),
    JUMP_INIT (doallocate, _IO_file_doallocate),
    JUMP_INIT (read, _IO_cookie_read),
    JUMP_INIT (write, _IO_cookie_write),
    JUMP_INIT (seek, _IO_cookie_seek),
    JUMP_INIT (close, _IO_cookie_close),
    JUMP_INIT (stat, _IO_default_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue),
  },
  /* _IO_proc_jumps  */
  [IO_PROC_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_new_file_finish),
    JUMP_INIT (overflow, _IO_new_file_overflow),
    JUMP_INIT (underflow, _IO_new_file_underflow),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_default_pbackfail),
    JUMP_INIT (xsputn, _IO_new_file_xsputn),
    JUMP_INIT (xsgetn, _IO_default_xsgetn),
    JUMP_INIT (seekoff, _IO_new_file_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_new_file_setbuf),
    JUMP_INIT (sync, _IO_new_file_sync),
    JUMP_INIT (doallocate, _IO_file_doallocate),
    JUMP_INIT (read, _IO_file_read),
    JUMP_INIT (write, _IO_new_file_write),
    JUMP_INIT (seek, _IO_file_seek),
    JUMP_INIT (close, _IO_new_proc_close),
    JUMP_INIT (stat, _IO_file_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_mem_jumps  */
  [IO_MEM_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_mem_finish),
    JUMP_INIT (overflow, _IO_str_overflow),
    JUMP_INIT (underflow, _IO_str_underflow),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_str_pbackfail),
    JUMP_INIT (xsputn, _IO_default_xsputn),
    JUMP_INIT (xsgetn, _IO_default_xsgetn),
    JUMP_INIT (seekoff, _IO_str_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_default_setbuf),
    JUMP_INIT (sync, _IO_mem_sync),
    JUMP_INIT (doallocate, _IO_default_doallocate),
    JUMP_INIT (read, _IO_default_read),
    JUMP_INIT (write, _IO_default_write),
    JUMP_INIT (seek, _IO_default_seek),
    JUMP_INIT (close, _IO_default_close),
    JUMP_INIT (stat, _IO_default_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  /* _IO_wmem_jumps  */
  [IO_WMEM_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_wmem_finish),
    JUMP_INIT (overflow, (_IO_overflow_t) _IO_wstr_overflow),
    JUMP_INIT (underflow, (_IO_underflow_t) _IO_wstr_underflow),
    JUMP_INIT (uflow, (_IO_underflow_t) _IO_wdefault_uflow),
    JUMP_INIT (pbackfail, (_IO_pbackfail_t) _IO_wstr_pbackfail),
    JUMP_INIT (xsputn, _IO_wdefault_xsputn),
    JUMP_INIT (xsgetn, _IO_wdefault_xsgetn),
    JUMP_INIT (seekoff, _IO_wstr_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_default_setbuf),
    JUMP_INIT (sync, _IO_wmem_sync),
    JUMP_INIT (doallocate, _IO_wdefault_doallocate),
    JUMP_INIT (read, _IO_default_read),
    JUMP_INIT (write, _IO_default_write),
    JUMP_INIT (seek, _IO_default_seek),
    JUMP_INIT (close, _IO_default_close),
    JUMP_INIT (stat, _IO_default_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
  [IO_PRINTF_BUFFER_AS_FILE_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, NULL),
    JUMP_INIT (overflow, __printf_buffer_as_file_overflow),
    JUMP_INIT (underflow, NULL),
    JUMP_INIT (uflow, NULL),
    JUMP_INIT (pbackfail, NULL),
    JUMP_INIT (xsputn, __printf_buffer_as_file_xsputn),
    JUMP_INIT (xsgetn, NULL),
    JUMP_INIT (seekoff, NULL),
    JUMP_INIT (seekpos, NULL),
    JUMP_INIT (setbuf, NULL),
    JUMP_INIT (sync, NULL),
    JUMP_INIT (doallocate, NULL),
    JUMP_INIT (read, NULL),
    JUMP_INIT (write, NULL),
    JUMP_INIT (seek, NULL),
    JUMP_INIT (close, NULL),
    JUMP_INIT (stat, NULL),
    JUMP_INIT (showmanyc, NULL),
    JUMP_INIT (imbue, NULL)
  },
  [IO_WPRINTF_BUFFER_AS_FILE_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, NULL),
    JUMP_INIT (overflow, (_IO_overflow_t) __wprintf_buffer_as_file_overflow),
    JUMP_INIT (underflow, NULL),
    JUMP_INIT (uflow, NULL),
    JUMP_INIT (pbackfail, NULL),
    JUMP_INIT (xsputn, __wprintf_buffer_as_file_xsputn),
    JUMP_INIT (xsgetn, NULL),
    JUMP_INIT (seekoff, NULL),
    JUMP_INIT (seekpos, NULL),
    JUMP_INIT (setbuf, NULL),
    JUMP_INIT (sync, NULL),
    JUMP_INIT (doallocate, NULL),
    JUMP_INIT (read, NULL),
    JUMP_INIT (write, NULL),
    JUMP_INIT (seek, NULL),
    JUMP_INIT (close, NULL),
    JUMP_INIT (stat, NULL),
    JUMP_INIT (showmanyc, NULL),
    JUMP_INIT (imbue, NULL)
  },

#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_1)
  /* _IO_old_file_jumps  */
  [IO_OLD_FILE_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_old_file_finish),
    JUMP_INIT (overflow, _IO_old_file_overflow),
    JUMP_INIT (underflow, _IO_old_file_underflow),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_default_pbackfail),
    JUMP_INIT (xsputn, _IO_old_file_xsputn),
    JUMP_INIT (xsgetn, _IO_default_xsgetn),
    JUMP_INIT (seekoff, _IO_old_file_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_old_file_setbuf),
    JUMP_INIT (sync, _IO_old_file_sync),
    JUMP_INIT (doallocate, _IO_file_doallocate),
    JUMP_INIT (read, _IO_file_read),
    JUMP_INIT (write, _IO_old_file_write),
    JUMP_INIT (seek, _IO_file_seek),
    JUMP_INIT (close, _IO_file_close),
    JUMP_INIT (stat, _IO_file_stat)
  },
  /*  _IO_old_proc_jumps  */
  [IO_OLD_PROC_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_old_file_finish),
    JUMP_INIT (overflow, _IO_old_file_overflow),
    JUMP_INIT (underflow, _IO_old_file_underflow),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_default_pbackfail),
    JUMP_INIT (xsputn, _IO_old_file_xsputn),
    JUMP_INIT (xsgetn, _IO_default_xsgetn),
    JUMP_INIT (seekoff, _IO_old_file_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_old_file_setbuf),
    JUMP_INIT (sync, _IO_old_file_sync),
    JUMP_INIT (doallocate, _IO_file_doallocate),
    JUMP_INIT (read, _IO_file_read),
    JUMP_INIT (write, _IO_old_file_write),
    JUMP_INIT (seek, _IO_file_seek),
    JUMP_INIT (close, _IO_old_proc_close),
    JUMP_INIT (stat, _IO_file_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue)
  },
#endif

#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_2)
  /* _IO_old_cookie_jumps  */
  [IO_OLD_COOKIED_JUMPS] = {
    JUMP_INIT_DUMMY,
    JUMP_INIT (finish, _IO_file_finish),
    JUMP_INIT (overflow, _IO_file_overflow),
    JUMP_INIT (underflow, _IO_file_underflow),
    JUMP_INIT (uflow, _IO_default_uflow),
    JUMP_INIT (pbackfail, _IO_default_pbackfail),
    JUMP_INIT (xsputn, _IO_file_xsputn),
    JUMP_INIT (xsgetn, _IO_default_xsgetn),
    JUMP_INIT (seekoff, _IO_cookie_seekoff),
    JUMP_INIT (seekpos, _IO_default_seekpos),
    JUMP_INIT (setbuf, _IO_file_setbuf),
    JUMP_INIT (sync, _IO_file_sync),
    JUMP_INIT (doallocate, _IO_file_doallocate),
    JUMP_INIT (read, _IO_cookie_read),
    JUMP_INIT (write, _IO_cookie_write),
    JUMP_INIT (seek, _IO_old_cookie_seek),
    JUMP_INIT (close, _IO_cookie_close),
    JUMP_INIT (stat, _IO_default_stat),
    JUMP_INIT (showmanyc, _IO_default_showmanyc),
    JUMP_INIT (imbue, _IO_default_imbue),
  },
#endif
};
_Static_assert (array_length (__io_vtables) == IO_VTABLES_NUM,
                "initializer count");

#ifdef SHARED

void (*IO_accept_foreign_vtables) (void) attribute_hidden;

#else  /* !SHARED */

/* Used to check whether static dlopen support is needed.  */
# pragma weak __dlopen

#endif

void attribute_hidden
_IO_vtable_check (void)
{
#ifdef SHARED
  /* Honor the compatibility flag.  */
  void (*flag) (void) = atomic_load_relaxed (&IO_accept_foreign_vtables);
  PTR_DEMANGLE (flag);
  if (flag == &_IO_vtable_check)
    return;

  /* In case this libc copy is in a non-default namespace, we always
     need to accept foreign vtables because there is always a
     possibility that FILE * objects are passed across the linking
     boundary.  */
  {
    Dl_info di;
    struct link_map *l;
    if (!rtld_active ()
        || (_dl_addr (_IO_vtable_check, &di, &l, NULL) != 0
            && l->l_ns != LM_ID_BASE))
      return;
  }

#else /* !SHARED */
  /* We cannot perform vtable validation in the static dlopen case
     because FILE * handles might be passed back and forth across the
     boundary.  Therefore, we disable checking in this case.  */
  if (__dlopen != NULL)
    return;
#endif

  __libc_fatal ("Fatal error: glibc detected an invalid stdio handle\n");
}

/* Some variants of libstdc++ interpose _IO_2_1_stdin_ etc. and
   install their own vtables directly, without calling _IO_init or
   other functions.  Detect this by looking at the vtables values
   during startup, and disable vtable validation in this case.  */
#ifdef SHARED
__attribute__ ((constructor))
static void
check_stdfiles_vtables (void)
{
  if (_IO_2_1_stdin_.vtable != &_IO_file_jumps
      || _IO_2_1_stdout_.vtable != &_IO_file_jumps
      || _IO_2_1_stderr_.vtable != &_IO_file_jumps)
    IO_set_accept_foreign_vtables (&_IO_vtable_check);
}
#endif

#define STR(s)  XSTR(s)
#define XSTR(s) #s

#undef _IO_file_jumps
#define _IO_file_jumps_alias  "__io_vtables + " STR(IO_FILE_JUMPS_OFFSET)
declare_object_symbol_alias (_IO_file_jumps, _IO_file_jumps_alias,
			     IO_JUMP_T_SIZE)
#undef _IO_wfile_jumps
#define _IO_wfile_jumps_alias  "__io_vtables + " STR(IO_WFILE_JUMPS_OFFSET)
declare_object_symbol_alias (_IO_wfile_jumps, _IO_wfile_jumps_alias,
			     IO_JUMP_T_SIZE)
