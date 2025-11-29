/* gtkcellareaboxcontext.h
 *
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/deprecated/gtkcellareacontext.h>
#include <gtk/deprecated/gtkcellareabox.h>
#include <gtk/deprecated/gtkcellrenderer.h>
#include <gtk/gtksizerequest.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_AREA_BOX_CONTEXT            (_gtk_cell_area_box_context_get_type ())
#define GTK_CELL_AREA_BOX_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_AREA_BOX_CONTEXT, GtkCellAreaBoxContext))
#define GTK_CELL_AREA_BOX_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_AREA_BOX_CONTEXT, GtkCellAreaBoxContextClass))
#define GTK_IS_CELL_AREA_BOX_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_AREA_BOX_CONTEXT))
#define GTK_IS_CELL_AREA_BOX_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_AREA_BOX_CONTEXT))
#define GTK_CELL_AREA_BOX_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_AREA_BOX_CONTEXT, GtkCellAreaBoxContextClass))

typedef struct _GtkCellAreaBoxContext              GtkCellAreaBoxContext;
typedef struct _GtkCellAreaBoxContextClass         GtkCellAreaBoxContextClass;
typedef struct _GtkCellAreaBoxContextPrivate       GtkCellAreaBoxContextPrivate;

struct _GtkCellAreaBoxContext
{
  GtkCellAreaContext parent_instance;

  GtkCellAreaBoxContextPrivate *priv;
};

struct _GtkCellAreaBoxContextClass
{
  GtkCellAreaContextClass parent_class;

};

GType   _gtk_cell_area_box_context_get_type                     (void) G_GNUC_CONST;


/* Create a duplicate of the context */
GtkCellAreaBoxContext *_gtk_cell_area_box_context_copy          (GtkCellAreaBox        *box,
                                                                GtkCellAreaBoxContext *box_context);

/* Initialize group array dimensions */
void    _gtk_cell_area_box_init_groups                         (GtkCellAreaBoxContext *box_context,
                                                                guint                  n_groups,
                                                                gboolean              *expand_groups,
                                                                gboolean              *align_groups);

/* Update cell-group sizes */
void    _gtk_cell_area_box_context_push_group_width             (GtkCellAreaBoxContext *box_context,
                                                                int                    group_idx,
                                                                int                    minimum_width,
                                                                int                    natural_width);

void    _gtk_cell_area_box_context_push_group_height_for_width  (GtkCellAreaBoxContext *box_context,
                                                                int                    group_idx,
                                                                int                    for_width,
                                                                int                    minimum_height,
                                                                int                    natural_height);

void    _gtk_cell_area_box_context_push_group_height            (GtkCellAreaBoxContext *box_context,
                                                                int                    group_idx,
                                                                int                    minimum_height,
                                                                int                    natural_height);

void    _gtk_cell_area_box_context_push_group_width_for_height  (GtkCellAreaBoxContext *box_context,
                                                                int                    group_idx,
                                                                int                    for_height,
                                                                int                    minimum_width,
                                                                int                    natural_width);

/* Fetch cell-group sizes */
void    _gtk_cell_area_box_context_get_group_width              (GtkCellAreaBoxContext *box_context,
                                                                int                    group_idx,
                                                                int                   *minimum_width,
                                                                int                   *natural_width);

void    _gtk_cell_area_box_context_get_group_height_for_width   (GtkCellAreaBoxContext *box_context,
                                                                int                    group_idx,
                                                                int                    for_width,
                                                                int                   *minimum_height,
                                                                int                   *natural_height);

void    _gtk_cell_area_box_context_get_group_height             (GtkCellAreaBoxContext *box_context,
                                                                int                    group_idx,
                                                                int                   *minimum_height,
                                                                int                   *natural_height);

void    _gtk_cell_area_box_context_get_group_width_for_height   (GtkCellAreaBoxContext *box_context,
                                                                int                    group_idx,
                                                                int                    for_height,
                                                                int                   *minimum_width,
                                                                int                   *natural_width);

GtkRequestedSize *_gtk_cell_area_box_context_get_widths         (GtkCellAreaBoxContext *box_context,
                                                                int                   *n_widths);
GtkRequestedSize *_gtk_cell_area_box_context_get_heights        (GtkCellAreaBoxContext *box_context,
                                                                int                   *n_heights);

/* Private context/area interaction */
typedef struct {
  int group_idx; /* Groups containing only invisible cells are not allocated */
  int position;  /* Relative group allocation position in the orientation of the box */
  int size;      /* Full allocated size of the cells in this group spacing inclusive */
} GtkCellAreaBoxAllocation;

GtkCellAreaBoxAllocation *
_gtk_cell_area_box_context_get_orientation_allocs (GtkCellAreaBoxContext *context,
                                                  int                   *n_allocs);

G_END_DECLS

