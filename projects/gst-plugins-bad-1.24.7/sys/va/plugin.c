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
 * plugin-va:
 *
 * Since: 1.18
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvaav1dec.h"
#include "gstvaav1enc.h"
#include "gstvacaps.h"
#include "gstvacompositor.h"
#include "gstvadeinterlace.h"
#include "gstvadevice.h"
#include "gstvafilter.h"
#include "gstvah264dec.h"
#include "gstvah264enc.h"
#include "gstvah265dec.h"
#include "gstvah265enc.h"
#include "gstvajpegdec.h"
#include "gstvampeg2dec.h"
#include "gstvaprofile.h"
#include "gstvavp8dec.h"
#include "gstvavp9dec.h"
#include "gstvavpp.h"

#define GST_CAT_DEFAULT gstva_debug
GST_DEBUG_CATEGORY (gstva_debug);

#ifdef G_OS_WIN32
/* Windows support is still experimental */
#define GST_VA_RANK_PRIMARY GST_RANK_NONE
#else
#define GST_VA_RANK_PRIMARY (GST_RANK_PRIMARY + 1)
#endif

static void
plugin_add_dependencies (GstPlugin * plugin)
{
#ifndef G_OS_WIN32
  const gchar *env_vars[] = { "LIBVA_DRIVER_NAME", "GST_VA_ALL_DRIVERS", NULL };
  const gchar *kernel_paths[] = { "/dev/dri", NULL };
  const gchar *kernel_names[] = { "renderD", NULL };

  /* features get updated upon changes in /dev/dri/renderD* */
  gst_plugin_add_dependency (plugin, NULL, kernel_paths, kernel_names,
      GST_PLUGIN_DEPENDENCY_FLAG_FILE_NAME_IS_PREFIX);

  /* features get updated upon changes on LIBVA_DRIVER_NAME and
   * GST_VA_ALL_DRIVERS envvar */
  gst_plugin_add_dependency (plugin, env_vars, NULL, NULL,
      GST_PLUGIN_DEPENDENCY_FLAG_NONE);

  /* features get updated upon changes in default VA drivers
   * directory */
  gst_plugin_add_dependency_simple (plugin, "LIBVA_DRIVERS_PATH",
      LIBVA_DRIVERS_PATH, "_drv_video.so",
      GST_PLUGIN_DEPENDENCY_FLAG_FILE_NAME_IS_SUFFIX |
      GST_PLUGIN_DEPENDENCY_FLAG_PATHS_ARE_DEFAULT_ONLY);
#endif
}

static void
plugin_register_decoders (GstPlugin * plugin, GstVaDevice * device,
    GHashTable * decoders)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, decoders);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    guint32 codec = *((gint64 *) key);
    GArray *profiles = (GArray *) value;
    GstCaps *sinkcaps = NULL, *srccaps = NULL;

    if (!profiles || profiles->len == 0)
      continue;

    if (!gst_va_caps_from_profiles (device->display, profiles, VAEntrypointVLD,
            &sinkcaps, &srccaps))
      continue;

    GST_LOG ("%d decoder codec: %" GST_FOURCC_FORMAT, profiles->len,
        GST_FOURCC_ARGS (codec));
    GST_LOG ("sink caps: %" GST_PTR_FORMAT, sinkcaps);
    GST_LOG ("src caps: %" GST_PTR_FORMAT, srccaps);

    switch (codec) {
      case H264:
        if (!gst_va_h264_dec_register (plugin, device, sinkcaps, srccaps,
                GST_VA_RANK_PRIMARY)) {
          GST_WARNING ("Failed to register H264 decoder: %s",
              device->render_device_path);
        }
        break;
      case HEVC:
        if (!gst_va_h265_dec_register (plugin, device, sinkcaps, srccaps,
                GST_VA_RANK_PRIMARY)) {
          GST_WARNING ("Failed to register H265 decoder: %s",
              device->render_device_path);
        }
        break;
      case VP8:
        if (!gst_va_vp8_dec_register (plugin, device, sinkcaps, srccaps,
                GST_VA_RANK_PRIMARY)) {
          GST_WARNING ("Failed to register VP8 decoder: %s",
              device->render_device_path);
        }
        break;
      case VP9:
        if (!gst_va_vp9_dec_register (plugin, device, sinkcaps, srccaps,
                GST_VA_RANK_PRIMARY)) {
          GST_WARNING ("Failed to register VP9 decoder: %s",
              device->render_device_path);
        }
        break;
      case MPEG2:
        if (!gst_va_mpeg2_dec_register (plugin, device, sinkcaps, srccaps,
                GST_VA_RANK_PRIMARY)) {
          GST_WARNING ("Failed to register Mpeg2 decoder: %s",
              device->render_device_path);
        }
        break;
      case AV1:
        if (!gst_va_av1_dec_register (plugin, device, sinkcaps, srccaps,
                GST_VA_RANK_PRIMARY)) {
          GST_WARNING ("Failed to register AV1 decoder: %s",
              device->render_device_path);
        }
        break;
      case JPEG:
        if (!gst_va_jpeg_dec_register (plugin, device, sinkcaps, srccaps,
                GST_RANK_NONE)) {
          GST_WARNING ("Failed to register JPEG decoder: %s",
              device->render_device_path);
        }
        break;
      default:
        GST_DEBUG ("No decoder implementation for %" GST_FOURCC_FORMAT,
            GST_FOURCC_ARGS (codec));
        break;
    }

    gst_caps_unref (srccaps);
    gst_caps_unref (sinkcaps);
  }
}

static void
plugin_register_encoders (GstPlugin * plugin, GstVaDevice * device,
    GHashTable * encoders, VAEntrypoint entrypoint)
{
  GHashTableIter iter;
  gpointer key, value;

  if (GST_VA_DISPLAY_IS_IMPLEMENTATION (device->display, INTEL_I965)
      && g_getenv ("GST_VA_ALL_DRIVERS") == NULL) {
    gst_plugin_add_status_warning (plugin,
        "The Intel i965 VA driver is blocklisted for encoding, use GST_VA_ALL_DRIVERS to overwrite.");
    return;
  }

  g_hash_table_iter_init (&iter, encoders);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    guint32 codec = *((gint64 *) key);
    GArray *profiles = (GArray *) value;
    GstCaps *sinkcaps = NULL, *srccaps = NULL;

    if (!profiles || profiles->len == 0)
      continue;

    if (!gst_va_caps_from_profiles (device->display, profiles, entrypoint,
            &srccaps, &sinkcaps))
      continue;

    GST_LOG ("%d encoder %scodec: %" GST_FOURCC_FORMAT, profiles->len,
        (entrypoint == VAEntrypointEncSliceLP) ? "low power " : "",
        GST_FOURCC_ARGS (codec));
    GST_LOG ("sink caps: %" GST_PTR_FORMAT, sinkcaps);
    GST_LOG ("src caps: %" GST_PTR_FORMAT, srccaps);

    switch (codec) {
      case H264:
        if (!gst_va_h264_enc_register (plugin, device, sinkcaps, srccaps,
                GST_RANK_NONE, entrypoint)) {
          GST_WARNING ("Failed to register H264 encoder: %s",
              device->render_device_path);
        }
        break;
      case HEVC:
        if (!gst_va_h265_enc_register (plugin, device, sinkcaps, srccaps,
                GST_RANK_NONE, entrypoint)) {
          GST_WARNING ("Failed to register H265 encoder: %s",
              device->render_device_path);
        }
        break;
#if VA_CHECK_VERSION(1, 15, 0)
      case AV1:
        if (!gst_va_av1_enc_register (plugin, device, sinkcaps, srccaps,
                GST_RANK_NONE, entrypoint)) {
          GST_WARNING ("Failed to register AV1 encoder: %s",
              device->render_device_path);
        }
        break;
#endif
      default:
        GST_DEBUG ("No encoder implementation for %" GST_FOURCC_FORMAT,
            GST_FOURCC_ARGS (codec));
        break;
    }

    gst_caps_unref (srccaps);
    gst_caps_unref (sinkcaps);
  }
}

static void
plugin_register_vpp (GstPlugin * plugin, GstVaDevice * device)
{
  GstVaFilter *filter;
  gboolean has_colorbalance, has_deinterlace, has_compose;

  has_colorbalance = FALSE;
  has_deinterlace = FALSE;
  has_compose = FALSE;
  filter = gst_va_filter_new (device->display);
  if (gst_va_filter_open (filter)) {
    has_colorbalance =
        gst_va_filter_has_filter (filter, VAProcFilterColorBalance);
    has_deinterlace =
        gst_va_filter_has_filter (filter, VAProcFilterDeinterlacing);
    has_compose = gst_va_filter_has_compose (filter);
  } else {
    GST_WARNING ("Failed open VA filter");
    gst_object_unref (filter);
    return;
  }
  gst_object_unref (filter);

  if (!gst_va_vpp_register (plugin, device, has_colorbalance, GST_RANK_NONE))
    GST_WARNING ("Failed to register postproc: %s", device->render_device_path);

  if (has_deinterlace) {
    if (!gst_va_deinterlace_register (plugin, device, GST_RANK_NONE)) {
      GST_WARNING ("Failed to register deinterlace: %s",
          device->render_device_path);
    }
  }

  if (has_compose) {
    if (!gst_va_compositor_register (plugin, device, GST_RANK_NONE)) {
      GST_WARNING ("Failed to register compositor: %s",
          device->render_device_path);
    }
  }
}

static inline void
_insert_profile_in_table (GHashTable * table, VAProfile profile)
{
  gint64 codec = gst_va_profile_codec (profile);
  GArray *profiles;

  if (codec == GST_MAKE_FOURCC ('N', 'O', 'N', 'E'))
    return;

  profiles = g_hash_table_lookup (table, &codec);
  if (!profiles) {
    gint64 *codec_ptr = g_new (gint64, 1);

    *codec_ptr = codec;
    profiles = g_array_new (FALSE, FALSE, sizeof (VAProfile));
    g_hash_table_insert (table, codec_ptr, profiles);
  }
  g_array_append_val (profiles, profile);
}

static gboolean
plugin_register_elements (GstPlugin * plugin, GstVaDevice * device)
{
  VADisplay dpy = gst_va_display_get_va_dpy (device->display);
  VAEntrypoint *entrypoints = g_new (VAEntrypoint, vaMaxNumEntrypoints (dpy));
  VAProfile *profiles = g_new (VAProfile, vaMaxNumProfiles (dpy));
  VAStatus status;
  GHashTable *decoders, *encoders, *encoderslp, *encodersimg;
  gint i, j, num_entrypoints = 0, num_profiles = 0;
  gboolean has_vpp = FALSE, ret = FALSE;

  decoders = g_hash_table_new_full (g_int64_hash, g_int64_equal,
      (GDestroyNotify) g_free, (GDestroyNotify) g_array_unref);
  encoders = g_hash_table_new_full (g_int64_hash, g_int64_equal,
      (GDestroyNotify) g_free, (GDestroyNotify) g_array_unref);
  encoderslp = g_hash_table_new_full (g_int64_hash, g_int64_equal,
      (GDestroyNotify) g_free, (GDestroyNotify) g_array_unref);
  encodersimg = g_hash_table_new_full (g_int64_hash, g_int64_equal,
      (GDestroyNotify) g_free, (GDestroyNotify) g_array_unref);

  status = vaQueryConfigProfiles (dpy, profiles, &num_profiles);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING ("vaQueryConfigProfile: %s", vaErrorStr (status));
    goto bail;
  }

  for (i = 0; i < num_profiles; i++) {
    status = vaQueryConfigEntrypoints (dpy, profiles[i], entrypoints,
        &num_entrypoints);
    if (status != VA_STATUS_SUCCESS) {
      GST_WARNING ("vaQueryConfigEntrypoints: %s", vaErrorStr (status));
      goto bail;
    }

    for (j = 0; j < num_entrypoints; j++) {
      if (entrypoints[j] == VAEntrypointVLD)
        _insert_profile_in_table (decoders, profiles[i]);
      else if (entrypoints[j] == VAEntrypointEncSlice)
        _insert_profile_in_table (encoders, profiles[i]);
      else if (entrypoints[j] == VAEntrypointEncSliceLP)
        _insert_profile_in_table (encoderslp, profiles[i]);
      else if (entrypoints[j] == VAEntrypointEncPicture)
        _insert_profile_in_table (encodersimg, profiles[i]);
      else if (entrypoints[j] == VAEntrypointVideoProc)
        has_vpp = TRUE;
    }
  }

  plugin_register_decoders (plugin, device, decoders);
  plugin_register_encoders (plugin, device, encoders, VAEntrypointEncSlice);
  plugin_register_encoders (plugin, device, encoderslp, VAEntrypointEncSliceLP);
  plugin_register_encoders (plugin, device, encodersimg,
      VAEntrypointEncPicture);
  if (has_vpp)
    plugin_register_vpp (plugin, device);

  ret = TRUE;

bail:
  g_hash_table_unref (encodersimg);
  g_hash_table_unref (encoderslp);
  g_hash_table_unref (encoders);
  g_hash_table_unref (decoders);
  g_free (entrypoints);
  g_free (profiles);

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GList *devices, *dev;

  GST_DEBUG_CATEGORY_INIT (gstva_debug, "va", 0, "VA general debug");

  plugin_add_dependencies (plugin);

  devices = gst_va_device_find_devices ();
  for (dev = devices; dev; dev = g_list_next (dev))
    plugin_register_elements (plugin, dev->data);

  gst_va_device_list_free (devices);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    va, "VA-API codecs plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
