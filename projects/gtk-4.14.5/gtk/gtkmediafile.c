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

#include "gtkmediafileprivate.h"

#include "gtkdebug.h"
#include "gtkprivate.h"
#include <glib/gi18n-lib.h>
#include "gtkmodulesprivate.h"
#include "gtknomediafileprivate.h"

/**
 * GtkMediaFile:
 *
 * `GtkMediaFile` implements `GtkMediaStream` for files.
 *
 * This provides a simple way to play back video files with GTK.
 *
 * GTK provides a GIO extension point for `GtkMediaFile` implementations
 * to allow for external implementations using various media frameworks.
 *
 * GTK itself includes an implementation using GStreamer.
 */

typedef struct _GtkMediaFilePrivate GtkMediaFilePrivate;

struct _GtkMediaFilePrivate
{
  GFile *file;
  GInputStream *input_stream;
};

enum {
  PROP_0,
  PROP_FILE,
  PROP_INPUT_STREAM,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkMediaFile, gtk_media_file, GTK_TYPE_MEDIA_STREAM,
                                  G_ADD_PRIVATE (GtkMediaFile))

#define GTK_MEDIA_FILE_WARN_NOT_IMPLEMENTED_METHOD(obj,method) \
  g_critical ("Media file of type '%s' does not implement GtkMediaFile::" # method, G_OBJECT_TYPE_NAME (obj))

static void
gtk_media_file_default_open (GtkMediaFile *self)
{
  GTK_MEDIA_FILE_WARN_NOT_IMPLEMENTED_METHOD (self, open);
}

static void
gtk_media_file_default_close (GtkMediaFile *self)
{
  gtk_media_stream_stream_unprepared (GTK_MEDIA_STREAM (self));
}

static void
gtk_media_file_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)

{
  GtkMediaFile *self = GTK_MEDIA_FILE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gtk_media_file_set_file (self, g_value_get_object (value));
      break;

    case PROP_INPUT_STREAM:
      gtk_media_file_set_input_stream (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_media_file_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkMediaFile *self = GTK_MEDIA_FILE (object);
  GtkMediaFilePrivate *priv = gtk_media_file_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, priv->file);
      break;

    case PROP_INPUT_STREAM:
      g_value_set_object (value, priv->input_stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_media_file_dispose (GObject *object)
{
  GtkMediaFile *self = GTK_MEDIA_FILE (object);
  GtkMediaFilePrivate *priv = gtk_media_file_get_instance_private (self);

  g_clear_object (&priv->file);
  g_clear_object (&priv->input_stream);

  G_OBJECT_CLASS (gtk_media_file_parent_class)->dispose (object);
}

static void
gtk_media_file_class_init (GtkMediaFileClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  class->open = gtk_media_file_default_open;
  class->close = gtk_media_file_default_close;

  gobject_class->set_property = gtk_media_file_set_property;
  gobject_class->get_property = gtk_media_file_get_property;
  gobject_class->dispose = gtk_media_file_dispose;

  /**
   * GtkMediaFile:file: (attributes org.gtk.Property.get=gtk_media_file_get_file org.gtk.Property.set=gtk_media_file_set_file)
   *
   * The file being played back or %NULL if not playing a file.
   */
  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMediaFile:input-stream: (attributes org.gtk.Property.get=gtk_media_file_get_input_stream org.gtk.Property.set=gtk_media_file_set_input_stream)
   *
   * The stream being played back or %NULL if not playing a stream.
   *
   * This is %NULL when playing a file.
   */
  properties[PROP_INPUT_STREAM] =
    g_param_spec_object ("input-stream", NULL, NULL,
                         G_TYPE_INPUT_STREAM,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_media_file_init (GtkMediaFile *self)
{
}

GIOExtension *
gtk_media_file_get_extension (void)
{
  const char *extension_name;
  GIOExtension *e;
  GIOExtensionPoint *ep;

  GTK_DEBUG (MODULES, "Looking up MediaFile extension");

  ep = g_io_extension_point_lookup (GTK_MEDIA_FILE_EXTENSION_POINT_NAME);
  e = NULL;

  extension_name = g_getenv ("GTK_MEDIA");
  if (extension_name)
    {
      if (g_str_equal (extension_name, "help"))
        {
          GList *l;

          g_print ("Supported arguments for GTK_MEDIA environment variable:\n");

          for (l = g_io_extension_point_get_extensions (ep); l; l = l->next)
            {
              e = l->data;

              g_print ("%10s - %d\n", g_io_extension_get_name (e), g_io_extension_get_priority (e));
            }

          e = NULL;
        }
      else
        {
          e = g_io_extension_point_get_extension_by_name (ep, extension_name);
          if (e == NULL)
            {
              g_warning ("Media extension \"%s\" from GTK_MEDIA environment variable not found.", extension_name);
            }
        }
    }

  if (e == NULL)
    {
      GList *l = g_io_extension_point_get_extensions (ep);

      if (l == NULL)
        {
          g_error ("GTK was run without any GtkMediaFile extension being present. This must not happen.");
        }

      e = l->data;
    }

  return e;
}

static GType
gtk_media_file_get_impl_type (void)
{
  static GType impl_type = G_TYPE_NONE;
  GIOExtension *e;

  if (G_LIKELY (impl_type != G_TYPE_NONE))
    return impl_type;

  e = gtk_media_file_get_extension ();
  impl_type = g_io_extension_get_type (e);

  GTK_DEBUG (MODULES, "Using %s from \"%s\" extension",
             g_type_name (impl_type), g_io_extension_get_name (e));

  return impl_type;
}

/**
 * gtk_media_file_new:
 *
 * Creates a new empty media file.
 *
 * Returns: (type Gtk.MediaFile): a new `GtkMediaFile`
 **/
GtkMediaStream *
gtk_media_file_new (void)
{
  return g_object_new (gtk_media_file_get_impl_type (), NULL);
}

/**
 * gtk_media_file_new_for_filename:
 * @filename: (type filename): filename to open
 *
 * Creates a new media file for the given filename.
 *
 * This is a utility function that converts the given @filename
 * to a `GFile` and calls [ctor@Gtk.MediaFile.new_for_file].
 *
 * Returns: (type Gtk.MediaFile): a new `GtkMediaFile` playing @filename
 */
GtkMediaStream *
gtk_media_file_new_for_filename (const char *filename)
{
  GtkMediaStream *result;
  GFile *file;

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  result = gtk_media_file_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

/**
 * gtk_media_file_new_for_resource:
 * @resource_path: resource path to open
 *
 * Creates a new new media file for the given resource.
 *
 * This is a utility function that converts the given @resource
 * to a `GFile` and calls [ctor@Gtk.MediaFile.new_for_file].
 *
 * Returns: (type Gtk.MediaFile): a new `GtkMediaFile` playing @resource_path
 */
GtkMediaStream *
gtk_media_file_new_for_resource (const char *resource_path)
{
  GtkMediaStream *result;
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

  result = gtk_media_file_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

/**
 * gtk_media_file_new_for_file:
 * @file: The file to play
 *
 * Creates a new media file to play @file.
 *
 * Returns: (type Gtk.MediaFile): a new `GtkMediaFile` playing @file
 */
GtkMediaStream *
gtk_media_file_new_for_file (GFile *file)
{
  g_return_val_if_fail (file == NULL || G_IS_FILE (file), NULL);

  return g_object_new (gtk_media_file_get_impl_type (),
                       "file", file,
                       NULL);
}

/**
 * gtk_media_file_new_for_input_stream:
 * @stream: The stream to play
 *
 * Creates a new media file to play @stream.
 *
 * If you want the resulting media to be seekable,
 * the stream should implement the `GSeekable` interface.
 *
 * Returns: (type Gtk.MediaFile): a new `GtkMediaFile`
 */
GtkMediaStream *
gtk_media_file_new_for_input_stream (GInputStream *stream)
{
  g_return_val_if_fail (stream == NULL || G_IS_INPUT_STREAM (stream), NULL);

  return g_object_new (gtk_media_file_get_impl_type (),
                       "input-stream", stream,
                       NULL);
}

static gboolean
gtk_media_file_is_open (GtkMediaFile *self)
{
  GtkMediaFilePrivate *priv = gtk_media_file_get_instance_private (self);

  return priv->file || priv->input_stream;
}

/**
 * gtk_media_file_clear:
 * @self: a `GtkMediaFile`
 *
 * Resets the media file to be empty.
 */
void
gtk_media_file_clear (GtkMediaFile *self)
{
  GtkMediaFilePrivate *priv = gtk_media_file_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_FILE (self));

  if (!gtk_media_file_is_open (self))
    return;

  GTK_MEDIA_FILE_GET_CLASS (self)->close (self);

  if (priv->input_stream)
    {
      g_clear_object (&priv->input_stream);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INPUT_STREAM]);
    }
  if (priv->file)
    {
      g_clear_object (&priv->file);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);
    }
}

/**
 * gtk_media_file_set_filename:
 * @self: a `GtkMediaFile`
 * @filename: (type filename) (nullable): name of file to play
 *
 * Sets the `GtkMediaFile to play the given file.
 *
 * This is a utility function that converts the given @filename
 * to a `GFile` and calls [method@Gtk.MediaFile.set_file].
 **/
void
gtk_media_file_set_filename (GtkMediaFile *self,
                             const char   *filename)
{
  GFile *file;

  g_return_if_fail (GTK_IS_MEDIA_FILE (self));

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  gtk_media_file_set_file (self, file);

  if (file)
    g_object_unref (file);
}

/**
 * gtk_media_file_set_resource:
 * @self: a `GtkMediaFile`
 * @resource_path: (nullable): path to resource to play
 *
 * Sets the `GtkMediaFile to play the given resource.
 *
 * This is a utility function that converts the given @resource_path
 * to a `GFile` and calls [method@Gtk.MediaFile.set_file].
 */
void
gtk_media_file_set_resource (GtkMediaFile *self,
                             const char   *resource_path)
{
  GFile *file;

  g_return_if_fail (GTK_IS_MEDIA_FILE (self));

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


  gtk_media_file_set_file (self, file);

  if (file)
    g_object_unref (file);
}

/**
 * gtk_media_file_set_file: (attributes org.gtk.Method.set_property=file)
 * @self: a `GtkMediaFile`
 * @file: (nullable): the file to play
 *
 * Sets the `GtkMediaFile` to play the given file.
 *
 * If any file is still playing, stop playing it.
 */
void
gtk_media_file_set_file (GtkMediaFile *self,
                         GFile        *file)
{
  GtkMediaFilePrivate *priv = gtk_media_file_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_FILE (self));
  g_return_if_fail (file == NULL || G_IS_FILE (file));

  if (file)
    g_object_ref (file);

  g_object_freeze_notify (G_OBJECT (self));

  gtk_media_file_clear (self);

  if (file)
    {
      priv->file = file;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);

      GTK_MEDIA_FILE_GET_CLASS (self)->open (self);
    }

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_media_file_get_file: (attributes org.gtk.Method.get_property=file)
 * @self: a `GtkMediaFile`
 *
 * Returns the file that @self is currently playing from.
 *
 * When @self is not playing or not playing from a file,
 * %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The currently playing file
 */
GFile *
gtk_media_file_get_file (GtkMediaFile *self)
{
  GtkMediaFilePrivate *priv = gtk_media_file_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_FILE (self), NULL);

  return priv->file;
}

/**
 * gtk_media_file_set_input_stream: (attributes org.gtk.Method.set_property=input-stream)
 * @self: a `GtkMediaFile`
 * @stream: (nullable): the stream to play from
 *
 * Sets the `GtkMediaFile` to play the given stream.
 *
 * If anything is still playing, stop playing it.
 *
 * Full control about the @stream is assumed for the duration of
 * playback. The stream will not be closed.
 */
void
gtk_media_file_set_input_stream (GtkMediaFile *self,
                                 GInputStream *stream)
{
  GtkMediaFilePrivate *priv = gtk_media_file_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_FILE (self));
  g_return_if_fail (stream == NULL || G_IS_INPUT_STREAM (stream));

  if (stream)
    g_object_ref (stream);

  g_object_freeze_notify (G_OBJECT (self));

  gtk_media_file_clear (self);

  if (stream)
    {
      priv->input_stream = stream;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INPUT_STREAM]);

      GTK_MEDIA_FILE_GET_CLASS (self)->open (self);
    }

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_media_file_get_input_stream: (attributes org.gtk.Method.get_property=input-stream)
 * @self: a `GtkMediaFile`
 *
 * Returns the stream that @self is currently playing from.
 *
 * When @self is not playing or not playing from a stream,
 * %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The currently playing stream
 */
GInputStream *
gtk_media_file_get_input_stream (GtkMediaFile *self)
{
  GtkMediaFilePrivate *priv = gtk_media_file_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_FILE (self), NULL);

  return priv->input_stream;
}

void
gtk_media_file_extension_init (void)
{
  GIOExtensionPoint *ep;
  GIOModuleScope *scope;
  char **paths;
  int i;

  GTK_DEBUG (MODULES, "Registering extension point %s", GTK_MEDIA_FILE_EXTENSION_POINT_NAME);

  ep = g_io_extension_point_register (GTK_MEDIA_FILE_EXTENSION_POINT_NAME);
  g_io_extension_point_set_required_type (ep, GTK_TYPE_MEDIA_FILE);

  g_type_ensure (GTK_TYPE_NO_MEDIA_FILE);

  scope = g_io_module_scope_new (G_IO_MODULE_SCOPE_BLOCK_DUPLICATES);

  paths = _gtk_get_module_path ("media");
  for (i = 0; paths[i]; i++)
    {
      GTK_DEBUG (MODULES, "Scanning io modules in %s", paths[i]);
      g_io_modules_scan_all_in_directory_with_scope (paths[i], scope);
    }
  g_strfreev (paths);

  g_io_module_scope_free (scope);

  if (GTK_DEBUG_CHECK (MODULES))
    {
      GList *list, *l;

      list = g_io_extension_point_get_extensions (ep);
      for (l = list; l; l = l->next)
        {
          GIOExtension *ext = l->data;
          g_print ("extension: %s: type %s\n",
                   g_io_extension_get_name (ext),
                   g_type_name (g_io_extension_get_type (ext)));
        }
    }

  /* If the env var is given, check at startup that things actually work */
  if (g_getenv ("GTK_MEDIA"))
    gtk_media_file_get_extension ();
}
