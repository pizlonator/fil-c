/*
 * Copyright © 2015 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <stdint.h>

#include "vtemacros.h"

G_BEGIN_DECLS

typedef struct _VteRegex VteRegex;

#define VTE_TYPE_REGEX (vte_regex_get_type())

_VTE_PUBLIC
GType vte_regex_get_type (void);

#define VTE_REGEX_ERROR (vte_regex_error_quark())

_VTE_PUBLIC
GQuark vte_regex_error_quark (void);

/* This is PCRE2_NO_UTF_CHECK | PCRE2_UTF | PCRE2_NEVER_BACKSLASH_C */
#define VTE_REGEX_FLAGS_DEFAULT (0x00080000u | 0x40000000u | 0x00100000u)

_VTE_PUBLIC
VteRegex *vte_regex_ref      (VteRegex *regex) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
VteRegex *vte_regex_unref    (VteRegex *regex) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
VteRegex *vte_regex_new_for_match (const char *pattern,
                                   gssize      pattern_length,
                                   guint32     flags,
                                   GError    **error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
VteRegex *vte_regex_new_for_match_full (char const* pattern,
                                        gssize pattern_length,
                                        uint32_t flags,
                                        uint32_t extra_flags,
                                        gsize* error_offset,
                                        GError** error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
VteRegex *vte_regex_new_for_search (const char *pattern,
                                    gssize      pattern_length,
                                    guint32     flags,
                                    GError    **error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
VteRegex *vte_regex_new_for_search_full (char const* pattern,
                                         gssize pattern_length,
                                         uint32_t flags,
                                         uint32_t extra_flags,
                                         gsize* error_offset,
                                         GError** error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean  vte_regex_jit     (VteRegex *regex,
                             guint32   flags,
                             GError  **error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
char *vte_regex_substitute(VteRegex *regex,
                           const char *subject,
                           const char *replacement,
                           guint32 flags,
                           GError **error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2, 3) G_GNUC_MALLOC;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(VteRegex, vte_regex_unref)

G_END_DECLS
