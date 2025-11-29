/* gtkellview.c
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
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

#include "config.h"

#include "gtkcellview.h"

#include "gtkbuildable.h"
#include "gtkcelllayout.h"
#include "gtkcellareabox.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrenderertext.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

#include <gobject/gmarshal.h>

#include <string.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkCellView:
 *
 * A widget displaying a single row of a GtkTreeModel
 *
 * A `GtkCellView` displays a single row of a `GtkTreeModel` using a `GtkCellArea`
 * and `GtkCellAreaContext`. A `GtkCellAreaContext` can be provided to the
 * `GtkCellView` at construction time in order to keep the cellview in context
 * of a group of cell views, this ensures that the renderers displayed will
 * be properly aligned with each other (like the aligned cells in the menus
 * of `GtkComboBox`).
 *
 * `GtkCellView` is `GtkOrientable` in order to decide in which orientation
 * the underlying `GtkCellAreaContext` should be allocated. Taking the `GtkComboBox`
 * menu as an example, cellviews should be oriented horizontally if the menus are
 * listed top-to-bottom and thus all share the same width but may have separate
 * individual heights (left-to-right menus should be allocated vertically since
 * they all share the same height but may have variable widths).
 *
 * ## CSS nodes
 *
 * GtkCellView has a single CSS node with name cellview.
 *
 * Deprecated: 4.10: List views use widgets to display their contents.
 *   You can use [class@Gtk.Box] instead
 */

static void        gtk_cell_view_constructed              (GObject          *object);
static void        gtk_cell_view_get_property             (GObject          *object,
                                                           guint             param_id,
                                                           GValue           *value,
                                                           GParamSpec       *pspec);
static void        gtk_cell_view_set_property             (GObject          *object,
                                                           guint             param_id,
                                                           const GValue     *value,
                                                           GParamSpec       *pspec);
static void        gtk_cell_view_finalize                 (GObject          *object);
static void        gtk_cell_view_dispose                  (GObject          *object);
static void        gtk_cell_view_size_allocate            (GtkWidget        *widget,
                                                           int               width,
                                                           int               height,
                                                           int               baseline);
static void        gtk_cell_view_snapshot                 (GtkWidget        *widget,
                                                           GtkSnapshot      *snapshot);
static void        gtk_cell_view_set_value                (GtkCellView     *cell_view,
                                                           GtkCellRenderer *renderer,
                                                           const char      *property,
                                                           GValue          *value);
static void        gtk_cell_view_set_cell_data            (GtkCellView      *cell_view);

/* celllayout */
static void        gtk_cell_view_cell_layout_init         (GtkCellLayoutIface *iface);
static GtkCellArea *gtk_cell_view_cell_layout_get_area         (GtkCellLayout         *layout);


/* buildable */
static void        gtk_cell_view_buildable_init             (GtkBuildableIface  *iface);
static gboolean    gtk_cell_view_buildable_custom_tag_start (GtkBuildable       *buildable,
                                                             GtkBuilder         *builder,
                                                             GObject            *child,
                                                             const char         *tagname,
                                                             GtkBuildableParser *parser,
                                                             gpointer           *data);
static void        gtk_cell_view_buildable_custom_tag_end   (GtkBuildable       *buildable,
                                                             GtkBuilder         *builder,
                                                             GObject            *child,
                                                             const char         *tagname,
                                                             gpointer            data);

static GtkSizeRequestMode gtk_cell_view_get_request_mode       (GtkWidget             *widget);
static void gtk_cell_view_measure (GtkWidget      *widget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline);
static void       context_size_changed_cb                      (GtkCellAreaContext   *context,
								GParamSpec           *pspec,
								GtkWidget            *view);
static void       row_changed_cb                               (GtkTreeModel         *model,
								GtkTreePath          *path,
								GtkTreeIter          *iter,
								GtkCellView          *view);

typedef struct _GtkCellViewClass        GtkCellViewClass;
typedef struct _GtkCellViewPrivate      GtkCellViewPrivate;

struct _GtkCellView
{
  GtkWidget parent_instance;
};

struct _GtkCellViewClass
{
  GtkWidgetClass parent_class;
};

struct _GtkCellViewPrivate
{
  GtkTreeModel        *model;
  GtkTreeRowReference *displayed_row;

  GtkCellArea         *area;
  GtkCellAreaContext  *context;

  gulong               size_changed_id;
  gulong               row_changed_id;

  GtkOrientation       orientation;

  guint                draw_sensitive : 1;
  guint                fit_model      : 1;
};

static GtkBuildableIface *parent_buildable_iface;

enum
{
  PROP_0,
  PROP_ORIENTATION,
  PROP_MODEL,
  PROP_CELL_AREA,
  PROP_CELL_AREA_CONTEXT,
  PROP_DRAW_SENSITIVE,
  PROP_FIT_MODEL
};

G_DEFINE_TYPE_WITH_CODE (GtkCellView, gtk_cell_view, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkCellView)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
						gtk_cell_view_cell_layout_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_cell_view_buildable_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
gtk_cell_view_class_init (GtkCellViewClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructed = gtk_cell_view_constructed;
  gobject_class->get_property = gtk_cell_view_get_property;
  gobject_class->set_property = gtk_cell_view_set_property;
  gobject_class->finalize = gtk_cell_view_finalize;
  gobject_class->dispose = gtk_cell_view_dispose;

  widget_class->snapshot                       = gtk_cell_view_snapshot;
  widget_class->size_allocate                  = gtk_cell_view_size_allocate;
  widget_class->get_request_mode               = gtk_cell_view_get_request_mode;
  widget_class->measure                        = gtk_cell_view_measure;

  /* properties */
  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

  /**
   * GtkCellView:model:
   *
   * The model for cell view
   *
   * since 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_MODEL,
				   g_param_spec_object  ("model", NULL, NULL,
							 GTK_TYPE_TREE_MODEL,
							 GTK_PARAM_READWRITE));


  /**
   * GtkCellView:cell-area:
   *
   * The `GtkCellArea` rendering cells
   *
   * If no area is specified when creating the cell view with gtk_cell_view_new_with_context()
   * a horizontally oriented `GtkCellArea`Box will be used.
   *
   * since 3.0
   */
   g_object_class_install_property (gobject_class,
                                    PROP_CELL_AREA,
                                    g_param_spec_object ("cell-area", NULL, NULL,
							 GTK_TYPE_CELL_AREA,
							 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkCellView:cell-area-context:
   *
   * The `GtkCellAreaContext` used to compute the geometry of the cell view.
   *
   * A group of cell views can be assigned the same context in order to
   * ensure the sizes and cell alignments match across all the views with
   * the same context.
   *
   * `GtkComboBox` menus uses this to assign the same context to all cell views
   * in the menu items for a single menu (each submenu creates its own
   * context since the size of each submenu does not depend on parent
   * or sibling menus).
   *
   * since 3.0
   */
   g_object_class_install_property (gobject_class,
                                    PROP_CELL_AREA_CONTEXT,
                                    g_param_spec_object ("cell-area-context", NULL, NULL,
							 GTK_TYPE_CELL_AREA_CONTEXT,
							 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkCellView:draw-sensitive:
   *
   * Whether all cells should be draw as sensitive for this view regardless
   * of the actual cell properties (used to make menus with submenus appear
   * sensitive when the items in submenus might be insensitive).
   *
   * since 3.0
   */
   g_object_class_install_property (gobject_class,
                                    PROP_DRAW_SENSITIVE,
                                    g_param_spec_boolean ("draw-sensitive", NULL, NULL,
							  FALSE,
							  GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellView:fit-model:
   *
   * Whether the view should request enough space to always fit
   * the size of every row in the model (used by the combo box to
   * ensure the combo box size doesn't change when different items
   * are selected).
   *
   * since 3.0
   */
   g_object_class_install_property (gobject_class,
                                    PROP_FIT_MODEL,
                                    g_param_spec_boolean ("fit-model", NULL, NULL,
							  FALSE,
							  GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (widget_class, I_("cellview"));
}

static void
gtk_cell_view_buildable_add_child (GtkBuildable *buildable,
                                   GtkBuilder   *builder,
                                   GObject      *child,
                                   const char   *type)
{
  if (GTK_IS_CELL_RENDERER (child))
    _gtk_cell_layout_buildable_add_child (buildable, builder, child, type);
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_cell_view_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_cell_view_buildable_add_child;
  iface->custom_tag_start = gtk_cell_view_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_cell_view_buildable_custom_tag_end;
}

static void
gtk_cell_view_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->get_area = gtk_cell_view_cell_layout_get_area;
}

static void
gtk_cell_view_constructed (GObject *object)
{
  GtkCellView *view = GTK_CELL_VIEW (object);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (view);

  G_OBJECT_CLASS (gtk_cell_view_parent_class)->constructed (object);

  if (!priv->area)
    {
      priv->area = gtk_cell_area_box_new ();
      g_object_ref_sink (priv->area);
    }

  if (!priv->context)
    priv->context = gtk_cell_area_create_context (priv->area);

  priv->size_changed_id =
    g_signal_connect (priv->context, "notify",
		      G_CALLBACK (context_size_changed_cb), view);
}

static void
gtk_cell_view_get_property (GObject    *object,
                            guint       param_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkCellView *view = GTK_CELL_VIEW (object);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (view);

  switch (param_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_MODEL:
      g_value_set_object (value, priv->model);
      break;
    case PROP_CELL_AREA:
      g_value_set_object (value, priv->area);
      break;
    case PROP_CELL_AREA_CONTEXT:
      g_value_set_object (value, priv->context);
      break;
    case PROP_DRAW_SENSITIVE:
      g_value_set_boolean (value, priv->draw_sensitive);
      break;
    case PROP_FIT_MODEL:
      g_value_set_boolean (value, priv->fit_model);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_cell_view_set_property (GObject      *object,
                            guint         param_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkCellView *view = GTK_CELL_VIEW (object);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (view);
  GtkCellArea *area;
  GtkCellAreaContext *context;

  switch (param_id)
    {
    case PROP_ORIENTATION:
      if (priv->orientation != g_value_get_enum (value))
        {
          priv->orientation = g_value_get_enum (value);
          if (priv->context)
            gtk_cell_area_context_reset (priv->context);
          gtk_widget_update_orientation (GTK_WIDGET (object), priv->orientation);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_MODEL:
      gtk_cell_view_set_model (view, g_value_get_object (value));
      break;
    case PROP_CELL_AREA:
      /* Construct-only, can only be assigned once */
      area = g_value_get_object (value);

      if (area)
        {
          if (priv->area != NULL)
            {
              g_warning ("cell-area has already been set, ignoring construct property");
              g_object_ref_sink (area);
              g_object_unref (area);
            }
          else
            priv->area = g_object_ref_sink (area);
        }
      break;
    case PROP_CELL_AREA_CONTEXT:
      /* Construct-only, can only be assigned once */
      context = g_value_get_object (value);

      if (context)
        {
          if (priv->context != NULL)
            {
              g_warning ("cell-area-context has already been set, ignoring construct property");
              g_object_ref_sink (context);
              g_object_unref (context);
            }
          else
            priv->context = g_object_ref (context);
        }
      break;

    case PROP_DRAW_SENSITIVE:
      gtk_cell_view_set_draw_sensitive (view, g_value_get_boolean (value));
      break;

    case PROP_FIT_MODEL:
      gtk_cell_view_set_fit_model (view, g_value_get_boolean (value));
      break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_cell_view_init (GtkCellView *cellview)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cellview);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
}

static void
gtk_cell_view_finalize (GObject *object)
{
  GtkCellView *cellview = GTK_CELL_VIEW (object);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cellview);

  if (priv->displayed_row)
    gtk_tree_row_reference_free (priv->displayed_row);

  G_OBJECT_CLASS (gtk_cell_view_parent_class)->finalize (object);
}

static void
gtk_cell_view_dispose (GObject *object)
{
  GtkCellView *cellview = GTK_CELL_VIEW (object);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cellview);

  gtk_cell_view_set_model (cellview, NULL);

  g_clear_object (&priv->area);

  if (priv->context)
    {
      g_signal_handler_disconnect (priv->context, priv->size_changed_id);

      g_object_unref (priv->context);
      priv->context = NULL;
      priv->size_changed_id = 0;
    }

  G_OBJECT_CLASS (gtk_cell_view_parent_class)->dispose (object);
}

static void
gtk_cell_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkCellView *cellview = GTK_CELL_VIEW (widget);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cellview);
  int alloc_width, alloc_height;

  gtk_cell_area_context_get_allocation (priv->context, &alloc_width, &alloc_height);

  /* The first cell view in context is responsible for allocating the context at
   * allocate time (or the cellview has its own context and is not grouped with
   * any other cell views)
   *
   * If the cellview is in "fit model" mode, we assume it's not in context and
   * needs to allocate every time.
   */
  if (priv->fit_model)
    gtk_cell_area_context_allocate (priv->context, width, height);
  else if (alloc_width != width && priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_cell_area_context_allocate (priv->context, width, -1);
  else if (alloc_height != height && priv->orientation == GTK_ORIENTATION_VERTICAL)
    gtk_cell_area_context_allocate (priv->context, -1, height);
}

static void
gtk_cell_view_request_model (GtkCellView        *cellview,
			     GtkTreeIter        *parent,
			     GtkOrientation      orientation,
			     int                 for_size,
			     int                *minimum_size,
			     int                *natural_size)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cellview);
  GtkTreeIter         iter;
  gboolean            valid;

  if (!priv->model)
    return;

  valid = gtk_tree_model_iter_children (priv->model, &iter, parent);
  while (valid)
    {
      int min, nat;

      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  if (for_size < 0)
	    gtk_cell_area_get_preferred_width (priv->area, priv->context,
					       GTK_WIDGET (cellview), &min, &nat);
	  else
	    gtk_cell_area_get_preferred_width_for_height (priv->area, priv->context,
							  GTK_WIDGET (cellview), for_size, &min, &nat);
	}
      else
	{
	  if (for_size < 0)
	    gtk_cell_area_get_preferred_height (priv->area, priv->context,
						GTK_WIDGET (cellview), &min, &nat);
	  else
	    gtk_cell_area_get_preferred_height_for_width (priv->area, priv->context,
							  GTK_WIDGET (cellview), for_size, &min, &nat);
	}

      *minimum_size = MAX (min, *minimum_size);
      *natural_size = MAX (nat, *natural_size);

      /* Recurse into children when they exist */
      gtk_cell_view_request_model (cellview, &iter, orientation, for_size, minimum_size, natural_size);

      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }
}

static GtkSizeRequestMode
gtk_cell_view_get_request_mode (GtkWidget *widget)
{
  GtkCellView        *cellview = GTK_CELL_VIEW (widget);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cellview);

  return gtk_cell_area_get_request_mode (priv->area);
}

static void
gtk_cell_view_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkCellView *cellview = GTK_CELL_VIEW (widget);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cellview);

  g_signal_handler_block (priv->context, priv->size_changed_id);

  if (orientation == GTK_ORIENTATION_HORIZONTAL && for_size == -1)
    {
      if (priv->fit_model)
        {
          int min = 0, nat = 0;
          gtk_cell_view_request_model (cellview, NULL, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat);
        }
      else
        {
          if (priv->displayed_row)
            gtk_cell_view_set_cell_data (cellview);

          gtk_cell_area_get_preferred_width (priv->area, priv->context, widget, NULL, NULL);
        }

      gtk_cell_area_context_get_preferred_width (priv->context, minimum, natural);
    }
  else if (orientation == GTK_ORIENTATION_VERTICAL && for_size == -1)
    {
      if (priv->fit_model)
        {
          int min = 0, nat = 0;
          gtk_cell_view_request_model (cellview, NULL, GTK_ORIENTATION_VERTICAL, -1, &min, &nat);
        }
      else
        {
          if (priv->displayed_row)
            gtk_cell_view_set_cell_data (cellview);

          gtk_cell_area_get_preferred_height (priv->area, priv->context, widget, NULL, NULL);
        }

      gtk_cell_area_context_get_preferred_height (priv->context, minimum, natural);
    }
  else if (orientation == GTK_ORIENTATION_HORIZONTAL && for_size >= 0)
    {
      if (priv->fit_model)
        {
          int min = 0, nat = 0;
          gtk_cell_view_request_model (cellview, NULL, GTK_ORIENTATION_HORIZONTAL, for_size, &min, &nat);

          *minimum = min;
          *natural = nat;
        }
      else
        {
          if (priv->displayed_row)
            gtk_cell_view_set_cell_data (cellview);

          gtk_cell_area_get_preferred_width_for_height (priv->area, priv->context, widget,
                                                        for_size, minimum, natural);
        }
    }
  else
   {
      if (priv->fit_model)
        {
          int min = 0, nat = 0;
          gtk_cell_view_request_model (cellview, NULL, GTK_ORIENTATION_VERTICAL, for_size, &min, &nat);

          *minimum = min;
          *natural = nat;
        }
      else
        {
          if (priv->displayed_row)
            gtk_cell_view_set_cell_data (cellview);

          gtk_cell_area_get_preferred_height_for_width (priv->area, priv->context, widget,
                                                        for_size, minimum, natural);
        }
    }

  g_signal_handler_unblock (priv->context, priv->size_changed_id);
}

static void
gtk_cell_view_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkCellView *cellview = GTK_CELL_VIEW (widget);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cellview);
  GdkRectangle area;
  GtkCellRendererState state;

  /* render cells */
  area.x = 0;
  area.y = 0;
  area.width = gtk_widget_get_width (widget);
  area.height = gtk_widget_get_height (widget);

  /* set cell data (if available) */
  if (priv->displayed_row)
    gtk_cell_view_set_cell_data (cellview);
  else if (priv->model)
    return;

  if (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_PRELIGHT)
    state = GTK_CELL_RENDERER_PRELIT;
  else
    state = 0;

  /* Render the cells */
  gtk_cell_area_snapshot (priv->area, priv->context,
			  widget, snapshot, &area, &area, state, FALSE);


}

static void
gtk_cell_view_set_cell_data (GtkCellView *cell_view)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cell_view);
  GtkTreeIter iter;
  GtkTreePath *path;

  g_return_if_fail (priv->displayed_row != NULL);

  path = gtk_tree_row_reference_get_path (priv->displayed_row);
  if (!path)
    return;

  gtk_tree_model_get_iter (priv->model, &iter, path);
  gtk_tree_path_free (path);

  gtk_cell_area_apply_attributes (priv->area,
				  priv->model,
				  &iter, FALSE, FALSE);

  if (priv->draw_sensitive)
    {
      GList *l, *cells =
	gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (priv->area));

      for (l = cells; l; l = l->next)
	{
	  GObject *renderer = l->data;

	  g_object_set (renderer, "sensitive", TRUE, NULL);
	}
      g_list_free (cells);
    }
}

/* GtkCellLayout implementation */
static GtkCellArea *
gtk_cell_view_cell_layout_get_area (GtkCellLayout   *layout)
{
  GtkCellView *cellview = GTK_CELL_VIEW (layout);
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cellview);

  if (G_UNLIKELY (!priv->area))
    {
      priv->area = gtk_cell_area_box_new ();
      g_object_ref_sink (priv->area);
    }

  return priv->area;
}

/* GtkBuildable implementation */
static gboolean
gtk_cell_view_buildable_custom_tag_start (GtkBuildable       *buildable,
                                          GtkBuilder         *builder,
                                          GObject            *child,
                                          const char         *tagname,
                                          GtkBuildableParser *parser,
                                          gpointer           *data)
{
  if (parent_buildable_iface->custom_tag_start &&
      parent_buildable_iface->custom_tag_start (buildable, builder, child,
						tagname, parser, data))
    return TRUE;

  return _gtk_cell_layout_buildable_custom_tag_start (buildable, builder, child,
						      tagname, parser, data);
}

static void
gtk_cell_view_buildable_custom_tag_end (GtkBuildable *buildable,
					GtkBuilder   *builder,
					GObject      *child,
					const char   *tagname,
					gpointer      data)
{
  if (_gtk_cell_layout_buildable_custom_tag_end (buildable, builder, child, tagname,
						 data))
    return;
  else if (parent_buildable_iface->custom_tag_end)
    parent_buildable_iface->custom_tag_end (buildable, builder, child, tagname,
					    data);
}

static void
context_size_changed_cb (GtkCellAreaContext  *context,
			 GParamSpec          *pspec,
			 GtkWidget           *view)
{
  if (!strcmp (pspec->name, "minimum-width") ||
      !strcmp (pspec->name, "natural-width") ||
      !strcmp (pspec->name, "minimum-height") ||
      !strcmp (pspec->name, "natural-height"))
    gtk_widget_queue_resize (view);
}

static void
row_changed_cb (GtkTreeModel         *model,
		GtkTreePath          *path,
		GtkTreeIter          *iter,
		GtkCellView          *view)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (view);
  GtkTreePath *row_path;

  if (priv->displayed_row)
    {
      row_path = gtk_tree_row_reference_get_path (priv->displayed_row);

      if (row_path)
	{
	  /* Resize everything in our context if our row changed */
	  if (gtk_tree_path_compare (row_path, path) == 0)
	    gtk_cell_area_context_reset (priv->context);

	  gtk_tree_path_free (row_path);
	}
    }
}

/**
 * gtk_cell_view_new:
 *
 * Creates a new `GtkCellView` widget.
 *
 * Returns: A newly created `GtkCellView` widget.
 *
 * Deprecated: 4.10
 */
GtkWidget *
gtk_cell_view_new (void)
{
  GtkCellView *cellview;

  cellview = g_object_new (GTK_TYPE_CELL_VIEW, NULL);

  return GTK_WIDGET (cellview);
}


/**
 * gtk_cell_view_new_with_context:
 * @area: the `GtkCellArea` to layout cells
 * @context: the `GtkCellAreaContext` in which to calculate cell geometry
 *
 * Creates a new `GtkCellView` widget with a specific `GtkCellArea`
 * to layout cells and a specific `GtkCellAreaContext`.
 *
 * Specifying the same context for a handful of cells lets
 * the underlying area synchronize the geometry for those cells,
 * in this way alignments with cellviews for other rows are
 * possible.
 *
 * Returns: A newly created `GtkCellView` widget.
 *
 * Deprecated: 4.10
 */
GtkWidget *
gtk_cell_view_new_with_context (GtkCellArea        *area,
				GtkCellAreaContext *context)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);
  g_return_val_if_fail (context == NULL || GTK_IS_CELL_AREA_CONTEXT (context), NULL);

  return (GtkWidget *)g_object_new (GTK_TYPE_CELL_VIEW,
				    "cell-area", area,
				    "cell-area-context", context,
				    NULL);
}

/**
 * gtk_cell_view_new_with_text:
 * @text: the text to display in the cell view
 *
 * Creates a new `GtkCellView` widget, adds a `GtkCellRendererText`
 * to it, and makes it show @text.
 *
 * Returns: A newly created `GtkCellView` widget.
 *
 * Deprecated: 4.10
 */
GtkWidget *
gtk_cell_view_new_with_text (const char *text)
{
  GtkCellView *cellview;
  GtkCellRenderer *renderer;
  GValue value = G_VALUE_INIT;

  cellview = GTK_CELL_VIEW (gtk_cell_view_new ());

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
			      renderer, TRUE);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, text);
  gtk_cell_view_set_value (cellview, renderer, "text", &value);
  g_value_unset (&value);

  return GTK_WIDGET (cellview);
}

/**
 * gtk_cell_view_new_with_markup:
 * @markup: the text to display in the cell view
 *
 * Creates a new `GtkCellView` widget, adds a `GtkCellRendererText`
 * to it, and makes it show @markup. The text can be marked up with
 * the [Pango text markup language](https://docs.gtk.org/Pango/pango_markup.html).
 *
 * Returns: A newly created `GtkCellView` widget.
 *
 * Deprecated: 4.10
 */
GtkWidget *
gtk_cell_view_new_with_markup (const char *markup)
{
  GtkCellView *cellview;
  GtkCellRenderer *renderer;
  GValue value = G_VALUE_INIT;

  cellview = GTK_CELL_VIEW (gtk_cell_view_new ());

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
			      renderer, TRUE);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, markup);
  gtk_cell_view_set_value (cellview, renderer, "markup", &value);
  g_value_unset (&value);

  return GTK_WIDGET (cellview);
}

/**
 * gtk_cell_view_new_with_texture:
 * @texture: the image to display in the cell view
 *
 * Creates a new `GtkCellView` widget, adds a `GtkCellRendererPixbuf`
 * to it, and makes it show @texture.
 *
 * Returns: A newly created `GtkCellView` widget.
 *
 * Deprecated: 4.10
 */
GtkWidget *
gtk_cell_view_new_with_texture (GdkTexture *texture)
{
  GtkCellView *cellview;
  GtkCellRenderer *renderer;
  GValue value = G_VALUE_INIT;

  cellview = GTK_CELL_VIEW (gtk_cell_view_new ());

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
			      renderer, TRUE);

  g_value_init (&value, GDK_TYPE_TEXTURE);
  g_value_set_object (&value, texture);
  gtk_cell_view_set_value (cellview, renderer, "texture", &value);
  g_value_unset (&value);

  return GTK_WIDGET (cellview);
}

/**
 * gtk_cell_view_set_value:
 * @cell_view: a `GtkCellView` widget
 * @renderer: one of the renderers of @cell_view
 * @property: the name of the property of @renderer to set
 * @value: the new value to set the property to
 *
 * Sets a property of a cell renderer of @cell_view, and
 * makes sure the display of @cell_view is updated.
 *
 * Deprecated: 4.10
 */
static void
gtk_cell_view_set_value (GtkCellView     *cell_view,
                         GtkCellRenderer *renderer,
                         const char      *property,
                         GValue          *value)
{
  g_object_set_property (G_OBJECT (renderer), property, value);

  /* force resize and redraw */
  gtk_widget_queue_resize (GTK_WIDGET (cell_view));
  gtk_widget_queue_draw (GTK_WIDGET (cell_view));
}

/**
 * gtk_cell_view_set_model:
 * @cell_view: a `GtkCellView`
 * @model: (nullable): a `GtkTreeModel`
 *
 * Sets the model for @cell_view.  If @cell_view already has a model
 * set, it will remove it before setting the new model.  If @model is
 * %NULL, then it will unset the old model.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_view_set_model (GtkCellView  *cell_view,
                         GtkTreeModel *model)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cell_view);

  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  if (priv->model)
    {
      g_signal_handler_disconnect (priv->model, priv->row_changed_id);
      priv->row_changed_id = 0;

      if (priv->displayed_row)
        gtk_tree_row_reference_free (priv->displayed_row);
      priv->displayed_row = NULL;

      g_object_unref (priv->model);
    }

  priv->model = model;

  if (priv->model)
    {
      g_object_ref (priv->model);

      priv->row_changed_id =
	g_signal_connect (priv->model, "row-changed",
			  G_CALLBACK (row_changed_cb), cell_view);
    }
}

/**
 * gtk_cell_view_get_model:
 * @cell_view: a `GtkCellView`
 *
 * Returns the model for @cell_view. If no model is used %NULL is
 * returned.
 *
 * Returns: (nullable) (transfer none): a `GtkTreeModel` used
 *
 * Deprecated: 4.10
 */
GtkTreeModel *
gtk_cell_view_get_model (GtkCellView *cell_view)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cell_view);

  g_return_val_if_fail (GTK_IS_CELL_VIEW (cell_view), NULL);

  return priv->model;
}

/**
 * gtk_cell_view_set_displayed_row:
 * @cell_view: a `GtkCellView`
 * @path: (nullable): a `GtkTreePath` or %NULL to unset.
 *
 * Sets the row of the model that is currently displayed
 * by the `GtkCellView`. If the path is unset, then the
 * contents of the cellview “stick” at their last value;
 * this is not normally a desired result, but may be
 * a needed intermediate state if say, the model for
 * the `GtkCellView` becomes temporarily empty.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_view_set_displayed_row (GtkCellView *cell_view,
                                 GtkTreePath *path)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cell_view);

  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (GTK_IS_TREE_MODEL (priv->model));

  if (priv->displayed_row)
    gtk_tree_row_reference_free (priv->displayed_row);

  if (path)
    priv->displayed_row = gtk_tree_row_reference_new (priv->model, path);
  else
    priv->displayed_row = NULL;

  /* force resize and redraw */
  gtk_widget_queue_resize (GTK_WIDGET (cell_view));
  gtk_widget_queue_draw (GTK_WIDGET (cell_view));
}

/**
 * gtk_cell_view_get_displayed_row:
 * @cell_view: a `GtkCellView`
 *
 * Returns a `GtkTreePath` referring to the currently
 * displayed row. If no row is currently displayed,
 * %NULL is returned.
 *
 * Returns: (nullable) (transfer full): the currently displayed row
 *
 * Deprecated: 4.10
 */
GtkTreePath *
gtk_cell_view_get_displayed_row (GtkCellView *cell_view)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cell_view);

  g_return_val_if_fail (GTK_IS_CELL_VIEW (cell_view), NULL);

  if (!priv->displayed_row)
    return NULL;

  return gtk_tree_row_reference_get_path (priv->displayed_row);
}

/**
 * gtk_cell_view_get_draw_sensitive:
 * @cell_view: a `GtkCellView`
 *
 * Gets whether @cell_view is configured to draw all of its
 * cells in a sensitive state.
 *
 * Returns: whether @cell_view draws all of its
 * cells in a sensitive state
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_view_get_draw_sensitive (GtkCellView     *cell_view)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cell_view);

  g_return_val_if_fail (GTK_IS_CELL_VIEW (cell_view), FALSE);

  return priv->draw_sensitive;
}

/**
 * gtk_cell_view_set_draw_sensitive:
 * @cell_view: a `GtkCellView`
 * @draw_sensitive: whether to draw all cells in a sensitive state.
 *
 * Sets whether @cell_view should draw all of its
 * cells in a sensitive state, this is used by `GtkComboBox` menus
 * to ensure that rows with insensitive cells that contain
 * children appear sensitive in the parent menu item.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_view_set_draw_sensitive (GtkCellView     *cell_view,
				  gboolean         draw_sensitive)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cell_view);

  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));

  if (priv->draw_sensitive != draw_sensitive)
    {
      priv->draw_sensitive = draw_sensitive;

      g_object_notify (G_OBJECT (cell_view), "draw-sensitive");
    }
}

/**
 * gtk_cell_view_get_fit_model:
 * @cell_view: a `GtkCellView`
 *
 * Gets whether @cell_view is configured to request space
 * to fit the entire `GtkTreeModel`.
 *
 * Returns: whether @cell_view requests space to fit
 * the entire `GtkTreeModel`.
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_view_get_fit_model (GtkCellView     *cell_view)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cell_view);

  g_return_val_if_fail (GTK_IS_CELL_VIEW (cell_view), FALSE);

  return priv->fit_model;
}

/**
 * gtk_cell_view_set_fit_model:
 * @cell_view: a `GtkCellView`
 * @fit_model: whether @cell_view should request space for the whole model.
 *
 * Sets whether @cell_view should request space to fit the entire `GtkTreeModel`.
 *
 * This is used by `GtkComboBox` to ensure that the cell view displayed on
 * the combo box’s button always gets enough space and does not resize
 * when selection changes.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_view_set_fit_model (GtkCellView *cell_view,
                             gboolean     fit_model)
{
  GtkCellViewPrivate *priv = gtk_cell_view_get_instance_private (cell_view);

  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));

  if (priv->fit_model != fit_model)
    {
      priv->fit_model = fit_model;

      gtk_cell_area_context_reset (priv->context);

      g_object_notify (G_OBJECT (cell_view), "fit-model");
    }
}
