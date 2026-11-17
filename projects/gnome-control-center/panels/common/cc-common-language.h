/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

enum {
        LOCALE_COL,
        DISPLAY_LOCALE_COL,
        SEPARATOR_COL,
        USER_LANGUAGE,
        NUM_COLS
};

gboolean cc_common_language_get_iter_for_language   (GtkTreeModel     *model,
						     const gchar      *lang,
						     GtkTreeIter      *iter);
gboolean cc_common_language_get_iter_for_region     (GtkTreeModel     *model,
						     const gchar      *lang,
						     GtkTreeIter      *iter);
guint    cc_common_language_add_available_languages (GtkListStore     *store,
                                                     gboolean          regions,
                                                     GHashTable       *user_langs);
gboolean cc_common_language_has_font                (const gchar  *locale);
gchar   *cc_common_language_get_current_language    (void);

GHashTable *cc_common_language_get_initial_languages   (void);
GHashTable *cc_common_language_get_user_languages      (void);
GHashTable *cc_common_language_get_initial_regions     (const gchar *lang);

void     cc_common_language_setup_list              (GtkWidget    *treeview,
						     GHashTable   *users,
						     GHashTable   *initial);
void     cc_common_language_select_current_language (GtkTreeView  *treeview);

void     cc_common_language_add_user_languages (GtkTreeModel *model);

G_END_DECLS
