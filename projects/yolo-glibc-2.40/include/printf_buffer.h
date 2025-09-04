/* Multibyte and wide buffers for implementing printf-related functions.
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

/* The naming of the multibyte and wide variants is intentionally
   consistent, so that it is possible to use the Xprintf macro in
   stdio-common/printf_buffer-char.h and
   stdio-common/printf_buffer-wchar_t.h to select between them in
   type-generic code.  */

#ifndef PRINTF_BUFFER_H
#define PRINTF_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <wchar.h>

/* <printf_buffer_as_file.h> introduces a way to use struct
   __printf_buffer objects from FILE * streams.  To avoid storing a
   function pointer (or vtable pointer) in struct __printf_buffer
   (which would defeat libio vtable hardening), a switch statement
   over the different flush implementations is used to implement
   __printf_buffer_flush.

   __printf_buffer_mode_failed is special: it is the sticky failure
   indicator.  Unlike struct alloc_buffer, this is not folded into
   write_ptr, so that snprintf and other string-writing functions can
   discover the end of the string even in the error case, to be able
   to add the null terminator.  */
enum __printf_buffer_mode
  {
    __printf_buffer_mode_failed,
    __printf_buffer_mode_sprintf,
    __printf_buffer_mode_snprintf,
    __printf_buffer_mode_sprintf_chk,
    __printf_buffer_mode_to_file,
    __printf_buffer_mode_asprintf,
    __printf_buffer_mode_dprintf,
    __printf_buffer_mode_strfmon,
    __printf_buffer_mode_fp,         /* For __printf_fp_l_buffer.  */
    __printf_buffer_mode_fp_to_wide, /* For __wprintf_fp_l_buffer.  */
    __printf_buffer_mode_fphex_to_wide, /* For __wprintf_fphex_l_buffer.  */
    __printf_buffer_mode_obstack,    /* For __printf_buffer_flush_obstack.  */
  };

/* Buffer for fast character writing with overflow handling.
   Typically embedded in another struct with further data that is used
   by the flush function.  */
struct __printf_buffer
{
  /* These pointer members follow FILE streams.  write_ptr and
     write_end must be initialized to cover the target buffer.  See
     __printf_buffer_init.

     Data can be written directly to *write_ptr while write_ptr !=
     write_end, and write_ptr can be advanced accordingly.  Note that
     is not possible to use the apparently-unused part of the buffer
     as scratch space because sprintf (and snprintf, but that is a bit
     iffy) must only write the minimum number of characters produced
     by the format string and its arguments.

     write_base must be initialized to be equal to write_ptr.  The
     framework uses this pointer to compute the total number of
     written bytes, together with the written field.  See
     __printf_buffer_done.

     write_base and write_end are only read by the generic functions
     after initialization, only the flush implementation called from
     __printf_buffer_flush might change these pointers.  See the
     comment on Xprintf (buffer_do_flush) in Xprintf_buffer_flush.c
     for details regarding the flush operation.  */
  char *write_base;
  char *write_ptr;
  char *write_end;

  /* Number of characters written so far (excluding the current
     buffer).  Potentially updated on flush.  The actual number of
     written bytes also includes the unflushed-but-written buffer
     part, write_ptr - write_base.  A 64-bit value is used to avoid
     the need for overflow checks.  */
  uint64_t written;

  /* Identifies the flush callback.  */
  enum __printf_buffer_mode mode;
};

/* Marks the buffer as failed, so that __printf_buffer_has_failed
   returns true and future flush operations are no-ops.  */
static inline void
__printf_buffer_mark_failed (struct __printf_buffer *buf)
{
  buf->mode = __printf_buffer_mode_failed;
}

/* Returns true if the sticky error indicator of the buffer has been
   set to failed.  */
static inline bool __attribute_warn_unused_result__
__printf_buffer_has_failed (struct __printf_buffer *buf)
{
  return buf->mode == __printf_buffer_mode_failed;
}

/* Initialization of a buffer, using the memory region from [BASE,
   END) as the initial buffer contents.  */
static inline void
__printf_buffer_init_end (struct __printf_buffer *buf, char *base, char *end,
                          enum __printf_buffer_mode mode)
{
  buf->write_base = base;
  buf->write_ptr = base;
  buf->write_end = end;
  buf->written = 0;
  buf->mode = mode;
}

/* Initialization of a buffer, using the memory region from [BASE, BASE +LEN)
   as the initial buffer contents.  LEN can be zero.  */
static inline void
__printf_buffer_init (struct __printf_buffer *buf, char *base, size_t len,
                      enum __printf_buffer_mode mode)
{
  __printf_buffer_init_end (buf, base, base + len, mode);
}

/* Called by printf_buffer_putc for a full buffer.  */
void __printf_buffer_putc_1 (struct __printf_buffer *buf, char ch)
  attribute_hidden;

/* Writes CH to BUF.  */
static inline void
__printf_buffer_putc (struct __printf_buffer *buf, char ch)
{
  if (buf->write_ptr != buf->write_end)
      *buf->write_ptr++ = ch;
  else
    __printf_buffer_putc_1 (buf, ch);
}

/* Writes COUNT repeats of CH to BUF.  */
void __printf_buffer_pad_1 (struct __printf_buffer *buf,
                            char ch, size_t count) attribute_hidden;

/* __printf_buffer_pad with fast path for no padding.  COUNT is
   ssize_t to accommodate signed uses in printf and elsewhere.  */
static inline void
__printf_buffer_pad (struct __printf_buffer *buf, char ch, ssize_t count)
{
  if (count > 0)
    __printf_buffer_pad_1 (buf, ch, count);
}

/* Write COUNT bytes starting at S to BUF.  S must not overlap with
   the internal buffer.  */
void __printf_buffer_write (struct __printf_buffer *buf, const char *s,
                            size_t count) attribute_hidden;

/* Write S to BUF.  S must not overlap with the internal buffer.  */
void __printf_buffer_puts_1 (struct __printf_buffer *buf, const char *s)
  attribute_hidden;

static inline void
__printf_buffer_puts (struct __printf_buffer *buf, const char *s)
{
  if (__builtin_constant_p (__builtin_strlen (s)))
    __printf_buffer_write (buf, s, __builtin_strlen (s));
  else
    __printf_buffer_puts_1 (buf, s);
}

/* Returns the number of bytes written through the buffer, or -1 if
   there was an error (that is, __printf_buffer_has_failed (BUF) is true).

   The number of written bytes includes pending bytes in the buffer
   (between BUF->write_base and BUF->write_ptr).

   If the number is larger than INT_MAX, returns -1 and sets errno to
   EOVERFLOW.  This function does not flush the buffer.  If the caller
   needs the side effect of flushing, it has to do this
   separately.  */
int __printf_buffer_done (struct __printf_buffer *buf) attribute_hidden;

/* Internally used to call the flush function.  This can be called
   explicitly for certain modes to flush the buffer prematuraly.  In
   such cases, it is often the case that the buffer mode is statically
   known, and the flush implementation can be called directly.  */
bool __printf_buffer_flush (struct __printf_buffer *buf) attribute_hidden;

/* Wide version of struct __printf_buffer follows.  */

enum __wprintf_buffer_mode
  {
    __wprintf_buffer_mode_failed,
    __wprintf_buffer_mode_swprintf,
    __wprintf_buffer_mode_to_file,
  };

struct __wprintf_buffer
{
  wchar_t *write_base;
  wchar_t *write_ptr;
  wchar_t *write_end;
  uint64_t written;
  enum __wprintf_buffer_mode mode;
};

static inline void
__wprintf_buffer_mark_failed (struct __wprintf_buffer *buf)
{
  buf->mode = __wprintf_buffer_mode_failed;
}

static inline bool __attribute_warn_unused_result__
__wprintf_buffer_has_failed (struct __wprintf_buffer *buf)
{
  return buf->mode == __wprintf_buffer_mode_failed;
}

static inline void
__wprintf_buffer_init (struct __wprintf_buffer *buf,
                       wchar_t *base, size_t len,
                       enum __wprintf_buffer_mode mode)
{
  buf->write_base = base;
  buf->write_ptr = base;
  buf->write_end = base + len;
  buf->written = 0;
  buf->mode = mode;
}

void __wprintf_buffer_putc_1 (struct __wprintf_buffer *buf, wchar_t ch)
  attribute_hidden;

static inline void
__wprintf_buffer_putc (struct __wprintf_buffer *buf, wchar_t ch)
{
  if (buf->write_ptr != buf->write_end)
      *buf->write_ptr++ = ch;
  else
    __wprintf_buffer_putc_1 (buf, ch);
}

void __wprintf_buffer_pad_1 (struct __wprintf_buffer *buf,
                             wchar_t ch, size_t count) attribute_hidden;

static inline void
__wprintf_buffer_pad (struct __wprintf_buffer *buf, char ch, ssize_t count)
{
  if (count > 0)
    __wprintf_buffer_pad_1 (buf, ch, count);
}

void __wprintf_buffer_write (struct __wprintf_buffer *buf, const wchar_t *s,
                             size_t count) attribute_hidden;

void __wprintf_buffer_puts (struct __wprintf_buffer *buf, const wchar_t *s)
  attribute_hidden;

int __wprintf_buffer_done (struct __wprintf_buffer *buf) attribute_hidden;

bool __wprintf_buffer_flush (struct __wprintf_buffer *buf) attribute_hidden;

/* Type-generic convenience macros.  They are useful if
   printf_buffer-char.h or printf_buffer-wchar_t.h is included as
   well.  */

#define Xprintf_buffer Xprintf (buffer)
#define Xprintf_buffer_done Xprintf (buffer_done)
#define Xprintf_buffer_flush Xprintf (buffer_flush)
#define Xprintf_buffer_has_failed Xprintf (buffer_has_failed)
#define Xprintf_buffer_mark_failed Xprintf (buffer_mark_failed)
#define Xprintf_buffer_pad Xprintf (buffer_pad)
#define Xprintf_buffer_putc Xprintf (buffer_putc)
#define Xprintf_buffer_puts Xprintf (buffer_puts)
#define Xprintf_buffer_write Xprintf (buffer_write)

/* Commonly used buffers.  */

struct __printf_buffer_snprintf
{
  struct __printf_buffer base;
#define PRINTF_BUFFER_SIZE_DISCARD 128
  char discard[PRINTF_BUFFER_SIZE_DISCARD]; /* Used in counting mode.  */
};

/* Sets up [BUFFER, BUFFER + LENGTH) as the write target.  If LENGTH
   is positive, also writes a NUL byte to *BUFFER.  */
void __printf_buffer_snprintf_init (struct __printf_buffer_snprintf *,
                                    char *buffer, size_t length)
  attribute_hidden;

/* Add the null terminator after everything has been written.  The
   return value is the one expected by printf (see __printf_buffer_done).  */
int __printf_buffer_snprintf_done (struct __printf_buffer_snprintf *)
  attribute_hidden;

/* Flush function implementations follow.  They are called from
   __printf_buffer_flush.  Generic code should not call these flush
   functions directly.  Some modes have inline implementations.  */

void __printf_buffer_flush_snprintf (struct __printf_buffer_snprintf *)
  attribute_hidden;
struct __printf_buffer_to_file;
void __printf_buffer_flush_to_file (struct __printf_buffer_to_file *)
  attribute_hidden;
struct __printf_buffer_asprintf;
void __printf_buffer_flush_asprintf (struct __printf_buffer_asprintf *)
  attribute_hidden;
struct __printf_buffer_dprintf;
void __printf_buffer_flush_dprintf (struct __printf_buffer_dprintf *)
  attribute_hidden;
struct __printf_buffer_fp;
void __printf_buffer_flush_fp (struct __printf_buffer_fp *)
  attribute_hidden;
struct __printf_buffer_fp_to_wide;
void __printf_buffer_flush_fp_to_wide (struct __printf_buffer_fp_to_wide *)
  attribute_hidden;
struct __printf_buffer_fphex_to_wide;
void __printf_buffer_flush_fphex_to_wide (struct
                                          __printf_buffer_fphex_to_wide *)
  attribute_hidden;
struct __printf_buffer_obstack;
void __printf_buffer_flush_obstack (struct __printf_buffer_obstack *)
  attribute_hidden;

struct __wprintf_buffer_to_file;
void __wprintf_buffer_flush_to_file (struct __wprintf_buffer_to_file *)
  attribute_hidden;

/* Buffer sizes.  These can be tuned as necessary.  There is a tension
   here between stack consumption, cache usage, and additional system
   calls or heap allocations (if the buffer is too small).

   Also see PRINTF_BUFFER_SIZE_DISCARD above for snprintf.  */

/* Fallback buffer if the underlying FILE * stream does not provide
   buffer space.  */
#define PRINTF_BUFFER_SIZE_TO_FILE_STAGE 128

/* Temporary buffer used during floating point digit translation.  */
#define PRINTF_BUFFER_SIZE_DIGITS 64

/* Size of the initial on-stack buffer for asprintf.  It should be
   large enough to copy almost all asprintf usages with just a single
   (final, correctly sized) heap allocation.  */
#define PRINTF_BUFFER_SIZE_ASPRINTF 200

/* This should cover most of the packet-oriented file descriptors,
   where boundaries between writes could be visible to readers.  But
   it is still small enough not to cause too many stack overflow issues.  */
#define PRINTF_BUFFER_SIZE_DPRINTF 2048

#endif /* PRINTF_BUFFER_H */
