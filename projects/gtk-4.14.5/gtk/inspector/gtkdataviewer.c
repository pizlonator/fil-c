/*
 * Copyright © 2021 Benjamin Otte
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

#include "gtkdataviewer.h"

#include "gtkbinlayout.h"
#include "gtklabel.h"
#include "gtkpicture.h"
#include "gtkcolorswatchprivate.h"
#include "gtkbox.h"


struct _GtkDataViewer
{
  GtkWidget parent_instance;

  GtkWidget *contents;
  GCancellable *cancellable;
  GError *error;

  enum {
    NOT_LOADED = 0,
    LOADING_DONE,
    LOADING_EXTERNALLY,
    LOADING_INTERNALLY,
    LOADING_FAILED
  } loading;
};

enum
{
  PROP_0,
  PROP_LOADING,

  N_PROPS
};

enum {
  LOAD,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkDataViewer, gtk_data_viewer, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL];

static void
gtk_data_viewer_ensure_loaded (GtkDataViewer *self)
{
  gboolean started_loading;

  if (self->loading != NOT_LOADED)
    return;

  self->loading = LOADING_EXTERNALLY;
  self->cancellable = g_cancellable_new ();
  g_signal_emit (self, signals[LOAD], 0, self->cancellable, &started_loading);

  if (!started_loading)
    {
      self->loading = LOADING_FAILED; /* avoid notify::is_loading */
      gtk_data_viewer_load_error (self, g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED, "Nothing to load"));
    }

  g_assert (self->loading != NOT_LOADED);

  if (gtk_data_viewer_is_loading (self))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
}

static void
gtk_data_viewer_realize (GtkWidget *widget)
{
  GtkDataViewer *self = GTK_DATA_VIEWER (widget);

  GTK_WIDGET_CLASS (gtk_data_viewer_parent_class)->realize (widget);

  gtk_data_viewer_ensure_loaded (self);
}

static void
gtk_data_viewer_unrealize (GtkWidget *widget)
{
  GtkDataViewer *self = GTK_DATA_VIEWER (widget);

  GTK_WIDGET_CLASS (gtk_data_viewer_parent_class)->unrealize (widget);

  gtk_data_viewer_reset (self);
}

static void
gtk_data_viewer_dispose (GObject *object)
{
  //GtkDataViewer *self = GTK_DATA_VIEWER (object);

  G_OBJECT_CLASS (gtk_data_viewer_parent_class)->dispose (object);
}

static void
gtk_data_viewer_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkDataViewer *self = GTK_DATA_VIEWER (object);

  switch (property_id)
    {
    case PROP_LOADING:
      g_value_set_boolean (value, gtk_data_viewer_is_loading (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_data_viewer_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  //GtkDataViewer *self = GTK_DATA_VIEWER (object);

  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_data_viewer_class_init (GtkDataViewerClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  widget_class->realize = gtk_data_viewer_realize;
  widget_class->unrealize = gtk_data_viewer_unrealize;

  gobject_class->dispose = gtk_data_viewer_dispose;
  gobject_class->get_property = gtk_data_viewer_get_property;
  gobject_class->set_property = gtk_data_viewer_set_property;

  properties[PROP_LOADING] =
    g_param_spec_boolean ("loading", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  signals[LOAD] =
      g_signal_new ("load",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    g_signal_accumulator_first_wins, NULL,
                    NULL,
                    G_TYPE_BOOLEAN, 1,
                    G_TYPE_CANCELLABLE);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "frame");
}

static void
gtk_data_viewer_init (GtkDataViewer *self)
{
}

GtkWidget *
gtk_data_viewer_new (void)
{
  return g_object_new (GTK_TYPE_DATA_VIEWER, NULL);
}

gboolean
gtk_data_viewer_is_loading (GtkDataViewer *self)
{
  g_return_val_if_fail (GTK_IS_DATA_VIEWER (self), FALSE);

  return self->loading == LOADING_EXTERNALLY ||
         self->loading == LOADING_INTERNALLY;
}

void
gtk_data_viewer_reset (GtkDataViewer *self)
{
  gboolean was_loading;

  g_return_if_fail (GTK_IS_DATA_VIEWER (self));

  g_object_freeze_notify (G_OBJECT (self));

  was_loading = gtk_data_viewer_is_loading (self);

  g_clear_pointer (&self->contents, gtk_widget_unparent);
  g_clear_error (&self->error);
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->loading = NOT_LOADED;

  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    gtk_data_viewer_ensure_loaded (self);

  if (was_loading != gtk_data_viewer_is_loading (self))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);

  g_object_thaw_notify (G_OBJECT (self));
}

void
gtk_data_viewer_load_value (GtkDataViewer *self,
                            const GValue  *value)
{
  gboolean was_loading;

  g_return_if_fail (GTK_IS_DATA_VIEWER (self));

  was_loading = gtk_data_viewer_is_loading (self);
  self->loading = LOADING_DONE;

  g_clear_pointer (&self->contents, gtk_widget_unparent);
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (g_type_is_a (G_VALUE_TYPE (value), G_TYPE_STRING))
    {
      self->contents = gtk_label_new (g_value_get_string (value));
      gtk_label_set_wrap (GTK_LABEL (self->contents), TRUE);
      gtk_widget_set_parent (self->contents, GTK_WIDGET (self));
    }
  else if (g_type_is_a (G_VALUE_TYPE (value), GDK_TYPE_PAINTABLE))
    {
      self->contents = gtk_picture_new_for_paintable (g_value_get_object (value));
      gtk_widget_set_size_request (self->contents, 256, 256);
      gtk_widget_set_parent (self->contents, GTK_WIDGET (self));
    }
  else if (g_type_is_a (G_VALUE_TYPE (value), GDK_TYPE_PIXBUF))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      self->contents = gtk_picture_new_for_pixbuf (g_value_get_object (value));
G_GNUC_END_IGNORE_DEPRECATIONS
      gtk_widget_set_size_request (self->contents, 256, 256);
      gtk_widget_set_parent (self->contents, GTK_WIDGET (self));
    }
  else if (g_type_is_a (G_VALUE_TYPE (value), GDK_TYPE_RGBA))
    {
      const GdkRGBA *color = g_value_get_boxed (value);

      self->contents = gtk_color_swatch_new ();
      gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (self->contents), color);
      gtk_widget_set_size_request (self->contents, 48, 32);
      gtk_widget_set_halign (self->contents, GTK_ALIGN_CENTER);
      gtk_widget_set_parent (self->contents, GTK_WIDGET (self));
    }
  else if (g_type_is_a (G_VALUE_TYPE (value), G_TYPE_FILE))
    {
      GFile *file = g_value_get_object (value);

      self->contents = gtk_label_new (g_file_peek_path (file));
      gtk_label_set_ellipsize (GTK_LABEL (self->contents), PANGO_ELLIPSIZE_START);
      gtk_widget_set_halign (self->contents, GTK_ALIGN_CENTER);
      gtk_widget_set_parent (self->contents, GTK_WIDGET (self));
    }
  else if (g_type_is_a (G_VALUE_TYPE (value), GDK_TYPE_FILE_LIST))
    {
      GList *l;

      self->contents = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_widget_set_parent (self->contents, GTK_WIDGET (self));

      for (l = g_value_get_boxed (value); l; l = l->next)
        {
          GFile *file = l->data;
          GtkWidget *label = gtk_label_new (g_file_peek_path (file));
          gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
          gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
          gtk_box_append (GTK_BOX (self->contents), label);
        }
    }
  else
    {
      gtk_data_viewer_load_error (self,
                                  g_error_new (G_IO_ERROR,
                                               G_IO_ERROR_FAILED,
                                               "Cannot display objects of type \"%s\"", G_VALUE_TYPE_NAME (value)));
    }

  if (was_loading)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
}

static void
gtk_data_viewer_load_stream_done (GObject      *source,
                                  GAsyncResult *res,
                                  gpointer      data)
{
  GtkDataViewer *self = data;
  GError *error = NULL;
  GValue value = G_VALUE_INIT;

  if (!gdk_content_deserialize_finish (res, &value, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        gtk_data_viewer_load_error (self, error);
      else
        g_clear_error (&error);

      g_object_unref (self);
      return;
    }

  gtk_data_viewer_load_value (self, &value);
  g_object_unref (self);
  g_value_unset (&value);
}

void
gtk_data_viewer_load_stream (GtkDataViewer *self,
                             GInputStream  *stream,
                             const char    *mime_type)
{
  GdkContentFormats *formats;
  const GType *gtypes;
  gboolean was_loading;

  g_return_if_fail (GTK_IS_DATA_VIEWER (self));
  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (mime_type != NULL);

  was_loading = gtk_data_viewer_is_loading (self);
  self->loading = LOADING_INTERNALLY;
  if (self->cancellable == NULL)
    self->cancellable = g_cancellable_new ();

  formats = gdk_content_formats_new (&mime_type, 1);
  formats = gdk_content_formats_union_deserialize_gtypes (formats);
  gtypes = gdk_content_formats_get_gtypes (formats, NULL);
  if (gtypes)
    {
      gdk_content_deserialize_async (stream,
                                     mime_type,
                                     gtypes[0],
                                     G_PRIORITY_DEFAULT,
                                     self->cancellable,
                                     gtk_data_viewer_load_stream_done,
                                     g_object_ref (self));

      if (!was_loading)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
    }
  else
    {
      gtk_data_viewer_load_error (self,
                                  g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                               "Cannot display data of type \"%s\"", mime_type));
    }

  gdk_content_formats_unref (formats);
}

void
gtk_data_viewer_load_error (GtkDataViewer *self,
                            GError        *error)
{
  gboolean was_loading;

  g_return_if_fail (GTK_IS_DATA_VIEWER (self));

  was_loading = gtk_data_viewer_is_loading (self);
  self->loading = LOADING_FAILED;

  g_clear_pointer (&self->contents, gtk_widget_unparent);
  g_clear_error (&self->error);
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->error = error;
  self->contents = gtk_label_new (error->message);
  gtk_widget_add_css_class (self->contents, "error");
  gtk_widget_set_halign (self->contents, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (self->contents, GTK_ALIGN_CENTER);
  gtk_widget_set_parent (self->contents, GTK_WIDGET (self));

  if (was_loading)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
}

