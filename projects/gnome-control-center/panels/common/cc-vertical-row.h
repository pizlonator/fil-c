/* cc-vertical-row.h
 *
 * Copyright 2018 Purism SPC
 *           2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *           2023 Red Hat, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_VERTICAL_ROW (cc_vertical_row_get_type())
G_DECLARE_DERIVABLE_TYPE (CcVerticalRow, cc_vertical_row, CC, VERTICAL_ROW, AdwPreferencesRow)

struct _CcVerticalRowClass
{
  AdwPreferencesRowClass parent_class;

  /*< private >*/
  gpointer padding[4];
};

const gchar *cc_vertical_row_get_subtitle (CcVerticalRow *self);
void         cc_vertical_row_set_subtitle (CcVerticalRow *self,
                                           const gchar    *subtitle);

const gchar *cc_vertical_row_get_icon_name (CcVerticalRow *self);
void         cc_vertical_row_set_icon_name (CcVerticalRow *self,
                                            const gchar   *icon_name);

GtkWidget *cc_vertical_row_get_activatable_widget (CcVerticalRow *self);
void       cc_vertical_row_set_activatable_widget (CcVerticalRow *self,
                                                   GtkWidget     *widget);

gboolean cc_vertical_row_get_use_underline (CcVerticalRow *self);
void     cc_vertical_row_set_use_underline (CcVerticalRow *self,
                                            gboolean       use_underline);

gint cc_vertical_row_get_title_lines (CcVerticalRow *self);
void cc_vertical_row_set_title_lines (CcVerticalRow *self,
                                      gint           title_lines);

gint cc_vertical_row_get_subtitle_lines (CcVerticalRow *self);
void cc_vertical_row_set_subtitle_lines (CcVerticalRow *self,
                                         gint           subtitle_lines);

void cc_vertical_row_add_prefix (CcVerticalRow *self,
                                 GtkWidget     *widget);

void cc_vertical_row_add_content (CcVerticalRow *self,
                                  GtkWidget     *widget);

void cc_vertical_row_activate (CcVerticalRow *self);

void cc_vertical_row_remove (CcVerticalRow *self,
                             GtkWidget     *child);

G_END_DECLS
