/* gtkcellrenderer.c
 * Copyright (C) 2000  Red Hat, Inc. Jonathan Blandford
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

#include "gtkcellrenderer.h"

#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtkstylecontext.h"
#include "gtktreeprivate.h"
#include "gtktypebuiltins.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkCellRenderer:
 *
 * An object for rendering a single cell
 *
 * The `GtkCellRenderer` is a base class of a set of objects used for
 * rendering a cell to a `cairo_t`.  These objects are used primarily by
 * the `GtkTreeView` widget, though they aren’t tied to them in any
 * specific way.  It is worth noting that `GtkCellRenderer` is not a
 * `GtkWidget` and cannot be treated as such.
 *
 * The primary use of a `GtkCellRenderer` is for drawing a certain graphical
 * elements on a `cairo_t`. Typically, one cell renderer is used to
 * draw many cells on the screen.  To this extent, it isn’t expected that a
 * CellRenderer keep any permanent state around.  Instead, any state is set
 * just prior to use using `GObject`s property system.  Then, the
 * cell is measured using gtk_cell_renderer_get_preferred_size(). Finally, the cell
 * is rendered in the correct location using gtk_cell_renderer_snapshot().
 *
 * There are a number of rules that must be followed when writing a new
 * `GtkCellRenderer`.  First and foremost, it’s important that a certain set
 * of properties will always yield a cell renderer of the same size,
 * barring a style change. The `GtkCellRenderer` also has a number of
 * generic properties that are expected to be honored by all children.
 *
 * Beyond merely rendering a cell, cell renderers can optionally
 * provide active user interface elements. A cell renderer can be
 * “activatable” like `GtkCellRenderer`Toggle,
 * which toggles when it gets activated by a mouse click, or it can be
 * “editable” like `GtkCellRenderer`Text, which
 * allows the user to edit the text using a widget implementing the
 * `GtkCellEditable` interface, e.g. `GtkEntry`.
 * To make a cell renderer activatable or editable, you have to
 * implement the `GtkCellRenderer`Class.activate or
 * `GtkCellRenderer`Class.start_editing virtual functions, respectively.
 *
 * Many properties of `GtkCellRenderer` and its subclasses have a
 * corresponding “set” property, e.g. “cell-background-set” corresponds
 * to “cell-background”. These “set” properties reflect whether a property
 * has been set or not. You should not set them independently.
 *
 * Deprecated: 4.10: List views use widgets for displaying their
 *   contents
 */


#define DEBUG_CELL_SIZE_REQUEST 0

static void gtk_cell_renderer_init          (GtkCellRenderer      *cell);
static void gtk_cell_renderer_class_init    (GtkCellRendererClass *class);
static void gtk_cell_renderer_get_property  (GObject              *object,
					     guint                 param_id,
					     GValue               *value,
					     GParamSpec           *pspec);
static void gtk_cell_renderer_set_property  (GObject              *object,
					     guint                 param_id,
					     const GValue         *value,
					     GParamSpec           *pspec);
static void set_cell_bg_color               (GtkCellRenderer      *cell,
					     GdkRGBA              *rgba);

static GtkSizeRequestMode gtk_cell_renderer_real_get_request_mode(GtkCellRenderer         *cell);
static void gtk_cell_renderer_real_get_preferred_width           (GtkCellRenderer         *cell,
                                                                  GtkWidget               *widget,
                                                                  int                     *minimum_size,
                                                                  int                     *natural_size);
static void gtk_cell_renderer_real_get_preferred_height          (GtkCellRenderer         *cell,
                                                                  GtkWidget               *widget,
                                                                  int                     *minimum_size,
                                                                  int                     *natural_size);
static void gtk_cell_renderer_real_get_preferred_height_for_width(GtkCellRenderer         *cell,
                                                                  GtkWidget               *widget,
                                                                  int                      width,
                                                                  int                     *minimum_height,
                                                                  int                     *natural_height);
static void gtk_cell_renderer_real_get_preferred_width_for_height(GtkCellRenderer         *cell,
                                                                  GtkWidget               *widget,
                                                                  int                      height,
                                                                  int                     *minimum_width,
                                                                  int                     *natural_width);
static void gtk_cell_renderer_real_get_aligned_area              (GtkCellRenderer         *cell,
								  GtkWidget               *widget,
								  GtkCellRendererState     flags,
								  const GdkRectangle      *cell_area,
								  GdkRectangle            *aligned_area);


struct _GtkCellRendererPrivate
{
  float xalign;
  float yalign;

  int width;
  int height;

  guint16 xpad;
  guint16 ypad;

  guint mode                : 2;
  guint visible             : 1;
  guint is_expander         : 1;
  guint is_expanded         : 1;
  guint cell_background_set : 1;
  guint sensitive           : 1;
  guint editing             : 1;

  GdkRGBA cell_background;
};

enum {
  PROP_0,
  PROP_MODE,
  PROP_VISIBLE,
  PROP_SENSITIVE,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_XPAD,
  PROP_YPAD,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_IS_EXPANDER,
  PROP_IS_EXPANDED,
  PROP_CELL_BACKGROUND,
  PROP_CELL_BACKGROUND_RGBA,
  PROP_CELL_BACKGROUND_SET,
  PROP_EDITING
};

/* Signal IDs */
enum {
  EDITING_CANCELED,
  EDITING_STARTED,
  LAST_SIGNAL
};

static int GtkCellRenderer_private_offset;
static guint  cell_renderer_signals[LAST_SIGNAL] = { 0 };

static inline gpointer
gtk_cell_renderer_get_instance_private (GtkCellRenderer *self)
{
  return (G_STRUCT_MEMBER_P (self, GtkCellRenderer_private_offset));
}

static void
gtk_cell_renderer_init (GtkCellRenderer *cell)
{
  GtkCellRendererPrivate *priv;

  cell->priv = gtk_cell_renderer_get_instance_private (cell);
  priv = cell->priv;

  priv->mode = GTK_CELL_RENDERER_MODE_INERT;
  priv->visible = TRUE;
  priv->width = -1;
  priv->height = -1;
  priv->xalign = 0.5;
  priv->yalign = 0.5;
  priv->xpad = 0;
  priv->ypad = 0;
  priv->sensitive = TRUE;
  priv->is_expander = FALSE;
  priv->is_expanded = FALSE;
  priv->editing = FALSE;
}

static void
gtk_cell_renderer_class_init (GtkCellRendererClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_cell_renderer_get_property;
  object_class->set_property = gtk_cell_renderer_set_property;

  class->snapshot = NULL;
  class->get_request_mode               = gtk_cell_renderer_real_get_request_mode;
  class->get_preferred_width            = gtk_cell_renderer_real_get_preferred_width;
  class->get_preferred_height           = gtk_cell_renderer_real_get_preferred_height;
  class->get_preferred_width_for_height = gtk_cell_renderer_real_get_preferred_width_for_height;
  class->get_preferred_height_for_width = gtk_cell_renderer_real_get_preferred_height_for_width;
  class->get_aligned_area               = gtk_cell_renderer_real_get_aligned_area;

  /**
   * GtkCellRenderer::editing-canceled:
   * @renderer: the object which received the signal
   *
   * This signal gets emitted when the user cancels the process of editing a
   * cell.  For example, an editable cell renderer could be written to cancel
   * editing when the user presses Escape.
   *
   * See also: gtk_cell_renderer_stop_editing().
   */
  cell_renderer_signals[EDITING_CANCELED] =
    g_signal_new (I_("editing-canceled"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCellRendererClass, editing_canceled),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkCellRenderer::editing-started:
   * @renderer: the object which received the signal
   * @editable: the `GtkCellEditable`
   * @path: the path identifying the edited cell
   *
   * This signal gets emitted when a cell starts to be edited.
   * The intended use of this signal is to do special setup
   * on @editable, e.g. adding a `GtkEntryCompletion` or setting
   * up additional columns in a `GtkComboBox`.
   *
   * See gtk_cell_editable_start_editing() for information on the lifecycle of
   * the @editable and a way to do setup that doesn’t depend on the @renderer.
   *
   * Note that GTK doesn't guarantee that cell renderers will
   * continue to use the same kind of widget for editing in future
   * releases, therefore you should check the type of @editable
   * before doing any specific setup, as in the following example:
   * |[<!-- language="C" -->
   * static void
   * text_editing_started (GtkCellRenderer *cell,
   *                       GtkCellEditable *editable,
   *                       const char      *path,
   *                       gpointer         data)
   * {
   *   if (GTK_IS_ENTRY (editable))
   *     {
   *       GtkEntry *entry = GTK_ENTRY (editable);
   *
   *       // ... create a GtkEntryCompletion
   *
   *       gtk_entry_set_completion (entry, completion);
   *     }
   * }
   * ]|
   */
  cell_renderer_signals[EDITING_STARTED] =
    g_signal_new (I_("editing-started"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCellRendererClass, editing_started),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_STRING,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_CELL_EDITABLE,
		  G_TYPE_STRING);
  g_signal_set_va_marshaller (cell_renderer_signals[EDITING_STARTED],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_VOID__OBJECT_STRINGv);

  g_object_class_install_property (object_class,
				   PROP_MODE,
				   g_param_spec_enum ("mode", NULL, NULL,
						      GTK_TYPE_CELL_RENDERER_MODE,
						      GTK_CELL_RENDERER_MODE_INERT,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible", NULL, NULL,
							 TRUE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (object_class,
				   PROP_SENSITIVE,
				   g_param_spec_boolean ("sensitive", NULL, NULL,
							 TRUE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_XALIGN,
				   g_param_spec_float ("xalign", NULL, NULL,
						       0.0,
						       1.0,
						       0.5,
						       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_YALIGN,
				   g_param_spec_float ("yalign", NULL, NULL,
						       0.0,
						       1.0,
						       0.5,
						       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_XPAD,
				   g_param_spec_uint ("xpad", NULL, NULL,
						      0,
						      G_MAXUINT,
						      0,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_YPAD,
				   g_param_spec_uint ("ypad", NULL, NULL,
						      0,
						      G_MAXUINT,
						      0,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_WIDTH,
				   g_param_spec_int ("width", NULL, NULL,
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_HEIGHT,
				   g_param_spec_int ("height", NULL, NULL,
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_IS_EXPANDER,
				   g_param_spec_boolean ("is-expander", NULL, NULL,
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  g_object_class_install_property (object_class,
				   PROP_IS_EXPANDED,
				   g_param_spec_boolean ("is-expanded", NULL, NULL,
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
				   PROP_CELL_BACKGROUND,
				   g_param_spec_string ("cell-background", NULL, NULL,
							NULL,
							GTK_PARAM_WRITABLE));

  /**
   * GtkCellRenderer:cell-background-rgba:
   *
   * Cell background as a `GdkRGBA`
   */
  g_object_class_install_property (object_class,
				   PROP_CELL_BACKGROUND_RGBA,
				   g_param_spec_boxed ("cell-background-rgba", NULL, NULL,
						       GDK_TYPE_RGBA,
						       GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_EDITING,
				   g_param_spec_boolean ("editing", NULL, NULL,
							 FALSE,
							 GTK_PARAM_READABLE));


#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (object_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY))

  ADD_SET_PROP ("cell-background-set", PROP_CELL_BACKGROUND_SET, NULL, NULL);

  if (GtkCellRenderer_private_offset != 0)
    g_type_class_adjust_private_offset (class, &GtkCellRenderer_private_offset);
}

GType
gtk_cell_renderer_get_type (void)
{
  static GType cell_renderer_type = 0;

  if (G_UNLIKELY (cell_renderer_type == 0))
    {
      const GTypeInfo cell_renderer_info =
      {
	sizeof (GtkCellRendererClass),
        NULL,
        NULL,
	(GClassInitFunc) gtk_cell_renderer_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_init */
	sizeof (GtkWidget),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_cell_renderer_init,
	NULL,		/* value_table */
      };
      cell_renderer_type = g_type_register_static (G_TYPE_INITIALLY_UNOWNED, "GtkCellRenderer",
                                                   &cell_renderer_info, G_TYPE_FLAG_ABSTRACT);

      GtkCellRenderer_private_offset =
        g_type_add_instance_private (cell_renderer_type, sizeof (GtkCellRendererPrivate));
    }

  return cell_renderer_type;
}

static void
gtk_cell_renderer_get_property (GObject     *object,
				guint        param_id,
				GValue      *value,
				GParamSpec  *pspec)
{
  GtkCellRenderer *cell = GTK_CELL_RENDERER (object);
  GtkCellRendererPrivate *priv = cell->priv;

  switch (param_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, priv->mode);
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, priv->visible);
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, priv->sensitive);
      break;
    case PROP_EDITING:
      g_value_set_boolean (value, priv->editing);
      break;
    case PROP_XALIGN:
      g_value_set_float (value, priv->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float (value, priv->yalign);
      break;
    case PROP_XPAD:
      g_value_set_uint (value, priv->xpad);
      break;
    case PROP_YPAD:
      g_value_set_uint (value, priv->ypad);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, priv->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, priv->height);
      break;
    case PROP_IS_EXPANDER:
      g_value_set_boolean (value, priv->is_expander);
      break;
    case PROP_IS_EXPANDED:
      g_value_set_boolean (value, priv->is_expanded);
      break;
    case PROP_CELL_BACKGROUND_RGBA:
      g_value_set_boxed (value, &priv->cell_background);
      break;
    case PROP_CELL_BACKGROUND_SET:
      g_value_set_boolean (value, priv->cell_background_set);
      break;
    case PROP_CELL_BACKGROUND:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }

}

static void
gtk_cell_renderer_set_property (GObject      *object,
				guint         param_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  GtkCellRenderer *cell = GTK_CELL_RENDERER (object);
  GtkCellRendererPrivate *priv = cell->priv;

  switch (param_id)
    {
    case PROP_MODE:
      if (priv->mode != g_value_get_enum (value))
        {
          priv->mode = g_value_get_enum (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_VISIBLE:
      if (priv->visible != g_value_get_boolean (value))
        {
          priv->visible = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_SENSITIVE:
      if (priv->sensitive != g_value_get_boolean (value))
        {
          priv->sensitive = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_XALIGN:
      if (priv->xalign != g_value_get_float (value))
        {
          priv->xalign = g_value_get_float (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_YALIGN:
      if (priv->yalign != g_value_get_float (value))
        {
          priv->yalign = g_value_get_float (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_XPAD:
      if (priv->xpad != g_value_get_uint (value))
        {
          priv->xpad = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_YPAD:
      if (priv->ypad != g_value_get_uint (value))
        {
          priv->ypad = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_WIDTH:
      if (priv->width != g_value_get_int (value))
        {
          priv->width = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_HEIGHT:
      if (priv->height != g_value_get_int (value))
        {
          priv->height = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_IS_EXPANDER:
      if (priv->is_expander != g_value_get_boolean (value))
        {
          priv->is_expander = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_IS_EXPANDED:
      if (priv->is_expanded != g_value_get_boolean (value))
        {
          priv->is_expanded = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_CELL_BACKGROUND:
      {
        GdkRGBA rgba;

        if (!g_value_get_string (value))
          set_cell_bg_color (cell, NULL);
        else if (gdk_rgba_parse (&rgba, g_value_get_string (value)))
          set_cell_bg_color (cell, &rgba);
        else
          g_warning ("Don't know color '%s'", g_value_get_string (value));

        g_object_notify (object, "cell-background");
      }
      break;
    case PROP_CELL_BACKGROUND_RGBA:
      set_cell_bg_color (cell, g_value_get_boxed (value));
      break;
    case PROP_CELL_BACKGROUND_SET:
      if (priv->cell_background_set != g_value_get_boolean (value))
        {
          priv->cell_background_set = g_value_get_boolean (value);
          g_object_notify (object, "cell-background-set");
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
set_cell_bg_color (GtkCellRenderer *cell,
                   GdkRGBA         *rgba)
{
  GtkCellRendererPrivate *priv = cell->priv;

  if (rgba)
    {
      if (!priv->cell_background_set)
        {
          priv->cell_background_set = TRUE;
          g_object_notify (G_OBJECT (cell), "cell-background-set");
        }

      priv->cell_background = *rgba;
    }
  else
    {
      if (priv->cell_background_set)
        {
	  priv->cell_background_set = FALSE;
	  g_object_notify (G_OBJECT (cell), "cell-background-set");
	}
    }
  g_object_notify (G_OBJECT (cell), "cell-background-rgba");
}

/**
 * gtk_cell_renderer_snapshot:
 * @cell: a `GtkCellRenderer`
 * @snapshot: a `GtkSnapshot` to draw to
 * @widget: the widget owning @window
 * @background_area: entire cell area (including tree expanders and maybe
 *    padding on the sides)
 * @cell_area: area normally rendered by a cell renderer
 * @flags: flags that affect rendering
 *
 * Invokes the virtual render function of the `GtkCellRenderer`. The three
 * passed-in rectangles are areas in @cr. Most renderers will draw within
 * @cell_area; the xalign, yalign, xpad, and ypad fields of the `GtkCellRenderer`
 * should be honored with respect to @cell_area. @background_area includes the
 * blank space around the cell, and also the area containing the tree expander;
 * so the @background_area rectangles for all cells tile to cover the entire
 * @window.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_renderer_snapshot (GtkCellRenderer      *cell,
                            GtkSnapshot          *snapshot,
                            GtkWidget            *widget,
                            const GdkRectangle   *background_area,
                            const GdkRectangle   *cell_area,
                            GtkCellRendererState  flags)
{
  gboolean selected = FALSE;
  GtkCellRendererPrivate *priv = cell->priv;
  GtkStyleContext *context;
  GtkStateFlags state;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_CELL_RENDERER_GET_CLASS (cell)->snapshot != NULL);
  g_return_if_fail (snapshot != NULL);

  selected = (flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED;

  gtk_snapshot_push_debug (snapshot, "%s", G_OBJECT_TYPE_NAME (cell));

  if (priv->cell_background_set && !selected)
    {
      gtk_snapshot_append_color (snapshot,
                                 &priv->cell_background,
                                 &GRAPHENE_RECT_INIT (
                                     background_area->x, background_area->y,
                                     background_area->width, background_area->height
                                 ));
    }

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT (
                              background_area->x, background_area->y,
                              background_area->width, background_area->height
                          ));

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "cell");

  state = gtk_cell_renderer_get_state (cell, widget, flags);
  gtk_style_context_set_state (context, state);

  GTK_CELL_RENDERER_GET_CLASS (cell)->snapshot (cell,
                                                snapshot,
                                                widget,
                                                background_area,
                                                cell_area,
                                                flags);
  gtk_style_context_restore (context);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_pop (snapshot);
}

/**
 * gtk_cell_renderer_activate:
 * @cell: a `GtkCellRenderer`
 * @event: a `GdkEvent`
 * @widget: widget that received the event
 * @path: widget-dependent string representation of the event location;
 *    e.g. for `GtkTreeView`, a string representation of `GtkTreePath`
 * @background_area: background area as passed to gtk_cell_renderer_render()
 * @cell_area: cell area as passed to gtk_cell_renderer_render()
 * @flags: render flags
 *
 * Passes an activate event to the cell renderer for possible processing.
 * Some cell renderers may use events; for example, `GtkCellRendererToggle`
 * toggles when it gets a mouse click.
 *
 * Returns: %TRUE if the event was consumed/handled
 *
 * Deprecated: 4.10
 **/
gboolean
gtk_cell_renderer_activate (GtkCellRenderer      *cell,
			    GdkEvent             *event,
			    GtkWidget            *widget,
			    const char           *path,
			    const GdkRectangle   *background_area,
			    const GdkRectangle   *cell_area,
			    GtkCellRendererState  flags)
{
  GtkCellRendererPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);

  priv = cell->priv;

  if (priv->mode != GTK_CELL_RENDERER_MODE_ACTIVATABLE)
    return FALSE;

  if (GTK_CELL_RENDERER_GET_CLASS (cell)->activate == NULL)
    return FALSE;

  return GTK_CELL_RENDERER_GET_CLASS (cell)->activate (cell,
						       event,
						       widget,
						       path,
						       (GdkRectangle *) background_area,
						       (GdkRectangle *) cell_area,
						       flags);
}

/**
 * gtk_cell_renderer_start_editing:
 * @cell: a `GtkCellRenderer`
 * @event: (nullable): a `GdkEvent`
 * @widget: widget that received the event
 * @path: widget-dependent string representation of the event location;
 *    e.g. for `GtkTreeView`, a string representation of `GtkTreePath`
 * @background_area: background area as passed to gtk_cell_renderer_render()
 * @cell_area: cell area as passed to gtk_cell_renderer_render()
 * @flags: render flags
 *
 * Starts editing the contents of this @cell, through a new `GtkCellEditable`
 * widget created by the `GtkCellRenderer`Class.start_editing virtual function.
 *
 * Returns: (nullable) (transfer none): A new `GtkCellEditable` for editing this
 *   @cell, or %NULL if editing is not possible
 *
 * Deprecated: 4.10
 **/
GtkCellEditable *
gtk_cell_renderer_start_editing (GtkCellRenderer      *cell,
				 GdkEvent             *event,
				 GtkWidget            *widget,
				 const char           *path,
				 const GdkRectangle   *background_area,
				 const GdkRectangle   *cell_area,
				 GtkCellRendererState  flags)

{
  GtkCellRendererPrivate *priv;
  GtkCellEditable *editable;

  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), NULL);

  priv = cell->priv;

  if (priv->mode != GTK_CELL_RENDERER_MODE_EDITABLE)
    return NULL;

  if (GTK_CELL_RENDERER_GET_CLASS (cell)->start_editing == NULL)
    return NULL;

  editable = GTK_CELL_RENDERER_GET_CLASS (cell)->start_editing (cell,
								event,
								widget,
								path,
								(GdkRectangle *) background_area,
								(GdkRectangle *) cell_area,
								flags);
  if (editable == NULL)
    return NULL;

  gtk_widget_add_css_class (GTK_WIDGET (editable), "cell");

  g_signal_emit (cell,
		 cell_renderer_signals[EDITING_STARTED], 0,
		 editable, path);

  priv->editing = TRUE;

  return editable;
}

/**
 * gtk_cell_renderer_set_fixed_size:
 * @cell: A `GtkCellRenderer`
 * @width: the width of the cell renderer, or -1
 * @height: the height of the cell renderer, or -1
 *
 * Sets the renderer size to be explicit, independent of the properties set.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_renderer_set_fixed_size (GtkCellRenderer *cell,
				  int              width,
				  int              height)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (width >= -1 && height >= -1);

  priv = cell->priv;

  if ((width != priv->width) || (height != priv->height))
    {
      g_object_freeze_notify (G_OBJECT (cell));

      if (width != priv->width)
        {
          priv->width = width;
          g_object_notify (G_OBJECT (cell), "width");
        }

      if (height != priv->height)
        {
          priv->height = height;
          g_object_notify (G_OBJECT (cell), "height");
        }

      g_object_thaw_notify (G_OBJECT (cell));
    }
}

/**
 * gtk_cell_renderer_get_fixed_size:
 * @cell: A `GtkCellRenderer`
 * @width: (out) (optional): location to fill in with the fixed width of the cell
 * @height: (out) (optional): location to fill in with the fixed height of the cell
 *
 * Fills in @width and @height with the appropriate size of @cell.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_get_fixed_size (GtkCellRenderer *cell,
				  int             *width,
				  int             *height)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  priv = cell->priv;

  if (width)
    *width = priv->width;
  if (height)
    *height = priv->height;
}

/**
 * gtk_cell_renderer_set_alignment:
 * @cell: A `GtkCellRenderer`
 * @xalign: the x alignment of the cell renderer
 * @yalign: the y alignment of the cell renderer
 *
 * Sets the renderer’s alignment within its available space.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_renderer_set_alignment (GtkCellRenderer *cell,
                                 float            xalign,
                                 float            yalign)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (xalign >= 0.0 && xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0 && yalign <= 1.0);

  priv = cell->priv;

  if ((xalign != priv->xalign) || (yalign != priv->yalign))
    {
      g_object_freeze_notify (G_OBJECT (cell));

      if (xalign != priv->xalign)
        {
          priv->xalign = xalign;
          g_object_notify (G_OBJECT (cell), "xalign");
        }

      if (yalign != priv->yalign)
        {
          priv->yalign = yalign;
          g_object_notify (G_OBJECT (cell), "yalign");
        }

      g_object_thaw_notify (G_OBJECT (cell));
    }
}

/**
 * gtk_cell_renderer_get_alignment:
 * @cell: A `GtkCellRenderer`
 * @xalign: (out) (optional): location to fill in with the x alignment of the cell
 * @yalign: (out) (optional): location to fill in with the y alignment of the cell
 *
 * Fills in @xalign and @yalign with the appropriate values of @cell.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_get_alignment (GtkCellRenderer *cell,
                                 float           *xalign,
                                 float           *yalign)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  priv = cell->priv;

  if (xalign)
    *xalign = priv->xalign;
  if (yalign)
    *yalign = priv->yalign;
}

/**
 * gtk_cell_renderer_set_padding:
 * @cell: A `GtkCellRenderer`
 * @xpad: the x padding of the cell renderer
 * @ypad: the y padding of the cell renderer
 *
 * Sets the renderer’s padding.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_renderer_set_padding (GtkCellRenderer *cell,
                               int              xpad,
                               int              ypad)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (xpad >= 0 && ypad >= 0);

  priv = cell->priv;

  if ((xpad != priv->xpad) || (ypad != priv->ypad))
    {
      g_object_freeze_notify (G_OBJECT (cell));

      if (xpad != priv->xpad)
        {
          priv->xpad = xpad;
          g_object_notify (G_OBJECT (cell), "xpad");
        }

      if (ypad != priv->ypad)
        {
          priv->ypad = ypad;
          g_object_notify (G_OBJECT (cell), "ypad");
        }

      g_object_thaw_notify (G_OBJECT (cell));
    }
}

/**
 * gtk_cell_renderer_get_padding:
 * @cell: A `GtkCellRenderer`
 * @xpad: (out) (optional): location to fill in with the x padding of the cell
 * @ypad: (out) (optional): location to fill in with the y padding of the cell
 *
 * Fills in @xpad and @ypad with the appropriate values of @cell.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_get_padding (GtkCellRenderer *cell,
                               int             *xpad,
                               int             *ypad)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  priv = cell->priv;

  if (xpad)
    *xpad = priv->xpad;
  if (ypad)
    *ypad = priv->ypad;
}

/**
 * gtk_cell_renderer_set_visible:
 * @cell: A `GtkCellRenderer`
 * @visible: the visibility of the cell
 *
 * Sets the cell renderer’s visibility.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_renderer_set_visible (GtkCellRenderer *cell,
                               gboolean         visible)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  priv = cell->priv;

  if (priv->visible != visible)
    {
      priv->visible = visible ? TRUE : FALSE;
      g_object_notify (G_OBJECT (cell), "visible");
    }
}

/**
 * gtk_cell_renderer_get_visible:
 * @cell: A `GtkCellRenderer`
 *
 * Returns the cell renderer’s visibility.
 *
 * Returns: %TRUE if the cell renderer is visible
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_renderer_get_visible (GtkCellRenderer *cell)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);

  return cell->priv->visible;
}

/**
 * gtk_cell_renderer_set_sensitive:
 * @cell: A `GtkCellRenderer`
 * @sensitive: the sensitivity of the cell
 *
 * Sets the cell renderer’s sensitivity.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_renderer_set_sensitive (GtkCellRenderer *cell,
                                 gboolean         sensitive)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  priv = cell->priv;

  if (priv->sensitive != sensitive)
    {
      priv->sensitive = sensitive ? TRUE : FALSE;
      g_object_notify (G_OBJECT (cell), "sensitive");
    }
}

/**
 * gtk_cell_renderer_get_sensitive:
 * @cell: A `GtkCellRenderer`
 *
 * Returns the cell renderer’s sensitivity.
 *
 * Returns: %TRUE if the cell renderer is sensitive
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_renderer_get_sensitive (GtkCellRenderer *cell)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);

  return cell->priv->sensitive;
}


/**
 * gtk_cell_renderer_is_activatable:
 * @cell: A `GtkCellRenderer`
 *
 * Checks whether the cell renderer can do something when activated.
 *
 * Returns: %TRUE if the cell renderer can do anything when activated
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_renderer_is_activatable (GtkCellRenderer *cell)
{
  GtkCellRendererPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);

  priv = cell->priv;

  return (priv->visible &&
          (priv->mode == GTK_CELL_RENDERER_MODE_EDITABLE ||
           priv->mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE));
}


/**
 * gtk_cell_renderer_stop_editing:
 * @cell: A `GtkCellRenderer`
 * @canceled: %TRUE if the editing has been canceled
 *
 * Informs the cell renderer that the editing is stopped.
 * If @canceled is %TRUE, the cell renderer will emit the
 * `GtkCellRenderer`::editing-canceled signal.
 *
 * This function should be called by cell renderer implementations
 * in response to the `GtkCellEditable::editing-done` signal of
 * `GtkCellEditable`.
 *
 * Deprecated: 4.10
 **/
void
gtk_cell_renderer_stop_editing (GtkCellRenderer *cell,
				gboolean         canceled)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  priv = cell->priv;

  if (priv->editing)
    {
      priv->editing = FALSE;
      if (canceled)
	g_signal_emit (cell, cell_renderer_signals[EDITING_CANCELED], 0);
    }
}

static void
gtk_cell_renderer_real_get_preferred_size (GtkCellRenderer   *cell,
                                           GtkWidget         *widget,
                                           GtkOrientation     orientation,
                                           int               *minimum_size,
                                           int               *natural_size)
{
  GtkRequisition min_req;

  min_req.width = 0;
  min_req.height = 0;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (minimum_size)
	*minimum_size = min_req.width;

      if (natural_size)
	*natural_size = min_req.width;
    }
  else
    {
      if (minimum_size)
	*minimum_size = min_req.height;

      if (natural_size)
	*natural_size = min_req.height;
    }
}

static GtkSizeRequestMode
gtk_cell_renderer_real_get_request_mode (GtkCellRenderer *cell)
{
  /* By default cell renderers are height-for-width. */
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_cell_renderer_real_get_preferred_width (GtkCellRenderer *cell,
                                            GtkWidget       *widget,
                                            int             *minimum_size,
                                            int             *natural_size)
{
  gtk_cell_renderer_real_get_preferred_size (cell, widget, GTK_ORIENTATION_HORIZONTAL,
                                             minimum_size, natural_size);
}

static void
gtk_cell_renderer_real_get_preferred_height (GtkCellRenderer *cell,
                                             GtkWidget       *widget,
                                             int             *minimum_size,
                                             int             *natural_size)
{
  gtk_cell_renderer_real_get_preferred_size (cell, widget, GTK_ORIENTATION_VERTICAL,
                                             minimum_size, natural_size);
}


static void
gtk_cell_renderer_real_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                       GtkWidget       *widget,
                                                       int              width,
                                                       int             *minimum_height,
                                                       int             *natural_height)
{
  gtk_cell_renderer_get_preferred_height (cell, widget, minimum_height, natural_height);
}

static void
gtk_cell_renderer_real_get_preferred_width_for_height (GtkCellRenderer *cell,
                                                       GtkWidget       *widget,
                                                       int              height,
                                                       int             *minimum_width,
                                                       int             *natural_width)
{
  gtk_cell_renderer_get_preferred_width (cell, widget, minimum_width, natural_width);
}


/* Default implementation assumes that a cell renderer will never use more
 * space than its natural size (this is fine for toggles and pixbufs etc
 * but needs to be overridden from wrapping/ellipsizing text renderers) */
static void
gtk_cell_renderer_real_get_aligned_area (GtkCellRenderer         *cell,
					 GtkWidget               *widget,
					 GtkCellRendererState     flags,
					 const GdkRectangle      *cell_area,
					 GdkRectangle            *aligned_area)
{
  int opposite_size, x_offset, y_offset;
  int natural_size;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cell_area != NULL);
  g_return_if_fail (aligned_area != NULL);

  *aligned_area = *cell_area;

  /* Trim up the aligned size */
  if (gtk_cell_renderer_get_request_mode (cell) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      gtk_cell_renderer_get_preferred_width (cell, widget,
					     NULL, &natural_size);

      aligned_area->width = MIN (aligned_area->width, natural_size);

      gtk_cell_renderer_get_preferred_height_for_width (cell, widget,
							aligned_area->width,
							NULL, &opposite_size);

      aligned_area->height = MIN (opposite_size, aligned_area->height);
    }
  else
    {
      gtk_cell_renderer_get_preferred_height (cell, widget,
					      NULL, &natural_size);

      aligned_area->height = MIN (aligned_area->width, natural_size);

      gtk_cell_renderer_get_preferred_width_for_height (cell, widget,
							aligned_area->height,
							NULL, &opposite_size);

      aligned_area->width = MIN (opposite_size, aligned_area->width);
    }

  /* offset the cell position */
  _gtk_cell_renderer_calc_offset (cell, cell_area,
				  gtk_widget_get_direction (widget),
				  aligned_area->width,
				  aligned_area->height,
				  &x_offset, &y_offset);

  aligned_area->x += x_offset;
  aligned_area->y += y_offset;
}


/* An internal convenience function for some containers to peek at the
 * cell alignment in a target allocation (used to draw focus and align
 * cells in the icon view).
 *
 * Note this is only a trivial “align * (allocation - request)” operation.
 */
void
_gtk_cell_renderer_calc_offset    (GtkCellRenderer      *cell,
				   const GdkRectangle   *cell_area,
				   GtkTextDirection      direction,
				   int                   width,
				   int                   height,
				   int                  *x_offset,
				   int                  *y_offset)
{
  GtkCellRendererPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (cell_area != NULL);
  g_return_if_fail (x_offset || y_offset);

  priv = cell->priv;

  if (x_offset)
    {
      *x_offset = (((direction == GTK_TEXT_DIR_RTL) ?
		    (1.0 - priv->xalign) : priv->xalign) *
		   (cell_area->width - width));
      *x_offset = MAX (*x_offset, 0);
    }
  if (y_offset)
    {
      *y_offset = (priv->yalign *
		   (cell_area->height - height));
      *y_offset = MAX (*y_offset, 0);
    }
}

/**
 * gtk_cell_renderer_get_request_mode:
 * @cell: a `GtkCellRenderer` instance
 *
 * Gets whether the cell renderer prefers a height-for-width layout
 * or a width-for-height layout.
 *
 * Returns: The `GtkSizeRequestMode` preferred by this renderer.
 *
 * Deprecated: 4.10
 */
GtkSizeRequestMode
gtk_cell_renderer_get_request_mode (GtkCellRenderer *cell)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);

  return GTK_CELL_RENDERER_GET_CLASS (cell)->get_request_mode (cell);
}

/**
 * gtk_cell_renderer_get_preferred_width:
 * @cell: a `GtkCellRenderer` instance
 * @widget: the `GtkWidget` this cell will be rendering to
 * @minimum_size: (out) (optional): location to store the minimum size
 * @natural_size: (out) (optional): location to store the natural size
 *
 * Retrieves a renderer’s natural size when rendered to @widget.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_get_preferred_width (GtkCellRenderer *cell,
                                       GtkWidget       *widget,
                                       int             *minimum_size,
                                       int             *natural_size)
{
  GtkCellRendererClass *klass;
  int width;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_size || NULL != natural_size);

  gtk_cell_renderer_get_fixed_size (GTK_CELL_RENDERER (cell), &width, NULL);

  if (width < 0)
    {
      klass = GTK_CELL_RENDERER_GET_CLASS (cell);
      klass->get_preferred_width (cell, widget, minimum_size, natural_size);
    }
  else
    {
      if (minimum_size)
	*minimum_size = width;
      if (natural_size)
	*natural_size = width;
    }

#if DEBUG_CELL_SIZE_REQUEST
  g_message ("%s returning minimum width: %d and natural width: %d",
	     G_OBJECT_TYPE_NAME (cell),
	     minimum_size ? *minimum_size : 20000,
	     natural_size ? *natural_size : 20000);
#endif
}


/**
 * gtk_cell_renderer_get_preferred_height:
 * @cell: a `GtkCellRenderer` instance
 * @widget: the `GtkWidget` this cell will be rendering to
 * @minimum_size: (out) (optional): location to store the minimum size
 * @natural_size: (out) (optional): location to store the natural size
 *
 * Retrieves a renderer’s natural size when rendered to @widget.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_get_preferred_height (GtkCellRenderer *cell,
                                        GtkWidget       *widget,
                                        int             *minimum_size,
                                        int             *natural_size)
{
  GtkCellRendererClass *klass;
  int height;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_size || NULL != natural_size);

  gtk_cell_renderer_get_fixed_size (GTK_CELL_RENDERER (cell), NULL, &height);

  if (height < 0)
    {
      klass = GTK_CELL_RENDERER_GET_CLASS (cell);
      klass->get_preferred_height (cell, widget, minimum_size, natural_size);
    }
  else
    {
      if (minimum_size)
	*minimum_size = height;
      if (natural_size)
	*natural_size = height;
    }

#if DEBUG_CELL_SIZE_REQUEST
  g_message ("%s returning minimum height: %d and natural height: %d",
	     G_OBJECT_TYPE_NAME (cell),
	     minimum_size ? *minimum_size : 20000,
	     natural_size ? *natural_size : 20000);
#endif
}


/**
 * gtk_cell_renderer_get_preferred_width_for_height:
 * @cell: a `GtkCellRenderer` instance
 * @widget: the `GtkWidget` this cell will be rendering to
 * @height: the size which is available for allocation
 * @minimum_width: (out) (optional): location for storing the minimum size
 * @natural_width: (out) (optional): location for storing the preferred size
 *
 * Retrieves a cell renderers’s minimum and natural width if it were rendered to
 * @widget with the specified @height.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_get_preferred_width_for_height (GtkCellRenderer *cell,
                                                  GtkWidget       *widget,
                                                  int              height,
                                                  int             *minimum_width,
                                                  int             *natural_width)
{
  GtkCellRendererClass *klass;
  int width;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_width || NULL != natural_width);

  gtk_cell_renderer_get_fixed_size (GTK_CELL_RENDERER (cell), &width, NULL);

  if (width < 0)
    {
      klass = GTK_CELL_RENDERER_GET_CLASS (cell);
      klass->get_preferred_width_for_height (cell, widget, height, minimum_width, natural_width);
    }
  else
    {
      if (minimum_width)
	*minimum_width = width;
      if (natural_width)
	*natural_width = width;
    }

#if DEBUG_CELL_SIZE_REQUEST
  g_message ("%s width for height: %d is minimum %d and natural: %d",
	     G_OBJECT_TYPE_NAME (cell), height,
	     minimum_width ? *minimum_width : 20000,
	     natural_width ? *natural_width : 20000);
#endif
}

/**
 * gtk_cell_renderer_get_preferred_height_for_width:
 * @cell: a `GtkCellRenderer` instance
 * @widget: the `GtkWidget` this cell will be rendering to
 * @width: the size which is available for allocation
 * @minimum_height: (out) (optional): location for storing the minimum size
 * @natural_height: (out) (optional): location for storing the preferred size
 *
 * Retrieves a cell renderers’s minimum and natural height if it were rendered to
 * @widget with the specified @width.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                  GtkWidget       *widget,
                                                  int              width,
                                                  int             *minimum_height,
                                                  int             *natural_height)
{
  GtkCellRendererClass *klass;
  int height;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_height || NULL != natural_height);

  gtk_cell_renderer_get_fixed_size (GTK_CELL_RENDERER (cell), NULL, &height);

  if (height < 0)
    {
      klass = GTK_CELL_RENDERER_GET_CLASS (cell);
      klass->get_preferred_height_for_width (cell, widget, width, minimum_height, natural_height);
    }
  else
    {
      if (minimum_height)
	*minimum_height = height;
      if (natural_height)
	*natural_height = height;
    }

#if DEBUG_CELL_SIZE_REQUEST
  g_message ("%s height for width: %d is minimum %d and natural: %d",
	     G_OBJECT_TYPE_NAME (cell), width,
	     minimum_height ? *minimum_height : 20000,
	     natural_height ? *natural_height : 20000);
#endif
}

/**
 * gtk_cell_renderer_get_preferred_size:
 * @cell: a `GtkCellRenderer` instance
 * @widget: the `GtkWidget` this cell will be rendering to
 * @minimum_size: (out) (optional): location for storing the minimum size
 * @natural_size: (out) (optional): location for storing the natural size
 *
 * Retrieves the minimum and natural size of a cell taking
 * into account the widget’s preference for height-for-width management.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_get_preferred_size (GtkCellRenderer *cell,
                                      GtkWidget       *widget,
                                      GtkRequisition  *minimum_size,
                                      GtkRequisition  *natural_size)
{
  int min_width, nat_width;
  int min_height, nat_height;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  if (gtk_cell_renderer_get_request_mode (cell) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      gtk_cell_renderer_get_preferred_width (cell, widget, &min_width, &nat_width);

      if (minimum_size)
	{
	  minimum_size->width = min_width;
	  gtk_cell_renderer_get_preferred_height_for_width (cell, widget, min_width,
                                                            &minimum_size->height, NULL);
	}

      if (natural_size)
	{
	  natural_size->width = nat_width;
	  gtk_cell_renderer_get_preferred_height_for_width (cell, widget, nat_width,
                                                            NULL, &natural_size->height);
	}
    }
  else /* GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT */
    {
      gtk_cell_renderer_get_preferred_height (cell, widget, &min_height, &nat_height);

      if (minimum_size)
	{
	  minimum_size->height = min_height;
	  gtk_cell_renderer_get_preferred_width_for_height (cell, widget, min_height,
                                                            &minimum_size->width, NULL);
	}

      if (natural_size)
	{
	  natural_size->height = nat_height;
	  gtk_cell_renderer_get_preferred_width_for_height (cell, widget, nat_height,
                                                            NULL, &natural_size->width);
	}
    }
}

/**
 * gtk_cell_renderer_get_aligned_area:
 * @cell: a `GtkCellRenderer` instance
 * @widget: the `GtkWidget` this cell will be rendering to
 * @flags: render flags
 * @cell_area: cell area which would be passed to gtk_cell_renderer_render()
 * @aligned_area: (out): the return location for the space inside @cell_area
 *                that would actually be used to render.
 *
 * Gets the aligned area used by @cell inside @cell_area. Used for finding
 * the appropriate edit and focus rectangle.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_get_aligned_area (GtkCellRenderer      *cell,
				    GtkWidget            *widget,
				    GtkCellRendererState  flags,
				    const GdkRectangle   *cell_area,
				    GdkRectangle         *aligned_area)
{
  GtkCellRendererClass *klass;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cell_area != NULL);
  g_return_if_fail (aligned_area != NULL);

  klass = GTK_CELL_RENDERER_GET_CLASS (cell);
  klass->get_aligned_area (cell, widget, flags, cell_area, aligned_area);

  g_assert (aligned_area->x >= cell_area->x && aligned_area->x <= cell_area->x + cell_area->width);
  g_assert (aligned_area->y >= cell_area->y && aligned_area->y <= cell_area->y + cell_area->height);
  g_assert ((aligned_area->x - cell_area->x) + aligned_area->width <= cell_area->width);
  g_assert ((aligned_area->y - cell_area->y) + aligned_area->height <= cell_area->height);
}

/**
 * gtk_cell_renderer_get_state:
 * @cell: (nullable): a `GtkCellRenderer`
 * @widget: (nullable): a `GtkWidget`
 * @cell_state: cell renderer state
 *
 * Translates the cell renderer state to `GtkStateFlags`,
 * based on the cell renderer and widget sensitivity, and
 * the given `GtkCellRenderer`State.
 *
 * Returns: the widget state flags applying to @cell
 *
 * Deprecated: 4.10
 **/
GtkStateFlags
gtk_cell_renderer_get_state (GtkCellRenderer      *cell,
			     GtkWidget            *widget,
			     GtkCellRendererState  cell_state)
{
  GtkStateFlags state = 0;

  g_return_val_if_fail (!cell || GTK_IS_CELL_RENDERER (cell), 0);
  g_return_val_if_fail (!widget || GTK_IS_WIDGET (widget), 0);

  if (widget)
    state |= gtk_widget_get_state_flags (widget);

  state &= ~(GTK_STATE_FLAG_FOCUSED | GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_DROP_ACTIVE);

  if ((state & GTK_STATE_FLAG_INSENSITIVE) != 0 ||
      (cell && !gtk_cell_renderer_get_sensitive (cell)) ||
      (cell_state & GTK_CELL_RENDERER_INSENSITIVE) != 0)
    {
      state |= GTK_STATE_FLAG_INSENSITIVE;
    }
  else
    {
      if ((widget && gtk_widget_has_focus (widget)) &&
          (cell_state & GTK_CELL_RENDERER_FOCUSED) != 0)
        state |= GTK_STATE_FLAG_FOCUSED;

      if ((cell_state & GTK_CELL_RENDERER_PRELIT) != 0)
        state |= GTK_STATE_FLAG_PRELIGHT;
    }

  if ((cell_state & GTK_CELL_RENDERER_SELECTED) != 0)
    state |= GTK_STATE_FLAG_SELECTED;

  return state;
}

/**
 * gtk_cell_renderer_set_is_expander:
 * @cell: a `GtkCellRenderer`
 * @is_expander: whether @cell is an expander
 *
 * Sets whether the given `GtkCellRenderer` is an expander.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_set_is_expander (GtkCellRenderer *cell,
                                   gboolean         is_expander)
{
  GtkCellRendererPrivate *priv = gtk_cell_renderer_get_instance_private (cell);

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  is_expander = !!is_expander;

  if (is_expander != priv->is_expander)
    {
      priv->is_expander = is_expander;

      g_object_notify (G_OBJECT (cell), "is-expander");
    }
}

/**
 * gtk_cell_renderer_get_is_expander:
 * @cell: a `GtkCellRenderer`
 *
 * Checks whether the given `GtkCellRenderer` is an expander.
 *
 * Returns: %TRUE if @cell is an expander, and %FALSE otherwise
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_renderer_get_is_expander (GtkCellRenderer *cell)
{
  GtkCellRendererPrivate *priv = gtk_cell_renderer_get_instance_private (cell);

  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);

  return priv->is_expander;
}

/**
 * gtk_cell_renderer_set_is_expanded:
 * @cell: a `GtkCellRenderer`
 * @is_expanded: whether @cell should be expanded
 *
 * Sets whether the given `GtkCellRenderer` is expanded.
 *
 * Deprecated: 4.10
 */
void
gtk_cell_renderer_set_is_expanded (GtkCellRenderer *cell,
                                   gboolean         is_expanded)
{
  GtkCellRendererPrivate *priv = gtk_cell_renderer_get_instance_private (cell);

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  is_expanded = !!is_expanded;

  if (is_expanded != priv->is_expanded)
    {
      priv->is_expanded = is_expanded;

      g_object_notify (G_OBJECT (cell), "is-expanded");
    }
}

/**
 * gtk_cell_renderer_get_is_expanded:
 * @cell: a `GtkCellRenderer`
 *
 * Checks whether the given `GtkCellRenderer` is expanded.
 *
 * Returns: %TRUE if the cell renderer is expanded
 *
 * Deprecated: 4.10
 */
gboolean
gtk_cell_renderer_get_is_expanded (GtkCellRenderer *cell)
{
  GtkCellRendererPrivate *priv = gtk_cell_renderer_get_instance_private (cell);

  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);

  return priv->is_expanded;
}
