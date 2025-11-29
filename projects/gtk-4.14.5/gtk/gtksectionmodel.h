/*
 * Copyright © 2022 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_SECTION_MODEL       (gtk_section_model_get_type ())

GDK_AVAILABLE_IN_4_12
G_DECLARE_INTERFACE (GtkSectionModel, gtk_section_model, GTK, SECTION_MODEL, GListModel)

/**
 * GtkSectionModelInterface:
 * @get_section: Return the section that covers the given position. If
 *   the position is outside the number of items, returns a single range from
 *   n_items to G_MAXUINT
 *
 * The list of virtual functions for the `GtkSectionModel` interface.
 * No function must be implemented, but unless `GtkSectionModel::get_section()`
 * is implemented, the whole model will just be a single section.
 *
 * Since: 4.12
 */
struct _GtkSectionModelInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  void                  (* get_section)                         (GtkSectionModel      *self,
                                                                 guint                 position,
                                                                 guint                *out_start,
                                                                 guint                *out_end);
};

GDK_AVAILABLE_IN_4_12
void                    gtk_section_model_get_section           (GtkSectionModel      *self,
                                                                 guint                 position,
                                                                 guint                *out_start,
                                                                 guint                *out_end);

/* for implementations only */
GDK_AVAILABLE_IN_4_12
void                    gtk_section_model_sections_changed      (GtkSectionModel      *self,
                                                                 guint                 position,
                                                                 guint                 n_items);

G_END_DECLS

