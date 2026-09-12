/* Support for GNU properties.  AArch64 version.
   Copyright (C) 2018-2026 Free Software Foundation, Inc.
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

#ifndef _DL_PROP_H
#define _DL_PROP_H

extern void _dl_bti_protect (struct link_map *, int) attribute_hidden;

extern void _dl_bti_check (struct link_map *, const char *)
    attribute_hidden;

extern void _dl_gcs_check (struct link_map *, const char *, int)
    attribute_hidden;

static inline void __attribute__ ((always_inline))
_rtld_main_check (struct link_map *m, const char *program)
{
  _dl_bti_check (m, program);
  _dl_gcs_check (m, program, 0);
}

static inline void __attribute__ ((always_inline))
_dl_open_check (struct link_map *m, int dlopen_mode)
{
  _dl_bti_check (m, NULL);
  _dl_gcs_check (m, NULL, dlopen_mode);
}

static inline int
_dl_process_gnu_property (struct link_map *l, int fd, uint32_t type,
			  uint32_t datasz, void *data)
{
  if (type == GNU_PROPERTY_AARCH64_FEATURE_1_AND)
    {
      /* Stop if the property note is ill-formed.  */
      if (datasz != 4)
	return 0;

      unsigned int feature_1 = *(unsigned int *) data;
      if (feature_1 & GNU_PROPERTY_AARCH64_FEATURE_1_BTI)
	if (GLRO(dl_aarch64_cpu_features).bti)
	  _dl_bti_protect (l, fd);

      if (feature_1 & GNU_PROPERTY_AARCH64_FEATURE_1_GCS)
	l->l_mach.gcs = 1;

      /* Stop if we processed the property note.  */
      return 0;
    }
  /* Continue.  */
  return 1;
}

#endif /* _DL_PROP_H */
