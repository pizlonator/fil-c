/* GDK - The GIMP Drawing Kit
 *
 * Copyright (C) 2017 Benjamin Otte <otte@gnome.org>
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

#include "gdkclipboard.h"
#include "gdkcontentformats.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate.h"

/**
 * GdkContentProvider:
 *
 * A `GdkContentProvider` is used to provide content for the clipboard or
 * for drag-and-drop operations in a number of formats.
 *
 * To create a `GdkContentProvider`, use [ctor@Gdk.ContentProvider.new_for_value]
 * or [ctor@Gdk.ContentProvider.new_for_bytes].
 *
 * GDK knows how to handle common text and image formats out-of-the-box. See
 * [class@Gdk.ContentSerializer] and [class@Gdk.ContentDeserializer] if you want
 * to add support for application-specific data formats.
 */

typedef struct _GdkContentProviderPrivate GdkContentProviderPrivate;

struct _GdkContentProviderPrivate
{
  GdkContentFormats *formats;
};

enum {
  PROP_0,
  PROP_FORMATS,
  PROP_STORABLE_FORMATS,
  N_PROPERTIES
};

enum {
  CONTENT_CHANGED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };
static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GdkContentProvider, gdk_content_provider, G_TYPE_OBJECT)

static void
gdk_content_provider_real_attach_clipboard (GdkContentProvider *provider,
                                            GdkClipboard       *clipboard)
{
}

static void
gdk_content_provider_real_detach_clipboard (GdkContentProvider *provider,
                                            GdkClipboard       *clipboard)
{
}

static GdkContentFormats *
gdk_content_provider_real_ref_formats (GdkContentProvider *provider)
{
  return gdk_content_formats_new (NULL, 0);
}

static GdkContentFormats *
gdk_content_provider_real_ref_storable_formats (GdkContentProvider *provider)
{
  return gdk_content_provider_ref_formats (provider);
}

static void
gdk_content_provider_real_write_mime_type_async (GdkContentProvider  *provider,
                                                 const char          *mime_type,
                                                 GOutputStream       *stream,
                                                 int                  io_priority,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data)
{
  GTask *task;

  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_content_provider_real_write_mime_type_async);

  g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Cannot provide contents as “%s”"), mime_type);
  g_object_unref (task);
}

static gboolean
gdk_content_provider_real_write_mime_type_finish (GdkContentProvider  *provider,
                                                  GAsyncResult        *result,
                                                  GError             **error)
{
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_content_provider_real_write_mime_type_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
gdk_content_provider_real_get_value (GdkContentProvider  *provider,
                                     GValue              *value,
                                     GError             **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
               _("Cannot provide contents as %s"), G_VALUE_TYPE_NAME (value));

  return FALSE;
}

static void
gdk_content_provider_get_property (GObject    *gobject,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GdkContentProvider *provider = GDK_CONTENT_PROVIDER (gobject);

  switch (prop_id)
    {
    case PROP_FORMATS:
      g_value_take_boxed (value, gdk_content_provider_ref_formats (provider));
      break;

    case PROP_STORABLE_FORMATS:
      g_value_take_boxed (value, gdk_content_provider_ref_storable_formats (provider));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_content_provider_class_init (GdkContentProviderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gdk_content_provider_get_property;

  class->attach_clipboard = gdk_content_provider_real_attach_clipboard;
  class->detach_clipboard = gdk_content_provider_real_detach_clipboard;
  class->ref_formats = gdk_content_provider_real_ref_formats;
  class->ref_storable_formats = gdk_content_provider_real_ref_storable_formats;
  class->write_mime_type_async = gdk_content_provider_real_write_mime_type_async;
  class->write_mime_type_finish = gdk_content_provider_real_write_mime_type_finish;
  class->get_value = gdk_content_provider_real_get_value;

  /**
   * GdkContentProvider:formats: (attributes org.gtk.Property.get=gdk_content_provider_ref_formats)
   *
   * The possible formats that the provider can provide its data in.
   */
  properties[PROP_FORMATS] =
    g_param_spec_boxed ("formats", NULL, NULL,
                        GDK_TYPE_CONTENT_FORMATS,
                        G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkContentProvider:storable-formats: (attributes org.gtk.Property.get=gdk_content_provider_ref_storable_formats)
   *
   * The subset of formats that clipboard managers should store this provider's data in.
   */
  properties[PROP_STORABLE_FORMATS] =
    g_param_spec_boxed ("storable-formats", NULL, NULL,
                        GDK_TYPE_CONTENT_FORMATS,
                        G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkContentProvider::content-changed:
   *
   * Emitted whenever the content provided by this provider has changed.
   */
  signals[CONTENT_CHANGED] =
    g_signal_new (I_("content-changed"),
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkContentProviderClass, content_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
gdk_content_provider_init (GdkContentProvider *provider)
{
}

/**
 * gdk_content_provider_ref_formats: (attributes org.gtk.Method.get_property=formats)
 * @provider: a `GdkContentProvider`
 *
 * Gets the formats that the provider can provide its current contents in.
 *
 * Returns: (transfer full): The formats of the provider
 */
GdkContentFormats *
gdk_content_provider_ref_formats (GdkContentProvider *provider)
{
  g_return_val_if_fail (GDK_IS_CONTENT_PROVIDER (provider), NULL);

  return GDK_CONTENT_PROVIDER_GET_CLASS (provider)->ref_formats (provider);
}

/**
 * gdk_content_provider_ref_storable_formats: (attributes org.gtk.Method.get_property=storable-formats)
 * @provider: a `GdkContentProvider`
 *
 * Gets the formats that the provider suggests other applications to store
 * the data in.
 *
 * An example of such an application would be a clipboard manager.
 *
 * This can be assumed to be a subset of [method@Gdk.ContentProvider.ref_formats].
 *
 * Returns: (transfer full): The storable formats of the provider
 */
GdkContentFormats *
gdk_content_provider_ref_storable_formats (GdkContentProvider *provider)
{
  g_return_val_if_fail (GDK_IS_CONTENT_PROVIDER (provider), NULL);

  return GDK_CONTENT_PROVIDER_GET_CLASS (provider)->ref_storable_formats (provider);
}

/**
 * gdk_content_provider_content_changed:
 * @provider: a `GdkContentProvider`
 *
 * Emits the ::content-changed signal.
 */
void
gdk_content_provider_content_changed (GdkContentProvider *provider)
{
  g_return_if_fail (GDK_IS_CONTENT_PROVIDER (provider));

  g_signal_emit (provider, signals[CONTENT_CHANGED], 0);

  g_object_notify_by_pspec (G_OBJECT (provider), properties[PROP_FORMATS]);
}

/**
 * gdk_content_provider_write_mime_type_async:
 * @provider: a `GdkContentProvider`
 * @mime_type: the mime type to provide the data in
 * @stream: the `GOutputStream` to write to
 * @io_priority: I/O priority of the request.
 * @cancellable: (nullable): optional `GCancellable` object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously writes the contents of @provider to @stream in the given
 * @mime_type.
 *
 * When the operation is finished @callback will be called. You must then call
 * [method@Gdk.ContentProvider.write_mime_type_finish] to get the result
 * of the operation.
 *
 * The given mime type does not need to be listed in the formats returned by
 * [method@Gdk.ContentProvider.ref_formats]. However, if the given `GType` is
 * not supported, `G_IO_ERROR_NOT_SUPPORTED` will be reported.
 *
 * The given @stream will not be closed.
 */
void
gdk_content_provider_write_mime_type_async (GdkContentProvider  *provider,
                                            const char          *mime_type,
                                            GOutputStream       *stream,
                                            int                  io_priority,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_return_if_fail (GDK_IS_CONTENT_PROVIDER (provider));
  g_return_if_fail (mime_type != NULL);
  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  GDK_CONTENT_PROVIDER_GET_CLASS (provider)->write_mime_type_async (provider,
                                                                    g_intern_string (mime_type),
                                                                    stream,
                                                                    io_priority,
                                                                    cancellable,
                                                                    callback,
                                                                    user_data);
}

/**
 * gdk_content_provider_write_mime_type_finish:
 * @provider: a `GdkContentProvider`
 * @result: a `GAsyncResult`
 * @error: a `GError` location to store the error occurring
 *
 * Finishes an asynchronous write operation.
 *
 * See [method@Gdk.ContentProvider.write_mime_type_async].
 *
 * Returns: %TRUE if the operation was completed successfully. Otherwise
 *   @error will be set to describe the failure.
 */
gboolean
gdk_content_provider_write_mime_type_finish (GdkContentProvider  *provider,
                                             GAsyncResult        *result,
                                             GError             **error)
{
  g_return_val_if_fail (GDK_IS_CONTENT_PROVIDER (provider), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GDK_CONTENT_PROVIDER_GET_CLASS (provider)->write_mime_type_finish (provider, result, error);
}

/**
 * gdk_content_provider_get_value:
 * @provider: a `GdkContentProvider`
 * @value: (out caller-allocates): the `GValue` to fill
 * @error: a `GError` location to store the error occurring
 *
 * Gets the contents of @provider stored in @value.
 *
 * The @value will have been initialized to the `GType` the value should be
 * provided in. This given `GType` does not need to be listed in the formats
 * returned by [method@Gdk.ContentProvider.ref_formats]. However, if the
 * given `GType` is not supported, this operation can fail and
 * `G_IO_ERROR_NOT_SUPPORTED` will be reported.
 *
 * Returns: %TRUE if the value was set successfully. Otherwise
 *   @error will be set to describe the failure.
 */
gboolean
gdk_content_provider_get_value (GdkContentProvider  *provider,
                                GValue              *value,
                                GError             **error)
{
  g_return_val_if_fail (GDK_IS_CONTENT_PROVIDER (provider), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GDK_CONTENT_PROVIDER_GET_CLASS (provider)->get_value (provider,
                                                               value,
                                                               error);
}

void
gdk_content_provider_attach_clipboard (GdkContentProvider *provider,
                                       GdkClipboard       *clipboard)
{
  g_return_if_fail (GDK_IS_CONTENT_PROVIDER (provider));
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CONTENT_PROVIDER_GET_CLASS (provider)->attach_clipboard (provider, clipboard);
}

void
gdk_content_provider_detach_clipboard (GdkContentProvider *provider,
                                       GdkClipboard       *clipboard)
{
  g_return_if_fail (GDK_IS_CONTENT_PROVIDER (provider));
  g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

  GDK_CONTENT_PROVIDER_GET_CLASS (provider)->detach_clipboard (provider, clipboard);
}
