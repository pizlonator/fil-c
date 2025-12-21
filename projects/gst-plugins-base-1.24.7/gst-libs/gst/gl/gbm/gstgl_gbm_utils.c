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

#include <unistd.h>
#include <fcntl.h>
#include <gudev/gudev.h>
#include "gstgl_gbm_utils.h"

GST_DEBUG_CATEGORY_EXTERN (gst_gl_gbm_debug);
#define GST_CAT_DEFAULT gst_gl_gbm_debug

const gchar *
gst_gl_gbm_get_name_for_drm_connector (drmModeConnector * connector)
{
  g_assert (connector != NULL);

  switch (connector->connector_type) {
    case DRM_MODE_CONNECTOR_Unknown:
      return "Unknown";
    case DRM_MODE_CONNECTOR_VGA:
      return "VGA";
    case DRM_MODE_CONNECTOR_DVII:
      return "DVI-I";
    case DRM_MODE_CONNECTOR_DVID:
      return "DVI-D";
    case DRM_MODE_CONNECTOR_DVIA:
      return "DVI-A";
    case DRM_MODE_CONNECTOR_Composite:
      return "Composite";
    case DRM_MODE_CONNECTOR_SVIDEO:
      return "S-Video";
    case DRM_MODE_CONNECTOR_LVDS:
      return "LVDS";
    case DRM_MODE_CONNECTOR_Component:
      return "Component";
    case DRM_MODE_CONNECTOR_9PinDIN:
      return "9-Pin DIN";
    case DRM_MODE_CONNECTOR_DisplayPort:
      return "DP";
    case DRM_MODE_CONNECTOR_HDMIA:
      return "HDMI-A";
    case DRM_MODE_CONNECTOR_HDMIB:
      return "HDMI-B";
    case DRM_MODE_CONNECTOR_TV:
      return "TV";
    case DRM_MODE_CONNECTOR_eDP:
      return "eDP";
    case DRM_MODE_CONNECTOR_VIRTUAL:
      return "Virtual";
    case DRM_MODE_CONNECTOR_DSI:
      return "DSI";
    case DRM_MODE_CONNECTOR_DPI:
      return "DPI";
    default:
      return "<unknown>";
  }
}


const gchar *
gst_gl_gbm_get_name_for_drm_encoder (drmModeEncoder * encoder)
{
  switch (encoder->encoder_type) {
    case DRM_MODE_ENCODER_NONE:
      return "none";
    case DRM_MODE_ENCODER_DAC:
      return "DAC";
    case DRM_MODE_ENCODER_TMDS:
      return "TMDS";
    case DRM_MODE_ENCODER_LVDS:
      return "LVDS";
    case DRM_MODE_ENCODER_TVDAC:
      return "TVDAC";
    case DRM_MODE_ENCODER_VIRTUAL:
      return "Virtual";
    case DRM_MODE_ENCODER_DSI:
      return "DSI";
    default:
      return "<unknown>";
  }
}


const gchar *
gst_gl_gbm_format_to_string (guint32 format)
{
  if (format == GBM_BO_FORMAT_XRGB8888)
    format = GBM_FORMAT_XRGB8888;
  if (format == GBM_BO_FORMAT_ARGB8888)
    format = GBM_FORMAT_ARGB8888;

  switch (format) {
    case GBM_FORMAT_C8:
      return "C8";
    case GBM_FORMAT_RGB332:
      return "RGB332";
    case GBM_FORMAT_BGR233:
      return "BGR233";
    case GBM_FORMAT_NV12:
      return "NV12";
    case GBM_FORMAT_XRGB4444:
      return "XRGB4444";
    case GBM_FORMAT_XBGR4444:
      return "XBGR4444";
    case GBM_FORMAT_RGBX4444:
      return "RGBX4444";
    case GBM_FORMAT_BGRX4444:
      return "BGRX4444";
    case GBM_FORMAT_XRGB1555:
      return "XRGB1555";
    case GBM_FORMAT_XBGR1555:
      return "XBGR1555";
    case GBM_FORMAT_RGBX5551:
      return "RGBX5551";
    case GBM_FORMAT_BGRX5551:
      return "BGRX5551";
    case GBM_FORMAT_ARGB4444:
      return "ARGB4444";
    case GBM_FORMAT_ABGR4444:
      return "ABGR4444";
    case GBM_FORMAT_RGBA4444:
      return "RGBA4444";
    case GBM_FORMAT_BGRA4444:
      return "BGRA4444";
    case GBM_FORMAT_ARGB1555:
      return "ARGB1555";
    case GBM_FORMAT_ABGR1555:
      return "ABGR1555";
    case GBM_FORMAT_RGBA5551:
      return "RGBA5551";
    case GBM_FORMAT_BGRA5551:
      return "BGRA5551";
    case GBM_FORMAT_RGB565:
      return "RGB565";
    case GBM_FORMAT_BGR565:
      return "BGR565";
    case GBM_FORMAT_YUYV:
      return "YUYV";
    case GBM_FORMAT_YVYU:
      return "YVYU";
    case GBM_FORMAT_UYVY:
      return "UYVY";
    case GBM_FORMAT_VYUY:
      return "VYUY";
    case GBM_FORMAT_RGB888:
      return "RGB888";
    case GBM_FORMAT_BGR888:
      return "BGR888";
    case GBM_FORMAT_XRGB8888:
      return "XRGB8888";
    case GBM_FORMAT_XBGR8888:
      return "XBGR8888";
    case GBM_FORMAT_RGBX8888:
      return "RGBX8888";
    case GBM_FORMAT_BGRX8888:
      return "BGRX8888";
    case GBM_FORMAT_AYUV:
      return "AYUV";
    case GBM_FORMAT_XRGB2101010:
      return "XRGB2101010";
    case GBM_FORMAT_XBGR2101010:
      return "XBGR2101010";
    case GBM_FORMAT_RGBX1010102:
      return "RGBX1010102";
    case GBM_FORMAT_BGRX1010102:
      return "BGRX1010102";
    case GBM_FORMAT_ARGB8888:
      return "ARGB8888";
    case GBM_FORMAT_ABGR8888:
      return "ABGR8888";
    case GBM_FORMAT_RGBA8888:
      return "RGBA8888";
    case GBM_FORMAT_BGRA8888:
      return "BGRA8888";
    case GBM_FORMAT_ARGB2101010:
      return "ARGB2101010";
    case GBM_FORMAT_ABGR2101010:
      return "ABGR2101010";
    case GBM_FORMAT_RGBA1010102:
      return "RGBA1010102";
    case GBM_FORMAT_BGRA1010102:
      return "BGRA1010102";

    default:
      return "<unknown>";
  }

  return NULL;
}


int
gst_gl_gbm_depth_from_format (guint32 format)
{
  if (format == GBM_BO_FORMAT_XRGB8888)
    format = GBM_FORMAT_XRGB8888;
  if (format == GBM_BO_FORMAT_ARGB8888)
    format = GBM_FORMAT_ARGB8888;

  switch (format) {
    case GBM_FORMAT_C8:
    case GBM_FORMAT_RGB332:
    case GBM_FORMAT_BGR233:
      return 8;

    case GBM_FORMAT_NV12:
    case GBM_FORMAT_XRGB4444:
    case GBM_FORMAT_XBGR4444:
    case GBM_FORMAT_RGBX4444:
    case GBM_FORMAT_BGRX4444:
      return 12;

    case GBM_FORMAT_XRGB1555:
    case GBM_FORMAT_XBGR1555:
    case GBM_FORMAT_RGBX5551:
    case GBM_FORMAT_BGRX5551:
      return 15;

    case GBM_FORMAT_ARGB4444:
    case GBM_FORMAT_ABGR4444:
    case GBM_FORMAT_RGBA4444:
    case GBM_FORMAT_BGRA4444:
    case GBM_FORMAT_ARGB1555:
    case GBM_FORMAT_ABGR1555:
    case GBM_FORMAT_RGBA5551:
    case GBM_FORMAT_BGRA5551:
    case GBM_FORMAT_RGB565:
    case GBM_FORMAT_BGR565:
    case GBM_FORMAT_YUYV:
    case GBM_FORMAT_YVYU:
    case GBM_FORMAT_UYVY:
    case GBM_FORMAT_VYUY:
      return 16;

    case GBM_FORMAT_RGB888:
    case GBM_FORMAT_BGR888:
    case GBM_FORMAT_XRGB8888:
    case GBM_FORMAT_XBGR8888:
    case GBM_FORMAT_RGBX8888:
    case GBM_FORMAT_BGRX8888:
    case GBM_FORMAT_AYUV:
      return 24;

    case GBM_FORMAT_XRGB2101010:
    case GBM_FORMAT_XBGR2101010:
    case GBM_FORMAT_RGBX1010102:
    case GBM_FORMAT_BGRX1010102:
      return 30;

    case GBM_FORMAT_ARGB8888:
    case GBM_FORMAT_ABGR8888:
    case GBM_FORMAT_RGBA8888:
    case GBM_FORMAT_BGRA8888:
    case GBM_FORMAT_ARGB2101010:
    case GBM_FORMAT_ABGR2101010:
    case GBM_FORMAT_RGBA1010102:
    case GBM_FORMAT_BGRA1010102:
      return 32;

    default:
      GST_ERROR ("unknown GBM format %" G_GUINT32_FORMAT, format);
  }

  return 0;
}


int
gst_gl_gbm_bpp_from_format (guint32 format)
{
  if (format == GBM_BO_FORMAT_XRGB8888)
    format = GBM_FORMAT_XRGB8888;
  if (format == GBM_BO_FORMAT_ARGB8888)
    format = GBM_FORMAT_ARGB8888;

  switch (format) {
    case GBM_FORMAT_C8:
    case GBM_FORMAT_RGB332:
    case GBM_FORMAT_BGR233:
      return 8;

    case GBM_FORMAT_NV12:
      return 12;

    case GBM_FORMAT_XRGB4444:
    case GBM_FORMAT_XBGR4444:
    case GBM_FORMAT_RGBX4444:
    case GBM_FORMAT_BGRX4444:
    case GBM_FORMAT_ARGB4444:
    case GBM_FORMAT_ABGR4444:
    case GBM_FORMAT_RGBA4444:
    case GBM_FORMAT_BGRA4444:
    case GBM_FORMAT_XRGB1555:
    case GBM_FORMAT_XBGR1555:
    case GBM_FORMAT_RGBX5551:
    case GBM_FORMAT_BGRX5551:
    case GBM_FORMAT_ARGB1555:
    case GBM_FORMAT_ABGR1555:
    case GBM_FORMAT_RGBA5551:
    case GBM_FORMAT_BGRA5551:
    case GBM_FORMAT_RGB565:
    case GBM_FORMAT_BGR565:
    case GBM_FORMAT_YUYV:
    case GBM_FORMAT_YVYU:
    case GBM_FORMAT_UYVY:
    case GBM_FORMAT_VYUY:
      return 16;

    case GBM_FORMAT_RGB888:
    case GBM_FORMAT_BGR888:
      return 24;

    case GBM_FORMAT_XRGB8888:
    case GBM_FORMAT_XBGR8888:
    case GBM_FORMAT_RGBX8888:
    case GBM_FORMAT_BGRX8888:
    case GBM_FORMAT_ARGB8888:
    case GBM_FORMAT_ABGR8888:
    case GBM_FORMAT_RGBA8888:
    case GBM_FORMAT_BGRA8888:
    case GBM_FORMAT_XRGB2101010:
    case GBM_FORMAT_XBGR2101010:
    case GBM_FORMAT_RGBX1010102:
    case GBM_FORMAT_BGRX1010102:
    case GBM_FORMAT_ARGB2101010:
    case GBM_FORMAT_ABGR2101010:
    case GBM_FORMAT_RGBA1010102:
    case GBM_FORMAT_BGRA1010102:
    case GBM_FORMAT_AYUV:
      return 32;

    default:
      GST_ERROR ("unknown GBM format %" G_GUINT32_FORMAT, format);
  }

  return 0;
}


static void
gst_gl_gbm_drm_fb_destroy_callback (struct gbm_bo *bo, void *data)
{
  int drm_fd = gbm_device_get_fd (gbm_bo_get_device (bo));
  GstGLDRMFramebuffer *fb = (GstGLDRMFramebuffer *) (data);

  if (fb->fb_id)
    drmModeRmFB (drm_fd, fb->fb_id);

  g_free (fb);
}


GstGLDRMFramebuffer *
gst_gl_gbm_drm_fb_get_from_bo (struct gbm_bo *bo)
{
  GstGLDRMFramebuffer *fb;
  int drm_fd;
  guint32 width, height, stride, format, handle;
  int depth, bpp;
  int ret;

  /* We want to use this buffer object (abbr. "bo") as a scanout buffer.
   * To that end, we associate the bo with the DRM by using drmModeAddFB().
   * However, this needs to be called exactly once for the given bo, and the
   * counterpart, drmModeRmFB(), needs to be called when the bo is cleaned up.
   *
   * To fulfill these requirements, add extra framebuffer information to the
   * bo as "user data". This way, if this user data pointer is NULL, it means
   * that no framebuffer information was generated yet & the bo was not set
   * as a scanout buffer with drmModeAddFB() yet, and we have perform these
   * steps. Otherwise, if it is non-NULL, we know we do not have to set up
   * anything (since it was done already) and just return the pointer to the
   * framebuffer information. */
  fb = (GstGLDRMFramebuffer *) (gbm_bo_get_user_data (bo));
  if (fb != NULL) {
    /* The bo was already set up as a scanout framebuffer. Just
     * return the framebuffer information. */
    return fb;
  }

  /* If this point is reached, then we have to setup the bo as a
   * scanout framebuffer. */

  drm_fd = gbm_device_get_fd (gbm_bo_get_device (bo));

  fb = g_new0 (GstGLDRMFramebuffer, 1);
  fb->bo = bo;

  width = gbm_bo_get_width (bo);
  height = gbm_bo_get_height (bo);
  stride = gbm_bo_get_stride (bo);
  format = gbm_bo_get_format (bo);
  handle = gbm_bo_get_handle (bo).u32;

  depth = gst_gl_gbm_depth_from_format (format);
  bpp = gst_gl_gbm_bpp_from_format (format);

  GST_DEBUG ("Attempting to add GBM BO as scanout framebuffer width/height: %"
      G_GUINT32_FORMAT "/%" G_GUINT32_FORMAT " pixels  stride: %"
      G_GUINT32_FORMAT " bytes  format: %s  depth: %d bits  total bpp: %d bits",
      width, height, stride, gst_gl_gbm_format_to_string (format), depth, bpp);

  /* Set the bo as a scanout framebuffer */
  ret = drmModeAddFB (drm_fd, width, height, depth, bpp, stride, handle,
      &fb->fb_id);
  if (ret != 0) {
    GST_ERROR ("Failed to add GBM BO as scanout framebuffer: %s (%d)",
        g_strerror (errno), errno);
    g_free (fb);
    return NULL;
  }

  /* Add the framebuffer information to the bo as user data, and also install a callback
   * that cleans up this extra information whenever the bo itself is discarded */
  gbm_bo_set_user_data (bo, fb, gst_gl_gbm_drm_fb_destroy_callback);

  return fb;
}


int
gst_gl_gbm_find_and_open_drm_node (void)
{
  /* In here we use GUDev to try to autodetect the GPU */

  int drm_fd = -1;
  GUdevClient *gudev_client = NULL;
  GUdevEnumerator *gudev_enum = NULL;
  GList *devlist = NULL;
  GList *deventry = NULL;
  const gchar *subsystems[2] = { "drm", NULL };

  gudev_client = g_udev_client_new (subsystems);
  if (gudev_client == NULL) {
    GST_ERROR ("Could not create gudev client");
    goto cleanup;
  }
  GST_DEBUG ("Created gudev client");

  gudev_enum = g_udev_enumerator_new (gudev_client);
  if (gudev_enum == NULL) {
    GST_ERROR ("Could not create gudev enumerator");
    goto cleanup;
  }
  GST_DEBUG ("Created gudev enumerator");

  /* TODO: To be 100% sure we pick the right device, also check
   * if this is a GPU, because a pure scanout device could also
   * have a DRM subsystem for example. However, currently it is
   * unclear how to do that. By trying to create an EGL context? */
  g_udev_enumerator_add_match_subsystem (gudev_enum, "drm");
  devlist = g_udev_enumerator_execute (gudev_enum);
  GST_DEBUG ("Scanned for udev devices with a drm subsystem");

  if (devlist == NULL) {
    GST_WARNING ("Found no matching DRM devices");
    goto cleanup;
  }
  GST_DEBUG ("Got %u potentially matching device(s)", g_list_length (devlist));

  for (deventry = devlist; deventry != NULL; deventry = deventry->next) {
    GUdevDevice *gudevice = G_UDEV_DEVICE (deventry->data);
    const gchar *devnode = g_udev_device_get_device_file (gudevice);

    if ((devnode == NULL) || !g_str_has_prefix (devnode, "/dev/dri/card"))
      continue;

    GST_DEBUG ("Found DRM device with device node \"%s\"", devnode);

    drm_fd = open (devnode, O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
      GST_WARNING ("Cannot open device node \"%s\": %s (%d)", devnode,
          g_strerror (errno), errno);
      continue;
    }

    GST_DEBUG ("Device node \"%s\" is a valid DRM device node", devnode);
    break;
  }


done:

  if (devlist != NULL) {
    g_list_free_full (devlist, g_object_unref);
    devlist = NULL;
    GST_DEBUG ("Cleaned up device list");
  }

  if (gudev_enum != NULL) {
    g_object_unref (G_OBJECT (gudev_enum));
    gudev_enum = NULL;
    GST_DEBUG ("Cleaned up gudev enumerator");
  }

  if (gudev_client != NULL) {
    g_object_unref (G_OBJECT (gudev_client));
    gudev_client = NULL;
    GST_DEBUG ("Cleaned up gudev client");
  }

  return drm_fd;


cleanup:

  if (drm_fd >= 0) {
    close (drm_fd);
    drm_fd = -1;
  }

  goto done;
}
