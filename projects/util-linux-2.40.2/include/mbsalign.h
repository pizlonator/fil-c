/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Align/Truncate a string in a given screen width
 * Copyright (C) 2009-2010 Free Software Foundation, Inc.
 * Copyright (C) 2010-2013 Karel Zak <kzak@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 2.1 of the License, or (at your
 * option) any later version.
 */
#ifndef UTIL_LINUX_MBSALIGN_H
# define UTIL_LINUX_MBSALIGN_H
# include <stddef.h>

typedef enum { MBS_ALIGN_LEFT, MBS_ALIGN_RIGHT, MBS_ALIGN_CENTER } mbs_align_t;

enum {
  /* Use unibyte mode for invalid multibyte strings or
     or when heap memory is exhausted.  */
  MBA_UNIBYTE_FALLBACK = 0x0001,

#if 0 /* Other possible options.  */
  /* Skip invalid multibyte chars rather than failing  */
  MBA_IGNORE_INVALID   = 0x0002,

  /* Align multibyte strings using "figure space" (\u2007)  */
  MBA_USE_FIGURE_SPACE = 0x0004,

  /* Don't add any padding  */
  MBA_TRUNCATE_ONLY    = 0x0008,

  /* Don't truncate  */
  MBA_PAD_ONLY         = 0x0010,
#endif
};

extern size_t mbs_truncate(char *str, size_t *width);

extern size_t mbsalign (const char *src, char *dest,
			size_t dest_size,  size_t *width,
			mbs_align_t align, int flags);

extern size_t mbsalign_with_padding (const char *src, char *dest, size_t dest_size,
	               size_t *width, mbs_align_t align, int flags,
		       int padchar);

extern size_t mbs_safe_nwidth(const char *buf, size_t bufsz, size_t *sz);
extern size_t mbs_safe_width(const char *s);

extern size_t mbs_nwidth(const char *buf, size_t bufsz);
extern size_t mbs_width(const char *s);

extern char *mbs_safe_encode(const char *s, size_t *width);
extern char *mbs_safe_encode_to_buffer(const char *s, size_t *width, char *buf, const char *safechars);
extern size_t mbs_safe_encode_size(size_t bytes);
extern size_t mbs_safe_decode_size(const char *s);

extern char *mbs_invalid_encode(const char *s, size_t *width);
extern char *mbs_invalid_encode_to_buffer(const char *s, size_t *width, char *buf);

#endif /* UTIL_LINUX_MBSALIGN_H */
