/* GDK - The GIMP Drawing Kit
 *
 * Copyright (C) 2017,2020 Benjamin Otte <otte@gnome.org>
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

#include "gdkcontentproviderprivate.h"

#include <gobject/gvaluecollector.h>

#include "gdkcontentformats.h"
#include "gdkcontentserializer.h"
#include <glib/gi18n-lib.h>
#include "gdkcontentproviderimpl.h"

#include "gdkprivate.h"

#define GDK_TYPE_CONTENT_PROVIDER_VALUE            (gdk_content_provider_value_get_type ())
#define GDK_CONTENT_PROVIDER_VALUE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CONTENT_PROVIDER_VALUE, GdkContentProviderValue))
#define GDK_IS_CONTENT_PROVIDER_VALUE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_CONTENT_PROVIDER_VALUE))
#define GDK_CONTENT_PROVIDER_VALUE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CONTENT_PROVIDER_VALUE, GdkContentProviderValueClass))
#define GDK_IS_CONTENT_PROVIDER_VALUE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CONTENT_PROVIDER_VALUE))
#define GDK_CONTENT_PROVIDER_VALUE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CONTENT_PROVIDER_VALUE, GdkContentProviderValueClass))

typedef struct _GdkContentProviderValue GdkContentProviderValue;
typedef struct _GdkContentProviderValueClass GdkContentProviderValueClass;

struct _GdkContentProviderValue
{
  GdkContentProvider parent;

  GValue value;
};

struct _GdkContentProviderValueClass
{
  GdkContentProviderClass parent_class;
};

GType gdk_content_provider_value_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GdkContentProviderValue, gdk_content_provider_value, GDK_TYPE_CONTENT_PROVIDER)

static void
gdk_content_provider_value_finalize (GObject *object)
{
  GdkContentProviderValue *content = GDK_CONTENT_PROVIDER_VALUE (object);

  g_value_unset (&content->value);

  G_OBJECT_CLASS (gdk_content_provider_value_parent_class)->finalize (object);
}

static GdkContentFormats *
gdk_content_provider_value_ref_formats (GdkContentProvider *provider)
{
  GdkContentProviderValue *content = GDK_CONTENT_PROVIDER_VALUE (provider);

  return gdk_content_formats_new_for_gtype (G_VALUE_TYPE (&content->value));
}

static gboolean
gdk_content_provider_value_get_value (GdkContentProvider  *provider,
                                      GValue              *value,
                                      GError             **error)
{
  GdkContentProviderValue *content = GDK_CONTENT_PROVIDER_VALUE (provider);

  if (G_VALUE_HOLDS (&content->value, G_VALUE_TYPE (value)))
    {
      g_value_copy (&content->value, value);
      return TRUE;
    }

  return GDK_CONTENT_PROVIDER_CLASS (gdk_content_provider_value_parent_class)->get_value (provider, value, error);
}

static void
gdk_content_provider_value_class_init (GdkContentProviderValueClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  object_class->finalize = gdk_content_provider_value_finalize;

  provider_class->ref_formats = gdk_content_provider_value_ref_formats;
  provider_class->get_value = gdk_content_provider_value_get_value;
}

static void
gdk_content_provider_value_init (GdkContentProviderValue *content)
{
}

/**
 * gdk_content_provider_new_for_value:
 * @value: a `GValue`
 *
 * Create a content provider that provides the given @value.
 *
 * Returns: a new `GdkContentProvider`
 */
GdkContentProvider *
gdk_content_provider_new_for_value (const GValue *value)
{
  GdkContentProviderValue *content;

  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  content = g_object_new (GDK_TYPE_CONTENT_PROVIDER_VALUE, NULL);
  g_value_init (&content->value, G_VALUE_TYPE (value));
  g_value_copy (value, &content->value);
  
  return GDK_CONTENT_PROVIDER (content);
}

/**
 * gdk_content_provider_new_typed:
 * @type: Type of value to follow
 * @...: value
 *
 * Create a content provider that provides the value of the given
 * @type.
 *
 * The value is provided using G_VALUE_COLLECT(), so the same rules
 * apply as when calling g_object_new() or g_object_set().
 *
 * Returns: a new `GdkContentProvider`
 */
GdkContentProvider *
gdk_content_provider_new_typed (GType type,
                                ...)
{
  GdkContentProviderValue *content;
  va_list args;
  char *error;

  content = g_object_new (GDK_TYPE_CONTENT_PROVIDER_VALUE, NULL);

  va_start (args, type);
  G_VALUE_COLLECT_INIT (&content->value, type, args, 0, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error);
      g_free (error);
      /* we purposely leak the value here, it might not be
       * in a sane state if an error condition occurred
       */
    }
  va_end (args);

  return GDK_CONTENT_PROVIDER (content);
}

#define GDK_TYPE_CONTENT_PROVIDER_UNION            (gdk_content_provider_union_get_type ())
#define GDK_CONTENT_PROVIDER_UNION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CONTENT_PROVIDER_UNION, GdkContentProviderUnion))
#define GDK_IS_CONTENT_PROVIDER_UNION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_CONTENT_PROVIDER_UNION))
#define GDK_CONTENT_PROVIDER_UNION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CONTENT_PROVIDER_UNION, GdkContentProviderUnionClass))
#define GDK_IS_CONTENT_PROVIDER_UNION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CONTENT_PROVIDER_UNION))
#define GDK_CONTENT_PROVIDER_UNION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CONTENT_PROVIDER_UNION, GdkContentProviderUnionClass))

typedef struct _GdkContentProviderUnion GdkContentProviderUnion;
typedef struct _GdkContentProviderUnionClass GdkContentProviderUnionClass;

struct _GdkContentProviderUnion
{
  GdkContentProvider parent;

  GdkContentProvider **providers;
  gsize n_providers;
};

struct _GdkContentProviderUnionClass
{
  GdkContentProviderClass parent_class;
};

GType gdk_content_provider_union_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GdkContentProviderUnion, gdk_content_provider_union, GDK_TYPE_CONTENT_PROVIDER)

static void
gdk_content_provider_union_attach_clipboard (GdkContentProvider *provider,
                                             GdkClipboard       *clipboard)
{
  GdkContentProviderUnion *self = GDK_CONTENT_PROVIDER_UNION (provider);
  gsize i;

  for (i = 0; i < self->n_providers; i++)
    gdk_content_provider_attach_clipboard (self->providers[i], clipboard);
}

static void
gdk_content_provider_union_detach_clipboard (GdkContentProvider *provider,
                                             GdkClipboard       *clipboard)
{
  GdkContentProviderUnion *self = GDK_CONTENT_PROVIDER_UNION (provider);
  gsize i;

  for (i = 0; i < self->n_providers; i++)
    gdk_content_provider_detach_clipboard (self->providers[i], clipboard);
}

static GdkContentFormats *
gdk_content_provider_union_ref_formats (GdkContentProvider *provider)
{
  GdkContentProviderUnion *self = GDK_CONTENT_PROVIDER_UNION (provider);
  GdkContentFormatsBuilder *builder;
  gsize i;

  builder = gdk_content_formats_builder_new ();

  for (i = 0; i < self->n_providers; i++)
    {
      GdkContentFormats *formats = gdk_content_provider_ref_formats (self->providers[i]);
      gdk_content_formats_builder_add_formats (builder, formats);
      gdk_content_formats_unref (formats);
    }

  return gdk_content_formats_builder_free_to_formats (builder);
}

static GdkContentFormats *
gdk_content_provider_union_ref_storable_formats (GdkContentProvider *provider)
{
  GdkContentProviderUnion *self = GDK_CONTENT_PROVIDER_UNION (provider);
  GdkContentFormatsBuilder *builder;
  gsize i;

  builder = gdk_content_formats_builder_new ();

  for (i = 0; i < self->n_providers; i++)
    {
      GdkContentFormats *formats = gdk_content_provider_ref_storable_formats (self->providers[i]);
      gdk_content_formats_builder_add_formats (builder, formats);
      gdk_content_formats_unref (formats);
    }

  return gdk_content_formats_builder_free_to_formats (builder);
}

static void
gdk_content_provider_union_write_mime_type_done (GObject      *source_object,
                                                 GAsyncResult *res,
                                                 gpointer      data)
{
  GTask *task = data;
  GError *error = NULL;

  if (!gdk_content_provider_write_mime_type_finish (GDK_CONTENT_PROVIDER (source_object), res, &error))
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }

  g_object_unref (task);
}

static void
gdk_content_provider_union_write_mime_type_async (GdkContentProvider     *provider,
                                                  const char             *mime_type,
                                                  GOutputStream          *stream,
                                                  int                     io_priority,
                                                  GCancellable           *cancellable,
                                                  GAsyncReadyCallback     callback,
                                                  gpointer                user_data)
{
  GdkContentProviderUnion *self = GDK_CONTENT_PROVIDER_UNION (provider);
  GTask *task;
  gsize i;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_content_provider_union_write_mime_type_async);

  for (i = 0; i < self->n_providers; i++)
    {
      GdkContentFormats *formats = gdk_content_provider_ref_formats (self->providers[i]);

      if (gdk_content_formats_contain_mime_type (formats, mime_type))
        {
          gdk_content_provider_write_mime_type_async (self->providers[i],
                                                      mime_type,
                                                      stream,
                                                      io_priority,
                                                      cancellable,
                                                      gdk_content_provider_union_write_mime_type_done,
                                                      task);
          gdk_content_formats_unref (formats);
          return;
        }
      gdk_content_formats_unref (formats);
    }

  g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Cannot provide contents as “%s”"), mime_type);
  g_object_unref (task);
}

static gboolean
gdk_content_provider_union_write_mime_type_finish (GdkContentProvider  *provider,
                                                   GAsyncResult        *result,
                                                   GError             **error)
{
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_content_provider_union_write_mime_type_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
gdk_content_provider_union_get_value (GdkContentProvider  *provider,
                                      GValue              *value,
                                      GError             **error)
{
  GdkContentProviderUnion *self = GDK_CONTENT_PROVIDER_UNION (provider);
  gsize i;

  for (i = 0; i < self->n_providers; i++)
    {
      GError *provider_error = NULL;

      if (gdk_content_provider_get_value (self->providers[i], value, &provider_error))
        return TRUE;

      if (!g_error_matches (provider_error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        {
          g_propagate_error (error, provider_error);
          return FALSE;
        }

      g_clear_error (&provider_error);
    }

  return GDK_CONTENT_PROVIDER_CLASS (gdk_content_provider_union_parent_class)->get_value (provider, value, error);
}

static void
gdk_content_provider_union_finalize (GObject *object)
{
  GdkContentProviderUnion *self = GDK_CONTENT_PROVIDER_UNION (object);
  gsize i;

  for (i = 0; i < self->n_providers; i++)
    {
      g_signal_handlers_disconnect_by_func (self->providers[i], gdk_content_provider_content_changed, self);
      g_object_unref (self->providers[i]);
    }

  g_free (self->providers);

  G_OBJECT_CLASS (gdk_content_provider_union_parent_class)->finalize (object);
}

static void
gdk_content_provider_union_class_init (GdkContentProviderUnionClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  object_class->finalize = gdk_content_provider_union_finalize;

  provider_class->attach_clipboard = gdk_content_provider_union_attach_clipboard;
  provider_class->detach_clipboard = gdk_content_provider_union_detach_clipboard;
  provider_class->ref_formats = gdk_content_provider_union_ref_formats;
  provider_class->ref_storable_formats = gdk_content_provider_union_ref_storable_formats;
  provider_class->write_mime_type_async = gdk_content_provider_union_write_mime_type_async;
  provider_class->write_mime_type_finish = gdk_content_provider_union_write_mime_type_finish;
  provider_class->get_value = gdk_content_provider_union_get_value;
}

static void
gdk_content_provider_union_init (GdkContentProviderUnion *self)
{
}

/**
 * gdk_content_provider_new_union:
 * @providers: (nullable) (array length=n_providers) (transfer full):
 *   The `GdkContentProvider`s to present the union of
 * @n_providers: the number of providers
 *
 * Creates a content provider that represents all the given @providers.
 *
 * Whenever data needs to be written, the union provider will try the given
 * @providers in the given order and the first one supporting a format will
 * be chosen to provide it.
 *
 * This allows an easy way to support providing data in different formats.
 * For example, an image may be provided by its file and by the image
 * contents with a call such as
 * ```c
 * gdk_content_provider_new_union ((GdkContentProvider *[2]) {
 *                                   gdk_content_provider_new_typed (G_TYPE_FILE, file),
 *                                   gdk_content_provider_new_typed (G_TYPE_TEXTURE, texture)
 *                                 }, 2);
 * ```
 *
 * Returns: a new `GdkContentProvider`
 */
GdkContentProvider *
gdk_content_provider_new_union (GdkContentProvider **providers,
                                gsize                n_providers)
{
  GdkContentProviderUnion *result;
  gsize i;

  g_return_val_if_fail (providers != NULL || n_providers == 0, NULL);

  result = g_object_new (GDK_TYPE_CONTENT_PROVIDER_UNION, NULL);

  result->n_providers = n_providers;
  result->providers = g_memdup2 (providers, sizeof (GdkContentProvider *) * n_providers);

  for (i = 0; i < n_providers; i++)
    {
      g_signal_connect_swapped (result->providers[i],
                                "content-changed",
                                G_CALLBACK (gdk_content_provider_content_changed),
                                result);
    }

  return GDK_CONTENT_PROVIDER (result);
}

#define GDK_TYPE_CONTENT_PROVIDER_BYTES            (gdk_content_provider_bytes_get_type ())
#define GDK_CONTENT_PROVIDER_BYTES(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CONTENT_PROVIDER_BYTES, GdkContentProviderBytes))
#define GDK_IS_CONTENT_PROVIDER_BYTES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_CONTENT_PROVIDER_BYTES))
#define GDK_CONTENT_PROVIDER_BYTES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CONTENT_PROVIDER_BYTES, GdkContentProviderBytesClass))
#define GDK_IS_CONTENT_PROVIDER_BYTES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CONTENT_PROVIDER_BYTES))
#define GDK_CONTENT_PROVIDER_BYTES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CONTENT_PROVIDER_BYTES, GdkContentProviderBytesClass))

typedef struct _GdkContentProviderBytes GdkContentProviderBytes;
typedef struct _GdkContentProviderBytesClass GdkContentProviderBytesClass;

struct _GdkContentProviderBytes
{
  GdkContentProvider parent;

  /* interned */const char *mime_type;
  GBytes *bytes;
};

struct _GdkContentProviderBytesClass
{
  GdkContentProviderClass parent_class;
};

GType gdk_content_provider_bytes_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GdkContentProviderBytes, gdk_content_provider_bytes, GDK_TYPE_CONTENT_PROVIDER)

static void
gdk_content_provider_bytes_finalize (GObject *object)
{
  GdkContentProviderBytes *content = GDK_CONTENT_PROVIDER_BYTES (object);

  g_bytes_unref (content->bytes);

  G_OBJECT_CLASS (gdk_content_provider_bytes_parent_class)->finalize (object);
}

static GdkContentFormats *
gdk_content_provider_bytes_ref_formats (GdkContentProvider *provider)
{
  GdkContentProviderBytes *content = GDK_CONTENT_PROVIDER_BYTES (provider);
  GdkContentFormatsBuilder *builder;

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_mime_type (builder, content->mime_type);
  return gdk_content_formats_builder_free_to_formats (builder);
}

static void
gdk_content_provider_bytes_write_mime_type_done (GObject      *stream,
                                                 GAsyncResult *result,
                                                 gpointer      task)
{
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (G_OUTPUT_STREAM (stream),
                                         result,
                                         NULL,
                                         &error))
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }

  g_object_unref (task);
}

static void
gdk_content_provider_bytes_write_mime_type_async (GdkContentProvider     *provider,
                                                  const char             *mime_type,
                                                  GOutputStream          *stream,
                                                  int                     io_priority,
                                                  GCancellable           *cancellable,
                                                  GAsyncReadyCallback     callback,
                                                  gpointer                user_data)
{
  GdkContentProviderBytes *content = GDK_CONTENT_PROVIDER_BYTES (provider);
  GTask *task;

  task = g_task_new (content, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_content_provider_bytes_write_mime_type_async);

  if (mime_type != content->mime_type)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("Cannot provide contents as “%s”"), mime_type);
      g_object_unref (task);
      return;
    }

  g_output_stream_write_all_async (stream,
                                   g_bytes_get_data (content->bytes, NULL),
                                   g_bytes_get_size (content->bytes),
                                   io_priority,
                                   cancellable,
                                   gdk_content_provider_bytes_write_mime_type_done,
                                   task);
}

static gboolean
gdk_content_provider_bytes_write_mime_type_finish (GdkContentProvider *provider,
                                                   GAsyncResult       *result,
                                                   GError            **error)
{
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_content_provider_bytes_write_mime_type_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gdk_content_provider_bytes_class_init (GdkContentProviderBytesClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  object_class->finalize = gdk_content_provider_bytes_finalize;

  provider_class->ref_formats = gdk_content_provider_bytes_ref_formats;
  provider_class->write_mime_type_async = gdk_content_provider_bytes_write_mime_type_async;
  provider_class->write_mime_type_finish = gdk_content_provider_bytes_write_mime_type_finish;
}

static void
gdk_content_provider_bytes_init (GdkContentProviderBytes *content)
{
}

/**
 * gdk_content_provider_new_for_bytes:
 * @mime_type: the mime type
 * @bytes: (transfer none): a `GBytes` with the data for @mime_type
 *
 * Create a content provider that provides the given @bytes as data for
 * the given @mime_type.
 *
 * Returns: a new `GdkContentProvider`
 */
GdkContentProvider *
gdk_content_provider_new_for_bytes (const char *mime_type,
                                    GBytes     *bytes)
{
  GdkContentProviderBytes *content;

  g_return_val_if_fail (mime_type != NULL, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  content = g_object_new (GDK_TYPE_CONTENT_PROVIDER_BYTES, NULL);
  content->mime_type = g_intern_string (mime_type);
  content->bytes = g_bytes_ref (bytes);
  
  return GDK_CONTENT_PROVIDER (content);
}

