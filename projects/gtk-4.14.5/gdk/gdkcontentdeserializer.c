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

#include "gdkcontentdeserializer.h"

#include "gdkcontentformats.h"
#include "filetransferportalprivate.h"
#include "gdktexture.h"
#include "gdkrgbaprivate.h"
#include "gdkprivate.h"
#include "loaders/gdkpngprivate.h"
#include "loaders/gdktiffprivate.h"

#include <gdk-pixbuf/gdk-pixbuf.h>


/**
 * GdkContentDeserializer:
 *
 * A `GdkContentDeserializer` is used to deserialize content received via
 * inter-application data transfers.
 *
 * The `GdkContentDeserializer` transforms serialized content that is
 * identified by a mime type into an object identified by a GType.
 *
 * GTK provides serializers and deserializers for common data types
 * such as text, colors, images or file lists. To register your own
 * deserialization functions, use [func@content_register_deserializer].
 *
 * Also see [class@Gdk.ContentSerializer].
 */

typedef struct _Deserializer Deserializer;

struct _Deserializer
{
  const char *                    mime_type; /* interned */
  GType                           type;
  GdkContentDeserializeFunc       deserialize;
  gpointer                        data;
  GDestroyNotify                  notify;
};

GQueue deserializers = G_QUEUE_INIT;

static void init (void);

#define GDK_CONTENT_DESERIALIZER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CONTENT_DESERIALIZER, GdkContentDeserializerClass))
#define GDK_IS_CONTENT_DESERIALIZER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CONTENT_DESERIALIZER))
#define GDK_CONTENT_DESERIALIZER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CONTENT_DESERIALIZER, GdkContentDeserializerClass))

typedef struct _GdkContentDeserializerClass GdkContentDeserializerClass;

struct _GdkContentDeserializer
{
  GObject parent_instance;

  const char *mime_type; /* interned */
  GValue value;
  GInputStream *stream;
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

struct _GdkContentDeserializerClass
{
  GObjectClass parent_class;
};

static gpointer
gdk_content_deserializer_async_result_get_user_data (GAsyncResult *res)
{
  return GDK_CONTENT_DESERIALIZER (res)->callback_data;
}

static GObject *
gdk_content_deserializer_async_result_get_source_object (GAsyncResult *res)
{
  return NULL;
}

static void
gdk_content_deserializer_async_result_iface_init (GAsyncResultIface *iface)
{
  iface->get_user_data = gdk_content_deserializer_async_result_get_user_data;
  iface->get_source_object = gdk_content_deserializer_async_result_get_source_object;
}

G_DEFINE_TYPE_WITH_CODE (GdkContentDeserializer, gdk_content_deserializer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT, gdk_content_deserializer_async_result_iface_init))

static void
gdk_content_deserializer_finalize (GObject *object)
{
  GdkContentDeserializer *deserializer = GDK_CONTENT_DESERIALIZER (object);

  g_value_unset (&deserializer->value);
  g_clear_object (&deserializer->stream);
  g_clear_object (&deserializer->cancellable);
  g_clear_error (&deserializer->error);

  if (deserializer->task_notify)
    deserializer->task_notify (deserializer->task_data);

  G_OBJECT_CLASS (gdk_content_deserializer_parent_class)->finalize (object);
}

static void
gdk_content_deserializer_class_init (GdkContentDeserializerClass *content_deserializer_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (content_deserializer_class);

  object_class->finalize = gdk_content_deserializer_finalize;
}

static void
gdk_content_deserializer_init (GdkContentDeserializer *content_deserializer)
{
}

static void
gdk_content_deserializer_run (const char                *mime_type,
                              GType                      type,
                              GInputStream              *stream,
                              int                        priority,
                              GCancellable              *cancellable,
                              GdkContentDeserializeFunc  deserialize_func,
                              gpointer                   user_data,
                              GAsyncReadyCallback        callback,
                              gpointer                   callback_data)
{
  GdkContentDeserializer *deserializer;

  deserializer = g_object_new (GDK_TYPE_CONTENT_DESERIALIZER, NULL);

  deserializer->mime_type = mime_type;
  g_value_init (&deserializer->value, type);
  deserializer->stream = g_object_ref (stream);
  deserializer->priority = priority;
  if (cancellable)
    deserializer->cancellable = g_object_ref (cancellable);
  deserializer->user_data = user_data;
  deserializer->callback = callback;
  deserializer->callback_data = callback_data;

  deserialize_func (deserializer);
}

/**
 * gdk_content_deserializer_get_mime_type:
 * @deserializer: a `GdkContentDeserializer`
 *
 * Gets the mime type to deserialize from.
 *
 * Returns: (transfer none): the mime type for the current operation
 */
const char *
gdk_content_deserializer_get_mime_type (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->mime_type;
}

/**
 * gdk_content_deserializer_get_gtype:
 * @deserializer: a `GdkContentDeserializer`
 *
 * Gets the `GType` to create an instance of.
 *
 * Returns: the `GType` for the current operation
 */
GType
gdk_content_deserializer_get_gtype (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), G_TYPE_INVALID);

  return G_VALUE_TYPE (&deserializer->value);
}

/**
 * gdk_content_deserializer_get_value:
 * @deserializer: a `GdkContentDeserializer`
 *
 * Gets the `GValue` to store the deserialized object in.
 *
 * Returns: (transfer none): the `GValue` for the current operation
 */
GValue *
gdk_content_deserializer_get_value (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return &deserializer->value;
}

/**
 * gdk_content_deserializer_get_input_stream:
 * @deserializer: a `GdkContentDeserializer`
 *
 * Gets the input stream for the current operation.
 *
 * This is the stream that was passed to [func@Gdk.content_deserialize_async].
 *
 * Returns: (transfer none): the input stream for the current operation
 */
GInputStream *
gdk_content_deserializer_get_input_stream (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->stream;
}

/**
 * gdk_content_deserializer_get_priority:
 * @deserializer: a `GdkContentDeserializer`
 *
 * Gets the I/O priority for the current operation.
 *
 * This is the priority that was passed to [func@Gdk.content_deserialize_async].
 *
 * Returns: the I/O priority for the current operation
 */
int
gdk_content_deserializer_get_priority (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), G_PRIORITY_DEFAULT);

  return deserializer->priority;
}

/**
 * gdk_content_deserializer_get_cancellable:
 * @deserializer: a `GdkContentDeserializer`
 *
 * Gets the cancellable for the current operation.
 *
 * This is the `GCancellable` that was passed to [func@Gdk.content_deserialize_async].
 *
 * Returns: (transfer none) (nullable): the cancellable for the current operation
 */
GCancellable *
gdk_content_deserializer_get_cancellable (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->cancellable;
}

/**
 * gdk_content_deserializer_get_user_data:
 * @deserializer: a `GdkContentDeserializer`
 *
 * Gets the user data that was passed when the deserializer was registered.
 *
 * Returns: (transfer none): the user data for this deserializer
 */
gpointer
gdk_content_deserializer_get_user_data (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->user_data;
}

/**
 * gdk_content_deserializer_set_task_data:
 * @deserializer: a `GdkContentDeserializer`
 * @data: data to associate with this operation
 * @notify: destroy notify for @data
 *
 * Associate data with the current deserialization operation.
 */
void
gdk_content_deserializer_set_task_data (GdkContentDeserializer *deserializer,
                                        gpointer                data,
                                        GDestroyNotify          notify)
{
  g_return_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer));

  if (deserializer->task_notify)
    deserializer->task_notify (deserializer->task_data);

  deserializer->task_data = data;
  deserializer->task_notify = notify;
}

/**
 * gdk_content_deserializer_get_task_data:
 * @deserializer: a `GdkContentDeserializer`
 *
 * Gets the data that was associated with the current operation.
 *
 * See [method@Gdk.ContentDeserializer.set_task_data].
 *
 * Returns: (transfer none): the task data for @deserializer
 */
gpointer
gdk_content_deserializer_get_task_data (GdkContentDeserializer *deserializer)
{
  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer), NULL);

  return deserializer->task_data;
}

static gboolean
gdk_content_deserializer_emit_callback (gpointer data)
{
  GdkContentDeserializer *deserializer = data;

  if (deserializer->callback)
    {
      deserializer->callback (NULL, G_ASYNC_RESULT (deserializer), deserializer->callback_data);
    }

  return G_SOURCE_REMOVE;
}

/**
 * gdk_content_deserializer_return_success:
 * @deserializer: a `GdkContentDeserializer`
 *
 * Indicate that the deserialization has been successfully completed.
 */
void
gdk_content_deserializer_return_success (GdkContentDeserializer *deserializer)
{
  g_return_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer));
  g_return_if_fail (!deserializer->returned);
  guint source_id;

  deserializer->returned = TRUE;
  source_id = g_idle_add_full (deserializer->priority,
                               gdk_content_deserializer_emit_callback,
                               deserializer,
                               g_object_unref);
  gdk_source_set_static_name_by_id (source_id, "[gtk] gdk_content_deserializer_emit_callback");
  /* NB: the idle will destroy our reference */
}

/**
 * gdk_content_deserializer_return_error:
 * @deserializer: a `GdkContentDeserializer`
 * @error: (transfer full): a `GError`
 *
 * Indicate that the deserialization has ended with an error.
 *
 * This function consumes @error.
 */
void
gdk_content_deserializer_return_error (GdkContentDeserializer *deserializer,
                                       GError                 *error)
{
  g_return_if_fail (GDK_IS_CONTENT_DESERIALIZER (deserializer));
  g_return_if_fail (!deserializer->returned);
  g_return_if_fail (error != NULL);

  deserializer->error = error;
  /* FIXME: naming */
  gdk_content_deserializer_return_success (deserializer);
}

/**
 * gdk_content_register_deserializer:
 * @mime_type: the mime type which the function can deserialize from
 * @type: the type of objects that the function creates
 * @deserialize: the callback
 * @data: data that @deserialize can access
 * @notify: destroy notify for @data
 *
 * Registers a function to deserialize object of a given type.
 */
void
gdk_content_register_deserializer (const char                *mime_type,
                                   GType                      type,
                                   GdkContentDeserializeFunc  deserialize,
                                   gpointer                   data,
                                   GDestroyNotify             notify)
{
  Deserializer *deserializer;

  g_return_if_fail (mime_type != NULL);
  g_return_if_fail (deserialize != NULL);

  deserializer = g_new0 (Deserializer, 1);

  deserializer->mime_type = g_intern_string (mime_type);
  deserializer->type = type;
  deserializer->deserialize = deserialize;
  deserializer->data = data;
  deserializer->notify = notify;

  g_queue_push_tail (&deserializers, deserializer);
}

static Deserializer *
lookup_deserializer (const char *mime_type,
                     GType       type)
{
  GList *l;

  g_return_val_if_fail (mime_type != NULL, NULL);

  init ();

  mime_type = g_intern_string (mime_type);

  for (l = g_queue_peek_head_link (&deserializers); l; l = l->next)
    {
      Deserializer *deserializer = l->data;

      if (deserializer->mime_type == mime_type &&
          deserializer->type == type)
        return deserializer;
    }

  return NULL;
}

/**
 * gdk_content_formats_union_deserialize_gtypes:
 * @formats: (transfer full): a `GdkContentFormats`
 *
 * Add GTypes for mime types in @formats for which deserializers are
 * registered.
 *
 * Return: a new `GdkContentFormats`
 */
GdkContentFormats *
gdk_content_formats_union_deserialize_gtypes (GdkContentFormats *formats)
{
  GdkContentFormatsBuilder *builder;
  GList *l;

  g_return_val_if_fail (formats != NULL, NULL);

  init ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, formats);

  for (l = g_queue_peek_head_link (&deserializers); l; l = l->next)
    {
      Deserializer *deserializer = l->data;

      if (gdk_content_formats_contain_mime_type (formats, deserializer->mime_type))
        gdk_content_formats_builder_add_gtype (builder, deserializer->type);
    }

  gdk_content_formats_unref (formats);

  return gdk_content_formats_builder_free_to_formats (builder);
}

/**
 * gdk_content_formats_union_deserialize_mime_types:
 * @formats: (transfer full): a `GdkContentFormats`
 *
 * Add mime types for GTypes in @formats for which deserializers are
 * registered.
 *
 * Return: a new `GdkContentFormats`
 */
GdkContentFormats *
gdk_content_formats_union_deserialize_mime_types (GdkContentFormats *formats)
{
  GdkContentFormatsBuilder *builder;
  GList *l;

  g_return_val_if_fail (formats != NULL, NULL);

  init ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, formats);

  for (l = g_queue_peek_head_link (&deserializers); l; l = l->next)
    {
      Deserializer *deserializer = l->data;

      if (gdk_content_formats_contain_gtype (formats, deserializer->type))
        gdk_content_formats_builder_add_mime_type (builder, deserializer->mime_type);
    }

  gdk_content_formats_unref (formats);

  return gdk_content_formats_builder_free_to_formats (builder);
}

static void
deserialize_not_found (GdkContentDeserializer *deserializer)
{
  GError *error = g_error_new (G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Could not convert data from %s to %s",
                               gdk_content_deserializer_get_mime_type (deserializer),
                               g_type_name (gdk_content_deserializer_get_gtype (deserializer)));
  gdk_content_deserializer_return_error (deserializer, error);
}

/**
 * gdk_content_deserialize_async:
 * @stream: a `GInputStream` to read the serialized content from
 * @mime_type: the mime type to deserialize from
 * @type: the GType to deserialize from
 * @io_priority: the I/O priority of the operation
 * @cancellable: (nullable): optional `GCancellable` object
 * @callback: (scope async): callback to call when the operation is done
 * @user_data: (closure): data to pass to the callback function
 *
 * Read content from the given input stream and deserialize it, asynchronously.
 *
 * The default I/O priority is %G_PRIORITY_DEFAULT (i.e. 0), and lower numbers
 * indicate a higher priority.
 *
 * When the operation is finished, @callback will be called. You must then
 * call [func@Gdk.content_deserialize_finish] to get the result of the operation.
 */
void
gdk_content_deserialize_async (GInputStream        *stream,
                               const char          *mime_type,
                               GType                type,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  Deserializer *deserializer;

  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (mime_type != NULL);
  g_return_if_fail (G_TYPE_IS_VALUE_TYPE (type));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  deserializer = lookup_deserializer (mime_type, type);

  gdk_content_deserializer_run (mime_type,
                                type,
                                stream,
                                io_priority,
                                cancellable,
                                deserializer ? deserializer->deserialize : deserialize_not_found,
                                deserializer ? deserializer->data : NULL,
                                callback,
                                user_data);
}

/**
 * gdk_content_deserialize_finish:
 * @result: the `GAsyncResult`
 * @value: (out): return location for the result of the operation
 * @error: return location for an error
 *
 * Finishes a content deserialization operation.
 *
 * Returns: %TRUE if the operation was successful. In this case,
 *   @value is set. %FALSE if an error occurred. In this case,
 *   @error is set
 */
gboolean
gdk_content_deserialize_finish (GAsyncResult  *result,
                                GValue        *value,
                                GError       **error)
{
  GdkContentDeserializer *deserializer;

  g_return_val_if_fail (GDK_IS_CONTENT_DESERIALIZER (result), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  deserializer = GDK_CONTENT_DESERIALIZER (result);
  if (G_VALUE_TYPE (value) == 0)
    g_value_init (value, G_VALUE_TYPE (&deserializer->value));
  else
    g_return_val_if_fail (G_VALUE_HOLDS (value, G_VALUE_TYPE (&deserializer->value)), FALSE);

  if (deserializer->error)
    {
      if (error)
        *error = g_error_copy (deserializer->error);
      return FALSE;
    }

  g_value_copy (&deserializer->value, value);
  return TRUE;
}

/*** DESERIALIZERS ***/

static void
pixbuf_deserializer_finish (GObject      *source,
                            GAsyncResult *res,
                            gpointer      deserializer)
{
  GdkPixbuf *pixbuf;
  GValue *value;
  GError *error = NULL;

  pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
  if (pixbuf == NULL)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  value = gdk_content_deserializer_get_value (deserializer);
  if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF))
    {
      g_value_take_object (value, pixbuf);
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
    {
      GdkTexture *texture;
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
      g_value_take_object (value, texture);
    }
  else
    {
      g_assert_not_reached ();
    }
  gdk_content_deserializer_return_success (deserializer);
}

static void
pixbuf_deserializer (GdkContentDeserializer *deserializer)
{
  gdk_pixbuf_new_from_stream_async (gdk_content_deserializer_get_input_stream (deserializer),
				    gdk_content_deserializer_get_cancellable (deserializer),
                                    pixbuf_deserializer_finish,
                                    deserializer);
}

static void
texture_deserializer_finish (GObject      *source,
                             GAsyncResult *result,
                             gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GBytes *bytes;
  GError *error = NULL;
  GdkTexture *texture = NULL;
  gssize written;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (stream));

  texture = gdk_texture_new_from_bytes (bytes, &error);
  g_bytes_unref (bytes);
  if (texture == NULL)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  g_value_take_object (gdk_content_deserializer_get_value (deserializer), texture);
  gdk_content_deserializer_return_success (deserializer);
}

static void
texture_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output;

  output = g_memory_output_stream_new_resizable ();

  g_output_stream_splice_async (output,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE
                                | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                texture_deserializer_finish,
                                deserializer);
  g_object_unref (output);
}

static void
string_deserializer_finish (GObject      *source,
                            GAsyncResult *result,
                            gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;
  gssize written;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }
  else if (written == 0)
    {
      /* Never return NULL, we only return that on error */
      g_value_set_string (gdk_content_deserializer_get_value (deserializer), "");
    }
  else
    {
      GOutputStream *mem_stream = g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (stream));

      /* write a terminating NULL byte */
      if (g_output_stream_write (mem_stream, "", 1, NULL, &error) < 0 ||
          !g_output_stream_close (mem_stream, NULL, &error))
        {
          gdk_content_deserializer_return_error (deserializer, error);
          return;
        }

      g_value_take_string (gdk_content_deserializer_get_value (deserializer),
                           g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (mem_stream)));
    }
  gdk_content_deserializer_return_success (deserializer);
}

static void
string_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output, *filter;
  GCharsetConverter *converter;
  GError *error = NULL;

  converter = g_charset_converter_new ("utf-8",
                                       gdk_content_deserializer_get_user_data (deserializer),
                                       &error);
  if (converter == NULL)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }
  g_charset_converter_set_use_fallback (converter, TRUE);

  output = g_memory_output_stream_new_resizable ();
  filter = g_converter_output_stream_new (output, G_CONVERTER (converter));
  g_object_unref (output);
  g_object_unref (converter);

  g_output_stream_splice_async (filter,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                string_deserializer_finish,
                                deserializer);
  g_object_unref (filter);
}

static void
file_uri_deserializer_finish (GObject      *source,
                              GAsyncResult *result,
                              gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;
  gssize written;
  GValue *value;
  char *str;
  char **uris;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  /* write terminating NULL */
  if (g_output_stream_write (stream, "", 1, NULL, &error) < 0 ||
      !g_output_stream_close (stream, NULL, &error))
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  str = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (stream));
  uris = g_uri_list_extract_uris (str);
  g_free (str);

  value = gdk_content_deserializer_get_value (deserializer);
  if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    {
      if (uris[0] != NULL)
        g_value_take_object (value, g_file_new_for_uri (uris[0]));
    }
  else
    {
      GSList *l = NULL;
      gsize i;

      for (i = 0; uris[i] != NULL; i++)
        l = g_slist_prepend (l, g_file_new_for_uri (uris[i]));

      g_value_take_boxed (value, g_slist_reverse (l));
    }
  g_strfreev (uris);

  gdk_content_deserializer_return_success (deserializer);
}

static void
file_uri_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output;

  output = g_memory_output_stream_new_resizable ();

  g_output_stream_splice_async (output,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                file_uri_deserializer_finish,
                                deserializer);
  g_object_unref (output);
}

static void
color_deserializer_finish (GObject      *source,
                           GAsyncResult *result,
                           gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;
  gssize written;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }
  else if (written == 0)
    {
      GdkRGBA black = GDK_RGBA ("000");

      /* Never return NULL, we only return that on error */
      g_value_set_boxed (gdk_content_deserializer_get_value (deserializer), &black);
    }
  else
    {
      guint16 *data;
      GdkRGBA rgba;

      data = (guint16 *)g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (stream));
      rgba.red = data[0] / 65535.0;
      rgba.green = data[1] / 65535.0;
      rgba.blue = data[2] / 65535.0;
      rgba.alpha = data[3] / 65535.0;

      g_value_set_boxed (gdk_content_deserializer_get_value (deserializer), &rgba);
    }
  gdk_content_deserializer_return_success (deserializer);
}

static void
color_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output;
  guint16 *data;

  data = g_new0 (guint16, 4);
  output = g_memory_output_stream_new (data, 4 * sizeof (guint16), NULL, g_free);

  g_output_stream_splice_async (output,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                color_deserializer_finish,
                                deserializer);
  g_object_unref (output);
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

  gdk_content_register_deserializer ("image/png",
                                     GDK_TYPE_TEXTURE,
                                     texture_deserializer,
                                     NULL,
                                     NULL);
  gdk_content_register_deserializer ("image/tiff",
                                     GDK_TYPE_TEXTURE,
                                     texture_deserializer,
                                     NULL,
                                     NULL);
  gdk_content_register_deserializer ("image/jpeg",
                                     GDK_TYPE_TEXTURE,
                                     texture_deserializer,
                                     NULL,
                                     NULL);


  formats = gdk_pixbuf_get_formats ();

  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;
      char **mimes, **m;
      char *name;

      name = gdk_pixbuf_format_get_name (fmt);
      mimes = gdk_pixbuf_format_get_mime_types (fmt);
      for (m = mimes; *m; m++)
        {
          /* Turning pngs, jpegs and tiffs into textures is handled above */
          if (!g_str_equal (name, "png") &&
              !g_str_equal (name, "jpeg") &&
              !g_str_equal (name, "tiff"))
            gdk_content_register_deserializer (*m,
                                               GDK_TYPE_TEXTURE,
                                               pixbuf_deserializer,
                                               NULL,
                                               NULL);
          gdk_content_register_deserializer (*m,
                                             GDK_TYPE_PIXBUF,
                                             pixbuf_deserializer,
                                             NULL,
                                             NULL);
        }
      g_strfreev (mimes);
      g_free (name);
    }

  g_slist_free (formats);

#if defined(G_OS_UNIX) && !defined(__APPLE__)
  file_transfer_portal_register ();
#endif

  gdk_content_register_deserializer ("text/uri-list",
                                     GDK_TYPE_FILE_LIST,
                                     file_uri_deserializer,
                                     NULL,
                                     NULL);

  gdk_content_register_deserializer ("text/uri-list",
                                     G_TYPE_FILE,
                                     file_uri_deserializer,
                                     NULL,
                                     NULL);

  gdk_content_register_deserializer ("text/plain;charset=utf-8",
                                     G_TYPE_STRING,
                                     string_deserializer,
                                     (gpointer) "utf-8",
                                     NULL);
  if (!g_get_charset (&charset))
    {
      char *mime = g_strdup_printf ("text/plain;charset=%s", charset);

      gdk_content_register_deserializer (mime,
                                         G_TYPE_STRING,
                                         string_deserializer,
                                         (gpointer) charset,
                                         g_free);
      g_free (mime);
    }
  gdk_content_register_deserializer ("text/plain",
                                     G_TYPE_STRING,
                                     string_deserializer,
                                     (gpointer) "ASCII",
                                     NULL);

  gdk_content_register_deserializer ("application/x-color",
                                     GDK_TYPE_RGBA,
                                     color_deserializer,
                                     NULL,
                                     NULL);
}

