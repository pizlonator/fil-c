/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Canonical Ltd.
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
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_LANGUAGE_ROW (cc_language_row_get_type ())
G_DECLARE_FINAL_TYPE (CcLanguageRow, cc_language_row, CC, LANGUAGE_ROW, GtkListBoxRow)

CcLanguageRow *cc_language_row_new                (const gchar *locale_id);

const gchar   *cc_language_row_get_locale_id      (CcLanguageRow *row);

const gchar   *cc_language_row_get_language       (CcLanguageRow *row);

const gchar   *cc_language_row_get_language_local (CcLanguageRow *row);

const gchar   *cc_language_row_get_country        (CcLanguageRow *row);

const gchar   *cc_language_row_get_country_local  (CcLanguageRow *row);

void           cc_language_row_set_checked        (CcLanguageRow *row, gboolean checked);

void           cc_language_row_set_is_extra       (CcLanguageRow *row, gboolean is_extra);

gboolean       cc_language_row_get_is_extra       (CcLanguageRow *row);

G_END_DECLS
