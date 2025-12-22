/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gstvadisplaydrm
 * @title: GstVaDisplayDrm
 * @short_description: VADisplay from a DRM device
 * @sources:
 * - gstvadisplay_drm.h
 *
 * This is a #GstVaDisplay subclass to instantiate with DRM devices.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvadisplay_drm.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <va/va_drm.h>

#ifdef HAVE_LIBDRM
#include <xf86drm.h>
#endif

/**
 * GstVaDisplayDrm:
 * @parent: parent #GstVaDisplay
 *
 * Since: 1.20
 */
struct _GstVaDisplayDrm
{
  GstVaDisplay parent;

  /* <private> */
  gchar *path;
  gint fd;
};

/**
 * GstVaDisplayDrmClass:
 * @parent_class: parent #GstVaDisplayClass
 *
 * Since: 1.20
 */
struct _GstVaDisplayDrmClass
{
  GstVaDisplayClass parent_class;
};

GST_DEBUG_CATEGORY_EXTERN (gst_va_display_debug);
#define GST_CAT_DEFAULT gst_va_display_debug

#define gst_va_display_drm_parent_class parent_class
G_DEFINE_TYPE (GstVaDisplayDrm, gst_va_display_drm, GST_TYPE_VA_DISPLAY);

enum
{
  PROP_PATH = 1,
  N_PROPERTIES
};

static GParamSpec *g_properties[N_PROPERTIES];

#define MAX_DEVICES 8

static void
gst_va_display_drm_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaDisplayDrm *self = GST_VA_DISPLAY_DRM (object);

  switch (prop_id) {
    case PROP_PATH:
      self->path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_va_display_drm_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVaDisplayDrm *self = GST_VA_DISPLAY_DRM (object);

  switch (prop_id) {
    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_va_display_drm_finalize (GObject * object)
{
  GstVaDisplayDrm *self = GST_VA_DISPLAY_DRM (object);

  g_free (self->path);
  if (self->fd > -1)
    close (self->fd);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gpointer
gst_va_display_drm_create_va_display (GstVaDisplay * display)
{
  int fd, saved_errno = 0;
  GstVaDisplayDrm *self = GST_VA_DISPLAY_DRM (display);

  fd = open (self->path, O_CLOEXEC | O_RDWR);
  saved_errno = errno;
  if (fd < 0) {
    GST_WARNING_OBJECT (self, "Failed to open %s: %s", self->path,
        g_strerror (saved_errno));
    return 0;
  }
#ifdef HAVE_LIBDRM
  {
    drmVersion *version;

    version = drmGetVersion (fd);
    if (!version) {
      GST_ERROR_OBJECT (self, "Device %s is not a DRM render node", self->path);
      return 0;
    }
    GST_INFO_OBJECT (self, "DRM render node with kernel driver %s",
        version->name);
    drmFreeVersion (version);
  }
#endif

  self->fd = fd;
  return vaGetDisplayDRM (self->fd);
}

static void
gst_va_display_drm_class_init (GstVaDisplayDrmClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVaDisplayClass *vadisplay_class = GST_VA_DISPLAY_CLASS (klass);

  gobject_class->set_property = gst_va_display_drm_set_property;
  gobject_class->get_property = gst_va_display_drm_get_property;
  gobject_class->finalize = gst_va_display_drm_finalize;

  vadisplay_class->create_va_display = gst_va_display_drm_create_va_display;

  g_properties[PROP_PATH] =
      g_param_spec_string ("path", "render-path", "The path of DRM device",
      "/dev/dri/renderD128",
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, g_properties);
}

static void
gst_va_display_drm_init (GstVaDisplayDrm * self)
{
  self->fd = -1;
}

/**
 * gst_va_display_drm_new_from_path:
 * @path: the path to the DRM device
 *
 * Creates a new #GstVaDisplay from a DRM device . It will try to open
 * and operate the device in @path.
 *
 * Returns: (transfer full): a newly allocated #GstVaDisplay if the
 *     specified DRM render device could be opened and initialized;
 *     otherwise %NULL is returned.
 *
 * Since: 1.20
 **/
GstVaDisplay *
gst_va_display_drm_new_from_path (const gchar * path)
{
  GstVaDisplay *dpy;

  g_return_val_if_fail (path, NULL);

  dpy = g_object_new (GST_TYPE_VA_DISPLAY_DRM, "path", path, NULL);
  if (!gst_va_display_initialize (dpy)) {
    gst_object_unref (dpy);
    return NULL;
  }

  return gst_object_ref_sink (dpy);
}
