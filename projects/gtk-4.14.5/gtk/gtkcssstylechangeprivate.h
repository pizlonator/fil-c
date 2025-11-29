/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkcssstyleprivate.h"

G_BEGIN_DECLS

struct _GtkCssStyleChange {
  GtkCssStyle   *old_style;
  GtkCssStyle   *new_style;

  GtkCssAffects  affects;
  GtkBitmask    *changes;
};

void            gtk_css_style_change_init               (GtkCssStyleChange      *change,
                                                         GtkCssStyle            *old_style,
                                                         GtkCssStyle            *new_style);
void            gtk_css_style_change_finish             (GtkCssStyleChange      *change);

GtkCssStyle *   gtk_css_style_change_get_old_style      (GtkCssStyleChange      *change);
GtkCssStyle *   gtk_css_style_change_get_new_style      (GtkCssStyleChange      *change);

gboolean        gtk_css_style_change_has_change         (GtkCssStyleChange      *change);
gboolean        gtk_css_style_change_affects            (GtkCssStyleChange      *change,
                                                         GtkCssAffects           affects);
gboolean        gtk_css_style_change_changes_property   (GtkCssStyleChange      *change,
                                                         guint                   id);
void            gtk_css_style_change_print              (GtkCssStyleChange      *change, GString *string);

char *          gtk_css_style_change_to_string          (GtkCssStyleChange      *change);
G_END_DECLS

