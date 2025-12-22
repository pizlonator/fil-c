/*
 * Copyright (C) 2012, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2015, Sebastian Dröge <sebastian@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ORC
#include <orc/orc.h>
#else
#define orc_memcpy memcpy
#endif

#ifdef HAVE_JNI_H
#include "gstahcsrc.h"
#include "gstahssrc.h"
#include "gstjniutils.h"
#endif

#include "gstamc.h"
#include "gstamc-constants.h"

#include "gstamcvideodec.h"
#include "gstamcvideoenc.h"
#include "gstamcaudiodec.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <string.h>

GST_DEBUG_CATEGORY (gst_amc_debug);
#define GST_CAT_DEFAULT gst_amc_debug

GQuark gst_amc_codec_info_quark = 0;

static GQueue codec_infos = G_QUEUE_INIT;
#ifdef GST_AMC_IGNORE_UNKNOWN_COLOR_FORMATS
static gboolean ignore_unknown_color_formats = TRUE;
#else
static gboolean ignore_unknown_color_formats = FALSE;
#endif

static gboolean accepted_color_formats (GstAmcCodecType * type,
    gboolean is_encoder);

static gboolean
scan_codecs (GstPlugin * plugin)
{
  gboolean ret = TRUE;
  gint codec_count, i;
  const GstStructure *cache_data;
  GError *error = NULL;

  GST_DEBUG ("Scanning codecs");

  if ((cache_data = gst_plugin_get_cache_data (plugin))) {
    const GValue *arr = gst_structure_get_value (cache_data, "codecs");
    guint i, n;

    GST_DEBUG ("Getting codecs from cache");
    n = gst_value_array_get_size (arr);
    for (i = 0; i < n; i++) {
      const GValue *cv = gst_value_array_get_value (arr, i);
      const GstStructure *cs = gst_value_get_structure (cv);
      const gchar *name;
      gboolean is_encoder;
      const GValue *starr;
      guint j, n2;
      GstAmcCodecInfo *gst_codec_info;

      gst_codec_info = g_new0 (GstAmcCodecInfo, 1);

      name = gst_structure_get_string (cs, "name");
      gst_structure_get_boolean (cs, "is-encoder", &is_encoder);
      gst_codec_info->name = g_strdup (name);
      gst_codec_info->is_encoder = is_encoder;

      starr = gst_structure_get_value (cs, "supported-types");
      n2 = gst_value_array_get_size (starr);

      gst_codec_info->n_supported_types = n2;
      gst_codec_info->supported_types = g_new0 (GstAmcCodecType, n2);

      for (j = 0; j < n2; j++) {
        const GValue *stv = gst_value_array_get_value (starr, j);
        const GstStructure *sts = gst_value_get_structure (stv);
        const gchar *mime;
        const GValue *cfarr;
        const GValue *plarr;
        guint k, n3;
        GstAmcCodecType *gst_codec_type = &gst_codec_info->supported_types[j];

        mime = gst_structure_get_string (sts, "mime");
        gst_codec_type->mime = g_strdup (mime);

        cfarr = gst_structure_get_value (sts, "color-formats");
        n3 = gst_value_array_get_size (cfarr);

        gst_codec_type->n_color_formats = n3;
        gst_codec_type->color_formats = g_new0 (gint, n3);

        for (k = 0; k < n3; k++) {
          const GValue *cfv = gst_value_array_get_value (cfarr, k);
          gint cf = g_value_get_int (cfv);

          gst_codec_type->color_formats[k] = cf;
        }

        plarr = gst_structure_get_value (sts, "profile-levels");
        n3 = gst_value_array_get_size (plarr);

        gst_codec_type->n_profile_levels = n3;
        gst_codec_type->profile_levels =
            g_malloc0 (sizeof (gst_codec_type->profile_levels[0]) * n3);

        for (k = 0; k < n3; k++) {
          const GValue *plv = gst_value_array_get_value (plarr, k);
          const GValue *p, *l;

          p = gst_value_array_get_value (plv, 0);
          l = gst_value_array_get_value (plv, 1);
          gst_codec_type->profile_levels[k].profile = g_value_get_int (p);
          gst_codec_type->profile_levels[k].level = g_value_get_int (l);
        }
      }

      g_queue_push_tail (&codec_infos, gst_codec_info);
    }

    return TRUE;
  }

  if (!gst_amc_codeclist_get_count (&codec_count, &error)) {
    GST_ERROR ("Failed to get number of available codecs");
    ret = FALSE;
    goto done;
  }

  GST_INFO ("Found %d available codecs", codec_count);

  for (i = 0; i < codec_count; i++) {
    GstAmcCodecInfo *gst_codec_info;
    GstAmcCodecInfoHandle *codec_info = NULL;
    gchar *name_str = NULL;
    gboolean is_encoder;
    gchar **supported_types = NULL;
    gsize n_supported_types;
    gsize j;
    gboolean valid_codec = TRUE;

    gst_codec_info = g_new0 (GstAmcCodecInfo, 1);

    codec_info = gst_amc_codeclist_get_codec_info_at (i, &error);
    if (!codec_info) {
      GST_ERROR ("Failed to get codec info %d", i);
      valid_codec = FALSE;
      goto next_codec;
    }

    name_str = gst_amc_codec_info_handle_get_name (codec_info, &error);
    if (!name_str) {
      GST_ERROR ("Failed to get codec name");
      valid_codec = FALSE;
      goto next_codec;
    }

    GST_INFO ("Checking codec '%s'", name_str);

    /* Compatibility codec names */
    if (strcmp (name_str, "AACEncoder") == 0 ||
        strcmp (name_str, "OMX.google.raw.decoder") == 0) {
      GST_INFO ("Skipping compatibility codec '%s'", name_str);
      valid_codec = FALSE;
      goto next_codec;
    }

    if (g_str_has_suffix (name_str, ".secure")) {
      GST_INFO ("Skipping DRM codec '%s'", name_str);
      valid_codec = FALSE;
      goto next_codec;
    }
#ifdef HAVE_JNI_H
    /* FIXME: Non-Google codecs usually just don't work and hang forever
     * or crash when not used from a process that started the Java
     * VM via the non-public AndroidRuntime class. Can we somehow
     * initialize all this?
     */
    if (gst_amc_jni_is_vm_started () &&
        !g_str_has_prefix (name_str, "OMX.google.")) {
      GST_INFO ("Skipping non-Google codec '%s' in standalone mode", name_str);
      valid_codec = FALSE;
      goto next_codec;
    }
#endif

    if (g_str_has_prefix (name_str, "OMX.ARICENT.")) {
      GST_INFO ("Skipping possible broken codec '%s'", name_str);
      valid_codec = FALSE;
      goto next_codec;
    }

    /* FIXME:
     *   - Vorbis: Generates clicks for multi-channel streams
     *   - *Law: Generates output with too low frequencies
     */
    if (strcmp (name_str, "OMX.google.vorbis.decoder") == 0 ||
        strcmp (name_str, "OMX.google.g711.alaw.decoder") == 0 ||
        strcmp (name_str, "OMX.google.g711.mlaw.decoder") == 0) {
      GST_INFO ("Skipping known broken codec '%s'", name_str);
      valid_codec = FALSE;
      goto next_codec;
    }
    gst_codec_info->name = g_strdup (name_str);

    if (!gst_amc_codec_info_handle_is_encoder (codec_info, &is_encoder, &error)) {
      GST_ERROR ("Failed to detect if codec is an encoder");
      valid_codec = FALSE;
      goto next_codec;
    }
    gst_codec_info->is_encoder = is_encoder;
    gst_codec_info->gl_output_only = FALSE;

    supported_types =
        gst_amc_codec_info_handle_get_supported_types (codec_info,
        &n_supported_types, &error);
    if (!supported_types) {
      GST_ERROR ("Failed to get supported types");
      valid_codec = FALSE;
      goto next_codec;
    }

    GST_INFO ("Codec '%s' has %" G_GSIZE_FORMAT " supported types", name_str,
        n_supported_types);

    gst_codec_info->supported_types =
        g_new0 (GstAmcCodecType, n_supported_types);
    gst_codec_info->n_supported_types = n_supported_types;

    if (n_supported_types == 0) {
      valid_codec = FALSE;
      GST_ERROR ("Codec has no supported types");
      goto next_codec;
    }

    for (j = 0; j < n_supported_types; j++) {
      GstAmcCodecType *gst_codec_type;
      const gchar *supported_type_str;
      GstAmcCodecCapabilitiesHandle *capabilities = NULL;
      gint k;

      gst_codec_type = &gst_codec_info->supported_types[j];
      supported_type_str = supported_types[j];

      GST_INFO ("Supported type '%s'", supported_type_str);
      gst_codec_type->mime = g_strdup (supported_type_str);

      capabilities =
          gst_amc_codec_info_handle_get_capabilities_for_type (codec_info,
          supported_type_str, &error);
      if (!capabilities) {
        GST_ERROR ("Failed to get capabilities for supported type");
        valid_codec = FALSE;
        goto next_supported_type;
      }

      if (g_str_has_prefix (gst_codec_type->mime, "video/")) {
        gst_codec_type->color_formats =
            gst_amc_codec_capabilities_handle_get_color_formats (capabilities,
            &gst_codec_type->n_color_formats, &error);
        if (!gst_codec_type->color_formats) {
          GST_ERROR ("Failed to get color format elements");
          valid_codec = FALSE;
          goto next_supported_type;
        }

        for (k = 0; k < gst_codec_type->n_color_formats; k++) {
          GST_INFO ("Color format %d: 0x%x", k,
              gst_codec_type->color_formats[k]);
        }

        if (!gst_codec_type->n_color_formats) {
          GST_ERROR ("No supported color formats for video codec");
          valid_codec = FALSE;
          goto next_supported_type;
        }

        if (!accepted_color_formats (gst_codec_type, is_encoder)) {
          if (!ignore_unknown_color_formats) {
            gst_codec_info->gl_output_only = TRUE;
            GST_WARNING
                ("%s %s has unknown color formats, only direct rendering will be supported",
                gst_codec_type->mime, is_encoder ? "encoder" : "decoder");
          }
        }
      }

      gst_codec_type->profile_levels =
          gst_amc_codec_capabilities_handle_get_profile_levels (capabilities,
          &gst_codec_type->n_profile_levels, &error);
      if (error) {
        GST_ERROR ("Failed to get profile/levels: %s", error->message);
        valid_codec = FALSE;
        goto next_supported_type;
      }

      for (k = 0; k < gst_codec_type->n_profile_levels; k++) {
        GST_INFO ("Level %d: 0x%08x", k,
            gst_codec_type->profile_levels[k].level);
        GST_INFO ("Profile %d: 0x%08x", k,
            gst_codec_type->profile_levels[k].profile);
      }

    next_supported_type:
      if (capabilities)
        gst_amc_codec_capabilities_handle_free (capabilities);
      capabilities = NULL;
      g_clear_error (&error);
      if (!valid_codec)
        break;
    }

    /* We need at least a valid supported type */
    if (valid_codec) {
      GList *l;

      for (l = codec_infos.head; l; l = l->next) {
        GstAmcCodecInfo *tmp = l->data;

        if (strcmp (tmp->name, gst_codec_info->name) == 0
            && !!tmp->is_encoder == !!gst_codec_info->is_encoder) {
          gint m = tmp->n_supported_types, n;

          GST_LOG ("Successfully scanned codec '%s', appending to existing",
              name_str);

          tmp->gl_output_only |= gst_codec_info->gl_output_only;
          tmp->n_supported_types += gst_codec_info->n_supported_types;
          tmp->supported_types =
              g_realloc (tmp->supported_types,
              tmp->n_supported_types * sizeof (GstAmcCodecType));

          for (n = 0; n < gst_codec_info->n_supported_types; n++, m++) {
            tmp->supported_types[m] = gst_codec_info->supported_types[n];
          }
          g_free (gst_codec_info->supported_types);
          g_free (gst_codec_info->name);
          g_free (gst_codec_info);
          gst_codec_info = NULL;

          break;
        }
      }

      /* Found no existing codec with this name */
      if (l == NULL) {
        GST_LOG ("Successfully scanned codec '%s'", name_str);
        g_queue_push_tail (&codec_infos, gst_codec_info);
        gst_codec_info = NULL;
      }
    }

    /* Clean up of all local references we got */
  next_codec:
    if (name_str)
      g_free (name_str);
    name_str = NULL;
    if (supported_types)
      g_strfreev (supported_types);
    supported_types = NULL;
    if (codec_info)
      gst_amc_codec_info_handle_free (codec_info);
    codec_info = NULL;
    if (gst_codec_info) {
      gint j;

      for (j = 0; j < gst_codec_info->n_supported_types; j++) {
        GstAmcCodecType *gst_codec_type = &gst_codec_info->supported_types[j];

        g_free (gst_codec_type->mime);
        g_free (gst_codec_type->color_formats);
        g_free (gst_codec_type->profile_levels);
      }
      g_free (gst_codec_info->supported_types);
      g_free (gst_codec_info->name);
      g_free (gst_codec_info);
    }
    gst_codec_info = NULL;
    valid_codec = TRUE;
    g_clear_error (&error);
  }

  ret = codec_infos.length != 0;

  /* If successful we store a cache of the codec information in
   * the registry. Otherwise we would always load all codecs during
   * plugin initialization which can take quite some time (because
   * of hardware) and also loads lots of shared libraries (which
   * number is limited by 64 in Android).
   */
  if (ret) {
    GstStructure *new_cache_data = gst_structure_new_empty ("gst-amc-cache");
    GList *l;
    GValue arr = { 0, };

    g_value_init (&arr, GST_TYPE_ARRAY);

    for (l = codec_infos.head; l; l = l->next) {
      GstAmcCodecInfo *gst_codec_info = l->data;
      GValue cv = { 0, };
      GstStructure *cs = gst_structure_new_empty ("gst-amc-codec");
      GValue starr = { 0, };
      gint i;

      gst_structure_set (cs, "name", G_TYPE_STRING, gst_codec_info->name,
          "is-encoder", G_TYPE_BOOLEAN, gst_codec_info->is_encoder, NULL);

      g_value_init (&starr, GST_TYPE_ARRAY);

      for (i = 0; i < gst_codec_info->n_supported_types; i++) {
        GstAmcCodecType *gst_codec_type = &gst_codec_info->supported_types[i];
        GstStructure *sts = gst_structure_new_empty ("gst-amc-supported-type");
        GValue stv = { 0, };
        GValue tmparr = { 0, };
        gint j;

        gst_structure_set (sts, "mime", G_TYPE_STRING, gst_codec_type->mime,
            NULL);

        g_value_init (&tmparr, GST_TYPE_ARRAY);
        for (j = 0; j < gst_codec_type->n_color_formats; j++) {
          GValue tmp = { 0, };

          g_value_init (&tmp, G_TYPE_INT);
          g_value_set_int (&tmp, gst_codec_type->color_formats[j]);
          gst_value_array_append_value (&tmparr, &tmp);
          g_value_unset (&tmp);
        }
        gst_structure_set_value (sts, "color-formats", &tmparr);
        g_value_unset (&tmparr);

        g_value_init (&tmparr, GST_TYPE_ARRAY);
        for (j = 0; j < gst_codec_type->n_profile_levels; j++) {
          GValue tmparr2 = { 0, };
          GValue tmp = { 0, };

          g_value_init (&tmparr2, GST_TYPE_ARRAY);
          g_value_init (&tmp, G_TYPE_INT);
          g_value_set_int (&tmp, gst_codec_type->profile_levels[j].profile);
          gst_value_array_append_value (&tmparr2, &tmp);
          g_value_set_int (&tmp, gst_codec_type->profile_levels[j].level);
          gst_value_array_append_value (&tmparr2, &tmp);
          gst_value_array_append_value (&tmparr, &tmparr2);
          g_value_unset (&tmp);
          g_value_unset (&tmparr2);
        }
        gst_structure_set_value (sts, "profile-levels", &tmparr);

        g_value_init (&stv, GST_TYPE_STRUCTURE);
        gst_value_set_structure (&stv, sts);
        gst_value_array_append_value (&starr, &stv);
        g_value_unset (&tmparr);
        gst_structure_free (sts);
      }

      gst_structure_set_value (cs, "supported-types", &starr);
      g_value_unset (&starr);

      g_value_init (&cv, GST_TYPE_STRUCTURE);
      gst_value_set_structure (&cv, cs);
      gst_value_array_append_value (&arr, &cv);
      g_value_unset (&cv);
      gst_structure_free (cs);
    }

    gst_structure_set_value (new_cache_data, "codecs", &arr);
    g_value_unset (&arr);

    gst_plugin_set_cache_data (plugin, new_cache_data);
  }

done:
  g_clear_error (&error);

  return ret;
}

static const struct
{
  gint color_format;
  GstVideoFormat video_format;
} color_format_mapping_table[] = {
  {
      COLOR_FormatYUV420Planar, GST_VIDEO_FORMAT_I420}, {
      COLOR_FormatYUV420Flexible, GST_VIDEO_FORMAT_I420}, {
      COLOR_FormatYUV420SemiPlanar, GST_VIDEO_FORMAT_NV12}, {
      COLOR_TI_FormatYUV420PackedSemiPlanar, GST_VIDEO_FORMAT_NV12}, {
      COLOR_TI_FormatYUV420PackedSemiPlanarInterlaced, GST_VIDEO_FORMAT_NV12}, {
      COLOR_INTEL_FormatYUV420PackedSemiPlanar, GST_VIDEO_FORMAT_NV12}, {
      COLOR_INTEL_FormatYUV420PackedSemiPlanar_Tiled, GST_VIDEO_FORMAT_NV12}, {
      COLOR_QCOM_FormatYUV420SemiPlanar, GST_VIDEO_FORMAT_NV12}, {
        COLOR_QCOM_FormatYUV420PackedSemiPlanar64x32Tile2m8ka,
      GST_VIDEO_FORMAT_NV12}, {
      COLOR_QCOM_FormatYVU420SemiPlanar32m, GST_VIDEO_FORMAT_NV12}, {
      COLOR_QCOM_FormatYVU420SemiPlanar32mMultiView, GST_VIDEO_FORMAT_NV12}, {
      COLOR_OMX_SEC_FormatNV12Tiled, GST_VIDEO_FORMAT_NV12}, {
      COLOR_FormatYCbYCr, GST_VIDEO_FORMAT_YUY2}, {
      COLOR_FormatYV12, GST_VIDEO_FORMAT_YV12}
};

static gboolean
accepted_color_formats (GstAmcCodecType * type, gboolean is_encoder)
{
  gint i, j;
  gint accepted = 0, all = type->n_color_formats;

  for (i = 0; i < type->n_color_formats; i++) {
    gboolean found = FALSE;
    /* We ignore this one */
    if (type->color_formats[i] == COLOR_FormatAndroidOpaque) {
      all--;
      continue;
    }

    for (j = 0; j < G_N_ELEMENTS (color_format_mapping_table); j++) {
      if (color_format_mapping_table[j].color_format == type->color_formats[i]) {
        found = TRUE;
        accepted++;
        break;
      }
    }

    if (!found) {
      GST_ERROR ("Unknown color format 0x%x, ignoring", type->color_formats[i]);
    }
  }

  if (is_encoder)
    return accepted > 0;
  else
    return accepted == all && all > 0;
}

GstVideoFormat
gst_amc_color_format_to_video_format (const GstAmcCodecInfo * codec_info,
    const gchar * mime, gint color_format)
{
  gint i;

  if (color_format == COLOR_FormatYCbYCr) {
    if (strcmp (codec_info->name, "OMX.k3.video.decoder.avc") == 0) {
      GST_INFO
          ("OMX.k3.video.decoder.avc: COLOR_FormatYCbYCr is actually GST_VIDEO_FORMAT_NV12.");
      return GST_VIDEO_FORMAT_NV12;
    }

    /* FIXME COLOR_FormatYCbYCr doesn't work properly for OMX.k3.video.encoder.avc temporarily. */
    if (strcmp (codec_info->name, "OMX.k3.video.encoder.avc") == 0) {
      GST_INFO
          ("OMX.k3.video.encoder.avc: COLOR_FormatYCbYCr is not supported yet.");
      return GST_VIDEO_FORMAT_UNKNOWN;
    }

    /* FIXME COLOR_FormatYCbYCr is not supported in gst_amc_color_format_info_set yet, mask it. */
    return GST_VIDEO_FORMAT_UNKNOWN;
  }

  if (color_format == COLOR_FormatYUV420SemiPlanar) {
    if (strcmp (codec_info->name, "OMX.k3.video.encoder.avc") == 0) {
      GST_INFO
          ("OMX.k3.video.encoder.avc: COLOR_FormatYUV420SemiPlanar is actually GST_VIDEO_FORMAT_NV21.");
      return GST_VIDEO_FORMAT_NV21;
    }
  }

  for (i = 0; i < G_N_ELEMENTS (color_format_mapping_table); i++) {
    if (color_format_mapping_table[i].color_format == color_format)
      return color_format_mapping_table[i].video_format;
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

gint
gst_amc_video_format_to_color_format (const GstAmcCodecInfo * codec_info,
    const gchar * mime, GstVideoFormat video_format)
{
  const GstAmcCodecType *codec_type = NULL;
  gint i, j;

  for (i = 0; i < codec_info->n_supported_types; i++) {
    if (strcmp (codec_info->supported_types[i].mime, mime) == 0) {
      codec_type = &codec_info->supported_types[i];
      break;
    }
  }

  if (!codec_type)
    return -1;

  if (video_format == GST_VIDEO_FORMAT_NV12) {
    if (strcmp (codec_info->name, "OMX.k3.video.decoder.avc") == 0) {
      GST_INFO
          ("OMX.k3.video.decoder.avc: GST_VIDEO_FORMAT_NV12 is reported as COLOR_FormatYCbYCr.");

      return COLOR_FormatYCbYCr;
    }
  }

  if (video_format == GST_VIDEO_FORMAT_NV21) {
    if (strcmp (codec_info->name, "OMX.k3.video.encoder.avc") == 0) {
      GST_INFO
          ("OMX.k3.video.encoder.avc: GST_VIDEO_FORMAT_NV21 is reported as COLOR_FormatYUV420SemiPlanar.");

      return COLOR_FormatYUV420SemiPlanar;
    }
  }

  for (i = 0; i < G_N_ELEMENTS (color_format_mapping_table); i++) {
    if (color_format_mapping_table[i].video_format == video_format) {
      gint color_format = color_format_mapping_table[i].color_format;

      for (j = 0; j < codec_type->n_color_formats; j++)
        if (color_format == codec_type->color_formats[j])
          return color_format;
    }
  }

  return -1;
}

/*
 * The format is called QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka.
 * Which is actually NV12 (interleaved U&V).
 */
#define TILE_WIDTH 64
#define TILE_HEIGHT 32
#define TILE_SIZE (TILE_WIDTH * TILE_HEIGHT)
#define TILE_GROUP_SIZE (4 * TILE_SIZE)

/* get frame tile coordinate. XXX: nothing to be understood here, don't try. */
static size_t
tile_pos (size_t x, size_t y, size_t w, size_t h)
{
  size_t flim = x + (y & ~1) * w;

  if (y & 1) {
    flim += (x & ~3) + 2;
  } else if ((h & 1) == 0 || y != (h - 1)) {
    flim += (x + 2) & ~3;
  }

  return flim;
}

gboolean
gst_amc_color_format_info_set (GstAmcColorFormatInfo * color_format_info,
    const GstAmcCodecInfo * codec_info, const gchar * mime, gint color_format,
    gint width, gint height, gint stride, gint slice_height, gint crop_left,
    gint crop_right, gint crop_top, gint crop_bottom)
{
  gint frame_size = 0;

  if (color_format == COLOR_FormatYCbYCr) {
    if (strcmp (codec_info->name, "OMX.k3.video.decoder.avc") == 0)
      color_format = COLOR_FormatYUV420SemiPlanar;
  }

  /* Samsung Galaxy S3 seems to report wrong strides.
   * I.e. BigBuckBunny 854x480 H264 reports a stride of 864 when it is
   * actually 854, so we use width instead of stride here.
   * This is obviously bound to break in the future. */
  if (g_str_has_prefix (codec_info->name, "OMX.SEC.")) {
    stride = width;
  }

  if (strcmp (codec_info->name, "OMX.k3.video.decoder.avc") == 0) {
    stride = width;
    slice_height = height;
  }

  if (slice_height == 0) {
    /* NVidia Tegra 3 on Nexus 7 does not set this */
    if (g_str_has_prefix (codec_info->name, "OMX.Nvidia."))
      slice_height = GST_ROUND_UP_16 (height);
  }

  if (width == 0 || height == 0) {
    GST_ERROR ("Width or height is 0");
    return FALSE;
  }

  switch (color_format) {
    case COLOR_FormatYUV420Planar:
    case COLOR_FormatYUV420Flexible:{
    case COLOR_FormatYV12:
      if (stride == 0 || slice_height == 0) {
        GST_ERROR ("Stride or slice height is 0");
        return FALSE;
      }

      frame_size =
          stride * slice_height + 2 * ((stride + 1) / 2) * ((slice_height +
              1) / 2);
      break;
    }
    case COLOR_INTEL_FormatYUV420PackedSemiPlanar:
    case COLOR_INTEL_FormatYUV420PackedSemiPlanar_Tiled:
      if (stride == 0) {
        GST_ERROR ("Stride is 0");
        return FALSE;
      }
      if (slice_height <= 0)
        slice_height = height;

      frame_size =
          stride * (slice_height - crop_top / 2) +
          (GST_ROUND_UP_2 (stride) * ((slice_height + 1) / 2));
      break;

    case COLOR_TI_FormatYUV420PackedSemiPlanar:
    case COLOR_TI_FormatYUV420PackedSemiPlanarInterlaced:{
      if (stride == 0 || slice_height == 0) {
        GST_ERROR ("Stride or slice height is 0");
        return FALSE;
      }

      frame_size =
          stride * (slice_height - crop_top / 2) +
          (GST_ROUND_UP_2 (stride) * ((slice_height + 1) / 2));
      break;
    }
    case COLOR_QCOM_FormatYUV420SemiPlanar:
    case COLOR_QCOM_FormatYVU420SemiPlanar32m:
    case COLOR_QCOM_FormatYVU420SemiPlanar32mMultiView:
    case COLOR_FormatYUV420SemiPlanar:{
      if (stride == 0 || slice_height == 0) {
        GST_ERROR ("Stride or slice height is 0");
        return FALSE;
      }

      frame_size = stride * slice_height + stride * ((slice_height + 1) / 2);
      break;
    }
    case COLOR_QCOM_FormatYUV420PackedSemiPlanar64x32Tile2m8ka:{
      const size_t tile_w = (width - 1) / TILE_WIDTH + 1;
      const size_t tile_w_align = (tile_w + 1) & ~1;
      const size_t tile_h_luma = (height - 1) / TILE_HEIGHT + 1;
      frame_size =
          tile_pos (tile_w, tile_h_luma, tile_w_align, tile_h_luma) * TILE_SIZE;
      break;
    }
    default:
      GST_ERROR ("Unsupported color format %d", color_format);
      return FALSE;
      break;
  }

  color_format_info->color_format = color_format;
  color_format_info->width = width;
  color_format_info->height = height;
  color_format_info->stride = stride;
  color_format_info->slice_height = slice_height;
  color_format_info->crop_left = crop_left;
  color_format_info->crop_right = crop_right;
  color_format_info->crop_top = crop_top;
  color_format_info->crop_bottom = crop_bottom;
  color_format_info->frame_size = frame_size;

  return TRUE;
}

/* The weird handling of cropping, alignment and everything is taken from
 * platform/frameworks/media/libstagefright/colorconversion/ColorConversion.cpp
 */
gboolean
gst_amc_color_format_copy (GstAmcColorFormatInfo * cinfo,
    GstAmcBuffer * cbuffer, const GstAmcBufferInfo * cbuffer_info,
    GstVideoInfo * vinfo, GstBuffer * vbuffer,
    GstAmcColorFormatCopyDirection direction)
{
  gboolean ret = FALSE;
  guint8 *cptr = NULL, *vptr = NULL;
  GstMapFlags vmap_mode;
  guint8 **src, **dest;

  if (direction == COLOR_FORMAT_COPY_OUT) {
    src = &cptr;
    dest = &vptr;
    vmap_mode = GST_MAP_WRITE;
  } else {
    src = &vptr;
    vmap_mode = GST_MAP_READ;
    dest = &cptr;
  }

  /* Same video format */
  if (cbuffer_info->size == gst_buffer_get_size (vbuffer)) {
    GstMapInfo minfo;

    GST_DEBUG ("Buffer sizes equal, doing fast copy");
    if (!gst_buffer_map (vbuffer, &minfo, vmap_mode))
      goto fail_map;

    cptr = cbuffer->data + cbuffer_info->offset;
    vptr = minfo.data;
    orc_memcpy (*dest, *src, cbuffer_info->size);

    gst_buffer_unmap (vbuffer, &minfo);
    ret = TRUE;
    goto done;
  }

  GST_DEBUG ("Sizes not equal (%d vs %" G_GSIZE_FORMAT
      "), doing slow line-by-line copying", cbuffer_info->size,
      gst_buffer_get_size (vbuffer));

  /* Different video format, try to convert */
  switch (cinfo->color_format) {
    case COLOR_FormatYUV420Planar:{
      GstVideoFrame vframe;
      gint i, j, height;
      gint stride, slice_height;
      gint c_stride, v_stride;
      gint row_length;

      stride = cinfo->stride;
      slice_height = cinfo->slice_height;
      g_assert (stride > 0 && slice_height > 0);

      if (!gst_video_frame_map (&vframe, vinfo, vbuffer, vmap_mode))
        goto fail_map;

      for (i = 0; i < 3; i++) {
        if (i == 0) {
          c_stride = stride;
          v_stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, i);
        } else {
          c_stride = (stride + 1) / 2;
          v_stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, i);
        }

        cptr = cbuffer->data + cbuffer_info->offset;

        if (i == 0) {
          cptr += cinfo->crop_top * stride;
          cptr += cinfo->crop_left;
          row_length = cinfo->width;
        } else if (i > 0) {
          /* skip the Y plane */
          cptr += slice_height * stride;

          /* crop_top/crop_left divided by two
           * because one byte of the U/V planes
           * corresponds to two pixels horizontally/vertically */
          cptr += cinfo->crop_top / 2 * c_stride;
          cptr += cinfo->crop_left / 2;
          row_length = (cinfo->width + 1) / 2;
        }
        if (i == 2) {
          /* skip the U plane */
          cptr += ((slice_height + 1) / 2) * ((stride + 1) / 2);
        }

        vptr = GST_VIDEO_FRAME_COMP_DATA (&vframe, i);
        height = GST_VIDEO_FRAME_COMP_HEIGHT (&vframe, i);

        for (j = 0; j < height; j++) {
          orc_memcpy (*dest, *src, row_length);
          cptr += c_stride;
          vptr += v_stride;
        }
      }
      gst_video_frame_unmap (&vframe);
      ret = TRUE;
      break;
    }
    case COLOR_TI_FormatYUV420PackedSemiPlanar:
    case COLOR_TI_FormatYUV420PackedSemiPlanarInterlaced:{
      gint i, j, height;
      gint c_stride, v_stride;
      gint row_length;
      GstVideoFrame vframe;

      /* This should always be set */
      g_assert (cinfo->stride > 0 && cinfo->slice_height > 0);

      /* FIXME: This does not work for odd widths or heights
       * but might as well be a bug in the codec */
      if (!gst_video_frame_map (&vframe, vinfo, vbuffer, vmap_mode))
        goto fail_map;

      for (i = 0; i < 2; i++) {
        if (i == 0) {
          c_stride = cinfo->stride;
          v_stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, i);
        } else {
          c_stride = GST_ROUND_UP_2 (cinfo->stride);
          v_stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, i);
        }

        cptr = cbuffer->data + cbuffer_info->offset;
        if (i == 0) {
          row_length = cinfo->width;
        } else if (i == 1) {
          cptr += (cinfo->slice_height - cinfo->crop_top / 2) * cinfo->stride;
          row_length = GST_ROUND_UP_2 (cinfo->width);
        }

        vptr = GST_VIDEO_FRAME_COMP_DATA (&vframe, i);
        height = GST_VIDEO_FRAME_COMP_HEIGHT (&vframe, i);

        for (j = 0; j < height; j++) {
          orc_memcpy (*dest, *src, row_length);
          cptr += c_stride;
          vptr += v_stride;
        }
      }
      gst_video_frame_unmap (&vframe);
      ret = TRUE;
      break;
    }
    case COLOR_QCOM_FormatYUV420SemiPlanar:
    case COLOR_QCOM_FormatYVU420SemiPlanar32m:
    case COLOR_QCOM_FormatYVU420SemiPlanar32mMultiView:
    case COLOR_FormatYUV420SemiPlanar:{
      gint i, j, height;
      gint c_stride, v_stride;
      gint row_length;
      GstVideoFrame vframe;

      /* This should always be set */
      g_assert (cinfo->stride > 0 && cinfo->slice_height > 0);

      if (!gst_video_frame_map (&vframe, vinfo, vbuffer, vmap_mode))
        goto fail_map;

      for (i = 0; i < 2; i++) {
        c_stride = cinfo->stride;
        v_stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, i);

        cptr = cbuffer->data + cbuffer_info->offset;
        if (i == 0) {
          cptr += cinfo->crop_top * cinfo->stride;
          cptr += cinfo->crop_left;
          row_length = cinfo->width;
        } else if (i == 1) {
          cptr += cinfo->slice_height * cinfo->stride;
          cptr += cinfo->crop_top * cinfo->stride;
          cptr += cinfo->crop_left;
          row_length = cinfo->width;
        }

        vptr = GST_VIDEO_FRAME_COMP_DATA (&vframe, i);
        height = GST_VIDEO_FRAME_COMP_HEIGHT (&vframe, i);

        for (j = 0; j < height; j++) {
          orc_memcpy (*dest, *src, row_length);
          cptr += c_stride;
          vptr += v_stride;
        }
      }
      gst_video_frame_unmap (&vframe);
      ret = TRUE;
      break;
    }
      /* FIXME: This should be in libgstvideo as MT12 or similar, see v4l2 */
    case COLOR_QCOM_FormatYUV420PackedSemiPlanar64x32Tile2m8ka:{
      GstVideoFrame vframe;
      gint width = cinfo->width;
      gint height = cinfo->height;
      gint v_luma_stride, v_chroma_stride;
      guint8 *cdata = cbuffer->data + cbuffer_info->offset;
      guint8 *v_luma, *v_chroma;
      gint y;
      const size_t tile_w = (width - 1) / TILE_WIDTH + 1;
      const size_t tile_w_align = (tile_w + 1) & ~1;
      const size_t tile_h_luma = (height - 1) / TILE_HEIGHT + 1;
      const size_t tile_h_chroma = (height / 2 - 1) / TILE_HEIGHT + 1;
      size_t luma_size = tile_w_align * tile_h_luma * TILE_SIZE;

      if (!gst_video_frame_map (&vframe, vinfo, vbuffer, vmap_mode))
        goto fail_map;

      v_luma = GST_VIDEO_FRAME_PLANE_DATA (&vframe, 0);
      v_chroma = GST_VIDEO_FRAME_PLANE_DATA (&vframe, 1);
      v_luma_stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, 0);
      v_chroma_stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, 1);

      if ((luma_size % TILE_GROUP_SIZE) != 0)
        luma_size = (((luma_size - 1) / TILE_GROUP_SIZE) + 1) * TILE_GROUP_SIZE;

      for (y = 0; y < tile_h_luma; y++) {
        size_t row_width = width;
        gint x;

        for (x = 0; x < tile_w; x++) {
          size_t tile_width = row_width;
          size_t tile_height = height;
          gint luma_idx;
          gint chroma_idx;
          /* luma source pointer for this tile */
          uint8_t *c_luma =
              cdata + tile_pos (x, y, tile_w_align, tile_h_luma) * TILE_SIZE;

          /* chroma source pointer for this tile */
          uint8_t *c_chroma =
              cdata + luma_size + tile_pos (x, y / 2, tile_w_align,
              tile_h_chroma) * TILE_SIZE;
          if (y & 1)
            c_chroma += TILE_SIZE / 2;

          /* account for right columns */
          if (tile_width > TILE_WIDTH)
            tile_width = TILE_WIDTH;

          /* account for bottom rows */
          if (tile_height > TILE_HEIGHT)
            tile_height = TILE_HEIGHT;

          /* vptr luma memory index for this tile */
          luma_idx = y * TILE_HEIGHT * v_luma_stride + x * TILE_WIDTH;

          /* vptr chroma memory index for this tile */
          /* XXX: remove divisions */
          chroma_idx = y * TILE_HEIGHT / 2 * v_chroma_stride + x * TILE_WIDTH;

          tile_height /= 2;     // we copy 2 luma lines at once
          while (tile_height--) {
            vptr = v_luma + luma_idx;
            cptr = c_luma;
            memcpy (*dest, *src, tile_width);
            c_luma += TILE_WIDTH;
            luma_idx += v_luma_stride;

            vptr = v_luma + luma_idx;
            cptr = c_luma;
            memcpy (*dest, *src, tile_width);
            c_luma += TILE_WIDTH;
            luma_idx += v_luma_stride;

            vptr = v_chroma + chroma_idx;
            cptr = c_chroma;
            memcpy (*dest, *src, tile_width);
            c_chroma += TILE_WIDTH;
            chroma_idx += v_chroma_stride;
          }
          row_width -= TILE_WIDTH;
        }
        height -= TILE_HEIGHT;
      }
      gst_video_frame_unmap (&vframe);
      ret = TRUE;
      break;

    }
    default:
      GST_ERROR ("Unsupported color format %d", cinfo->color_format);
      goto done;
      break;
  }

done:
  return ret;

fail_map:
  GST_ERROR ("Failed to map GStreamer buffer memory in mode %d", vmap_mode);
  return FALSE;
}

static const struct
{
  gint id;
  const gchar *str;
} hevc_profile_mapping_table[] = {
  {
      HEVCProfileMain, "main"}, {
      HEVCProfileMain10, "main-10"}
};

const gchar *
gst_amc_hevc_profile_to_string (gint profile)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (hevc_profile_mapping_table); i++) {
    if (hevc_profile_mapping_table[i].id == profile) {
      return hevc_profile_mapping_table[i].str;
    }
  }

  return NULL;
}

gint
gst_amc_hevc_profile_from_string (const gchar * profile)
{
  gint i;

  g_return_val_if_fail (profile != NULL, -1);

  for (i = 0; i < G_N_ELEMENTS (hevc_profile_mapping_table); i++) {
    if (strcmp (hevc_profile_mapping_table[i].str, profile) == 0)
      return hevc_profile_mapping_table[i].id;
  }

  return -1;
}

static const struct
{
  gint id;
  const gchar *tier_str;
  const gchar *level_str;
} hevc_tier_level_mapping_table[] = {
  {
      HEVCMainTierLevel1, "main", "1"}, {
      HEVCMainTierLevel2, "main", "2"}, {
      HEVCMainTierLevel21, "main", "2.1"}, {
      HEVCMainTierLevel3, "main", "3"}, {
      HEVCMainTierLevel31, "main", "3.1"}, {
      HEVCMainTierLevel4, "main", "4"}, {
      HEVCMainTierLevel41, "main", "4.1"}, {
      HEVCMainTierLevel5, "main", "5"}, {
      HEVCMainTierLevel51, "main", "5.1"}, {
      HEVCMainTierLevel52, "main", "5.2"}, {
      HEVCMainTierLevel6, "main", "6"}, {
      HEVCMainTierLevel61, "main", "6.1"}, {
      HEVCMainTierLevel62, "main", "6.2"}, {
      HEVCHighTierLevel1, "high", "1"}, {
      HEVCHighTierLevel2, "high", "2"}, {
      HEVCHighTierLevel21, "high", "2.1"}, {
      HEVCHighTierLevel3, "high", "3"}, {
      HEVCHighTierLevel31, "high", "3.1"}, {
      HEVCHighTierLevel4, "high", "4"}, {
      HEVCHighTierLevel41, "high", "4.1"}, {
      HEVCHighTierLevel5, "high", "5"}, {
      HEVCHighTierLevel51, "high", "5.1"}, {
      HEVCHighTierLevel52, "high", "5.2"}, {
      HEVCHighTierLevel6, "high", "6"}, {
      HEVCHighTierLevel61, "high", "6.1"}, {
      HEVCHighTierLevel62, "high", "6.2"}
};

const gchar *
gst_amc_hevc_tier_level_to_string (gint tier_level, const gchar ** tier)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (hevc_tier_level_mapping_table); i++) {
    if (hevc_tier_level_mapping_table[i].id == tier_level)
      *tier = hevc_tier_level_mapping_table[i].tier_str;
    return hevc_tier_level_mapping_table[i].level_str;
  }

  return NULL;
}

gint
gst_amc_hevc_tier_level_from_string (const gchar * tier, const gchar * level)
{
  gint i;

  g_return_val_if_fail (level != NULL, -1);

  for (i = 0; i < G_N_ELEMENTS (hevc_tier_level_mapping_table); i++) {
    if (strcmp (hevc_tier_level_mapping_table[i].tier_str, tier) == 0 &&
        strcmp (hevc_tier_level_mapping_table[i].level_str, level) == 0)
      return hevc_tier_level_mapping_table[i].id;
  }

  return -1;
}

static const struct
{
  gint id;
  const gchar *str;
  const gchar *alt_str;
} avc_profile_mapping_table[] = {
  {
      AVCProfileBaseline, "baseline", "constrained-baseline"}, {
      AVCProfileMain, "main", NULL}, {
      AVCProfileExtended, "extended", NULL}, {
      AVCProfileHigh, "high"}, {
      AVCProfileHigh10, "high-10", "high-10-intra"}, {
      AVCProfileHigh422, "high-4:2:2", "high-4:2:2-intra"}, {
      AVCProfileHigh444, "high-4:4:4", "high-4:4:4-intra"}
};

const gchar *
gst_amc_avc_profile_to_string (gint profile, const gchar ** alternative)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (avc_profile_mapping_table); i++) {
    if (avc_profile_mapping_table[i].id == profile) {
      if (alternative != NULL)
        *alternative = avc_profile_mapping_table[i].alt_str;
      return avc_profile_mapping_table[i].str;
    }
  }

  return NULL;
}

gint
gst_amc_avc_profile_from_string (const gchar * profile)
{
  gint i;

  g_return_val_if_fail (profile != NULL, -1);

  for (i = 0; i < G_N_ELEMENTS (avc_profile_mapping_table); i++) {
    if (strcmp (avc_profile_mapping_table[i].str, profile) == 0 ||
        (avc_profile_mapping_table[i].alt_str &&
            strcmp (avc_profile_mapping_table[i].alt_str, profile) == 0))
      return avc_profile_mapping_table[i].id;
  }

  return -1;
}

static const struct
{
  gint id;
  const gchar *str;
} avc_level_mapping_table[] = {
  {
      AVCLevel1, "1"}, {
      AVCLevel1b, "1b"}, {
      AVCLevel11, "1.1"}, {
      AVCLevel12, "1.2"}, {
      AVCLevel13, "1.3"}, {
      AVCLevel2, "2"}, {
      AVCLevel21, "2.1"}, {
      AVCLevel22, "2.2"}, {
      AVCLevel3, "3"}, {
      AVCLevel31, "3.1"}, {
      AVCLevel32, "3.2"}, {
      AVCLevel4, "4"}, {
      AVCLevel41, "4.1"}, {
      AVCLevel42, "4.2"}, {
      AVCLevel5, "5"}, {
      AVCLevel51, "5.1"}
};

const gchar *
gst_amc_avc_level_to_string (gint level)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (avc_level_mapping_table); i++) {
    if (avc_level_mapping_table[i].id == level)
      return avc_level_mapping_table[i].str;
  }

  return NULL;
}

gint
gst_amc_avc_level_from_string (const gchar * level)
{
  gint i;

  g_return_val_if_fail (level != NULL, -1);

  for (i = 0; i < G_N_ELEMENTS (avc_level_mapping_table); i++) {
    if (strcmp (avc_level_mapping_table[i].str, level) == 0)
      return avc_level_mapping_table[i].id;
  }

  return -1;
}

static const struct
{
  gint id;
  gint gst_id;
} h263_profile_mapping_table[] = {
  {
      H263ProfileBaseline, 0}, {
      H263ProfileH320Coding, 1}, {
      H263ProfileBackwardCompatible, 2}, {
      H263ProfileISWV2, 3}, {
      H263ProfileISWV3, 4}, {
      H263ProfileHighCompression, 5}, {
      H263ProfileInternet, 6}, {
      H263ProfileInterlace, 7}, {
      H263ProfileHighLatency, 8}
};

gint
gst_amc_h263_profile_to_gst_id (gint profile)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (h263_profile_mapping_table); i++) {
    if (h263_profile_mapping_table[i].id == profile)
      return h263_profile_mapping_table[i].gst_id;
  }

  return -1;
}

gint
gst_amc_h263_profile_from_gst_id (gint profile)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (h263_profile_mapping_table); i++) {
    if (h263_profile_mapping_table[i].gst_id == profile)
      return h263_profile_mapping_table[i].id;
  }

  return -1;
}

static const struct
{
  gint id;
  gint gst_id;
} h263_level_mapping_table[] = {
  {
      H263Level10, 10}, {
      H263Level20, 20}, {
      H263Level30, 30}, {
      H263Level40, 40}, {
      H263Level50, 50}, {
      H263Level60, 60}, {
      H263Level70, 70}
};

gint
gst_amc_h263_level_to_gst_id (gint level)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (h263_level_mapping_table); i++) {
    if (h263_level_mapping_table[i].id == level)
      return h263_level_mapping_table[i].gst_id;
  }

  return -1;
}

gint
gst_amc_h263_level_from_gst_id (gint level)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (h263_level_mapping_table); i++) {
    if (h263_level_mapping_table[i].gst_id == level)
      return h263_level_mapping_table[i].id;
  }

  return -1;
}

static const struct
{
  gint id;
  const gchar *str;
} mpeg4_profile_mapping_table[] = {
  {
      MPEG4ProfileSimple, "simple"}, {
      MPEG4ProfileSimpleScalable, "simple-scalable"}, {
      MPEG4ProfileCore, "core"}, {
      MPEG4ProfileMain, "main"}, {
      MPEG4ProfileNbit, "n-bit"}, {
      MPEG4ProfileScalableTexture, "scalable"}, {
      MPEG4ProfileSimpleFace, "simple-face"}, {
      MPEG4ProfileSimpleFBA, "simple-fba"}, {
      MPEG4ProfileBasicAnimated, "basic-animated-texture"}, {
      MPEG4ProfileHybrid, "hybrid"}, {
      MPEG4ProfileAdvancedRealTime, "advanced-real-time"}, {
      MPEG4ProfileCoreScalable, "core-scalable"}, {
      MPEG4ProfileAdvancedCoding, "advanced-coding-efficiency"}, {
      MPEG4ProfileAdvancedCore, "advanced-core"}, {
      MPEG4ProfileAdvancedScalable, "advanced-scalable-texture"}, {
      MPEG4ProfileAdvancedSimple, "advanced-simple"}
};

const gchar *
gst_amc_mpeg4_profile_to_string (gint profile)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (mpeg4_profile_mapping_table); i++) {
    if (mpeg4_profile_mapping_table[i].id == profile)
      return mpeg4_profile_mapping_table[i].str;
  }

  return NULL;
}

gint
gst_amc_mpeg4_profile_from_string (const gchar * profile)
{
  gint i;

  g_return_val_if_fail (profile != NULL, -1);

  for (i = 0; i < G_N_ELEMENTS (mpeg4_profile_mapping_table); i++) {
    if (strcmp (mpeg4_profile_mapping_table[i].str, profile) == 0)
      return mpeg4_profile_mapping_table[i].id;
  }

  return -1;
}

static const struct
{
  gint id;
  const gchar *str;
} mpeg4_level_mapping_table[] = {
  {
      MPEG4Level0, "0"}, {
      MPEG4Level0b, "0b"}, {
      MPEG4Level1, "1"}, {
      MPEG4Level2, "2"}, {
      MPEG4Level3, "3"}, {
      MPEG4Level4, "4"}, {
      MPEG4Level4a, "4a"}, {
      MPEG4Level5, "5"},
};

const gchar *
gst_amc_mpeg4_level_to_string (gint level)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (mpeg4_level_mapping_table); i++) {
    if (mpeg4_level_mapping_table[i].id == level)
      return mpeg4_level_mapping_table[i].str;
  }

  return NULL;
}

gint
gst_amc_mpeg4_level_from_string (const gchar * level)
{
  gint i;

  g_return_val_if_fail (level != NULL, -1);

  for (i = 0; i < G_N_ELEMENTS (mpeg4_level_mapping_table); i++) {
    if (strcmp (mpeg4_level_mapping_table[i].str, level) == 0)
      return mpeg4_level_mapping_table[i].id;
  }

  return -1;
}

static const struct
{
  gint id;
  const gchar *str;
} aac_profile_mapping_table[] = {
  {
      AACObjectMain, "main"}, {
      AACObjectLC, "lc"}, {
      AACObjectSSR, "ssr"}, {
      AACObjectLTP, "ltp"}
};

const gchar *
gst_amc_aac_profile_to_string (gint profile)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (aac_profile_mapping_table); i++) {
    if (aac_profile_mapping_table[i].id == profile)
      return aac_profile_mapping_table[i].str;
  }

  return NULL;
}

gint
gst_amc_aac_profile_from_string (const gchar * profile)
{
  gint i;

  g_return_val_if_fail (profile != NULL, -1);

  for (i = 0; i < G_N_ELEMENTS (aac_profile_mapping_table); i++) {
    if (strcmp (aac_profile_mapping_table[i].str, profile) == 0)
      return aac_profile_mapping_table[i].id;
  }

  return -1;
}

static const struct
{
  guint32 mask;
  GstAudioChannelPosition pos;
} channel_mapping_table[] = {
  {
      CHANNEL_OUT_FRONT_LEFT, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT}, {
      CHANNEL_OUT_FRONT_RIGHT, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT}, {
      CHANNEL_OUT_FRONT_CENTER, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER}, {
      CHANNEL_OUT_LOW_FREQUENCY, GST_AUDIO_CHANNEL_POSITION_LFE1}, {
      CHANNEL_OUT_BACK_LEFT, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT}, {
      CHANNEL_OUT_BACK_RIGHT, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT}, {
        CHANNEL_OUT_FRONT_LEFT_OF_CENTER,
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER}, {
        CHANNEL_OUT_FRONT_RIGHT_OF_CENTER,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER}, {
      CHANNEL_OUT_BACK_CENTER, GST_AUDIO_CHANNEL_POSITION_REAR_CENTER}, {
      CHANNEL_OUT_SIDE_LEFT, GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT}, {
      CHANNEL_OUT_SIDE_RIGHT, GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT}, {
      CHANNEL_OUT_TOP_CENTER, GST_AUDIO_CHANNEL_POSITION_INVALID}, {
      CHANNEL_OUT_TOP_FRONT_LEFT, GST_AUDIO_CHANNEL_POSITION_INVALID}, {
      CHANNEL_OUT_TOP_FRONT_CENTER, GST_AUDIO_CHANNEL_POSITION_INVALID}, {
      CHANNEL_OUT_TOP_FRONT_RIGHT, GST_AUDIO_CHANNEL_POSITION_INVALID}, {
      CHANNEL_OUT_TOP_BACK_LEFT, GST_AUDIO_CHANNEL_POSITION_INVALID}, {
      CHANNEL_OUT_TOP_BACK_CENTER, GST_AUDIO_CHANNEL_POSITION_INVALID}, {
      CHANNEL_OUT_TOP_BACK_RIGHT, GST_AUDIO_CHANNEL_POSITION_INVALID}
};

gboolean
gst_amc_audio_channel_mask_to_positions (guint32 channel_mask, gint channels,
    GstAudioChannelPosition * pos)
{
  gint i, j;

  if (channel_mask == 0) {
    if (channels == 1) {
      pos[0] = GST_AUDIO_CHANNEL_POSITION_MONO;
      return TRUE;
    }
    if (channels == 2) {
      pos[0] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
      pos[1] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
      return TRUE;
    }

    /* Now let the guesswork begin, these are the
     * AAC default channel assignments for these numbers
     * of channels */
    if (channels == 3) {
      channel_mask =
          CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
          CHANNEL_OUT_FRONT_CENTER;
    } else if (channels == 4) {
      channel_mask =
          CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
          CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_BACK_CENTER;
    } else if (channels == 5) {
      channel_mask =
          CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
          CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_BACK_LEFT |
          CHANNEL_OUT_BACK_RIGHT;
    } else if (channels == 6) {
      channel_mask =
          CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
          CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_BACK_LEFT |
          CHANNEL_OUT_BACK_RIGHT | CHANNEL_OUT_LOW_FREQUENCY;
    } else if (channels == 8) {
      channel_mask =
          CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
          CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_BACK_LEFT |
          CHANNEL_OUT_BACK_RIGHT | CHANNEL_OUT_LOW_FREQUENCY |
          CHANNEL_OUT_FRONT_LEFT_OF_CENTER | CHANNEL_OUT_FRONT_RIGHT_OF_CENTER;
    }
  }

  for (i = 0, j = 0; i < G_N_ELEMENTS (channel_mapping_table); i++) {
    if ((channel_mask & channel_mapping_table[i].mask)) {
      pos[j++] = channel_mapping_table[i].pos;
      if (channel_mapping_table[i].pos == GST_AUDIO_CHANNEL_POSITION_INVALID) {
        memset (pos, 0, sizeof (GstAudioChannelPosition) * channels);
        GST_ERROR ("Unable to map channel mask 0x%08x",
            channel_mapping_table[i].mask);
        return FALSE;
      }
      if (j == channels)
        break;
    }
  }

  if (j != channels) {
    memset (pos, 0, sizeof (GstAudioChannelPosition) * channels);
    GST_ERROR ("Unable to map all channel positions in mask 0x%08x",
        channel_mask);
    return FALSE;
  }

  return TRUE;
}

guint32
gst_amc_audio_channel_mask_from_positions (GstAudioChannelPosition * positions,
    gint channels)
{
  gint i, j;
  guint32 channel_mask = 0;

  if (channels == 1 && !positions)
    return CHANNEL_OUT_FRONT_CENTER;
  if (channels == 2 && !positions)
    return CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT;

  for (i = 0; i < channels; i++) {
    if (positions[i] == GST_AUDIO_CHANNEL_POSITION_INVALID)
      return 0;

    for (j = 0; j < G_N_ELEMENTS (channel_mapping_table); j++) {
      if (channel_mapping_table[j].pos == positions[i]) {
        channel_mask |= channel_mapping_table[j].mask;
        break;
      }
    }

    if (j == G_N_ELEMENTS (channel_mapping_table)) {
      GST_ERROR ("Unable to map channel position %d", positions[i]);
      return 0;
    }
  }

  return channel_mask;
}

static gchar *
create_type_name (const gchar * parent_name, const gchar * codec_name)
{
  gchar *typified_name;
  gint i, k;
  gint parent_name_len = strlen (parent_name);
  gint codec_name_len = strlen (codec_name);
  gboolean upper = TRUE;

  typified_name = g_new0 (gchar, parent_name_len + 1 + strlen (codec_name) + 1);
  memcpy (typified_name, parent_name, parent_name_len);
  typified_name[parent_name_len] = '-';

  for (i = 0, k = 0; i < codec_name_len; i++) {
    if (g_ascii_isalnum (codec_name[i])) {
      if (upper)
        typified_name[parent_name_len + 1 + k++] =
            g_ascii_toupper (codec_name[i]);
      else
        typified_name[parent_name_len + 1 + k++] =
            g_ascii_tolower (codec_name[i]);

      upper = FALSE;
    } else {
      /* Skip all non-alnum chars and start a new upper case word */
      upper = TRUE;
    }
  }

  return typified_name;
}

static gchar *
create_element_name (gboolean video, gboolean encoder, const gchar * codec_name)
{
#define PREFIX_LEN 10
  static const gchar *prefixes[] = {
    "amcviddec-",
    "amcauddec-",
    "amcvidenc-",
    "amcaudenc-"
  };
  gchar *element_name;
  gint i, k;
  gint codec_name_len = strlen (codec_name);
  const gchar *prefix;

  if (video && !encoder)
    prefix = prefixes[0];
  else if (!video && !encoder)
    prefix = prefixes[1];
  else if (video && encoder)
    prefix = prefixes[2];
  else
    prefix = prefixes[3];

  element_name = g_new0 (gchar, PREFIX_LEN + strlen (codec_name) + 1);
  memcpy (element_name, prefix, PREFIX_LEN);

  for (i = 0, k = 0; i < codec_name_len; i++) {
    if (g_ascii_isalnum (codec_name[i])) {
      element_name[PREFIX_LEN + k++] = g_ascii_tolower (codec_name[i]);
    }
    /* Skip all non-alnum chars */
  }

  return element_name;
}

#undef PREFIX_LEN

static gboolean
register_codecs (GstPlugin * plugin)
{
  gboolean ret = TRUE;
  GList *l;

  GST_DEBUG ("Registering plugins");

  for (l = codec_infos.head; l; l = l->next) {
    GstAmcCodecInfo *codec_info = l->data;
    gboolean is_audio = FALSE;
    gboolean is_video = FALSE;
    gint i;
    gint n_types;

    GST_DEBUG ("Registering codec '%s'", codec_info->name);
    for (i = 0; i < codec_info->n_supported_types; i++) {
      GstAmcCodecType *codec_type = &codec_info->supported_types[i];

      if (g_str_has_prefix (codec_type->mime, "audio/"))
        is_audio = TRUE;
      else if (g_str_has_prefix (codec_type->mime, "video/"))
        is_video = TRUE;
    }

    n_types = 0;
    if (is_audio)
      n_types++;
    if (is_video)
      n_types++;

    for (i = 0; i < n_types; i++) {
      GTypeQuery type_query;
      GTypeInfo type_info = { 0, };
      GType type, subtype;
      gchar *type_name, *element_name;
      guint rank;

      if (is_video) {
        if (codec_info->is_encoder)
          type = gst_amc_video_enc_get_type ();
        else
          type = gst_amc_video_dec_get_type ();
      } else if (is_audio && !codec_info->is_encoder) {
        type = gst_amc_audio_dec_get_type ();
      } else {
        GST_DEBUG ("Skipping unsupported codec type");
        continue;
      }

      g_type_query (type, &type_query);
      memset (&type_info, 0, sizeof (type_info));
      type_info.class_size = type_query.class_size;
      type_info.instance_size = type_query.instance_size;
      type_name = create_type_name (type_query.type_name, codec_info->name);

      if (g_type_from_name (type_name) != G_TYPE_INVALID) {
        GST_ERROR ("Type '%s' already exists for codec '%s'", type_name,
            codec_info->name);
        g_free (type_name);
        continue;
      }

      subtype = g_type_register_static (type, type_name, &type_info, 0);
      g_free (type_name);

      g_type_set_qdata (subtype, gst_amc_codec_info_quark, codec_info);

      element_name =
          create_element_name (is_video, codec_info->is_encoder,
          codec_info->name);

      /* Give the Google software codec a secondary rank,
       * everything else is likely a hardware codec, except
       * OMX.SEC.*.sw.dec (as seen in Galaxy S4).
       *
       * Also on some devices there are codecs that don't start
       * with OMX., while there are also some that do. And on
       * some of these devices the ones that don't start with
       * OMX. just crash during initialization while the others
       * work. To make things even more complicated other devices
       * have codecs with the same name that work and no alternatives.
       * So just give a lower rank to these non-OMX codecs and hope
       * that there's an alternative with a higher rank.
       */
      if (g_str_has_prefix (codec_info->name, "OMX.google") ||
          g_str_has_suffix (codec_info->name, ".sw.dec")) {
        /* For video we prefer hardware codecs, for audio we prefer software
         * codecs. Hardware codecs don't make much sense for audio */
        rank = is_video ? GST_RANK_SECONDARY : GST_RANK_PRIMARY;
      } else if (g_str_has_prefix (codec_info->name, "OMX.Exynos.")
          && !is_video) {
        /* OMX.Exynos. audio codecs are existing on some devices like the
         * Galaxy S5 mini, and cause random crashes (of the device,
         * not the app!) and generally misbehave. That specific device
         * has other codecs that work with a different name, but let's
         * just give them marginal rank in case there are devices that
         * have no other codecs and these are actually the only working
         * ones
         */
        rank = GST_RANK_MARGINAL;
      } else if (g_str_has_prefix (codec_info->name, "OMX.")) {
        rank = is_video ? GST_RANK_PRIMARY : GST_RANK_SECONDARY;
      } else {
        rank = GST_RANK_MARGINAL;
      }

      ret |= gst_element_register (plugin, element_name, rank, subtype);
      g_free (element_name);

      is_video = FALSE;
    }
  }

  return ret;
}

static gboolean
amc_init (GstPlugin * plugin)
{
  const gchar *ignore;

  gst_plugin_add_dependency_simple (plugin, NULL, "/etc:/system/vendor/etc",
      "media_codecs.xml", GST_PLUGIN_DEPENDENCY_FLAG_NONE);

  gst_amc_codec_info_quark = g_quark_from_static_string ("gst-amc-codec-info");

  if (!gst_amc_static_init ())
    return FALSE;

  /* Set this to TRUE to allow registering decoders that have
   * any unknown color formats, or encoders that only have
   * unknown color formats
   */
  ignore = g_getenv ("GST_AMC_IGNORE_UNKNOWN_COLOR_FORMATS");
  if (ignore && strcmp (ignore, "yes") == 0)
    ignore_unknown_color_formats = TRUE;

  if (!scan_codecs (plugin))
    return FALSE;

  if (!register_codecs (plugin))
    return FALSE;

  return TRUE;
}

#ifdef HAVE_JNI_H
static gboolean
ahc_init (GstPlugin * plugin)
{
  if (!gst_android_graphics_imageformat_init ()) {
    GST_ERROR ("Failed to init android image format");
    return FALSE;
  }

  if (!gst_android_hardware_camera_init ()) {
    gst_android_graphics_imageformat_deinit ();
    return FALSE;
  }

  if (!gst_element_register (plugin, "ahcsrc", GST_RANK_NONE, GST_TYPE_AHC_SRC)) {
    GST_ERROR ("Failed to register android camera source");
    gst_android_hardware_camera_deinit ();
    gst_android_graphics_imageformat_deinit ();
    return FALSE;
  }

  return TRUE;
}

static gboolean
ahs_init (GstPlugin * plugin)
{
  if (!gst_android_hardware_sensor_init ())
    return FALSE;

  if (!gst_element_register (plugin, "ahssrc", GST_RANK_NONE, GST_TYPE_AHS_SRC)) {
    GST_ERROR ("Failed to register android sensor source");
    gst_android_hardware_sensor_deinit ();
    return FALSE;
  }

  return TRUE;
}
#endif

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean init_ok = FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_amc_debug, "amc", 0, "android-media-codec");

#ifdef HAVE_JNI_H
  if (!gst_amc_jni_initialize ())
    return FALSE;
#endif

  if (amc_init (plugin))
    init_ok = TRUE;

#ifdef HAVE_JNI_H
  if (ahc_init (plugin))
    init_ok = TRUE;

  if (ahs_init (plugin))
    init_ok = TRUE;
#endif

  return init_ok;
}

void
gst_amc_codec_info_to_caps (const GstAmcCodecInfo * codec_info,
    GstCaps ** sink_caps, GstCaps ** src_caps)
{
  GstCaps *raw_ret = NULL, *encoded_ret = NULL;
  gint i;

  if (codec_info->is_encoder) {
    if (sink_caps)
      *sink_caps = raw_ret = gst_caps_new_empty ();

    if (src_caps)
      *src_caps = encoded_ret = gst_caps_new_empty ();
  } else {
    if (sink_caps)
      *sink_caps = encoded_ret = gst_caps_new_empty ();

    if (src_caps)
      *src_caps = raw_ret = gst_caps_new_empty ();
  }

  for (i = 0; i < codec_info->n_supported_types; i++) {
    const GstAmcCodecType *type = &codec_info->supported_types[i];
    GstStructure *tmp, *tmp2, *tmp3;

    if (g_str_has_prefix (type->mime, "audio/")) {
      if (raw_ret) {
        tmp = gst_structure_new ("audio/x-raw",
            "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT,
            "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
            "layout", G_TYPE_STRING, "interleaved", NULL);

        raw_ret = gst_caps_merge_structure (raw_ret, tmp);
      }

      if (encoded_ret) {
        if (strcmp (type->mime, "audio/mpeg") == 0) {
          tmp = gst_structure_new ("audio/mpeg",
              "mpegversion", G_TYPE_INT, 1,
              "layer", G_TYPE_INT, 3,
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "audio/3gpp") == 0) {
          tmp = gst_structure_new ("audio/AMR",
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "audio/amr-wb") == 0) {
          tmp = gst_structure_new ("audio/AMR-WB",
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "audio/mp4a-latm") == 0) {
          gint j;
          gboolean have_profile = FALSE;
          GValue va = { 0, };
          GValue v = { 0, };

          g_value_init (&va, GST_TYPE_LIST);
          g_value_init (&v, G_TYPE_STRING);
          g_value_set_string (&v, "raw");
          gst_value_list_append_value (&va, &v);
          g_value_set_string (&v, "adts");
          gst_value_list_append_value (&va, &v);
          g_value_unset (&v);

          tmp = gst_structure_new ("audio/mpeg",
              "mpegversion", G_TYPE_INT, 4,
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "framed", G_TYPE_BOOLEAN, TRUE, NULL);
          gst_structure_set_value (tmp, "stream-format", &va);
          g_value_unset (&va);

          for (j = 0; j < type->n_profile_levels; j++) {
            const gchar *profile;

            profile =
                gst_amc_aac_profile_to_string (type->profile_levels[j].profile);

            if (!profile) {
              GST_ERROR ("Unable to map AAC profile 0x%08x",
                  type->profile_levels[j].profile);
              continue;
            }

            tmp2 = gst_structure_copy (tmp);
            gst_structure_set (tmp2, "profile", G_TYPE_STRING, profile, NULL);
            encoded_ret = gst_caps_merge_structure (encoded_ret, tmp2);

            have_profile = TRUE;
          }

          if (!have_profile) {
            encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
          } else {
            gst_structure_free (tmp);
          }
        } else if (strcmp (type->mime, "audio/g711-alaw") == 0) {
          tmp = gst_structure_new ("audio/x-alaw",
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "audio/g711-mlaw") == 0) {
          tmp = gst_structure_new ("audio/x-mulaw",
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "audio/vorbis") == 0) {
          tmp = gst_structure_new ("audio/x-vorbis",
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "audio/opus") == 0) {
          tmp = gst_structure_new ("audio/x-opus",
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "audio/flac") == 0) {
          tmp = gst_structure_new ("audio/x-flac",
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "framed", G_TYPE_BOOLEAN, TRUE, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "audio/mpeg-L2") == 0) {
          tmp = gst_structure_new ("audio/mpeg",
              "mpegversion", G_TYPE_INT, 1,
              "layer", G_TYPE_INT, 2,
              "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else {
          GST_WARNING ("Unsupported mimetype '%s'", type->mime);
        }
      }
    } else if (g_str_has_prefix (type->mime, "video/")) {
      if (raw_ret) {
        gint j;

        for (j = 0; j < type->n_color_formats; j++) {
          GstVideoFormat format;

          /* Skip here without a warning, this is special and handled
           * in the decoder when doing rendering to a surface */
          if (type->color_formats[j] == COLOR_FormatAndroidOpaque)
            continue;

          format =
              gst_amc_color_format_to_video_format (codec_info,
              type->mime, type->color_formats[j]);
          if (format == GST_VIDEO_FORMAT_UNKNOWN) {
            GST_WARNING ("Unknown color format 0x%08x for codec %s",
                type->color_formats[j], type->mime);
            continue;
          }

          tmp = gst_structure_new ("video/x-raw",
              "format", G_TYPE_STRING, gst_video_format_to_string (format),
              "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
              "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

          raw_ret = gst_caps_merge_structure (raw_ret, tmp);
        }
      }

      if (encoded_ret) {
        if (strcmp (type->mime, "video/mp4v-es") == 0) {
          gint j;
          gboolean have_profile_level = FALSE;

          tmp = gst_structure_new ("video/mpeg",
              "width", GST_TYPE_INT_RANGE, 16, 4096,
              "height", GST_TYPE_INT_RANGE, 16, 4096,
              "framerate", GST_TYPE_FRACTION_RANGE,
              0, 1, G_MAXINT, 1,
              "mpegversion", G_TYPE_INT, 4,
              "systemstream", G_TYPE_BOOLEAN, FALSE,
              "parsed", G_TYPE_BOOLEAN, TRUE, NULL);

          if (type->n_profile_levels) {
            for (j = type->n_profile_levels - 1; j >= 0; j--) {
              const gchar *profile;

              profile =
                  gst_amc_mpeg4_profile_to_string (type->profile_levels[j].
                  profile);
              if (!profile) {
                GST_ERROR ("Unable to map MPEG4 profile 0x%08x",
                    type->profile_levels[j].profile);
                continue;
              }

              tmp2 = gst_structure_copy (tmp);
              gst_structure_set (tmp2, "profile", G_TYPE_STRING, profile, NULL);

              /* Don't put the level restrictions on the sinkpad caps for decoders,
               * see 2b94641a4 */
              if (codec_info->is_encoder) {
                const gchar *level;
                gint k;
                GValue va = { 0, };
                GValue v = { 0, };

                g_value_init (&va, GST_TYPE_LIST);
                g_value_init (&v, G_TYPE_STRING);

                for (k = 1; k <= type->profile_levels[j].level && k != 0;
                    k <<= 1) {
                  level = gst_amc_mpeg4_level_to_string (k);
                  if (!level)
                    continue;

                  g_value_set_string (&v, level);
                  gst_value_list_append_value (&va, &v);
                  g_value_reset (&v);
                }

                gst_structure_set_value (tmp2, "level", &va);
                g_value_unset (&va);
                g_value_unset (&v);
              }

              encoded_ret = gst_caps_merge_structure (encoded_ret, tmp2);
              have_profile_level = TRUE;
            }
          }

          if (!have_profile_level) {
            encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
          } else {
            gst_structure_free (tmp);
          }

          tmp = gst_structure_new ("video/x-divx",
              "width", GST_TYPE_INT_RANGE, 16, 4096,
              "height", GST_TYPE_INT_RANGE, 16, 4096,
              "framerate", GST_TYPE_FRACTION_RANGE,
              0, 1, G_MAXINT, 1,
              "divxversion", GST_TYPE_INT_RANGE, 3, 5,
              "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "video/3gpp") == 0) {
          gint j;
          gboolean have_profile_level = FALSE;

          tmp = gst_structure_new ("video/x-h263",
              "width", GST_TYPE_INT_RANGE, 16, 4096,
              "height", GST_TYPE_INT_RANGE, 16, 4096,
              "framerate", GST_TYPE_FRACTION_RANGE,
              0, 1, G_MAXINT, 1,
              "parsed", G_TYPE_BOOLEAN, TRUE,
              "variant", G_TYPE_STRING, "itu", NULL);

          if (type->n_profile_levels) {
            for (j = type->n_profile_levels - 1; j >= 0; j--) {
              gint profile;

              profile =
                  gst_amc_h263_profile_to_gst_id (type->profile_levels[j].
                  profile);

              if (profile == -1) {
                GST_ERROR ("Unable to map h263 profile 0x%08x",
                    type->profile_levels[j].profile);
                continue;
              }

              tmp2 = gst_structure_copy (tmp);
              gst_structure_set (tmp2, "profile", G_TYPE_UINT, profile, NULL);

              if (codec_info->is_encoder) {
                gint k;
                gint level;
                GValue va = { 0, };
                GValue v = { 0, };

                g_value_init (&va, GST_TYPE_LIST);
                g_value_init (&v, G_TYPE_UINT);

                for (k = 1; k <= type->profile_levels[j].level && k != 0;
                    k <<= 1) {
                  level = gst_amc_h263_level_to_gst_id (k);
                  if (level == -1)
                    continue;

                  g_value_set_uint (&v, level);
                  gst_value_list_append_value (&va, &v);
                  g_value_reset (&v);
                }

                gst_structure_set_value (tmp2, "level", &va);
                g_value_unset (&va);
                g_value_unset (&v);
              }

              encoded_ret = gst_caps_merge_structure (encoded_ret, tmp2);
              have_profile_level = TRUE;
            }
          }

          if (!have_profile_level) {
            encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
          } else {
            gst_structure_free (tmp);
          }
        } else if (strcmp (type->mime, "video/avc") == 0) {
          gint j;
          gboolean have_profile_level = FALSE;

          tmp = gst_structure_new ("video/x-h264",
              "width", GST_TYPE_INT_RANGE, 16, 4096,
              "height", GST_TYPE_INT_RANGE, 16, 4096,
              "framerate", GST_TYPE_FRACTION_RANGE,
              0, 1, G_MAXINT, 1,
              "parsed", G_TYPE_BOOLEAN, TRUE,
              "stream-format", G_TYPE_STRING, "byte-stream",
              "alignment", G_TYPE_STRING, "au", NULL);

          if (type->n_profile_levels) {
            for (j = type->n_profile_levels - 1; j >= 0; j--) {
              const gchar *profile, *alternative = NULL;

              profile =
                  gst_amc_avc_profile_to_string (type->profile_levels[j].
                  profile, &alternative);

              if (!profile) {
                GST_ERROR ("Unable to map H264 profile 0x%08x",
                    type->profile_levels[j].profile);
                continue;
              }

              tmp2 = gst_structure_copy (tmp);
              gst_structure_set (tmp2, "profile", G_TYPE_STRING, profile, NULL);

              if (alternative) {
                tmp3 = gst_structure_copy (tmp);
                gst_structure_set (tmp3, "profile", G_TYPE_STRING, alternative,
                    NULL);
              } else
                tmp3 = NULL;

              if (codec_info->is_encoder) {
                const gchar *level;
                gint k;
                GValue va = { 0, };
                GValue v = { 0, };

                g_value_init (&va, GST_TYPE_LIST);
                g_value_init (&v, G_TYPE_STRING);
                for (k = 1; k <= type->profile_levels[j].level && k != 0;
                    k <<= 1) {
                  level = gst_amc_avc_level_to_string (k);
                  if (!level)
                    continue;

                  g_value_set_string (&v, level);
                  gst_value_list_append_value (&va, &v);
                  g_value_reset (&v);
                }

                gst_structure_set_value (tmp2, "level", &va);
                if (tmp3)
                  gst_structure_set_value (tmp3, "level", &va);

                g_value_unset (&va);
                g_value_unset (&v);
              }

              encoded_ret = gst_caps_merge_structure (encoded_ret, tmp2);
              if (tmp3)
                encoded_ret = gst_caps_merge_structure (encoded_ret, tmp3);
              have_profile_level = TRUE;
            }
          }

          if (!have_profile_level) {
            encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
          } else {
            gst_structure_free (tmp);
          }
        } else if (strcmp (type->mime, "video/hevc") == 0) {
          gint j;
          gboolean have_profile_level = FALSE;

          tmp = gst_structure_new ("video/x-h265",
              "width", GST_TYPE_INT_RANGE, 16, 4096,
              "height", GST_TYPE_INT_RANGE, 16, 4096,
              "framerate", GST_TYPE_FRACTION_RANGE,
              0, 1, G_MAXINT, 1,
              "parsed", G_TYPE_BOOLEAN, TRUE,
              "stream-format", G_TYPE_STRING, "byte-stream",
              "alignment", G_TYPE_STRING, "au", NULL);

          if (type->n_profile_levels) {
            for (j = type->n_profile_levels - 1; j >= 0; j--) {
              const gchar *profile;

              profile =
                  gst_amc_hevc_profile_to_string (type->profile_levels[j].
                  profile);

              if (!profile) {
                GST_ERROR ("Unable to map H265 profile 0x%08x",
                    type->profile_levels[j].profile);
                continue;
              }

              tmp2 = gst_structure_copy (tmp);
              gst_structure_set (tmp2, "profile", G_TYPE_STRING, profile, NULL);

              if (codec_info->is_encoder) {
                const gchar *level, *tier;
                gint k;
                GValue v = { 0, };

                g_value_init (&v, G_TYPE_STRING);
                for (k = 1; k <= type->profile_levels[j].level && k != 0;
                    k <<= 1) {
                  level = gst_amc_hevc_tier_level_to_string (k, &tier);
                  if (!level || !tier)
                    continue;

                  tmp3 = gst_structure_copy (tmp2);

                  g_value_set_string (&v, tier);
                  gst_structure_set_value (tmp3, "tier", &v);
                  g_value_reset (&v);

                  g_value_set_string (&v, level);
                  gst_structure_set_value (tmp3, "level", &v);
                  g_value_reset (&v);

                  encoded_ret = gst_caps_merge_structure (encoded_ret, tmp3);
                  have_profile_level = TRUE;
                }
                gst_structure_free (tmp2);
              } else {
                encoded_ret = gst_caps_merge_structure (encoded_ret, tmp2);
                have_profile_level = TRUE;
              }
            }
          }

          if (!have_profile_level) {
            encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
          } else {
            gst_structure_free (tmp);
          }
        } else if (strcmp (type->mime, "video/x-vnd.on2.vp8") == 0) {
          tmp = gst_structure_new ("video/x-vp8",
              "width", GST_TYPE_INT_RANGE, 16, 4096,
              "height", GST_TYPE_INT_RANGE, 16, 4096,
              "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "video/x-vnd.on2.vp9") == 0) {
          tmp = gst_structure_new ("video/x-vp9",
              "width", GST_TYPE_INT_RANGE, 16, 4096,
              "height", GST_TYPE_INT_RANGE, 16, 4096,
              "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "video/av01") == 0) {
          tmp = gst_structure_new ("video/x-av1",
              "stream-format", G_TYPE_STRING, "obu-stream",
              "alignment", G_TYPE_STRING, "tu",
              "width", GST_TYPE_INT_RANGE, 16, 4096,
              "height", GST_TYPE_INT_RANGE, 16, 4096,
              "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else if (strcmp (type->mime, "video/mpeg2") == 0) {
          tmp = gst_structure_new ("video/mpeg",
              "width", GST_TYPE_INT_RANGE, 16, 4096,
              "height", GST_TYPE_INT_RANGE, 16, 4096,
              "framerate", GST_TYPE_FRACTION_RANGE,
              0, 1, G_MAXINT, 1,
              "mpegversion", GST_TYPE_INT_RANGE, 1, 2,
              "systemstream", G_TYPE_BOOLEAN, FALSE,
              "parsed", G_TYPE_BOOLEAN, TRUE, NULL);

          encoded_ret = gst_caps_merge_structure (encoded_ret, tmp);
        } else {
          GST_WARNING ("Unsupported mimetype '%s'", type->mime);
        }
      }
    }
  }

  GST_DEBUG ("Returning caps for '%s':", codec_info->name);
  GST_DEBUG ("  raw caps: %" GST_PTR_FORMAT, raw_ret);
  GST_DEBUG ("  encoded caps: %" GST_PTR_FORMAT, encoded_ret);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    PLUGIN_NAME,
    PLUGIN_DESCRIPTION,
    plugin_init,
    PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
