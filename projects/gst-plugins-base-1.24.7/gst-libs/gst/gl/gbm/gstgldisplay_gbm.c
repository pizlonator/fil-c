/*
 * GStreamer
 * Copyright (C) 2018 Carlos Rafael Giani <dv@pseudoterminal.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgldisplay_gbm.h"
#include "gstgl_gbm_utils.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

GST_DEBUG_CATEGORY (gst_gl_gbm_debug);

GST_DEBUG_CATEGORY_STATIC (gst_gl_display_debug);
#define GST_CAT_DEFAULT gst_gl_display_debug


#define INVALID_CRTC ((guint32)0)


G_DEFINE_TYPE (GstGLDisplayGBM, gst_gl_display_gbm, GST_TYPE_GL_DISPLAY);


static void gst_gl_display_gbm_finalize (GObject * object);
static guintptr gst_gl_display_gbm_get_handle (GstGLDisplay * display);

static guint32 gst_gl_gbm_find_crtc_id_for_encoder (GstGLDisplayGBM *
    display_gbm, drmModeEncoder const *encoder);
static guint32 gst_gl_gbm_find_crtc_id_for_connector (GstGLDisplayGBM *
    display_gbm);

static gboolean gst_gl_display_gbm_setup_drm (GstGLDisplayGBM * display_gbm,
    const gchar * drm_connector_name);
static void gst_gl_display_gbm_shutdown_drm (GstGLDisplayGBM * display_gbm);

static gboolean gst_gl_display_gbm_setup_gbm (GstGLDisplayGBM * display_gbm);
static void gst_gl_display_gbm_shutdown_gbm (GstGLDisplayGBM * display_gbm);


static void
gst_gl_display_gbm_class_init (GstGLDisplayGBMClass * klass)
{
  GST_GL_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_gl_display_gbm_get_handle);

  G_OBJECT_CLASS (klass)->finalize = gst_gl_display_gbm_finalize;
}

static void
gst_gl_display_gbm_init (GstGLDisplayGBM * display_gbm)
{
  GstGLDisplay *display = (GstGLDisplay *) display_gbm;
  display->type = GST_GL_DISPLAY_TYPE_GBM;

  display_gbm->drm_fd = -1;
}

static void
gst_gl_display_gbm_finalize (GObject * object)
{
  GstGLDisplayGBM *display_gbm = GST_GL_DISPLAY_GBM (object);

  gst_gl_display_gbm_shutdown_gbm (display_gbm);
  gst_gl_display_gbm_shutdown_drm (display_gbm);

  if (display_gbm->drm_fd >= 0)
    close (display_gbm->drm_fd);

  G_OBJECT_CLASS (gst_gl_display_gbm_parent_class)->finalize (object);
}

static guintptr
gst_gl_display_gbm_get_handle (GstGLDisplay * display)
{
  return (guintptr) GST_GL_DISPLAY_GBM (display)->gbm_dev;
}


static guint32
gst_gl_gbm_find_crtc_id_for_encoder (GstGLDisplayGBM * display_gbm,
    drmModeEncoder const *encoder)
{
  int i;
  for (i = 0; i < display_gbm->drm_mode_resources->count_crtcs; ++i) {
    /* possible_crtcs is a bitmask as described here:
     * https://dvdhrm.wordpress.com/2012/09/13/linux-drm-mode-setting-api */
    guint32 const crtc_mask = 1 << i;
    guint32 const crtc_id = display_gbm->drm_mode_resources->crtcs[i];

    if (encoder->possible_crtcs & crtc_mask)
      return crtc_id;
  }

  /* No match found */
  return INVALID_CRTC;
}


static guint32
gst_gl_gbm_find_crtc_id_for_connector (GstGLDisplayGBM * display_gbm)
{
  int i;
  for (i = 0; i < display_gbm->drm_mode_connector->count_encoders; ++i) {
    guint32 encoder_id = display_gbm->drm_mode_connector->encoders[i];
    drmModeEncoder *encoder =
        drmModeGetEncoder (display_gbm->drm_fd, encoder_id);

    if (encoder != NULL) {
      guint32 crtc_id =
          gst_gl_gbm_find_crtc_id_for_encoder (display_gbm, encoder);
      drmModeFreeEncoder (encoder);

      if (crtc_id != INVALID_CRTC)
        return crtc_id;
    }
  }

  /* No match found */
  return INVALID_CRTC;
}


static gboolean
gst_gl_display_gbm_setup_drm (GstGLDisplayGBM * display_gbm, const gchar *
    drm_connector_name)
{
  int i;

  g_assert (display_gbm != NULL);
  g_assert (display_gbm->drm_fd >= 0);

  /* Get the DRM mode resources */
  display_gbm->drm_mode_resources = drmModeGetResources (display_gbm->drm_fd);
  if (display_gbm->drm_mode_resources == NULL) {
    GST_ERROR ("Could not get DRM resources: %s (%d)", g_strerror (errno),
        errno);
    goto cleanup;
  }
  GST_DEBUG ("Got DRM resources");

  /* Find a connected connector. The connector is where the pixel data is
   * finally sent to, and typically connects to some form of display, like an
   * HDMI TV, an LVDS panel etc. */
  {
    drmModeConnector *connected_connector = NULL;

    GST_DEBUG ("Checking %d DRM connector(s)",
        display_gbm->drm_mode_resources->count_connectors);
    for (i = 0; i < display_gbm->drm_mode_resources->count_connectors; ++i) {
      drmModeConnector *candidate_connector =
          drmModeGetConnector (display_gbm->drm_fd,
          display_gbm->drm_mode_resources->connectors[i]);
      gchar *candidate_name;

      candidate_name = g_strdup_printf ("%s-%i",
          gst_gl_gbm_get_name_for_drm_connector (candidate_connector),
          candidate_connector->connector_type_id);

      GST_DEBUG ("Found DRM connector #%d \"%s\" with ID %" G_GUINT32_FORMAT, i,
          candidate_name, candidate_connector->connector_id);

      /* If we already picked a connector, and connected_connector is therefore
       * non-NULL, then are just printing information about the other connectors
       * for logging purposes by now, so don't actually do anything with this
       * connector. Just loop instead. */
      if (connected_connector != NULL) {
        drmModeFreeConnector (candidate_connector);
        g_free (candidate_name);
        continue;
      }

      if (drm_connector_name != NULL) {
        if (g_ascii_strcasecmp (drm_connector_name, candidate_name) != 0) {
          drmModeFreeConnector (candidate_connector);
          g_free (candidate_name);
          continue;
        }
      }

      if (candidate_connector->connection == DRM_MODE_CONNECTED) {
        if (drm_connector_name != NULL)
          GST_DEBUG ("Picking DRM connector #%d because it is connected and "
              "has a matching name \"%s\"", i, candidate_name);
        else
          GST_DEBUG ("Picking DRM connector #%d because it is connected", i);
        connected_connector = candidate_connector;
        g_free (candidate_name);
        break;
      } else {
        if (drm_connector_name != NULL)
          GST_WARNING ("DRM connector #%d has a matching name \"%s\" but is "
              "not connected; not picking it", i, candidate_name);
        drmModeFreeConnector (candidate_connector);
        g_free (candidate_name);
      }
    }

    if (connected_connector == NULL) {
      GST_ERROR ("No connected DRM connector found");
      goto cleanup;
    }

    display_gbm->drm_mode_connector = connected_connector;
  }

  /* Check out what modes are supported by the chosen connector,
   * and pick either the "preferred" mode or the one with the largest
   * pixel area. */
  {
    int selected_mode_index = -1;
    int selected_mode_area = -1;
    gboolean preferred_mode_found = FALSE;

    GST_DEBUG ("Checking %d DRM mode(s) from selected connector",
        display_gbm->drm_mode_connector->count_modes);
    for (i = 0; i < display_gbm->drm_mode_connector->count_modes; ++i) {
      drmModeModeInfo *current_mode =
          &(display_gbm->drm_mode_connector->modes[i]);
      int current_mode_area = current_mode->hdisplay * current_mode->vdisplay;

      GST_DEBUG ("Found DRM mode #%d width/height %" G_GUINT16_FORMAT "/%"
          G_GUINT16_FORMAT " hsync/vsync start %" G_GUINT16_FORMAT "/%"
          G_GUINT16_FORMAT " hsync/vsync end %" G_GUINT16_FORMAT "/%"
          G_GUINT16_FORMAT " htotal/vtotal %" G_GUINT16_FORMAT "/%"
          G_GUINT16_FORMAT " hskew %" G_GUINT16_FORMAT " vscan %"
          G_GUINT16_FORMAT " vrefresh %" G_GUINT32_FORMAT " preferred %d", i,
          current_mode->hdisplay, current_mode->vdisplay,
          current_mode->hsync_start, current_mode->vsync_start,
          current_mode->hsync_end, current_mode->vsync_end,
          current_mode->htotal, current_mode->vtotal, current_mode->hskew,
          current_mode->vscan, current_mode->vrefresh,
          (current_mode->type & DRM_MODE_TYPE_PREFERRED) ? TRUE : FALSE);

      if (!preferred_mode_found
          && ((current_mode->type & DRM_MODE_TYPE_PREFERRED)
              || (current_mode_area > selected_mode_area))) {
        display_gbm->drm_mode_info = current_mode;
        selected_mode_area = current_mode_area;
        selected_mode_index = i;

        if (current_mode->type & DRM_MODE_TYPE_PREFERRED)
          preferred_mode_found = TRUE;
      }
    }

    if (display_gbm->drm_mode_info == NULL) {
      GST_ERROR ("No usable DRM mode found");
      goto cleanup;
    }

    GST_DEBUG ("Selected DRM mode #%d (is preferred: %d)", selected_mode_index,
        preferred_mode_found);
  }

  /* Find an encoder that is attached to the chosen connector. Also find the
   * index/id of the CRTC associated with this encoder. The encoder takes pixel
   * data from the CRTC and transmits it to the connector. The CRTC roughly
   * represents the scanout framebuffer.
   *
   * Ultimately, we only care about the CRTC index & ID, so the encoder
   * reference is discarded here once these are found. The CRTC index is the
   * index in the m_drm_mode_resources' CRTC array, while the ID is an identifier
   * used by the DRM to refer to the CRTC universally. (We need the CRTC
   * information for page flipping and DRM scanout framebuffer configuration.) */
  {
    drmModeEncoder *selected_encoder = NULL;

    GST_DEBUG ("Checking %d DRM encoder(s)",
        display_gbm->drm_mode_resources->count_encoders);
    for (i = 0; i < display_gbm->drm_mode_resources->count_encoders; ++i) {
      drmModeEncoder *candidate_encoder =
          drmModeGetEncoder (display_gbm->drm_fd,
          display_gbm->drm_mode_resources->encoders[i]);

      GST_DEBUG ("Found DRM encoder #%d \"%s\"", i,
          gst_gl_gbm_get_name_for_drm_encoder (candidate_encoder));

      if ((selected_encoder == NULL) &&
          (candidate_encoder->encoder_id ==
              display_gbm->drm_mode_connector->encoder_id)) {
        selected_encoder = candidate_encoder;
        GST_DEBUG ("DRM encoder #%d corresponds to selected DRM connector "
            "-> selected", i);
      } else
        drmModeFreeEncoder (candidate_encoder);
    }

    if (selected_encoder == NULL) {
      GST_DEBUG ("No encoder found; searching for CRTC ID in the connector");
      display_gbm->crtc_id =
          gst_gl_gbm_find_crtc_id_for_connector (display_gbm);
    } else {
      GST_DEBUG ("Using CRTC ID from selected encoder");
      display_gbm->crtc_id = selected_encoder->crtc_id;
      drmModeFreeEncoder (selected_encoder);
    }

    if (display_gbm->crtc_id == INVALID_CRTC) {
      GST_ERROR ("No CRTC found");
      goto cleanup;
    }

    GST_DEBUG ("CRTC with ID %" G_GUINT32_FORMAT " found; now locating it in "
        "the DRM mode resources CRTC array", display_gbm->crtc_id);

    for (i = 0; i < display_gbm->drm_mode_resources->count_crtcs; ++i) {
      if (display_gbm->drm_mode_resources->crtcs[i] == display_gbm->crtc_id) {
        display_gbm->crtc_index = i;
        break;
      }
    }

    if (display_gbm->crtc_index < 0) {
      GST_ERROR ("No matching CRTC entry in DRM resources found");
      goto cleanup;
    }

    GST_DEBUG ("CRTC with ID %" G_GUINT32_FORMAT " can be found at index #%d "
        "in the DRM mode resources CRTC array", display_gbm->crtc_id,
        display_gbm->crtc_index);
  }

  GST_DEBUG ("DRM structures initialized");
  return TRUE;

cleanup:
  gst_gl_display_gbm_shutdown_drm (display_gbm);
  return FALSE;
}


static void
gst_gl_display_gbm_shutdown_drm (GstGLDisplayGBM * display_gbm)
{
  g_assert (display_gbm != NULL);

  display_gbm->drm_mode_info = NULL;

  display_gbm->crtc_index = -1;
  display_gbm->crtc_id = INVALID_CRTC;

  if (display_gbm->drm_mode_connector != NULL) {
    drmModeFreeConnector (display_gbm->drm_mode_connector);
    display_gbm->drm_mode_connector = NULL;
  }

  if (display_gbm->drm_mode_resources != NULL) {
    drmModeFreeResources (display_gbm->drm_mode_resources);
    display_gbm->drm_mode_resources = NULL;
  }
}


static gboolean
gst_gl_display_gbm_setup_gbm (GstGLDisplayGBM * display_gbm)
{
  display_gbm->gbm_dev = gbm_create_device (display_gbm->drm_fd);
  if (display_gbm->gbm_dev == NULL) {
    GST_ERROR ("Creating GBM device failed");
    return FALSE;
  }

  GST_DEBUG ("GBM structures initialized");
  return TRUE;
}


static void
gst_gl_display_gbm_shutdown_gbm (GstGLDisplayGBM * display_gbm)
{
  if (display_gbm->gbm_dev != NULL) {
    gbm_device_destroy (display_gbm->gbm_dev);
    display_gbm->gbm_dev = NULL;
  }
}


static void
_init_debug (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");
    GST_DEBUG_CATEGORY_INIT (gst_gl_gbm_debug, "gleglgbm", 0,
        "Mesa3D EGL GBM debugging");
    g_once_init_leave (&_init, 1);
  }
}


GstGLDisplayGBM *
gst_gl_display_gbm_new (void)
{
  int drm_fd = -1;
  GstGLDisplayGBM *display;
  const gchar *drm_node_name;
  const gchar *drm_connector_name;

  _init_debug ();

  drm_node_name = g_getenv ("GST_GL_GBM_DRM_DEVICE");
  drm_connector_name = g_getenv ("GST_GL_GBM_DRM_CONNECTOR");

  if (drm_node_name != NULL) {
    GST_DEBUG ("attempting to open device %s (specified by the "
        "GST_GL_GBM_DRM_DEVICE environment variable)", drm_node_name);
    drm_fd = open (drm_node_name, O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
      GST_ERROR ("could not open DRM device %s: %s (%d)", drm_node_name,
          g_strerror (errno), errno);
      return NULL;
    }
  } else {
    GST_DEBUG ("GST_GL_GBM_DRM_DEVICE environment variable is not "
        "set - trying to autodetect device");
    drm_fd = gst_gl_gbm_find_and_open_drm_node ();
    if (drm_fd < 0) {
      GST_ERROR ("could not find or open DRM device");
      return NULL;
    }
  }

  display = g_object_new (GST_TYPE_GL_DISPLAY_GBM, NULL);
  display->drm_fd = drm_fd;

  if (drm_connector_name != NULL) {
    GST_DEBUG ("GST_GL_GBM_DRM_CONNECTOR variable set to value \"%s\"; "
        "will use this name to match connector(s) against", drm_connector_name);
  }

  if (!gst_gl_display_gbm_setup_drm (display, drm_connector_name)) {
    GST_WARNING ("Failed to initialize DRM");
  }

  if (!gst_gl_display_gbm_setup_gbm (display)) {
    GST_ERROR ("Failed to initialize GBM");
    goto cleanup;
  }

  GST_DEBUG ("Created GBM EGL display %p", (gpointer) display);

  return display;

cleanup:
  gst_gl_display_gbm_shutdown_gbm (display);
  gst_gl_display_gbm_shutdown_drm (display);
  gst_object_unref (G_OBJECT (display));
  if (drm_fd >= 0)
    close (drm_fd);
  return NULL;
}
