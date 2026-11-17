/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2008 - 2018 Red Hat, Inc.
 */

#ifndef __NM_GLIB_H__
#define __NM_GLIB_H__


#include <gio/gio.h>
#include <string.h>

#include "gsystem-local-alloc.h"

#ifdef __clang__

#undef G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#undef G_GNUC_END_IGNORE_DEPRECATIONS

#define G_GNUC_BEGIN_IGNORE_DEPRECATIONS \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")

#define G_GNUC_END_IGNORE_DEPRECATIONS \
    _Pragma("clang diagnostic pop")

#endif

/* g_assert_cmpmem() is only available since glib 2.46. */
#if !GLIB_CHECK_VERSION (2, 45, 7)
#define g_assert_cmpmem(m1, l1, m2, l2) G_STMT_START {\
                                             gconstpointer __m1 = m1, __m2 = m2; \
                                             int __l1 = l1, __l2 = l2; \
                                             if (__l1 != __l2) \
                                               g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                           #l1 " (len(" #m1 ")) == " #l2 " (len(" #m2 "))", __l1, "==", __l2, 'i'); \
                                             else if (memcmp (__m1, __m2, __l1) != 0) \
                                               g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                    "assertion failed (" #m1 " == " #m2 ")"); \
                                        } G_STMT_END
#endif

/* Rumtime check for glib version. First do a compile time check which
 * (if satisfied) shortcuts the runtime check. */
static inline gboolean
nm_glib_check_version (guint major, guint minor, guint micro)
{
	return    GLIB_CHECK_VERSION (major, minor, micro)
	       || (   (   glib_major_version > major)
	           || (   glib_major_version == major
	               && glib_minor_version > minor)
	           || (   glib_major_version == major
	               && glib_minor_version == minor
	               && glib_micro_version < micro));
}

#if !GLIB_CHECK_VERSION(2, 44, 0)
static inline gpointer
g_steal_pointer (gpointer pp)
{
	gpointer *ptr = (gpointer *) pp;
	gpointer ref;

	ref = *ptr;
	*ptr = NULL;

	return ref;
}

/* type safety */
#define g_steal_pointer(pp) \
  (0 ? (*(pp)) : (g_steal_pointer) (pp))
#endif


static inline gboolean
_nm_g_strv_contains (const gchar * const *strv,
                     const gchar         *str)
{
#if !GLIB_CHECK_VERSION(2, 44, 0)
	g_return_val_if_fail (strv != NULL, FALSE);
	g_return_val_if_fail (str != NULL, FALSE);

	for (; *strv != NULL; strv++) {
		if (g_str_equal (str, *strv))
			return TRUE;
	}

	return FALSE;
#else
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	return g_strv_contains (strv, str);
	G_GNUC_END_IGNORE_DEPRECATIONS
#endif
}
#define g_strv_contains _nm_g_strv_contains

#if !GLIB_CHECK_VERSION (2, 56, 0)
#define g_object_ref(Obj)      ((typeof(Obj)) g_object_ref (Obj))
#define g_object_ref_sink(Obj) ((typeof(Obj)) g_object_ref_sink (Obj))
#endif

#ifndef g_autofree
/* we still don't rely on recent glib to provide g_autofree. Hence, we continue
 * to use our gs_* free macros that we took from libgsystem.
 *
 * To ease migration towards g_auto*, add a compat define for g_autofree. */
#define g_autofree gs_free
#endif

#endif  /* __NM_GLIB_H__ */
