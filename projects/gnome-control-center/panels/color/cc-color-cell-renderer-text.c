/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <gtk/gtk.h>

#include "cc-color-cell-renderer-text.h"

enum {
  PROP_0,
  PROP_IS_DIM_LABEL,
  PROP_LAST
};

struct _CcColorCellRendererText
{
  GtkCellRendererText  parent_instance;

  gboolean             is_dim_label;
};

G_DEFINE_TYPE (CcColorCellRendererText, cc_color_cell_renderer_text, GTK_TYPE_CELL_RENDERER_TEXT)

static gpointer parent_class = NULL;

static void
cc_color_cell_renderer_text_get_property (GObject *object, guint param_id,
                                          GValue *value, GParamSpec *pspec)
{
  CcColorCellRendererText *renderer = CC_COLOR_CELL_RENDERER_TEXT (object);

  switch (param_id)
    {
      case PROP_IS_DIM_LABEL:
        g_value_set_boolean (value, renderer->is_dim_label);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
cc_color_cell_renderer_text_set_property (GObject *object, guint param_id,
                                          const GValue *value, GParamSpec *pspec)
{
  CcColorCellRendererText *renderer = CC_COLOR_CELL_RENDERER_TEXT (object);

  switch (param_id)
    {
      case PROP_IS_DIM_LABEL:
        renderer->is_dim_label = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
cc_color_cell_renderer_snapshot (GtkCellRenderer      *cell,
                                 GtkSnapshot          *snapshot,
                                 GtkWidget            *widget,
                                 const GdkRectangle   *background_area,
                                 const GdkRectangle   *cell_area,
                                 GtkCellRendererState  flags)
{
  CcColorCellRendererText *renderer;
  GtkStyleContext *context;

  renderer = CC_COLOR_CELL_RENDERER_TEXT (cell);
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (context);
  if (renderer->is_dim_label)
    gtk_style_context_add_class (context, "dim-label");
  else
    gtk_style_context_remove_class (context, "dim-label");
  GTK_CELL_RENDERER_CLASS (parent_class)->snapshot (cell, snapshot, widget,
                                                    background_area,
                                                    cell_area, flags);
  gtk_style_context_restore (context);
}

static void
cc_color_cell_renderer_text_class_init (CcColorCellRendererTextClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *object_class_gcr = GTK_CELL_RENDERER_CLASS (class);
  object_class_gcr->snapshot = cc_color_cell_renderer_snapshot;

  parent_class = g_type_class_peek_parent (class);

  object_class->get_property = cc_color_cell_renderer_text_get_property;
  object_class->set_property = cc_color_cell_renderer_text_set_property;

  g_object_class_install_property (object_class, PROP_IS_DIM_LABEL,
                                   g_param_spec_boolean ("is-dim-label",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
cc_color_cell_renderer_text_init (CcColorCellRendererText *renderer)
{
  renderer->is_dim_label = FALSE;
}

GtkCellRenderer *
cc_color_cell_renderer_text_new (void)
{
  return g_object_new (CC_COLOR_TYPE_CELL_RENDERER_TEXT, NULL);
}
