/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkimageprivate.h"

#include "gtkiconhelperprivate.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gdktextureutilsprivate.h"

#include <math.h>
#include <string.h>
#include <cairo-gobject.h>

/**
 * GtkImage:
 *
 * The `GtkImage` widget displays an image.
 *
 * ![An example GtkImage](image.png)
 *
 * Various kinds of object can be displayed as an image; most typically,
 * you would load a `GdkTexture` from a file, using the convenience function
 * [ctor@Gtk.Image.new_from_file], for instance:
 *
 * ```c
 * GtkWidget *image = gtk_image_new_from_file ("myfile.png");
 * ```
 *
 * If the file isn’t loaded successfully, the image will contain a
 * “broken image” icon similar to that used in many web browsers.
 *
 * If you want to handle errors in loading the file yourself,
 * for example by displaying an error message, then load the image with
 * [ctor@Gdk.Texture.new_from_file], then create the `GtkImage` with
 * [ctor@Gtk.Image.new_from_paintable].
 *
 * Sometimes an application will want to avoid depending on external data
 * files, such as image files. See the documentation of `GResource` inside
 * GIO, for details. In this case, [property@Gtk.Image:resource],
 * [ctor@Gtk.Image.new_from_resource], and [method@Gtk.Image.set_from_resource]
 * should be used.
 *
 * `GtkImage` displays its image as an icon, with a size that is determined
 * by the application. See [class@Gtk.Picture] if you want to show an image
 * at is actual size.
 *
 * ## CSS nodes
 *
 * `GtkImage` has a single CSS node with the name `image`. The style classes
 * `.normal-icons` or `.large-icons` may appear, depending on the
 * [property@Gtk.Image:icon-size] property.
 *
 * ## Accessibility
 *
 * `GtkImage` uses the `GTK_ACCESSIBLE_ROLE_IMG` role.
 */

typedef struct _GtkImageClass GtkImageClass;

struct _GtkImage
{
  GtkWidget parent_instance;

  GtkIconHelper *icon_helper;
  GtkIconSize icon_size;

  float baseline_align;

  char *filename;
  char *resource_path;
};

struct _GtkImageClass
{
  GtkWidgetClass parent_class;
};


static void gtk_image_snapshot             (GtkWidget    *widget,
                                            GtkSnapshot  *snapshot);
static void gtk_image_unrealize            (GtkWidget    *widget);
static void gtk_image_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int            for_size,
                               int           *minimum,
                               int           *natural,
                               int           *minimum_baseline,
                               int           *natural_baseline);

static void gtk_image_css_changed          (GtkWidget    *widget,
                                            GtkCssStyleChange *change);
static void gtk_image_system_setting_changed (GtkWidget        *widget,
                                              GtkSystemSetting  setting);
static void gtk_image_finalize             (GObject      *object);

static void gtk_image_set_property         (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void gtk_image_get_property         (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);
static void gtk_image_clear_internal       (GtkImage *self,
                                            gboolean  notify);

enum
{
  PROP_0,
  PROP_PAINTABLE,
  PROP_FILE,
  PROP_ICON_SIZE,
  PROP_PIXEL_SIZE,
  PROP_ICON_NAME,
  PROP_STORAGE_TYPE,
  PROP_GICON,
  PROP_RESOURCE,
  PROP_USE_FALLBACK,
  NUM_PROPERTIES
};

static GParamSpec *image_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkImage, gtk_image, GTK_TYPE_WIDGET)

static void
gtk_image_class_init (GtkImageClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_image_set_property;
  gobject_class->get_property = gtk_image_get_property;
  gobject_class->finalize = gtk_image_finalize;

  widget_class = GTK_WIDGET_CLASS (class);
  widget_class->snapshot = gtk_image_snapshot;
  widget_class->measure = gtk_image_measure;
  widget_class->unrealize = gtk_image_unrealize;
  widget_class->css_changed = gtk_image_css_changed;
  widget_class->system_setting_changed = gtk_image_system_setting_changed;

  /**
   * GtkImage:paintable: (attributes org.gtk.Property.get=gtk_image_get_paintable org.gtk.Property.set=gtk_image_set_from_paintable)
   *
   * The `GdkPaintable` to display.
   */
  image_props[PROP_PAINTABLE] =
      g_param_spec_object ("paintable", NULL, NULL,
                           GDK_TYPE_PAINTABLE,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:file: (attributes org.gtk.Property.set=gtk_image_set_from_file)
   *
   * A path to the file to display.
   */
  image_props[PROP_FILE] =
      g_param_spec_string ("file", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:icon-size: (attributes org.gtk.Property.get=gtk_image_get_icon_size org.gtk.Property.set=gtk_image_set_icon_size org.gtk.Property.set=gtk_image_set_icon_size)
   *
   * The symbolic size to display icons at.
   */
  image_props[PROP_ICON_SIZE] =
      g_param_spec_enum ("icon-size", NULL, NULL,
                         GTK_TYPE_ICON_SIZE,
                         GTK_ICON_SIZE_INHERIT,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkImage:pixel-size: (attributes org.gtk.Property.get=gtk_image_get_pixel_size org.gtk.Property.set=gtk_image_set_pixel_size)
   *
   * The size in pixels to display icons at.
   *
   * If set to a value != -1, this property overrides the
   * [property@Gtk.Image:icon-size] property for images of type
   * `GTK_IMAGE_ICON_NAME`.
   */
  image_props[PROP_PIXEL_SIZE] =
      g_param_spec_int ("pixel-size", NULL, NULL,
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkImage:icon-name: (attributes org.gtk.Property.get=gtk_image_get_icon_name org.gtk.Property.set=gtk_image_set_from_icon_name)
   *
   * The name of the icon in the icon theme.
   *
   * If the icon theme is changed, the image will be updated automatically.
   */
  image_props[PROP_ICON_NAME] =
      g_param_spec_string ("icon-name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:gicon: (attributes org.gtk.Property.get=gtk_image_get_gicon org.gtk.Property.set=gtk_image_set_from_gicon)
   *
   * The `GIcon` displayed in the GtkImage.
   *
   * For themed icons, If the icon theme is changed, the image will be updated
   * automatically.
   */
  image_props[PROP_GICON] =
      g_param_spec_object ("gicon", NULL, NULL,
                           G_TYPE_ICON,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:resource: (attributes org.gtk.Property.set=gtk_image_set_from_resource)
   *
   * A path to a resource file to display.
   */
  image_props[PROP_RESOURCE] =
      g_param_spec_string ("resource", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:storage-type: (attributes org.gtk.Property.get=gtk_image_get_storage_type)
   *
   * The representation being used for image data.
   */
  image_props[PROP_STORAGE_TYPE] =
      g_param_spec_enum ("storage-type", NULL, NULL,
                         GTK_TYPE_IMAGE_TYPE,
                         GTK_IMAGE_EMPTY,
                         GTK_PARAM_READABLE);

  /**
   * GtkImage:use-fallback:
   *
   * Whether the icon displayed in the `GtkImage` will use
   * standard icon names fallback.
   *
   * The value of this property is only relevant for images of type
   * %GTK_IMAGE_ICON_NAME and %GTK_IMAGE_GICON.
   */
  image_props[PROP_USE_FALLBACK] =
      g_param_spec_boolean ("use-fallback", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, image_props);

  gtk_widget_class_set_css_name (widget_class, I_("image"));

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_IMG);
}

static void
gtk_image_init (GtkImage *image)
{
  GtkCssNode *widget_node;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (image));

  image->icon_helper = gtk_icon_helper_new (widget_node, GTK_WIDGET (image));
}

static void
gtk_image_finalize (GObject *object)
{
  GtkImage *image = GTK_IMAGE (object);

  gtk_image_clear_internal (image, FALSE);

  g_clear_object (&image->icon_helper);

  g_free (image->filename);
  g_free (image->resource_path);

  G_OBJECT_CLASS (gtk_image_parent_class)->finalize (object);
};

static void
gtk_image_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkImage *image = GTK_IMAGE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      gtk_image_set_from_paintable (image, g_value_get_object (value));
      break;
    case PROP_FILE:
      gtk_image_set_from_file (image, g_value_get_string (value));
      break;
    case PROP_ICON_SIZE:
      gtk_image_set_icon_size (image, g_value_get_enum (value));
      break;
    case PROP_PIXEL_SIZE:
      gtk_image_set_pixel_size (image, g_value_get_int (value));
      break;
    case PROP_ICON_NAME:
      gtk_image_set_from_icon_name (image, g_value_get_string (value));
      break;
    case PROP_GICON:
      gtk_image_set_from_gicon (image, g_value_get_object (value));
      break;
    case PROP_RESOURCE:
      gtk_image_set_from_resource (image, g_value_get_string (value));
      break;

    case PROP_USE_FALLBACK:
      if (_gtk_icon_helper_set_use_fallback (image->icon_helper, g_value_get_boolean (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_image_get_property (GObject     *object,
			guint        prop_id,
			GValue      *value,
			GParamSpec  *pspec)
{
  GtkImage *image = GTK_IMAGE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, _gtk_icon_helper_peek_paintable (image->icon_helper));
      break;
    case PROP_FILE:
      g_value_set_string (value, image->filename);
      break;
    case PROP_ICON_SIZE:
      g_value_set_enum (value, image->icon_size);
      break;
    case PROP_PIXEL_SIZE:
      g_value_set_int (value, _gtk_icon_helper_get_pixel_size (image->icon_helper));
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, _gtk_icon_helper_get_icon_name (image->icon_helper));
      break;
    case PROP_GICON:
      g_value_set_object (value, _gtk_icon_helper_peek_gicon (image->icon_helper));
      break;
    case PROP_RESOURCE:
      g_value_set_string (value, image->resource_path);
      break;
    case PROP_USE_FALLBACK:
      g_value_set_boolean (value, _gtk_icon_helper_get_use_fallback (image->icon_helper));
      break;
    case PROP_STORAGE_TYPE:
      g_value_set_enum (value, _gtk_icon_helper_get_storage_type (image->icon_helper));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/**
 * gtk_image_new_from_file:
 * @filename: (type filename): a filename
 *
 * Creates a new `GtkImage` displaying the file @filename.
 *
 * If the file isn’t found or can’t be loaded, the resulting `GtkImage`
 * will display a “broken image” icon. This function never returns %NULL,
 * it always returns a valid `GtkImage` widget.
 *
 * If you need to detect failures to load the file, use
 * [ctor@Gdk.Texture.new_from_file] to load the file yourself,
 * then create the `GtkImage` from the texture.
 *
 * The storage type (see [method@Gtk.Image.get_storage_type])
 * of the returned image is not defined, it will be whatever
 * is appropriate for displaying the file.
 *
 * Returns: a new `GtkImage`
 */
GtkWidget*
gtk_image_new_from_file   (const char *filename)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_file (image, filename);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_resource:
 * @resource_path: a resource path
 *
 * Creates a new `GtkImage` displaying the resource file @resource_path.
 *
 * If the file isn’t found or can’t be loaded, the resulting `GtkImage` will
 * display a “broken image” icon. This function never returns %NULL,
 * it always returns a valid `GtkImage` widget.
 *
 * If you need to detect failures to load the file, use
 * [ctor@GdkPixbuf.Pixbuf.new_from_file] to load the file yourself,
 * then create the `GtkImage` from the pixbuf.
 *
 * The storage type (see [method@Gtk.Image.get_storage_type]) of
 * the returned image is not defined, it will be whatever is
 * appropriate for displaying the file.
 *
 * Returns: a new `GtkImage`
 */
GtkWidget*
gtk_image_new_from_resource (const char *resource_path)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_resource (image, resource_path);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_pixbuf:
 * @pixbuf: (nullable): a `GdkPixbuf`
 *
 * Creates a new `GtkImage` displaying @pixbuf.
 *
 * The `GtkImage` does not assume a reference to the pixbuf; you still
 * need to unref it if you own references. `GtkImage` will add its own
 * reference rather than adopting yours.
 *
 * This is a helper for [ctor@Gtk.Image.new_from_paintable], and you can't
 * get back the exact pixbuf once this is called, only a texture.
 *
 * Note that this function just creates an `GtkImage` from the pixbuf.
 * The `GtkImage` created will not react to state changes. Should you
 * want that, you should use [ctor@Gtk.Image.new_from_icon_name].
 *
 * Returns: a new `GtkImage`
 *
 * Deprecated: 4.12: Use [ctor@Gtk.Image.new_from_paintable] and
 *   [ctor@Gdk.Texture.new_for_pixbuf] instead
 */
GtkWidget*
gtk_image_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_image_set_from_pixbuf (image, pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_paintable:
 * @paintable: (nullable): a `GdkPaintable`
 *
 * Creates a new `GtkImage` displaying @paintable.
 *
 * The `GtkImage` does not assume a reference to the paintable; you still
 * need to unref it if you own references. `GtkImage` will add its own
 * reference rather than adopting yours.
 *
 * The `GtkImage` will track changes to the @paintable and update
 * its size and contents in response to it.
 *
 * Returns: a new `GtkImage`
 */
GtkWidget*
gtk_image_new_from_paintable (GdkPaintable *paintable)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_paintable (image, paintable);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_icon_name:
 * @icon_name: (nullable): an icon name
 *
 * Creates a `GtkImage` displaying an icon from the current icon theme.
 *
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead. If the current icon theme is changed, the icon
 * will be updated appropriately.
 *
 * Returns: a new `GtkImage` displaying the themed icon
 */
GtkWidget*
gtk_image_new_from_icon_name (const char *icon_name)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_icon_name (image, icon_name);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_gicon:
 * @icon: an icon
 *
 * Creates a `GtkImage` displaying an icon from the current icon theme.
 *
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead. If the current icon theme is changed, the icon
 * will be updated appropriately.
 *
 * Returns: a new `GtkImage` displaying the themed icon
 */
GtkWidget*
gtk_image_new_from_gicon (GIcon *icon)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_gicon (image, icon);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_set_from_file: (attributes org.gtk.Method.set_property=file)
 * @image: a `GtkImage`
 * @filename: (type filename) (nullable): a filename
 *
 * Sets a `GtkImage` to show a file.
 *
 * See [ctor@Gtk.Image.new_from_file] for details.
 */
void
gtk_image_set_from_file (GtkImage    *image,
                         const char *filename)
{
  int scale_factor;
  GdkPaintable *paintable;

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (filename == NULL)
    {
      image->filename = NULL;
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (image));
  paintable = gdk_paintable_new_from_filename_scaled (filename, scale_factor);

  if (paintable == NULL)
    {
      gtk_image_set_from_icon_name (image, "image-missing");
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  gtk_image_set_from_paintable (image, paintable);

  g_object_unref (paintable);

  image->filename = g_strdup (filename);

  g_object_thaw_notify (G_OBJECT (image));
}

#ifndef GDK_PIXBUF_MAGIC_NUMBER
#define GDK_PIXBUF_MAGIC_NUMBER (0x47646b50)    /* 'GdkP' */
#endif

static gboolean
resource_is_pixdata (const char *resource_path)
{
  const guint8 *stream;
  guint32 magic;
  gsize data_size;
  GBytes *bytes;
  gboolean ret = FALSE;

  bytes = g_resources_lookup_data (resource_path, 0, NULL);
  if (bytes == NULL)
    return FALSE;

  stream = g_bytes_get_data (bytes, &data_size);
  if (data_size < sizeof(guint32))
    goto out;

  magic = (((guint32)(stream[0])) << 24) | (((guint32)(stream[1])) << 16) | (((guint32)(stream[2])) << 8) | (guint32)(stream[3]);
  if (magic == GDK_PIXBUF_MAGIC_NUMBER)
    ret = TRUE;

out:
  g_bytes_unref (bytes);
  return ret;
}

/**
 * gtk_image_set_from_resource: (attributes org.gtk.Method.set_property=resource)
 * @image: a `GtkImage`
 * @resource_path: (nullable): a resource path
 *
 * Sets a `GtkImage` to show a resource.
 *
 * See [ctor@Gtk.Image.new_from_resource] for details.
 */
void
gtk_image_set_from_resource (GtkImage   *image,
                             const char *resource_path)
{
  int scale_factor;
  GdkPaintable *paintable;

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (resource_path == NULL)
    {
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  if (resource_is_pixdata (resource_path))
    {
      g_warning ("GdkPixdata format images are not supported, remove the \"to-pixdata\" option from your GResource files");
      paintable = NULL;
    }
  else
    {
      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (image));
      paintable = gdk_paintable_new_from_resource_scaled (resource_path, scale_factor);
    }

  if (paintable == NULL)
    {
      gtk_image_set_from_icon_name (image, "image-missing");
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  gtk_image_set_from_paintable (image, paintable);

  g_object_unref (paintable);

  image->resource_path = g_strdup (resource_path);

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_RESOURCE]);

  g_object_thaw_notify (G_OBJECT (image));
}


/**
 * gtk_image_set_from_pixbuf: (attributes org.gtk.Method.set_property=paintable)
 * @image: a `GtkImage`
 * @pixbuf: (nullable): a `GdkPixbuf` or `NULL`
 *
 * Sets a `GtkImage` to show a `GdkPixbuf`.
 *
 * See [ctor@Gtk.Image.new_from_pixbuf] for details.
 *
 * Note: This is a helper for [method@Gtk.Image.set_from_paintable],
 * and you can't get back the exact pixbuf once this is called,
 * only a paintable.
 *
 * Deprecated: 4.12: Use [method@Gtk.Image.set_from_paintable] instead
 */
void
gtk_image_set_from_pixbuf (GtkImage  *image,
                           GdkPixbuf *pixbuf)
{
  GdkTexture *texture;

  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  if (pixbuf)
    texture = gdk_texture_new_for_pixbuf (pixbuf);
  else
    texture = NULL;

  gtk_image_set_from_paintable (image, GDK_PAINTABLE (texture));

  if (texture)
    g_object_unref (texture);
}

/**
 * gtk_image_set_from_icon_name: (attributes org.gtk.Method.set_property=icon-name)
 * @image: a `GtkImage`
 * @icon_name: (nullable): an icon name
 *
 * Sets a `GtkImage` to show a named icon.
 *
 * See [ctor@Gtk.Image.new_from_icon_name] for details.
 */
void
gtk_image_set_from_icon_name  (GtkImage    *image,
			       const char *icon_name)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (icon_name)
    _gtk_icon_helper_set_icon_name (image->icon_helper, icon_name);

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_NAME]);
  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_STORAGE_TYPE]);

  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_gicon: (attributes org.gtk.Method.set_property=gicon)
 * @image: a `GtkImage`
 * @icon: an icon
 *
 * Sets a `GtkImage` to show a `GIcon`.
 *
 * See [ctor@Gtk.Image.new_from_gicon] for details.
 */
void
gtk_image_set_from_gicon  (GtkImage       *image,
			   GIcon          *icon)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  if (icon)
    g_object_ref (icon);

  gtk_image_clear (image);

  if (icon)
    {
      _gtk_icon_helper_set_gicon (image->icon_helper, icon);
      g_object_unref (icon);
    }

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_GICON]);
  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_STORAGE_TYPE]);

  g_object_thaw_notify (G_OBJECT (image));
}

static void
gtk_image_paintable_invalidate_contents (GdkPaintable *paintable,
                                         GtkImage     *image)
{
  gtk_widget_queue_draw (GTK_WIDGET (image));
}

static void
gtk_image_paintable_invalidate_size (GdkPaintable *paintable,
                                     GtkImage     *image)
{
  gtk_icon_helper_invalidate (image->icon_helper);
}

/**
 * gtk_image_set_from_paintable: (attributes org.gtk.Method.set_property=paintable)
 * @image: a `GtkImage`
 * @paintable: (nullable): a `GdkPaintable`
 *
 * Sets a `GtkImage` to show a `GdkPaintable`.
 *
 * See [ctor@Gtk.Image.new_from_paintable] for details.
 */
void
gtk_image_set_from_paintable (GtkImage     *image,
			      GdkPaintable *paintable)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  g_object_freeze_notify (G_OBJECT (image));

  if (paintable)
    g_object_ref (paintable);

  gtk_image_clear (image);

  if (paintable)
    {
      const guint flags = gdk_paintable_get_flags (paintable);

      _gtk_icon_helper_set_paintable (image->icon_helper, paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_connect (paintable,
                          "invalidate-contents",
                          G_CALLBACK (gtk_image_paintable_invalidate_contents),
                          image);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_connect (paintable,
                          "invalidate-size",
                          G_CALLBACK (gtk_image_paintable_invalidate_size),
                          image);
      g_object_unref (paintable);
    }

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PAINTABLE]);
  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_STORAGE_TYPE]);

  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_get_storage_type: (attributes org.gtk.Method.get_property=storage-type)
 * @image: a `GtkImage`
 *
 * Gets the type of representation being used by the `GtkImage`
 * to store image data.
 *
 * If the `GtkImage` has no image data, the return value will
 * be %GTK_IMAGE_EMPTY.
 *
 * Returns: image representation being used
 */
GtkImageType
gtk_image_get_storage_type (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), GTK_IMAGE_EMPTY);

  return _gtk_icon_helper_get_storage_type (image->icon_helper);
}

/**
 * gtk_image_get_paintable: (attributes org.gtk.Method.get_property=paintable)
 * @image: a `GtkImage`
 *
 * Gets the image `GdkPaintable` being displayed by the `GtkImage`.
 *
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_PAINTABLE (see [method@Gtk.Image.get_storage_type]).
 * The caller of this function does not own a reference to the
 * returned paintable.
 *
 * Returns: (nullable) (transfer none): the displayed paintable
 */
GdkPaintable *
gtk_image_get_paintable (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  return _gtk_icon_helper_peek_paintable (image->icon_helper);
}

/**
 * gtk_image_get_icon_name: (attributes org.gtk.Method.get_property=icon-name)
 * @image: a `GtkImage`
 *
 * Gets the icon name and size being displayed by the `GtkImage`.
 *
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ICON_NAME (see [method@Gtk.Image.get_storage_type]).
 * The returned string is owned by the `GtkImage` and should not
 * be freed.
 *
 * Returns: (transfer none) (nullable): the icon name
 */
const char *
gtk_image_get_icon_name (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  return _gtk_icon_helper_get_icon_name (image->icon_helper);
}

/**
 * gtk_image_get_gicon: (attributes org.gtk.Method.get_property=gicon)
 * @image: a `GtkImage`
 *
 * Gets the `GIcon` being displayed by the `GtkImage`.
 *
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_GICON (see [method@Gtk.Image.get_storage_type]).
 * The caller of this function does not own a reference to the
 * returned `GIcon`.
 *
 * Returns: (transfer none) (nullable): a `GIcon`
 **/
GIcon *
gtk_image_get_gicon (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  return _gtk_icon_helper_peek_gicon (image->icon_helper);
}

/**
 * gtk_image_new:
 *
 * Creates a new empty `GtkImage` widget.
 *
 * Returns: a newly created `GtkImage` widget.
 */
GtkWidget*
gtk_image_new (void)
{
  return g_object_new (GTK_TYPE_IMAGE, NULL);
}

static void
gtk_image_unrealize (GtkWidget *widget)
{
  GtkImage *image = GTK_IMAGE (widget);

  gtk_icon_helper_invalidate (image->icon_helper);

  GTK_WIDGET_CLASS (gtk_image_parent_class)->unrealize (widget);
}

static float
gtk_image_get_baseline_align (GtkImage *image)
{
  PangoContext *pango_context;
  PangoFontMetrics *metrics;

  if (image->baseline_align == 0.0)
    {
      pango_context = gtk_widget_get_pango_context (GTK_WIDGET (image));
      metrics = pango_context_get_metrics (pango_context, NULL, NULL);
      image->baseline_align =
                (float)pango_font_metrics_get_ascent (metrics) /
                (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics));

      pango_font_metrics_unref (metrics);
    }

  return image->baseline_align;
}

static void
gtk_image_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkImage *image = GTK_IMAGE (widget);
  double ratio;
  int x, y, width, height, baseline;
  double w, h;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);
  ratio = gdk_paintable_get_intrinsic_aspect_ratio (GDK_PAINTABLE (image->icon_helper));

  if (ratio == 0)
    {
      gdk_paintable_snapshot (GDK_PAINTABLE (image->icon_helper), snapshot, width, height);
    }
  else
    {
      double image_ratio = (double) width / height;

      if (ratio > image_ratio)
        {
          w = width;
          h = width / ratio;
        }
      else
        {
          w = height * ratio;
          h = height;
        }

      x = (width - ceil (w)) / 2;

      baseline = gtk_widget_get_baseline (widget);
      if (baseline == -1)
        y = floor(height - ceil (h)) / 2;
      else
        y = CLAMP (baseline - h * gtk_image_get_baseline_align (image), 0, height - ceil (h));

      if (x != 0 || y != 0)
        {
          gtk_snapshot_save (snapshot);
          gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
          gdk_paintable_snapshot (GDK_PAINTABLE (image->icon_helper), snapshot, w, h);
          gtk_snapshot_restore (snapshot);
        }
      else
        {
          gdk_paintable_snapshot (GDK_PAINTABLE (image->icon_helper), snapshot, w, h);
        }
    }
}

static void
gtk_image_notify_for_storage_type (GtkImage     *image,
                                   GtkImageType  storage_type)
{
  switch (storage_type)
    {
    case GTK_IMAGE_ICON_NAME:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_NAME]);
      break;
    case GTK_IMAGE_GICON:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_GICON]);
      break;
    case GTK_IMAGE_PAINTABLE:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PAINTABLE]);
      break;
    case GTK_IMAGE_EMPTY:
    default:
      break;
    }
}

void
gtk_image_set_from_definition (GtkImage           *image,
                               GtkImageDefinition *def)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (def != NULL)
    {
      _gtk_icon_helper_set_definition (image->icon_helper, def);

      gtk_image_notify_for_storage_type (image, gtk_image_definition_get_storage_type (def));
    }

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_STORAGE_TYPE]);

  g_object_thaw_notify (G_OBJECT (image));
}

GtkImageDefinition *
gtk_image_get_definition (GtkImage *image)
{
  return gtk_icon_helper_get_definition (image->icon_helper);
}

static void
gtk_image_clear_internal (GtkImage *self,
                          gboolean  notify)
{
  GtkImageType storage_type = gtk_image_get_storage_type (self);
  GObject *gobject = G_OBJECT (self);

  if (notify)
    {
      if (storage_type != GTK_IMAGE_EMPTY)
        g_object_notify_by_pspec (gobject, image_props[PROP_STORAGE_TYPE]);

      g_object_notify_by_pspec (gobject, image_props[PROP_ICON_SIZE]);

      gtk_image_notify_for_storage_type (self, storage_type);
    }

  if (self->filename)
    {
      g_free (self->filename);
      self->filename = NULL;

      if (notify)
        g_object_notify_by_pspec (gobject, image_props[PROP_FILE]);
    }

  if (self->resource_path)
    {
      g_free (self->resource_path);
      self->resource_path = NULL;

      if (notify)
        g_object_notify_by_pspec (gobject, image_props[PROP_RESOURCE]);
    }

  if (storage_type == GTK_IMAGE_PAINTABLE)
    {
      GdkPaintable *paintable = _gtk_icon_helper_peek_paintable (self->icon_helper);
      const guint flags = gdk_paintable_get_flags (paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_handlers_disconnect_by_func (paintable,
                                              gtk_image_paintable_invalidate_contents,
                                              self);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_handlers_disconnect_by_func (paintable,
                                              gtk_image_paintable_invalidate_size,
                                              self);
    }

  _gtk_icon_helper_clear (self->icon_helper);
}

/**
 * gtk_image_clear:
 * @image: a `GtkImage`
 *
 * Resets the image to be empty.
 */
void
gtk_image_clear (GtkImage *image)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));
  gtk_image_clear_internal (image, TRUE);
  g_object_thaw_notify (G_OBJECT (image));
}

static void
gtk_image_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int            for_size,
                   int           *minimum,
                   int           *natural,
                   int           *minimum_baseline,
                   int           *natural_baseline)
{
  GtkImage *image = GTK_IMAGE (widget);
  float baseline_align;

  *minimum = *natural = gtk_icon_helper_get_size (image->icon_helper);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      baseline_align = gtk_image_get_baseline_align (GTK_IMAGE (widget));
      *minimum_baseline = *minimum * baseline_align;
      *natural_baseline = *natural * baseline_align;
    }
}

static void
gtk_image_css_changed (GtkWidget         *widget,
                       GtkCssStyleChange *change)
{
  GtkImage *image = GTK_IMAGE (widget);

  gtk_icon_helper_invalidate_for_change (image->icon_helper, change);

  GTK_WIDGET_CLASS (gtk_image_parent_class)->css_changed (widget, change);

  image->baseline_align = 0.0;
}

static void
gtk_image_system_setting_changed (GtkWidget        *widget,
                                  GtkSystemSetting  setting)
{
  GtkImage *image = GTK_IMAGE (widget);

  if (setting == GTK_SYSTEM_SETTING_ICON_THEME)
    gtk_icon_helper_invalidate (image->icon_helper);

  GTK_WIDGET_CLASS (gtk_image_parent_class)->system_setting_changed (widget, setting);
}

/**
 * gtk_image_set_pixel_size: (attributes org.gtk.Method.set_property=pixel-size)
 * @image: a `GtkImage`
 * @pixel_size: the new pixel size
 *
 * Sets the pixel size to use for named icons.
 *
 * If the pixel size is set to a value != -1, it is used instead
 * of the icon size set by [method@Gtk.Image.set_from_icon_name].
 */
void
gtk_image_set_pixel_size (GtkImage *image,
			  int       pixel_size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  if (_gtk_icon_helper_set_pixel_size (image->icon_helper, pixel_size))
    {
      if (gtk_widget_get_visible (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PIXEL_SIZE]);
    }
}

/**
 * gtk_image_get_pixel_size: (attributes org.gtk.Method.get_property=pixel-size)
 * @image: a `GtkImage`
 *
 * Gets the pixel size used for named icons.
 *
 * Returns: the pixel size used for named icons.
 */
int
gtk_image_get_pixel_size (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), -1);

  return _gtk_icon_helper_get_pixel_size (image->icon_helper);
}

/**
 * gtk_image_set_icon_size: (attributes org.gtk.Method.set_property=icon-size)
 * @image: a `GtkImage`
 * @icon_size: the new icon size
 *
 * Suggests an icon size to the theme for named icons.
 */
void
gtk_image_set_icon_size (GtkImage    *image,
			 GtkIconSize  icon_size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  if (image->icon_size == icon_size)
    return;

  image->icon_size = icon_size;
  gtk_icon_size_set_style_classes (gtk_widget_get_css_node (GTK_WIDGET (image)), icon_size);
  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_SIZE]);
}

/**
 * gtk_image_get_icon_size: (attributes org.gtk.Method.get_property=icon-size)
 * @image: a `GtkImage`
 *
 * Gets the icon size used by the @image when rendering icons.
 *
 * Returns: the image size used by icons
 */
GtkIconSize
gtk_image_get_icon_size (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), GTK_ICON_SIZE_INHERIT);

  return image->icon_size;
}

void
gtk_image_get_image_size (GtkImage *image,
                          int      *width,
                          int      *height)
{
  *width = *height = gtk_icon_helper_get_size (image->icon_helper);
}
