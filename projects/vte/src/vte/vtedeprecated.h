/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
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

#include "vteterminal.h"
#include "vtepty.h"
#include "vtemacros.h"

#if !defined(VTE_DISABLE_DEPRECATION_WARNINGS) && !defined(VTE_COMPILATION)
#define _VTE_DEPRECATED G_DEPRECATED
#else
#define _VTE_DEPRECATED
#endif

G_BEGIN_DECLS

#if _VTE_GTK == 3

_VTE_DEPRECATED
_VTE_PUBLIC
int vte_terminal_match_add_gregex(VteTerminal *terminal,
                                  GRegex *gregex,
                                  GRegexMatchFlags gflags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

#endif /* _VTE_GTK == 3 */

_VTE_DEPRECATED
_VTE_PUBLIC
void vte_terminal_match_set_cursor(VteTerminal *terminal,
                                   int tag,
                                   GdkCursor *cursor) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

#if _VTE_GTK == 3
_VTE_DEPRECATED
_VTE_PUBLIC
void vte_terminal_match_set_cursor_type(VteTerminal *terminal,
					int tag,
                                        GdkCursorType cursor_type) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
#endif

_VTE_DEPRECATED
_VTE_PUBLIC
char *vte_terminal_match_check(VteTerminal *terminal,
			       glong column, glong row,
			       int *tag) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) G_GNUC_MALLOC;

#if _VTE_GTK == 3

_VTE_DEPRECATED
_VTE_PUBLIC
gboolean vte_terminal_event_check_gregex_simple(VteTerminal *terminal,
                                                GdkEvent *event,
                                                GRegex **regexes,
                                                gsize n_regexes,
                                                GRegexMatchFlags match_flags,
                                                char **matches) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_DEPRECATED
_VTE_PUBLIC
void      vte_terminal_search_set_gregex      (VteTerminal *terminal,
					       GRegex      *gregex,
                                               GRegexMatchFlags gflags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_DEPRECATED
_VTE_PUBLIC
GRegex   *vte_terminal_search_get_gregex      (VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

#endif /* _VTE_GTK == 3 */

_VTE_DEPRECATED
_VTE_PUBLIC
gboolean vte_terminal_spawn_sync(VteTerminal *terminal,
                                 VtePtyFlags pty_flags,
                                 const char *working_directory,
                                 char **argv,
                                 char **envv,
                                 GSpawnFlags spawn_flags,
                                 GSpawnChildSetupFunc child_setup,
                                 gpointer child_setup_data,
                                 GPid *child_pid /* out */,
                                 GCancellable *cancellable,
                                 GError **error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 4);

_VTE_DEPRECATED
_VTE_PUBLIC
void vte_pty_close (VtePty *pty) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_DEPRECATED
_VTE_PUBLIC
void vte_terminal_copy_clipboard(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

#if _VTE_GTK == 3

_VTE_DEPRECATED
_VTE_PUBLIC
void vte_terminal_get_geometry_hints(VteTerminal *terminal,
                                     GdkGeometry *hints,
                                     int min_rows,
                                     int min_columns) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_DEPRECATED
_VTE_PUBLIC
void vte_terminal_set_geometry_hints_for_window(VteTerminal *terminal,
                                                GtkWindow *window) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

#endif /* _VTE_GTK == 3 */

_VTE_DEPRECATED
_VTE_PUBLIC
const char *vte_terminal_get_icon_title(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_DEPRECATED
_VTE_PUBLIC
gboolean vte_terminal_set_encoding(VteTerminal *terminal,
                                   const char *codeset,
                                   GError **error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_DEPRECATED
_VTE_PUBLIC
const char *vte_terminal_get_encoding(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

typedef gboolean (*VteSelectionFunc)(VteTerminal *terminal,
                                     glong column,
                                     glong row,
                                     gpointer data) _VTE_GNUC_NONNULL(1) _VTE_DEPRECATED;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
_VTE_DEPRECATED
_VTE_PUBLIC
char *vte_terminal_get_text(VteTerminal *terminal,
			    VteSelectionFunc is_selected,
			    gpointer user_data,
			    GArray *attributes) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) G_GNUC_MALLOC;
G_GNUC_END_IGNORE_DEPRECATIONS

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
_VTE_DEPRECATED
_VTE_PUBLIC
char *vte_terminal_get_text_range(VteTerminal *terminal,
				  glong start_row, glong start_col,
				  glong end_row, glong end_col,
				  VteSelectionFunc is_selected,
				  gpointer user_data,
				  GArray *attributes) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) G_GNUC_MALLOC;
G_GNUC_END_IGNORE_DEPRECATIONS

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
_VTE_DEPRECATED
_VTE_PUBLIC
char *vte_terminal_get_text_include_trailing_spaces(VteTerminal *terminal,
						    VteSelectionFunc is_selected,
						    gpointer user_data,
						    GArray *attributes) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) G_GNUC_MALLOC;
G_GNUC_END_IGNORE_DEPRECATIONS

_VTE_DEPRECATED
_VTE_PUBLIC
void vte_terminal_set_rewrap_on_resize(VteTerminal *terminal,
                                       gboolean rewrap) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_DEPRECATED
_VTE_PUBLIC
gboolean vte_terminal_get_rewrap_on_resize(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_DEPRECATED
_VTE_PUBLIC
void vte_terminal_set_allow_bold(VteTerminal *terminal,
                                 gboolean allow_bold) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_DEPRECATED
_VTE_PUBLIC
gboolean vte_terminal_get_allow_bold(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_DEPRECATED
_VTE_PUBLIC
void vte_terminal_feed_child_binary(VteTerminal *terminal,
                                    const guint8 *data,
                                    gsize length) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_DEPRECATED
_VTE_PUBLIC
char **vte_get_encodings(gboolean include_aliases) _VTE_CXX_NOEXCEPT;

_VTE_DEPRECATED
_VTE_PUBLIC
gboolean vte_get_encoding_supported(const char *encoding) _VTE_CXX_NOEXCEPT;

typedef struct _VteCharAttributes VteCharAttributes _VTE_DEPRECATED;

/* The structure we return as the supplemental attributes for strings. */
struct _VteCharAttributes {
        /*< private >*/
        long row, column;  /* logical column */
	PangoColor fore, back;
	guint underline:1, strikethrough:1, columns:4;
} _VTE_DEPRECATED;

_VTE_DEPRECATED
_VTE_PUBLIC
const char *vte_terminal_get_window_title(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_DEPRECATED
_VTE_PUBLIC
const char *vte_terminal_get_current_directory_uri(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_DEPRECATED
_VTE_PUBLIC
const char *vte_terminal_get_current_file_uri(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

G_END_DECLS

#undef _VTE_DEPRECATED
