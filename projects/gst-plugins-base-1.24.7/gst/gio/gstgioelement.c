/* GStreamer
 *
 * Copyright (C) 2007 Rene Stadler <mail@renestadler.de>
 * Copyright (C) 2007 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgioelements.h"

GST_DEBUG_CATEGORY_STATIC (gst_gio_debug);
#define GST_CAT_DEFAULT gst_gio_debug

/* @func_name: Name of the GIO function, for debugging messages.
 * @err: Error location.  *err may be NULL, but err must be non-NULL.
 * @ret: Flow return location.  May be NULL.  Is set to either #GST_FLOW_ERROR
 * or #GST_FLOW_FLUSHING.
 *
 * Returns: TRUE to indicate a handled error.  Error at given location err will
 * be freed and *err will be set to NULL.  A FALSE return indicates an unhandled
 * error: The err location is unchanged and guaranteed to be != NULL.  ret, if
 * given, is set to GST_FLOW_ERROR.
 */
gboolean
gst_gio_error (gpointer element, const gchar * func_name, GError ** err,
    GstFlowReturn * ret)
{
  gboolean handled = TRUE;

  if (ret)
    *ret = GST_FLOW_ERROR;

  if (GST_GIO_ERROR_MATCHES (*err, CANCELLED)) {
    GST_DEBUG_OBJECT (element, "blocking I/O call cancelled (%s)", func_name);
    if (ret)
      *ret = GST_FLOW_FLUSHING;
  } else if (*err != NULL) {
    handled = FALSE;
  } else {
    GST_ELEMENT_ERROR (element, LIBRARY, FAILED, (NULL),
        ("%s call failed without error set", func_name));
  }

  if (handled)
    g_clear_error (err);

  return handled;
}

GstFlowReturn
gst_gio_seek (gpointer element, GSeekable * stream, guint64 offset,
    GCancellable * cancel)
{
  gboolean success;
  GstFlowReturn ret;
  GError *err = NULL;

  GST_LOG_OBJECT (element, "seeking to offset %" G_GINT64_FORMAT, offset);

  success = g_seekable_seek (stream, offset, G_SEEK_SET, cancel, &err);

  if (success)
    ret = GST_FLOW_OK;
  else if (!gst_gio_error (element, "g_seekable_seek", &err, &ret)) {
    GST_ELEMENT_ERROR (element, RESOURCE, SEEK, (NULL),
        ("Could not seek: %s", err->message));
    g_clear_error (&err);
  }

  return ret;
}

static gpointer
_internal_get_supported_protocols (gpointer data)
{
  const gchar *const *schemes;
  gchar **our_schemes;
  guint num;
  gint i, j;

  schemes = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());

  if (schemes != NULL)
    num = g_strv_length ((gchar **) schemes);
  else
    num = 0;

  if (num == 0) {
    GST_WARNING ("No GIO supported URI schemes found");
    return NULL;
  }

  our_schemes = g_new0 (gchar *, num + 1);

  /* - Filter http/https as we can't support the icy stuff with GIO.
   *   Use souphttpsrc if you need that.
   * - Filter cdda as it doesn't support musicbrainz stuff and everything
   *   else one expects from a cdda source. Use cdparanoiasrc or cdiosrc
   *   for cdda.
   */
  for (i = 0, j = 0; i < num; i++) {
    if (strcmp (schemes[i], "http") == 0 || strcmp (schemes[i], "https") == 0
        || strcmp (schemes[i], "cdda") == 0)
      continue;

    our_schemes[j] = g_strdup (schemes[i]);
    j++;
  }

  return our_schemes;
}

static gchar **
gst_gio_get_supported_protocols (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, _internal_get_supported_protocols, NULL);
  return (gchar **) once.retval;
}

static GstURIType
gst_gio_uri_handler_get_type_sink (GType type)
{
  return GST_URI_SINK;
}

static GstURIType
gst_gio_uri_handler_get_type_src (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_gio_uri_handler_get_protocols (GType type)
{
  static const gchar *const *protocols = NULL;

  if (!protocols)
    protocols = (const gchar * const *) gst_gio_get_supported_protocols ();

  return protocols;
}

static gchar *
gst_gio_uri_handler_get_uri (GstURIHandler * handler)
{
  GstElement *element = GST_ELEMENT (handler);
  gchar *uri;

  g_return_val_if_fail (GST_IS_ELEMENT (element), NULL);

  g_object_get (G_OBJECT (element), "location", &uri, NULL);

  return uri;
}

static gboolean
gst_gio_uri_handler_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstElement *element = GST_ELEMENT (handler);

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);

  if (GST_STATE (element) == GST_STATE_PLAYING ||
      GST_STATE (element) == GST_STATE_PAUSED) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        "Changing the 'location' property while the element is running is "
        "not supported");
    return FALSE;
  }

  g_object_set (G_OBJECT (element), "location", uri, NULL);
  return TRUE;
}

static void
gst_gio_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;
  gboolean sink = GPOINTER_TO_INT (iface_data); /* See in do_init below. */

  if (sink)
    iface->get_type = gst_gio_uri_handler_get_type_sink;
  else
    iface->get_type = gst_gio_uri_handler_get_type_src;
  iface->get_protocols = gst_gio_uri_handler_get_protocols;
  iface->get_uri = gst_gio_uri_handler_get_uri;
  iface->set_uri = gst_gio_uri_handler_set_uri;
}

void
gst_gio_uri_handler_do_init (GType type)
{
  GInterfaceInfo uri_handler_info = {
    gst_gio_uri_handler_init,
    NULL,
    NULL
  };

  /* Store information for uri_handler_init to use for distinguishing the
   * element types.  This lets us use a single interface implementation for both
   * classes. */
  uri_handler_info.interface_data = GINT_TO_POINTER (g_type_is_a (type,
          GST_TYPE_BASE_SINK));

  g_type_add_interface_static (type, GST_TYPE_URI_HANDLER, &uri_handler_info);
}

#define GIO_GVFS_MOUNTS_DIR GIO_PREFIX \
    G_DIR_SEPARATOR_S "share" \
    G_DIR_SEPARATOR_S "gvfs" \
    G_DIR_SEPARATOR_S "mounts"

void
gio_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;

  if (g_once_init_enter (&res)) {
    GST_DEBUG_CATEGORY_INIT (gst_gio_debug, "gio", 0, "GIO elements");

    gst_plugin_add_dependency_simple (plugin, NULL, GIO_MODULE_DIR, NULL,
        GST_PLUGIN_DEPENDENCY_FLAG_NONE);
    gst_plugin_add_dependency_simple (plugin, NULL, GIO_GVFS_MOUNTS_DIR, NULL,
        GST_PLUGIN_DEPENDENCY_FLAG_NONE);
    g_once_init_leave (&res, TRUE);
  }
}
