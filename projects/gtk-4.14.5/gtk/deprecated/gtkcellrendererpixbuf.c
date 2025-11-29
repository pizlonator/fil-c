/* gtkcellrendererpixbuf.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#include "gtkcellrendererpixbuf.h"

#include "gtkiconhelperprivate.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"

#include <cairo-gobject.h>
#include <stdlib.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkCellRendererPixbuf:
 *
 * Renders a pixbuf in a cell
 *
 * A `GtkCellRendererPixbuf` can be used to render an image in a cell. It allows
 * to render either a given `GdkPixbuf` (set via the
 * `GtkCellRendererPixbuf:pixbuf` property) or a named icon (set via the
 * `GtkCellRendererPixbuf:icon-name` property).
 *
 * To support the tree view, `GtkCellRendererPixbuf` also supports rendering two
 * alternative pixbufs, when the `GtkCellRenderer:is-expander` property is %TRUE.
 * If the `GtkCellRenderer:is-expanded property` is %TRUE and the
 * `GtkCellRendererPixbuf:pixbuf-expander-open` property is set to a pixbuf, it
 * renders that pixbuf, if the `GtkCellRenderer:is-expanded` property is %FALSE
 * and the `GtkCellRendererPixbuf:pixbuf-expander-closed` property is set to a
 * pixbuf, it renders that one.
 *
 * Deprecated: 4.10: List views use widgets to display their contents. You
 *   should use [class@Gtk.Image] for icons, and [class@Gtk.Picture] for images
 */


static void gtk_cell_renderer_pixbuf_get_property  (GObject                    *object,
						    guint                       param_id,
						    GValue                     *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_pixbuf_set_property  (GObject                    *object,
						    guint                       param_id,
						    const GValue               *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_pixbuf_get_size   (GtkCellRendererPixbuf      *self,
						 GtkWidget                  *widget,
						 const GdkRectangle         *rectangle,
						 int                        *x_offset,
						 int                        *y_offset,
						 int                        *width,
						 int                        *height);
static void gtk_cell_renderer_pixbuf_snapshot   (GtkCellRenderer            *cell,
						 GtkSnapshot                *snapshot,
						 GtkWidget                  *widget,
						 const GdkRectangle         *background_area,
						 const GdkRectangle         *cell_area,
						 GtkCellRendererState        flags);


enum {
  PROP_0,
  PROP_PIXBUF,
  PROP_PIXBUF_EXPANDER_OPEN,
  PROP_PIXBUF_EXPANDER_CLOSED,
  PROP_TEXTURE,
  PROP_ICON_SIZE,
  PROP_ICON_NAME,
  PROP_GICON
};

typedef struct _GtkCellRendererPixbufPrivate       GtkCellRendererPixbufPrivate;
typedef struct _GtkCellRendererPixbufClass         GtkCellRendererPixbufClass;

struct _GtkCellRendererPixbuf
{
  GtkCellRenderer parent;
};

struct _GtkCellRendererPixbufClass
{
  GtkCellRendererClass parent_class;
};

struct _GtkCellRendererPixbufPrivate
{
  GtkImageDefinition *image_def;
  GtkIconSize         icon_size;

  GdkPixbuf *pixbuf_expander_open;
  GdkPixbuf *pixbuf_expander_closed;
  GdkTexture *texture_expander_open;
  GdkTexture *texture_expander_closed;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellRendererPixbuf, gtk_cell_renderer_pixbuf, GTK_TYPE_CELL_RENDERER)

static void
gtk_cell_renderer_pixbuf_init (GtkCellRendererPixbuf *cellpixbuf)
{
  GtkCellRendererPixbufPrivate *priv = gtk_cell_renderer_pixbuf_get_instance_private (cellpixbuf);

  priv->image_def = gtk_image_definition_new_empty ();
}

static void
gtk_cell_renderer_pixbuf_finalize (GObject *object)
{
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (object);
  GtkCellRendererPixbufPrivate *priv = gtk_cell_renderer_pixbuf_get_instance_private (cellpixbuf);

  gtk_image_definition_unref (priv->image_def);

  g_clear_object (&priv->pixbuf_expander_open);
  g_clear_object (&priv->pixbuf_expander_closed);
  g_clear_object (&priv->texture_expander_open);
  g_clear_object (&priv->texture_expander_closed);

  G_OBJECT_CLASS (gtk_cell_renderer_pixbuf_parent_class)->finalize (object);
}

static GtkSizeRequestMode
gtk_cell_renderer_pixbuf_get_request_mode (GtkCellRenderer *cell)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_cell_renderer_pixbuf_get_preferred_width (GtkCellRenderer *cell,
                                              GtkWidget       *widget,
                                              int             *minimum,
                                              int             *natural)
{
  int size = 0;

  gtk_cell_renderer_pixbuf_get_size (GTK_CELL_RENDERER_PIXBUF (cell), widget, NULL,
                                     NULL, NULL, &size, NULL);

  if (minimum != NULL)
    *minimum = size;

  if (natural != NULL)
    *natural = size;
}

static void
gtk_cell_renderer_pixbuf_get_preferred_height (GtkCellRenderer *cell,
                                               GtkWidget       *widget,
                                               int             *minimum,
                                               int             *natural)
{
  int size = 0;

  gtk_cell_renderer_pixbuf_get_size (GTK_CELL_RENDERER_PIXBUF (cell), widget, NULL,
                                     NULL, NULL, NULL, &size);

  if (minimum != NULL)
    *minimum = size;

  if (natural != NULL)
    *natural = size;
}

static void
gtk_cell_renderer_pixbuf_class_init (GtkCellRendererPixbufClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->finalize = gtk_cell_renderer_pixbuf_finalize;

  object_class->get_property = gtk_cell_renderer_pixbuf_get_property;
  object_class->set_property = gtk_cell_renderer_pixbuf_set_property;

  cell_class->get_request_mode = gtk_cell_renderer_pixbuf_get_request_mode;
  cell_class->get_preferred_width = gtk_cell_renderer_pixbuf_get_preferred_width;
  cell_class->get_preferred_height = gtk_cell_renderer_pixbuf_get_preferred_height;
  cell_class->snapshot = gtk_cell_renderer_pixbuf_snapshot;

  g_object_class_install_property (object_class,
				   PROP_PIXBUF,
				   g_param_spec_object ("pixbuf", NULL, NULL,
							GDK_TYPE_PIXBUF,
							GTK_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_PIXBUF_EXPANDER_OPEN,
				   g_param_spec_object ("pixbuf-expander-open", NULL, NULL,
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_PIXBUF_EXPANDER_CLOSED,
				   g_param_spec_object ("pixbuf-expander-closed", NULL, NULL,
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererPixbuf:texture:
   */
  g_object_class_install_property (object_class,
				   PROP_TEXTURE,
				   g_param_spec_object ("texture", NULL, NULL,
                                                        GDK_TYPE_TEXTURE,
						        GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererPixbuf:icon-size:
   *
   * The `GtkIconSize` value that specifies the size of the rendered icon.
   */
  g_object_class_install_property (object_class,
				   PROP_ICON_SIZE,
				   g_param_spec_enum ("icon-size", NULL, NULL,
                                                      GTK_TYPE_ICON_SIZE,
						      GTK_ICON_SIZE_INHERIT,
						      GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererPixbuf:icon-name:
   *
   * The name of the themed icon to display.
   * This property only has an effect if not overridden by the "pixbuf" property.
   */
  g_object_class_install_property (object_class,
				   PROP_ICON_NAME,
				   g_param_spec_string ("icon-name", NULL, NULL,
							NULL,
							GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererPixbuf:gicon:
   *
   * The GIcon representing the icon to display.
   * If the icon theme is changed, the image will be updated
   * automatically.
   */
  g_object_class_install_property (object_class,
                                   PROP_GICON,
                                   g_param_spec_object ("gicon", NULL, NULL,
                                                        G_TYPE_ICON,
                                                        GTK_PARAM_READWRITE));
}

static void
gtk_cell_renderer_pixbuf_get_property (GObject        *object,
				       guint           param_id,
				       GValue         *value,
				       GParamSpec     *pspec)
{
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (object);
  GtkCellRendererPixbufPrivate *priv = gtk_cell_renderer_pixbuf_get_instance_private (cellpixbuf);

  switch (param_id)
    {
    case PROP_PIXBUF_EXPANDER_OPEN:
      g_value_set_object (value, priv->pixbuf_expander_open);
      break;
    case PROP_PIXBUF_EXPANDER_CLOSED:
      g_value_set_object (value, priv->pixbuf_expander_closed);
      break;
    case PROP_TEXTURE:
      g_value_set_object (value, gtk_image_definition_get_paintable (priv->image_def));
      break;
    case PROP_ICON_SIZE:
      g_value_set_enum (value, priv->icon_size);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, gtk_image_definition_get_icon_name (priv->image_def));
      break;
    case PROP_GICON:
      g_value_set_object (value, gtk_image_definition_get_gicon (priv->image_def));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
notify_storage_type (GtkCellRendererPixbuf *cellpixbuf,
                     GtkImageType           storage_type)
{
  switch (storage_type)
    {
    case GTK_IMAGE_PAINTABLE:
      g_object_notify (G_OBJECT (cellpixbuf), "texture");
      break;
    case GTK_IMAGE_ICON_NAME:
      g_object_notify (G_OBJECT (cellpixbuf), "icon-name");
      break;
    case GTK_IMAGE_GICON:
      g_object_notify (G_OBJECT (cellpixbuf), "gicon");
      break;
    default:
      g_assert_not_reached ();
    case GTK_IMAGE_EMPTY:
      break;
    }
}

static void
take_image_definition (GtkCellRendererPixbuf *cellpixbuf,
                       GtkImageDefinition    *def)
{
  GtkCellRendererPixbufPrivate *priv = gtk_cell_renderer_pixbuf_get_instance_private (cellpixbuf);
  GtkImageType old_storage_type, new_storage_type;

  if (def == NULL)
    def = gtk_image_definition_new_empty ();

  old_storage_type = gtk_image_definition_get_storage_type (priv->image_def);
  new_storage_type = gtk_image_definition_get_storage_type (def);

  if (new_storage_type != old_storage_type)
    notify_storage_type (cellpixbuf, old_storage_type);

  gtk_image_definition_unref (priv->image_def);
  priv->image_def = def;
}

static void
gtk_cell_renderer_pixbuf_set_icon_size (GtkCellRendererPixbuf *cellpixbuf,
                                        GtkIconSize            icon_size)
{
  GtkCellRendererPixbufPrivate *priv = gtk_cell_renderer_pixbuf_get_instance_private (cellpixbuf);

  if (priv->icon_size == icon_size)
    return;

  priv->icon_size = icon_size;
  g_object_notify (G_OBJECT (cellpixbuf), "icon-size");
}

static void
gtk_cell_renderer_pixbuf_set_property (GObject      *object,
				       guint         param_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (object);
  GtkCellRendererPixbufPrivate *priv = gtk_cell_renderer_pixbuf_get_instance_private (cellpixbuf);
  GdkTexture *texture;
  GdkPixbuf *pixbuf;

  switch (param_id)
    {
    case PROP_PIXBUF:
      pixbuf = g_value_get_object (value);
      if (pixbuf)
        texture = gdk_texture_new_for_pixbuf (pixbuf);
      else
        texture = NULL;
      take_image_definition (cellpixbuf, gtk_image_definition_new_paintable (GDK_PAINTABLE (texture)));
      g_clear_object (&texture);
      break;
    case PROP_PIXBUF_EXPANDER_OPEN:
      g_clear_object (&priv->pixbuf_expander_open);
      g_clear_object (&priv->texture_expander_open);
      priv->pixbuf_expander_open = (GdkPixbuf*) g_value_dup_object (value);
      priv->texture_expander_open = gdk_texture_new_for_pixbuf (priv->pixbuf_expander_open);
      break;
    case PROP_PIXBUF_EXPANDER_CLOSED:
      g_clear_object (&priv->pixbuf_expander_closed);
      g_clear_object (&priv->texture_expander_closed);
      priv->pixbuf_expander_closed = (GdkPixbuf*) g_value_dup_object (value);
      priv->texture_expander_closed = gdk_texture_new_for_pixbuf (priv->pixbuf_expander_open);
      break;
    case PROP_TEXTURE:
      take_image_definition (cellpixbuf, gtk_image_definition_new_paintable (g_value_get_object (value)));
      break;
    case PROP_ICON_SIZE:
      gtk_cell_renderer_pixbuf_set_icon_size (cellpixbuf, g_value_get_enum (value));
      break;
    case PROP_ICON_NAME:
      take_image_definition (cellpixbuf, gtk_image_definition_new_icon_name (g_value_get_string (value)));
      break;
    case PROP_GICON:
      take_image_definition (cellpixbuf, gtk_image_definition_new_gicon (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * gtk_cell_renderer_pixbuf_new:
 *
 * Creates a new `GtkCellRendererPixbuf`. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with `GtkTreeViewColumn`, you
 * can bind a property to a value in a `GtkTreeModel`. For example, you
 * can bind the “pixbuf” property on the cell renderer to a pixbuf value
 * in the model, thus rendering a different image in each row of the
 * `GtkTreeView`.
 *
 * Returns: the new cell renderer
 *
 * Deprecated: 4.10
 **/
GtkCellRenderer *
gtk_cell_renderer_pixbuf_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF, NULL);
}

static GtkIconHelper *
create_icon_helper (GtkCellRendererPixbuf *cellpixbuf,
                    GtkWidget             *widget)
{
  GtkCellRendererPixbufPrivate *priv = gtk_cell_renderer_pixbuf_get_instance_private (cellpixbuf);
  GtkIconHelper *icon_helper;

  icon_helper = gtk_icon_helper_new (gtk_style_context_get_node (gtk_widget_get_style_context (widget)),
                                     widget);
  _gtk_icon_helper_set_use_fallback (icon_helper, TRUE);
  _gtk_icon_helper_set_definition (icon_helper, priv->image_def);

  return icon_helper;
}

static void
gtk_cell_renderer_pixbuf_get_size (GtkCellRendererPixbuf *self,
				   GtkWidget             *widget,
				   const GdkRectangle    *cell_area,
				   int                   *x_offset,
				   int                   *y_offset,
				   int                   *width,
				   int                   *height)
{
  GtkCellRendererPixbufPrivate *priv = gtk_cell_renderer_pixbuf_get_instance_private (self);
  GtkCellRenderer *cell = GTK_CELL_RENDERER (self);
  int pixbuf_width;
  int pixbuf_height;
  int calc_width;
  int calc_height;
  int xpad, ypad;
  GtkStyleContext *context;
  GtkIconHelper *icon_helper;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "image");
  gtk_icon_size_set_style_classes (gtk_style_context_get_node (context), priv->icon_size);
  icon_helper = create_icon_helper (self, widget);

  if (_gtk_icon_helper_get_is_empty (icon_helper))
    pixbuf_width = pixbuf_height = 0;
  else if (gtk_image_definition_get_paintable (priv->image_def))
    {
      GdkPaintable *paintable = gtk_image_definition_get_paintable (priv->image_def);
      pixbuf_width = gdk_paintable_get_intrinsic_width (paintable);
      pixbuf_height = gdk_paintable_get_intrinsic_height (paintable);
    }
  else
    pixbuf_width = pixbuf_height = gtk_icon_helper_get_size (icon_helper);

  g_object_unref (icon_helper);
  gtk_style_context_restore (context);

  if (priv->pixbuf_expander_open)
    {
      pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (priv->pixbuf_expander_open));
      pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (priv->pixbuf_expander_open));
    }
  if (priv->pixbuf_expander_closed)
    {
      pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (priv->pixbuf_expander_closed));
      pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (priv->pixbuf_expander_closed));
    }

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
  calc_width  = (int) xpad * 2 + pixbuf_width;
  calc_height = (int) ypad * 2 + pixbuf_height;

  if (cell_area && pixbuf_width > 0 && pixbuf_height > 0)
    {
      float xalign, yalign;

      gtk_cell_renderer_get_alignment (cell, &xalign, &yalign);
      if (x_offset)
	{
	  *x_offset = (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
                        (1.0 - xalign) : xalign) *
                       (cell_area->width - calc_width));
	  *x_offset = MAX (*x_offset, 0);
	}
      if (y_offset)
	{
	  *y_offset = (yalign *
                       (cell_area->height - calc_height));
          *y_offset = MAX (*y_offset, 0);
	}
    }
  else
    {
      if (x_offset) *x_offset = 0;
      if (y_offset) *y_offset = 0;
    }

  if (width)
    *width = calc_width;

  if (height)
    *height = calc_height;
}

static void
gtk_cell_renderer_pixbuf_snapshot (GtkCellRenderer      *cell,
                                   GtkSnapshot          *snapshot,
                                   GtkWidget            *widget,
                                   const GdkRectangle   *background_area,
                                   const GdkRectangle   *cell_area,
                                   GtkCellRendererState  flags)

{
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (cell);
  GtkCellRendererPixbufPrivate *priv = gtk_cell_renderer_pixbuf_get_instance_private (cellpixbuf);
  GtkStyleContext *context;
  GdkRectangle pix_rect;
  gboolean is_expander;
  int xpad, ypad;
  GtkIconHelper *icon_helper;

  gtk_cell_renderer_pixbuf_get_size (cellpixbuf, widget,
                                     cell_area,
				     &pix_rect.x,
                                     &pix_rect.y,
                                     &pix_rect.width,
                                     &pix_rect.height);

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
  pix_rect.x += cell_area->x + xpad;
  pix_rect.y += cell_area->y + ypad;
  pix_rect.width -= xpad * 2;
  pix_rect.height -= ypad * 2;

  if (!gdk_rectangle_intersect (cell_area, &pix_rect, NULL))
    return;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (context);

  gtk_style_context_add_class (context, "image");
  gtk_icon_size_set_style_classes (gtk_style_context_get_node (context), priv->icon_size);

  is_expander = gtk_cell_renderer_get_is_expander (cell);
  if (is_expander)
    {
      gboolean is_expanded = gtk_cell_renderer_get_is_expanded (cell);;

      if (is_expanded && priv->pixbuf_expander_open != NULL)
        {
          icon_helper = gtk_icon_helper_new (gtk_style_context_get_node (context), widget);
          _gtk_icon_helper_set_paintable (icon_helper, GDK_PAINTABLE (priv->texture_expander_open));
        }
      else if (!is_expanded && priv->pixbuf_expander_closed != NULL)
        {
          icon_helper = gtk_icon_helper_new (gtk_style_context_get_node (context), widget);
          _gtk_icon_helper_set_paintable (icon_helper, GDK_PAINTABLE (priv->texture_expander_closed));
        }
      else
        {
          icon_helper = create_icon_helper (cellpixbuf, widget);
        }
    }
  else
    {
      icon_helper = create_icon_helper (cellpixbuf, widget);
    }

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (pix_rect.x, pix_rect.y));
  gdk_paintable_snapshot (GDK_PAINTABLE (icon_helper), snapshot, pix_rect.width, pix_rect.height);
  gtk_snapshot_restore (snapshot);

  g_object_unref (icon_helper);
  gtk_style_context_restore (context);
}
