/*
 * Copyright © 2018 Benjamin Otte
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

#include "config.h"

#include "gtkpicture.h"

#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gdktextureutilsprivate.h"

/**
 * GtkPicture:
 *
 * The `GtkPicture` widget displays a `GdkPaintable`.
 *
 * ![An example GtkPicture](picture.png)
 *
 * Many convenience functions are provided to make pictures simple to use.
 * For example, if you want to load an image from a file, and then display
 * it, there’s a convenience function to do this:
 *
 * ```c
 * GtkWidget *widget = gtk_picture_new_for_filename ("myfile.png");
 * ```
 *
 * If the file isn’t loaded successfully, the picture will contain a
 * “broken image” icon similar to that used in many web browsers.
 * If you want to handle errors in loading the file yourself,
 * for example by displaying an error message, then load the image with
 * [ctor@Gdk.Texture.new_from_file], then create the `GtkPicture` with
 * [ctor@Gtk.Picture.new_for_paintable].
 *
 * Sometimes an application will want to avoid depending on external data
 * files, such as image files. See the documentation of `GResource` for details.
 * In this case, [ctor@Gtk.Picture.new_for_resource] and
 * [method@Gtk.Picture.set_resource] should be used.
 *
 * `GtkPicture` displays an image at its natural size. See [class@Gtk.Image]
 * if you want to display a fixed-size image, such as an icon.
 *
 * ## Sizing the paintable
 *
 * You can influence how the paintable is displayed inside the `GtkPicture`
 * by changing [property@Gtk.Picture:content-fit]. See [enum@Gtk.ContentFit]
 * for details. [property@Gtk.Picture:can-shrink] can be unset to make sure
 * that paintables are never made smaller than their ideal size - but
 * be careful if you do not know the size of the paintable in use (like
 * when displaying user-loaded images). This can easily cause the picture to
 * grow larger than the screen. And [property@Gtk.Widget:halign] and
 * [property@Gtk.Widget:valign] can be used to make sure the paintable doesn't
 * fill all available space but is instead displayed at its original size.
 *
 * ## CSS nodes
 *
 * `GtkPicture` has a single CSS node with the name `picture`.
 *
 * ## Accessibility
 *
 * `GtkPicture` uses the `GTK_ACCESSIBLE_ROLE_IMG` role.
 */

enum
{
  PROP_0,
  PROP_PAINTABLE,
  PROP_FILE,
  PROP_ALTERNATIVE_TEXT,
  PROP_KEEP_ASPECT_RATIO,
  PROP_CAN_SHRINK,
  PROP_CONTENT_FIT,
  NUM_PROPERTIES
};

struct _GtkPicture
{
  GtkWidget parent_instance;

  GdkPaintable *paintable;
  GFile *file;

  char *alternative_text;
  guint can_shrink : 1;
  GtkContentFit content_fit;
};

struct _GtkPictureClass
{
  GtkWidgetClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkPicture, gtk_picture, GTK_TYPE_WIDGET)

static void
gtk_picture_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkPicture *self = GTK_PICTURE (widget);
  double ratio;
  int x, y, width, height;
  double w, h;

  if (self->paintable == NULL)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);
  ratio = gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);

  if (self->content_fit == GTK_CONTENT_FIT_FILL || ratio == 0)
    {
      gdk_paintable_snapshot (self->paintable, snapshot, width, height);
    }
  else
    {
      double picture_ratio = (double) width / height;
      int paintable_width = gdk_paintable_get_intrinsic_width (self->paintable);
      int paintable_height = gdk_paintable_get_intrinsic_height (self->paintable);

      if (self->content_fit == GTK_CONTENT_FIT_SCALE_DOWN &&
          width >= paintable_width && height >= paintable_height)
        {
          w = paintable_width;
          h = paintable_height;
        }
      else if (ratio > picture_ratio)
        {
          if (self->content_fit == GTK_CONTENT_FIT_COVER)
            {
              w = height * ratio;
              h = height;
            }
          else
            {
              w = width;
              h = width / ratio;
            }
        }
      else
        {
          if (self->content_fit == GTK_CONTENT_FIT_COVER)
            {
              w = width;
              h = width / ratio;
            }
          else
            {
              w = height * ratio;
              h = height;
            }
        }

      w = ceil (w);
      h = ceil (h);

      x = (width - w) / 2;
      y = floor(height - h) / 2;

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
      gdk_paintable_snapshot (self->paintable, snapshot, w, h);
      gtk_snapshot_restore (snapshot);
    }
}

static GtkSizeRequestMode
gtk_picture_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_picture_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int            for_size,
                     int           *minimum,
                     int           *natural,
                     int           *minimum_baseline,
                     int           *natural_baseline)
{
  GtkPicture *self = GTK_PICTURE (widget);
  GtkCssStyle *style;
  double min_width, min_height, nat_width, nat_height;
  double default_size;

  /* for_size = 0 below is treated as -1, but we want to return zeros. */
  if (self->paintable == NULL || for_size == 0)
    {
      *minimum = 0;
      *natural = 0;
      return;
    }

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  default_size = _gtk_css_number_value_get (style->icon->icon_size, 100);

  if (self->can_shrink)
    {
      min_width = min_height = 0;
    }
  else
    {
      gdk_paintable_compute_concrete_size (self->paintable,
                                           0, 0,
                                           default_size, default_size,
                                           &min_width, &min_height);
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gdk_paintable_compute_concrete_size (self->paintable,
                                           0,
                                           for_size < 0 ? 0 : for_size,
                                           default_size, default_size,
                                           &nat_width, &nat_height);
      *minimum = ceil (min_width);
      *natural = ceil (nat_width);
    }
  else
    {
      gdk_paintable_compute_concrete_size (self->paintable,
                                           for_size < 0 ? 0 : for_size,
                                           0,
                                           default_size, default_size,
                                           &nat_width, &nat_height);
      *minimum = ceil (min_height);
      *natural = ceil (nat_height);
    }
}

static void
gtk_picture_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkPicture *self = GTK_PICTURE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      gtk_picture_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_FILE:
      gtk_picture_set_file (self, g_value_get_object (value));
      break;

    case PROP_ALTERNATIVE_TEXT:
      gtk_picture_set_alternative_text (self, g_value_get_string (value));
      break;

    case PROP_KEEP_ASPECT_RATIO:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_picture_set_keep_aspect_ratio (self, g_value_get_boolean (value));
      G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    case PROP_CAN_SHRINK:
      gtk_picture_set_can_shrink (self, g_value_get_boolean (value));
      break;

    case PROP_CONTENT_FIT:
      gtk_picture_set_content_fit (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_picture_get_property (GObject     *object,
                          guint        prop_id,
                          GValue      *value,
                          GParamSpec  *pspec)
{
  GtkPicture *self = GTK_PICTURE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_ALTERNATIVE_TEXT:
      g_value_set_string (value, self->alternative_text);
      break;

    case PROP_KEEP_ASPECT_RATIO:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_value_set_boolean (value, gtk_picture_get_keep_aspect_ratio (self));
      G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    case PROP_CAN_SHRINK:
      g_value_set_boolean (value, self->can_shrink);
      break;

    case PROP_CONTENT_FIT:
      g_value_set_enum (value, self->content_fit);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_picture_paintable_invalidate_contents (GdkPaintable *paintable,
                                           GtkPicture   *self)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_picture_paintable_invalidate_size (GdkPaintable *paintable,
                                       GtkPicture   *self)
{
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_picture_clear_paintable (GtkPicture *self)
{
  guint flags;

  if (self->paintable == NULL)
    return;

  flags = gdk_paintable_get_flags (self->paintable);

  if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
    g_signal_handlers_disconnect_by_func (self->paintable,
                                          gtk_picture_paintable_invalidate_contents,
                                          self);

  if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
    g_signal_handlers_disconnect_by_func (self->paintable,
                                          gtk_picture_paintable_invalidate_size,
                                          self);

  g_object_unref (self->paintable);
}

static void
gtk_picture_dispose (GObject *object)
{
  GtkPicture *self = GTK_PICTURE (object);

  gtk_picture_clear_paintable (self);

  g_clear_object (&self->file);
  g_clear_pointer (&self->alternative_text, g_free);

  G_OBJECT_CLASS (gtk_picture_parent_class)->dispose (object);
};

static void
gtk_picture_class_init (GtkPictureClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->set_property = gtk_picture_set_property;
  gobject_class->get_property = gtk_picture_get_property;
  gobject_class->dispose = gtk_picture_dispose;

  widget_class->snapshot = gtk_picture_snapshot;
  widget_class->get_request_mode = gtk_picture_get_request_mode;
  widget_class->measure = gtk_picture_measure;

  /**
   * GtkPicture:paintable: (attributes org.gtk.Property.get=gtk_picture_get_paintable org.gtk.Property.set=gtk_picture_set_paintable)
   *
   * The `GdkPaintable` to be displayed by this `GtkPicture`.
   */
  properties[PROP_PAINTABLE] =
      g_param_spec_object ("paintable", NULL, NULL,
                           GDK_TYPE_PAINTABLE,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPicture:file: (attributes org.gtk.Property.get=gtk_picture_get_file org.gtk.Property.set=gtk_picture_set_file)
   *
   * The `GFile` that is displayed or %NULL if none.
   */
  properties[PROP_FILE] =
      g_param_spec_object ("file", NULL, NULL,
                           G_TYPE_FILE,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPicture:alternative-text: (attributes org.gtk.Property.get=gtk_picture_get_alternative_text org.gtk.Property.set=gtk_picture_set_alternative_text)
   *
   * The alternative textual description for the picture.
   */
  properties[PROP_ALTERNATIVE_TEXT] =
      g_param_spec_string ("alternative-text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPicture:keep-aspect-ratio: (attributes org.gtk.Property.get=gtk_picture_get_keep_aspect_ratio org.gtk.Property.set=gtk_picture_set_keep_aspect_ratio)
   *
   * Whether the GtkPicture will render its contents trying to preserve the aspect
   * ratio.
   *
   * Deprecated: 4.8: Use [property@Gtk.Picture:content-fit] instead.
   */
  properties[PROP_KEEP_ASPECT_RATIO] =
      g_param_spec_boolean ("keep-aspect-ratio", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE |
                            G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_DEPRECATED);

  /**
   * GtkPicture:can-shrink: (attributes org.gtk.Property.get=gtk_picture_get_can_shrink org.gtk.Property.set=gtk_picture_set_can_shrink)
   *
   * If the `GtkPicture` can be made smaller than the natural size of its contents.
   */
  properties[PROP_CAN_SHRINK] =
      g_param_spec_boolean ("can-shrink", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPicture:content-fit: (attributes org.gtk.Property.get=gtk_picture_get_content_fit org.gtk.Property.set=gtk_picture_set_content_fit)
   *
   * How the content should be resized to fit inside the `GtkPicture`.
   *
   * Since: 4.8
   */
  properties[PROP_CONTENT_FIT] =
      g_param_spec_enum ("content-fit", NULL, NULL,
                         GTK_TYPE_CONTENT_FIT,
                         GTK_CONTENT_FIT_CONTAIN,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_css_name (widget_class, I_("picture"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_IMG);
}

static void
gtk_picture_init (GtkPicture *self)
{
  self->can_shrink = TRUE;
  self->content_fit = GTK_CONTENT_FIT_CONTAIN;

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}

/**
 * gtk_picture_new:
 *
 * Creates a new empty `GtkPicture` widget.
 *
 * Returns: a newly created `GtkPicture` widget.
 */
GtkWidget*
gtk_picture_new (void)
{
  return g_object_new (GTK_TYPE_PICTURE, NULL);
}

/**
 * gtk_picture_new_for_paintable:
 * @paintable: (nullable): a `GdkPaintable`
 *
 * Creates a new `GtkPicture` displaying @paintable.
 *
 * The `GtkPicture` will track changes to the @paintable and update
 * its size and contents in response to it.
 *
 * Returns: a new `GtkPicture`
 */
GtkWidget*
gtk_picture_new_for_paintable (GdkPaintable *paintable)
{
  g_return_val_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable), NULL);

  return g_object_new (GTK_TYPE_PICTURE,
                       "paintable", paintable,
                       NULL);
}

/**
 * gtk_picture_new_for_pixbuf:
 * @pixbuf: (nullable): a `GdkPixbuf`
 *
 * Creates a new `GtkPicture` displaying @pixbuf.
 *
 * This is a utility function that calls [ctor@Gtk.Picture.new_for_paintable],
 * See that function for details.
 *
 * The pixbuf must not be modified after passing it to this function.
 *
 * Returns: a new `GtkPicture`
 *
 * Deprecated: 4.12: Use [ctor@Gtk.Picture.new_for_paintable] and
 *   [ctor@Gdk.Texture.new_for_pixbuf] instead
 */
GtkWidget*
gtk_picture_new_for_pixbuf (GdkPixbuf *pixbuf)
{
  GtkWidget *result;
  GdkPaintable *paintable;

  g_return_val_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf), NULL);

  if (pixbuf)
    paintable = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (pixbuf));
  else
    paintable = NULL;

  result = gtk_picture_new_for_paintable (paintable);

  if (paintable)
    g_object_unref (paintable);

  return result;
}

/**
 * gtk_picture_new_for_file:
 * @file: (nullable): a `GFile`
 *
 * Creates a new `GtkPicture` displaying the given @file.
 *
 * If the file isn’t found or can’t be loaded, the resulting
 * `GtkPicture` is empty.
 *
 * If you need to detect failures to load the file, use
 * [ctor@Gdk.Texture.new_from_file] to load the file yourself,
 * then create the `GtkPicture` from the texture.
 *
 * Returns: a new `GtkPicture`
 */
GtkWidget*
gtk_picture_new_for_file (GFile *file)
{
  g_return_val_if_fail (file == NULL || G_IS_FILE (file), NULL);

  return g_object_new (GTK_TYPE_PICTURE,
                       "file", file,
                       NULL);
}

/**
 * gtk_picture_new_for_filename:
 * @filename: (type filename) (nullable): a filename
 *
 * Creates a new `GtkPicture` displaying the file @filename.
 *
 * This is a utility function that calls [ctor@Gtk.Picture.new_for_file].
 * See that function for details.
 *
 * Returns: a new `GtkPicture`
 */
GtkWidget*
gtk_picture_new_for_filename (const char *filename)
{
  GtkWidget *result;
  GFile *file;

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  result = gtk_picture_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

/**
 * gtk_picture_new_for_resource:
 * @resource_path: (nullable): resource path to play back
 *
 * Creates a new `GtkPicture` displaying the resource at @resource_path.
 *
 * This is a utility function that calls [ctor@Gtk.Picture.new_for_file].
 * See that function for details.
 *
 * Returns: a new `GtkPicture`
 */
GtkWidget *
gtk_picture_new_for_resource (const char *resource_path)
{
  GtkWidget *result;
  GFile *file;

  if (resource_path)
    {
      char *uri, *escaped;

      escaped = g_uri_escape_string (resource_path,
                                     G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
      uri = g_strconcat ("resource://", escaped, NULL);
      g_free (escaped);

      file = g_file_new_for_uri (uri);
      g_free (uri);
    }
  else
    {
      file = NULL;
    }

  result = gtk_picture_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

/**
 * gtk_picture_set_file: (attributes org.gtk.Method.set_property=file)
 * @self: a `GtkPicture`
 * @file: (nullable): a `GFile`
 *
 * Makes @self load and display @file.
 *
 * See [ctor@Gtk.Picture.new_for_file] for details.
 */
void
gtk_picture_set_file (GtkPicture *self,
                      GFile      *file)
{
  GdkPaintable *paintable;

  g_return_if_fail (GTK_IS_PICTURE (self));
  g_return_if_fail (file == NULL || G_IS_FILE (file));

  if (self->file == file)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  g_set_object (&self->file, file);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);

  if (file)
    paintable = gdk_paintable_new_from_file_scaled (file, gtk_widget_get_scale_factor (GTK_WIDGET (self)));
  else
    paintable = NULL;

  gtk_picture_set_paintable (self, paintable);
  g_clear_object (&paintable);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_picture_get_file: (attributes org.gtk.Method.get_property=file)
 * @self: a `GtkPicture`
 *
 * Gets the `GFile` currently displayed if @self is displaying a file.
 *
 * If @self is not displaying a file, for example when
 * [method@Gtk.Picture.set_paintable] was used, then %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The `GFile` displayed by @self.
 */
GFile *
gtk_picture_get_file (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), FALSE);

  return self->file;
}

/**
 * gtk_picture_set_filename:
 * @self: a `GtkPicture`
 * @filename: (type filename) (nullable): the filename to play
 *
 * Makes @self load and display the given @filename.
 *
 * This is a utility function that calls [method@Gtk.Picture.set_file].
 */
void
gtk_picture_set_filename (GtkPicture *self,
                          const char *filename)
{
  GFile *file;

  g_return_if_fail (GTK_IS_PICTURE (self));

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  gtk_picture_set_file (self, file);

  if (file)
    g_object_unref (file);
}

/**
 * gtk_picture_set_resource:
 * @self: a `GtkPicture`
 * @resource_path: (nullable): the resource to set
 *
 * Makes @self load and display the resource at the given
 * @resource_path.
 *
 * This is a utility function that calls [method@Gtk.Picture.set_file].
 */
void
gtk_picture_set_resource (GtkPicture *self,
                          const char *resource_path)
{
  GFile *file;

  g_return_if_fail (GTK_IS_PICTURE (self));

  if (resource_path)
    {
      char *uri, *escaped;

      escaped = g_uri_escape_string (resource_path,
                                     G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
      uri = g_strconcat ("resource://", escaped, NULL);
      g_free (escaped);

      file = g_file_new_for_uri (uri);
      g_free (uri);
    }
  else
    {
      file = NULL;
    }

  gtk_picture_set_file (self, file);

  if (file)
    g_object_unref (file);
}

/**
 * gtk_picture_set_pixbuf:
 * @self: a `GtkPicture`
 * @pixbuf: (nullable): a `GdkPixbuf`
 *
 * Sets a `GtkPicture` to show a `GdkPixbuf`.
 *
 * See [ctor@Gtk.Picture.new_for_pixbuf] for details.
 *
 * This is a utility function that calls [method@Gtk.Picture.set_paintable].
 *
 * Deprecated: 4.12: Use [method@Gtk.Picture.set_paintable] instead
 */
void
gtk_picture_set_pixbuf (GtkPicture *self,
                        GdkPixbuf  *pixbuf)
{
  GdkTexture *texture;

  g_return_if_fail (GTK_IS_PICTURE (self));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  if (pixbuf)
    texture = gdk_texture_new_for_pixbuf (pixbuf);
  else
    texture = NULL;

  gtk_picture_set_paintable (self, GDK_PAINTABLE (texture));

  if (texture)
    g_object_unref (texture);
}

/**
 * gtk_picture_set_paintable: (attributes org.gtk.Method.set_property=paintable)
 * @self: a `GtkPicture`
 * @paintable: (nullable): a `GdkPaintable`
 *
 * Makes @self display the given @paintable.
 *
 * If @paintable is %NULL, nothing will be displayed.
 *
 * See [ctor@Gtk.Picture.new_for_paintable] for details.
 */
void
gtk_picture_set_paintable (GtkPicture   *self,
                           GdkPaintable *paintable)
{
  g_return_if_fail (GTK_IS_PICTURE (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  if (self->paintable == paintable)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  if (paintable)
    g_object_ref (paintable);

  gtk_picture_clear_paintable (self);

  self->paintable = paintable;

  if (paintable)
    {
      const guint flags = gdk_paintable_get_flags (paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_connect (paintable,
                          "invalidate-contents",
                          G_CALLBACK (gtk_picture_paintable_invalidate_contents),
                          self);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_connect (paintable,
                          "invalidate-size",
                          G_CALLBACK (gtk_picture_paintable_invalidate_size),
                          self);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_picture_get_paintable: (attributes org.gtk.Method.get_property=paintable)
 * @self: a `GtkPicture`
 *
 * Gets the `GdkPaintable` being displayed by the `GtkPicture`.
 *
 * Returns: (nullable) (transfer none): the displayed paintable
 */
GdkPaintable *
gtk_picture_get_paintable (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), NULL);

  return self->paintable;
}

/**
 * gtk_picture_set_keep_aspect_ratio: (attributes org.gtk.Method.set_property=keep-aspect-ratio)
 * @self: a `GtkPicture`
 * @keep_aspect_ratio: whether to keep aspect ratio
 *
 * If set to %TRUE, the @self will render its contents according to
 * their aspect ratio.
 *
 * That means that empty space may show up at the top/bottom or
 * left/right of @self.
 *
 * If set to %FALSE or if the contents provide no aspect ratio,
 * the contents will be stretched over the picture's whole area.
 *
 * Deprecated: 4.8: Use [method@Gtk.Picture.set_content_fit] instead. If still
 *   used, this method will always set the [property@Gtk.Picture:content-fit]
 *   property to `GTK_CONTENT_FIT_CONTAIN` if @keep_aspect_ratio is true,
 *   otherwise it will set it to `GTK_CONTENT_FIT_FILL`.
 */
void
gtk_picture_set_keep_aspect_ratio (GtkPicture *self,
                                   gboolean    keep_aspect_ratio)
{
  if (keep_aspect_ratio)
    gtk_picture_set_content_fit (self, GTK_CONTENT_FIT_CONTAIN);
  else
    gtk_picture_set_content_fit (self, GTK_CONTENT_FIT_FILL);
}

/**
 * gtk_picture_get_keep_aspect_ratio: (attributes org.gtk.Method.get_property=keep-aspect-ratio)
 * @self: a `GtkPicture`
 *
 * Returns whether the `GtkPicture` preserves its contents aspect ratio.
 *
 * Returns: %TRUE if the self tries to keep the contents' aspect ratio
 *
 * Deprecated: 4.8: Use [method@Gtk.Picture.get_content_fit] instead. This will
 *   now return `FALSE` only if [property@Gtk.Picture:content-fit] is
 *   `GTK_CONTENT_FIT_FILL`. Returns `TRUE` otherwise.
 */
gboolean
gtk_picture_get_keep_aspect_ratio (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), TRUE);

  return self->content_fit != GTK_CONTENT_FIT_FILL;
}

/**
 * gtk_picture_set_can_shrink: (attributes org.gtk.Method.set_property=can-shrink)
 * @self: a `GtkPicture`
 * @can_shrink: if @self can be made smaller than its contents
 *
 * If set to %TRUE, the @self can be made smaller than its contents.
 *
 * The contents will then be scaled down when rendering.
 *
 * If you want to still force a minimum size manually, consider using
 * [method@Gtk.Widget.set_size_request].
 *
 * Also of note is that a similar function for growing does not exist
 * because the grow behavior can be controlled via
 * [method@Gtk.Widget.set_halign] and [method@Gtk.Widget.set_valign].
 */
void
gtk_picture_set_can_shrink (GtkPicture *self,
                            gboolean    can_shrink)
{
  g_return_if_fail (GTK_IS_PICTURE (self));

  if (self->can_shrink == can_shrink)
    return;

  self->can_shrink = can_shrink;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CAN_SHRINK]);
}

/**
 * gtk_picture_get_can_shrink: (attributes org.gtk.Method.get_property=can-shrink)
 * @self: a `GtkPicture`
 *
 * Returns whether the `GtkPicture` respects its contents size.
 *
 * Returns: %TRUE if the picture can be made smaller than its contents
 */
gboolean
gtk_picture_get_can_shrink (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), FALSE);

  return self->can_shrink;
}

/**
 * gtk_picture_set_content_fit: (attributes org.gtk.Method.set_property=content-fit)
 * @self: a `GtkPicture`
 * @content_fit: the content fit mode
 *
 * Sets how the content should be resized to fit the `GtkPicture`.
 *
 * See [enum@Gtk.ContentFit] for details.
 *
 * Since: 4.8
 */
void
gtk_picture_set_content_fit (GtkPicture    *self,
                             GtkContentFit  content_fit)
{
  gboolean notify_keep_aspect_ratio;

  g_return_if_fail (GTK_IS_PICTURE (self));

  if (self->content_fit == content_fit)
    return;

  notify_keep_aspect_ratio = (content_fit == GTK_CONTENT_FIT_FILL ||
                              self->content_fit == GTK_CONTENT_FIT_FILL);

  self->content_fit = content_fit;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTENT_FIT]);

  if (notify_keep_aspect_ratio)
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEEP_ASPECT_RATIO]);
}

/**
 * gtk_picture_get_content_fit: (attributes org.gtk.Method.get_property=content-fit)
 * @self: a `GtkPicture`
 *
 * Returns the fit mode for the content of the `GtkPicture`.
 *
 * See [enum@Gtk.ContentFit] for details.
 *
 * Returns: the content fit mode
 *
 * Since: 4.8
 */
GtkContentFit
gtk_picture_get_content_fit (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), FALSE);

  return self->content_fit;
}

/**
 * gtk_picture_set_alternative_text: (attributes org.gtk.Method.set_property=alternative-text)
 * @self: a `GtkPicture`
 * @alternative_text: (nullable): a textual description of the contents
 *
 * Sets an alternative textual description for the picture contents.
 *
 * It is equivalent to the "alt" attribute for images on websites.
 *
 * This text will be made available to accessibility tools.
 *
 * If the picture cannot be described textually, set this property to %NULL.
 */
void
gtk_picture_set_alternative_text (GtkPicture *self,
                                  const char *alternative_text)
{
  g_return_if_fail (GTK_IS_PICTURE (self));

  if (g_strcmp0 (self->alternative_text, alternative_text) == 0)
    return;

  g_free (self->alternative_text);
  self->alternative_text = g_strdup (alternative_text);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, alternative_text,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALTERNATIVE_TEXT]);
}

/**
 * gtk_picture_get_alternative_text: (attributes org.gtk.Method.get_property=alternative-text)
 * @self: a `GtkPicture`
 *
 * Gets the alternative textual description of the picture.
 *
 * The returned string will be %NULL if the picture cannot be described textually.
 *
 * Returns: (nullable) (transfer none): the alternative textual description of @self.
 */
const char *
gtk_picture_get_alternative_text (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), NULL);

  return self->alternative_text;
}

