/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Benjamin Otte
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

#include "config.h"

#include <gio/gio.h>

#include "gdkcontentserializer.h"

#include "gdkcontentformats.h"
#include "deprecated/gdkpixbuf.h"
#include "filetransferportalprivate.h"
#include "gdktextureprivate.h"
#include "gdkrgba.h"
#include "loaders/gdkpngprivate.h"
#include "loaders/gdktiffprivate.h"
#include "loaders/gdkjpegprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdkprivate.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>


/**
 * GdkContentSerializer:
 *
 * A `GdkContentSerializer` is used to serialize content for
 * inter-application data transfers.
 *
 * The `GdkContentSerializer` transforms an object that is identified
 * by a GType into a serialized form (i.e. a byte stream) that is
 * identified by a mime type.
 *
 * GTK provides serializers and deserializers for common data types
 * such as text, colors, images or file lists. To register your own
 * serialization functions, use [func@Gdk.content_register_serializer].
 *
 * Also see [class@Gdk.ContentDeserializer].
 */

typedef struct _Serializer Serializer;

struct _Serializer
{
  const char *                    mime_type; /* interned */
  GType                           type;
  GdkContentSerializeFunc         serialize;
  gpointer                        data;
  GDestroyNotify                  notify;
};

GQueue serializers = G_QUEUE_INIT;

static void init (void);

#define GDK_CONTENT_SERIALIZER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CONTENT_SERIALIZER, GdkContentSerializerClass))
#define GDK_IS_CONTENT_SERIALIZER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CONTENT_SERIALIZER))
#define GDK_CONTENT_SERIALIZER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CONTENT_SERIALIZER, GdkContentSerializerClass))

typedef struct _GdkContentSerializerClass GdkContentSerializerClass;

struct _GdkContentSerializer
{
  GObject parent_instance;

  const char *mime_type; /* interned */
  GValue value;
  GOutputStream *stream;
  int priority;
  gboolean returned;
  GCancellable *cancellable;
  gpointer user_data;
  GAsyncReadyCallback callback;
  gpointer callback_data;

  gpointer task_data;
  GDestroyNotify task_notify;

  GError *error;
};

struct _GdkContentSerializerClass
{
  GObjectClass parent_class;
};

static gpointer
gdk_content_serializer_async_result_get_user_data (GAsyncResult *res)
{
  return GDK_CONTENT_SERIALIZER (res)->callback_data;
}

static GObject *
gdk_content_serializer_async_result_get_source_object (GAsyncResult *res)
{
  return NULL;
}

static void
gdk_content_serializer_async_result_iface_init (GAsyncResultIface *iface)
{
  iface->get_user_data = gdk_content_serializer_async_result_get_user_data;
  iface->get_source_object = gdk_content_serializer_async_result_get_source_object;
}

G_DEFINE_TYPE_WITH_CODE (GdkContentSerializer, gdk_content_serializer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT, gdk_content_serializer_async_result_iface_init))

static void
gdk_content_serializer_finalize (GObject *object)
{
  GdkContentSerializer *serializer = GDK_CONTENT_SERIALIZER (object);

  g_value_unset (&serializer->value);
  g_clear_object (&serializer->stream);
  g_clear_object (&serializer->cancellable);
  g_clear_error (&serializer->error);

  if (serializer->task_notify)
    serializer->task_notify (serializer->task_data);

  G_OBJECT_CLASS (gdk_content_serializer_parent_class)->finalize (object);
}

static void
gdk_content_serializer_class_init (GdkContentSerializerClass *content_serializer_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (content_serializer_class);

  object_class->finalize = gdk_content_serializer_finalize;
}

static void
gdk_content_serializer_init (GdkContentSerializer *content_serializer)
{
}

static void
gdk_content_serializer_run (const char              *mime_type,
                            const GValue            *value,
                            GOutputStream           *stream,
                            int                      priority,
                            GCancellable            *cancellable,
                            GdkContentSerializeFunc  serialize_func,
                            gpointer                 user_data,
                            GAsyncReadyCallback      callback,
                            gpointer                 callback_data)
{
  GdkContentSerializer *serializer;

  serializer = g_object_new (GDK_TYPE_CONTENT_SERIALIZER, NULL);

  serializer->mime_type = mime_type;
  g_value_init (&serializer->value, G_VALUE_TYPE (value));
  g_value_copy (value, &serializer->value);
  serializer->stream = g_object_ref (stream);
  serializer->priority = priority;
  if (cancellable)
    serializer->cancellable = g_object_ref (cancellable);
  serializer->user_data = user_data;
  serializer->callback = callback;
  serializer->callback_data = callback_data;

  serialize_func (serializer);
}

/**
 * gdk_content_serializer_get_mime_type:
 * @serializer: a `GdkContentSerializer`
 *
 * Gets the mime type to serialize to.
 *
 * Returns: (transfer none): the mime type for the current operation
 */
const char *
gdk_content_serializer_get_mime_type (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->mime_type;
}

/**
 * gdk_content_serializer_get_gtype:
 * @serializer: a `GdkContentSerializer`
 *
 * Gets the `GType` to of the object to serialize.
 *
 * Returns: the `GType` for the current operation
 */
GType
gdk_content_serializer_get_gtype (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), G_TYPE_INVALID);

  return G_VALUE_TYPE (&serializer->value);
}

/**
 * gdk_content_serializer_get_value:
 * @serializer: a `GdkContentSerializer`
 *
 * Gets the `GValue` to read the object to serialize from.
 *
 * Returns: (transfer none): the `GValue` for the current operation
 */
const GValue *
gdk_content_serializer_get_value (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return &serializer->value;
}

/**
 * gdk_content_serializer_get_output_stream:
 * @serializer: a `GdkContentSerializer`
 *
 * Gets the output stream for the current operation.
 *
 * This is the stream that was passed to [func@content_serialize_async].
 *
 * Returns: (transfer none): the output stream for the current operation
 */
GOutputStream *
gdk_content_serializer_get_output_stream (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->stream;
}

/**
 * gdk_content_serializer_get_priority:
 * @serializer: a `GdkContentSerializer`
 *
 * Gets the I/O priority for the current operation.
 *
 * This is the priority that was passed to [func@content_serialize_async].
 *
 * Returns: the I/O priority for the current operation
 */
int
gdk_content_serializer_get_priority (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), G_PRIORITY_DEFAULT);

  return serializer->priority;
}

/**
 * gdk_content_serializer_get_cancellable:
 * @serializer: a `GdkContentSerializer`
 *
 * Gets the cancellable for the current operation.
 *
 * This is the `GCancellable` that was passed to [func@content_serialize_async].
 *
 * Returns: (transfer none) (nullable): the cancellable for the current operation
 */
GCancellable *
gdk_content_serializer_get_cancellable (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->cancellable;
}

/**
 * gdk_content_serializer_get_user_data:
 * @serializer: a `GdkContentSerializer`
 *
 * Gets the user data that was passed when the serializer was registered.
 *
 * Returns: (transfer none): the user data for this serializer
 */
gpointer
gdk_content_serializer_get_user_data (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->user_data;
}

/**
 * gdk_content_serializer_set_task_data:
 * @serializer: a `GdkContentSerializer`
 * @data: data to associate with this operation
 * @notify: destroy notify for @data
 *
 * Associate data with the current serialization operation.
 */
void
gdk_content_serializer_set_task_data (GdkContentSerializer   *serializer,
                                      gpointer                data,
                                      GDestroyNotify          notify)
{
  g_return_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer));

  if (serializer->task_notify)
    serializer->task_notify (serializer->task_data);

  serializer->task_data = data;
  serializer->task_notify = notify;
}

/**
 * gdk_content_serializer_get_task_data:
 * @serializer: a `GdkContentSerializer`
 *
 * Gets the data that was associated with the current operation.
 *
 * See [method@Gdk.ContentSerializer.set_task_data].
 *
 * Returns: (transfer none): the task data for @serializer
 */
gpointer
gdk_content_serializer_get_task_data (GdkContentSerializer *serializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer), NULL);

  return serializer->task_data;
}

static gboolean
gdk_content_serializer_emit_callback (gpointer data)
{
  GdkContentSerializer *serializer = data;

  if (serializer->callback)
    {
      serializer->callback (NULL, G_ASYNC_RESULT (serializer), serializer->callback_data);
    }

  return G_SOURCE_REMOVE;
}

/**
 * gdk_content_serializer_return_success:
 * @serializer: a `GdkContentSerializer`
 *
 * Indicate that the serialization has been successfully completed.
 */
void
gdk_content_serializer_return_success (GdkContentSerializer *serializer)
{
  g_return_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer));
  g_return_if_fail (!serializer->returned);
  guint source_id;

  serializer->returned = TRUE;
  source_id = g_idle_add_full (serializer->priority,
                               gdk_content_serializer_emit_callback,
                               serializer,
                               g_object_unref);
  gdk_source_set_static_name_by_id (source_id, "[gtk] gdk_content_serializer_emit_callback");
  /* NB: the idle will destroy our reference */
}

/**
 * gdk_content_serializer_return_error:
 * @serializer: a `GdkContentSerializer`
 * @error: (transfer full): a `GError`
 *
 * Indicate that the serialization has ended with an error.
 *
 * This function consumes @error.
 */
void
gdk_content_serializer_return_error (GdkContentSerializer *serializer,
                                     GError               *error)
{
  g_return_if_fail (GDK_IS_CONTENT_SERIALIZER (serializer));
  g_return_if_fail (!serializer->returned);
  g_return_if_fail (error != NULL);

  serializer->error = error;
  /* FIXME: naming */
  gdk_content_serializer_return_success (serializer);
}

/**
 * gdk_content_register_serializer:
 * @type: the type of objects that the function can serialize
 * @mime_type: the mime type to serialize to
 * @serialize: the callback
 * @data: data that @serialize can access
 * @notify: destroy notify for @data
 *
 * Registers a function to serialize objects of a given type.
 */
void
gdk_content_register_serializer (GType                    type,
                                 const char              *mime_type,
                                 GdkContentSerializeFunc  serialize,
                                 gpointer                 data,
                                 GDestroyNotify           notify)
{
  Serializer *serializer;

  g_return_if_fail (mime_type != NULL);
  g_return_if_fail (serialize != NULL);

  serializer = g_new0 (Serializer, 1);

  serializer->mime_type = g_intern_string (mime_type);
  serializer->type = type;
  serializer->serialize = serialize;
  serializer->data = data;
  serializer->notify = notify;

  g_queue_push_tail (&serializers, serializer);
}

static Serializer *
lookup_serializer (const char *mime_type,
                   GType       type)
{
  GList *l;

  g_return_val_if_fail (mime_type != NULL, NULL);

  init ();

  mime_type = g_intern_string (mime_type);

  for (l = g_queue_peek_head_link (&serializers); l; l = l->next)
    {
      Serializer *serializer = l->data;

      if (serializer->mime_type == mime_type &&
          serializer->type == type)
        return serializer;
    }

  return NULL;
}

/**
 * gdk_content_formats_union_serialize_gtypes:
 * @formats: (transfer full): a `GdkContentFormats`
 *
 * Add GTypes for the mime types in @formats for which serializers are
 * registered.
 *
 * Return: a new `GdkContentFormats`
 */
GdkContentFormats *
gdk_content_formats_union_serialize_gtypes (GdkContentFormats *formats)
{
  GdkContentFormatsBuilder *builder;
  GList *l;

  g_return_val_if_fail (formats != NULL, NULL);

  init ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, formats);

  for (l = g_queue_peek_head_link (&serializers); l; l = l->next)
    {
      Serializer *serializer = l->data;

      if (gdk_content_formats_contain_mime_type (formats, serializer->mime_type))
        gdk_content_formats_builder_add_gtype (builder, serializer->type);
    }

  gdk_content_formats_unref (formats);

  return gdk_content_formats_builder_free_to_formats (builder);
}

/**
 * gdk_content_formats_union_serialize_mime_types:
 * @formats: (transfer full):  a `GdkContentFormats`
 *
 * Add mime types for GTypes in @formats for which serializers are
 * registered.
 *
 * Return: a new `GdkContentFormats`
 */
GdkContentFormats *
gdk_content_formats_union_serialize_mime_types (GdkContentFormats *formats)
{
  GdkContentFormatsBuilder *builder;
  GList *l;

  g_return_val_if_fail (formats != NULL, NULL);

  init ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, formats);

  for (l = g_queue_peek_head_link (&serializers); l; l = l->next)
    {
      Serializer *serializer = l->data;

      if (gdk_content_formats_contain_gtype (formats, serializer->type))
        gdk_content_formats_builder_add_mime_type (builder, serializer->mime_type);
    }

  gdk_content_formats_unref (formats);

  return gdk_content_formats_builder_free_to_formats (builder);
}

static void
serialize_not_found (GdkContentSerializer *serializer)
{
  GError *error = g_error_new (G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Could not convert data from %s to %s",
                               g_type_name (gdk_content_serializer_get_gtype (serializer)),
                               gdk_content_serializer_get_mime_type (serializer));
  gdk_content_serializer_return_error (serializer, error);
}

/**
 * gdk_content_serialize_async:
 * @stream: a `GOutputStream` to write the serialized content to
 * @mime_type: the mime type to serialize to
 * @value: the content to serialize
 * @io_priority: the I/O priority of the operation
 * @cancellable: (nullable): optional `GCancellable` object
 * @callback: (scope async): callback to call when the operation is done
 * @user_data: (closure): data to pass to the callback function
 *
 * Serialize content and write it to the given output stream, asynchronously.
 *
 * The default I/O priority is %G_PRIORITY_DEFAULT (i.e. 0), and lower numbers
 * indicate a higher priority.
 *
 * When the operation is finished, @callback will be called. You must then
 * call [func@Gdk.content_serialize_finish] to get the result of the operation.
 */
void
gdk_content_serialize_async (GOutputStream       *stream,
                             const char          *mime_type,
                             const GValue        *value,
                             int                  io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  Serializer *serializer;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  serializer = lookup_serializer (mime_type, G_VALUE_TYPE (value));

  gdk_content_serializer_run (mime_type,
                              value,
                              stream,
                              io_priority,
                              cancellable,
                              serializer ? serializer->serialize : serialize_not_found,
                              serializer ? serializer->data : NULL,
                              callback,
                              user_data);
}

/**
 * gdk_content_serialize_finish:
 * @result: the `GAsyncResult`
 * @error: return location for an error
 *
 * Finishes a content serialization operation.
 *
 * Returns: %TRUE if the operation was successful, %FALSE if an
 *   error occurred. In this case, @error is set
 */
gboolean
gdk_content_serialize_finish (GAsyncResult  *result,
                              GError       **error)
{
  GdkContentSerializer *serializer;

  g_return_val_if_fail (GDK_IS_CONTENT_SERIALIZER (result), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  serializer = GDK_CONTENT_SERIALIZER (result);

  if (serializer->error)
    {
      if (error)
        *error = g_error_copy (serializer->error);
      return FALSE;
    }

  return TRUE;
}

/*** SERIALIZERS ***/


static void
pixbuf_serializer_finish (GObject      *source,
                          GAsyncResult *res,
                          gpointer      serializer)
{
  GError *error = NULL;

  if (!gdk_pixbuf_save_to_stream_finish (res, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
pixbuf_serializer (GdkContentSerializer *serializer)
{
  const GValue *value;
  GdkPixbuf *pixbuf;
  const char *name;

  name = gdk_content_serializer_get_user_data (serializer);
  value = gdk_content_serializer_get_value (serializer);

  if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF))
    {
      pixbuf = g_value_dup_object (value);
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
    {
      GdkTexture *texture = g_value_get_object (value);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      pixbuf = gdk_pixbuf_get_from_texture (texture);
G_GNUC_END_IGNORE_DEPRECATIONS
    }
  else
    {
      g_assert_not_reached ();
    }

  gdk_pixbuf_save_to_stream_async (pixbuf,
                                   gdk_content_serializer_get_output_stream (serializer),
                                   name,
                                   gdk_content_serializer_get_cancellable (serializer),
                                   pixbuf_serializer_finish,
                                   serializer,
                                   g_str_equal (name, "png") ? "compression" : NULL, "2",
                                   NULL);
  g_object_unref (pixbuf);
}

static void
texture_serializer_finish (GObject      *source,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  GdkContentSerializer *serializer = GDK_CONTENT_SERIALIZER (source);
  GError *error = NULL;

  if (!g_task_propagate_boolean (G_TASK (res), &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
serialize_texture_in_thread (GTask        *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
  GdkContentSerializer *serializer = source_object;
  const GValue *value;
  GdkTexture *texture;
  GBytes *bytes = NULL;
  GError *error = NULL;
  gboolean result = FALSE;
  GInputStream *input;
  gssize spliced;

  value = gdk_content_serializer_get_value (serializer);
  texture = g_value_get_object (value);

  if (strcmp (gdk_content_serializer_get_mime_type (serializer), "image/png") == 0)
    bytes = gdk_save_png (texture);
  else if (strcmp (gdk_content_serializer_get_mime_type (serializer), "image/tiff") == 0)
    bytes = gdk_save_tiff (texture);
  else if (strcmp (gdk_content_serializer_get_mime_type (serializer), "image/jpeg") == 0)
    bytes = gdk_save_jpeg (texture);
  else
    g_assert_not_reached ();

  input = g_memory_input_stream_new_from_bytes (bytes);
  spliced = g_output_stream_splice (gdk_content_serializer_get_output_stream (serializer),
                                    input,
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                    gdk_content_serializer_get_cancellable (serializer),
                                    &error);
  g_object_unref (input);
  g_bytes_unref (bytes);

  result = spliced != -1;

  if (result)
    g_task_return_boolean (task, result);
  else
    g_task_return_error (task, error);
}

static void
texture_serializer (GdkContentSerializer *serializer)
{
  GTask *task;

  task = g_task_new (serializer,
                     gdk_content_serializer_get_cancellable (serializer),
                     texture_serializer_finish,
                     NULL);
  g_task_run_in_thread (task, serialize_texture_in_thread);
  g_object_unref (task);
}

static void
string_serializer_finish (GObject      *source,
                          GAsyncResult *result,
                          gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (stream, result, NULL, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
string_serializer (GdkContentSerializer *serializer)
{
  GOutputStream *filter;
  GCharsetConverter *converter;
  GError *error = NULL;
  const char *text;

  converter = g_charset_converter_new (gdk_content_serializer_get_user_data (serializer),
                                       "utf-8",
                                       &error);
  if (converter == NULL)
    {
      gdk_content_serializer_return_error (serializer, error);
      return;
    }
  g_charset_converter_set_use_fallback (converter, TRUE);

  filter = g_converter_output_stream_new (gdk_content_serializer_get_output_stream (serializer),
                                          G_CONVERTER (converter));
  g_object_unref (converter);

  text = g_value_get_string (gdk_content_serializer_get_value (serializer));
  if (text == NULL)
    text = "";

  g_output_stream_write_all_async (filter,
                                   text,
                                   strlen (text),
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   string_serializer_finish,
                                   serializer);
  g_object_unref (filter);
}

static void
file_serializer_finish (GObject      *source,
                        GAsyncResult *result,
                        gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (stream, result, NULL, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static char *
file_get_native_uri (GFile *file)
{
  char *path;

  path = g_file_get_path (file);
  if (path != NULL)
    {
      char *uri = g_filename_to_uri (path, NULL, NULL);
      g_free (path);
      return uri;
    }

  return g_file_get_uri (file);
}

static void
file_uri_serializer (GdkContentSerializer *serializer)
{
  GFile *file;
  GString *str;
  const GValue *value;
  char *uri;

  str = g_string_new (NULL);
  value = gdk_content_serializer_get_value (serializer);

  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    {
      file = g_value_get_object (gdk_content_serializer_get_value (serializer));
      if (file)
        {
          uri = file_get_native_uri (file);
          g_string_append (str, uri);
          g_free (uri);
        }
      else
        {
          g_string_append (str, "# GTK does not crash when copying a NULL GFile!");
        }
      g_string_append (str, "\r\n");
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      GSList *l;

      for (l = g_value_get_boxed (value); l; l = l->next)
        {
          uri = file_get_native_uri (l->data);
          g_string_append (str, uri);
          g_free (uri);
          g_string_append (str, "\r\n");
        }
    }

  g_output_stream_write_all_async (gdk_content_serializer_get_output_stream (serializer),
                                   str->str,
                                   str->len,
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   file_serializer_finish,
                                   serializer);
  gdk_content_serializer_set_task_data (serializer, g_string_free (str, FALSE), g_free);
}

static void
file_text_serializer (GdkContentSerializer *serializer)
{
  const GValue *value;
  char *path = NULL;

  value = gdk_content_serializer_get_value (serializer);

  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    {
      GFile *file;

      file = g_value_get_object (value);
      if (file)
        {
          path = g_file_get_path (file);
          if (path == NULL)
            path = g_file_get_uri (file);
        }
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      GString *str;
      GSList *l;

      str = g_string_new (NULL);

      for (l = g_value_get_boxed (value); l; l = l->next)
        {
          path = g_file_get_path (l->data);
          if (path == NULL)
            path = g_file_get_uri (l->data);
          g_string_append (str, path);
          g_free (path);
          if (l->next)
            g_string_append (str, "\n");
        }
      path = g_string_free (str, FALSE);
    }

  g_assert (path != NULL);

  g_output_stream_write_all_async (gdk_content_serializer_get_output_stream (serializer),
                                   path,
                                   strlen (path),
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   file_serializer_finish,
                                   serializer);
  gdk_content_serializer_set_task_data (serializer, path, g_free);
}

static void
color_serializer_finish (GObject      *source,
                         GAsyncResult *result,
                         gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (stream, result, NULL, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
color_serializer (GdkContentSerializer *serializer)
{
  const GValue *value;
  GdkRGBA *rgba;
  guint16 *data;

  value = gdk_content_serializer_get_value (serializer);
  rgba = g_value_get_boxed (value);
  data = g_new0 (guint16, 4);
  if (rgba)
    {
      data[0] = (guint16) (rgba->red * 65535);
      data[1] = (guint16) (rgba->green * 65535);
      data[2] = (guint16) (rgba->blue * 65535);
      data[3] = (guint16) (rgba->alpha * 65535);
    }

  g_output_stream_write_all_async (gdk_content_serializer_get_output_stream (serializer),
                                   data,
                                   4 * sizeof (guint16),
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   color_serializer_finish,
                                   serializer);
  gdk_content_serializer_set_task_data (serializer, data, g_free);
}

static void
init (void)
{
  static gboolean initialized = FALSE;
  GSList *formats, *f;
  const char *charset;

  if (initialized)
    return;

  initialized = TRUE;

  gdk_content_register_serializer (GDK_TYPE_TEXTURE,
                                   "image/png",
                                   texture_serializer,
                                   NULL, NULL);

  gdk_content_register_serializer (GDK_TYPE_TEXTURE,
                                   "image/tiff",
                                   texture_serializer,
                                   NULL, NULL);

  gdk_content_register_serializer (GDK_TYPE_TEXTURE,
                                   "image/jpeg",
                                   texture_serializer,
                                   NULL, NULL);

  formats = gdk_pixbuf_get_formats ();

  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;
      char **mimes, **m;
      char *name;

      if (!gdk_pixbuf_format_is_writable (fmt))
        continue;

      name = gdk_pixbuf_format_get_name (fmt);
      mimes = gdk_pixbuf_format_get_mime_types (fmt);
      for (m = mimes; *m; m++)
        {
          /* Turning textures into pngs, tiffs or jpegs is handled above */
          if (!g_str_equal (name, "png") &&
              !g_str_equal (name, "tiff") &&
              !g_str_equal (name, "jpeg"))
            gdk_content_register_serializer (GDK_TYPE_TEXTURE,
                                             *m,
                                             pixbuf_serializer,
                                             gdk_pixbuf_format_get_name (fmt),
                                             g_free);
          gdk_content_register_serializer (GDK_TYPE_PIXBUF,
                                           *m,
                                           pixbuf_serializer,
                                           gdk_pixbuf_format_get_name (fmt),
                                           g_free);
        }
      g_strfreev (mimes);
      g_free (name);
    }

  g_slist_free (formats);

#if defined(G_OS_UNIX) && !defined(__APPLE__)
  file_transfer_portal_register ();
#endif

  gdk_content_register_serializer (G_TYPE_FILE,
                                   "text/uri-list",
                                   file_uri_serializer,
                                   NULL,
                                   NULL);
  gdk_content_register_serializer (G_TYPE_FILE,
                                   "text/plain;charset=utf-8",
                                   file_text_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_serializer (GDK_TYPE_FILE_LIST,
                                   "text/uri-list",
                                   file_uri_serializer,
                                   NULL,
                                   NULL);
  gdk_content_register_serializer (GDK_TYPE_FILE_LIST,
                                   "text/plain;charset=utf-8",
                                   file_text_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_serializer (G_TYPE_STRING,
                                   "text/plain;charset=utf-8",
                                   string_serializer,
                                   (gpointer) "utf-8",
                                   NULL);
  if (!g_get_charset (&charset))
    {
      char *mime = g_strdup_printf ("text/plain;charset=%s", charset);
      gdk_content_register_serializer (G_TYPE_STRING,
                                       mime,
                                       string_serializer,
                                       (gpointer) charset,
                                       NULL);
      g_free (mime);
    }
  gdk_content_register_serializer (G_TYPE_STRING,
                                   "text/plain",
                                   string_serializer,
                                   (gpointer) "ASCII",
                                   NULL);

  gdk_content_register_serializer (GDK_TYPE_RGBA,
                                   "application/x-color",
                                   color_serializer,
                                   NULL,
                                   NULL);
}

