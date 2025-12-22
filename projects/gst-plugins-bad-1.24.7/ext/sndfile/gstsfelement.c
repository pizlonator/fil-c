/* GStreamer libsndfile plugin
 * Copyright (C) 2003 Andy Wingo <wingo at pobox dot com>
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

#include <glib/gi18n-lib.h>
#include <string.h>

#include "gstsfelements.h"

/* sf formats */

GstCaps *
gst_sf_create_audio_template_caps (void)
{
  GstCaps *caps = gst_caps_new_empty ();
  SF_FORMAT_INFO format_info;
  const gchar *fmt;
  gint k, count;

  sf_command (NULL, SFC_GET_FORMAT_MAJOR_COUNT, &count, sizeof (gint));

  for (k = 0; k < count; k++) {
    format_info.format = k;
    sf_command (NULL, SFC_GET_FORMAT_MAJOR, &format_info, sizeof (format_info));

    switch (format_info.format) {
      case SF_FORMAT_IRCAM:    /* Berkeley/IRCAM/CARL */
        fmt = "audio/x-ircam";
        break;
      case SF_FORMAT_NIST:     /* Sphere NIST format. */
        fmt = "audio/x-nist";
        break;
      case SF_FORMAT_PAF:      /* Ensoniq PARIS file format. */
        fmt = "audio/x-paris";
        break;
      case SF_FORMAT_SDS:      /* Midi Sample Dump Standard */
        fmt = "audio/x-sds";
        break;
      case SF_FORMAT_SVX:      /* Amiga IFF / SVX8 / SV16 format. */
        fmt = "audio/x-svx";
        break;
      case SF_FORMAT_VOC:      /* VOC files. */
        fmt = "audio/x-voc";
        break;
      case SF_FORMAT_W64:      /* Sonic Foundry's 64 bit RIFF/WAV */
        fmt = "audio/x-w64";
        break;
      case SF_FORMAT_XI:       /* Fasttracker 2 Extended Instrument */
        fmt = "audio/x-xi";
        break;
      case SF_FORMAT_RF64:     /* RF64 WAV file */
        fmt = "audio/x-rf64";
        break;
        /* does not make sense to expose that */
      case SF_FORMAT_RAW:      /* RAW PCM data. */
        /* we have other elements to handle these */
      case SF_FORMAT_AIFF:     /* Apple/SGI AIFF format */
      case SF_FORMAT_AU:       /* Sun/NeXT AU format */
      case SF_FORMAT_FLAC:     /* FLAC lossless file format */
      case SF_FORMAT_OGG:      /* Xiph OGG container */
      case SF_FORMAT_WAV:      /* Microsoft WAV format */
      case SF_FORMAT_WAVEX:    /* MS WAVE with WAVEFORMATEX */
        fmt = NULL;
        GST_LOG ("skipping format '%s'", format_info.name);
        break;
      case SF_FORMAT_MAT4:     /* Matlab (tm) V4.2 / GNU Octave 2.0 */
      case SF_FORMAT_MAT5:     /* Matlab (tm) V5.0 / GNU Octave 2.1 */
      case SF_FORMAT_PVF:      /* Portable Voice Format */
      case SF_FORMAT_HTK:      /* HMM Tool Kit format */
      case SF_FORMAT_AVR:      /* Audio Visual Research */
      case SF_FORMAT_SD2:      /* Sound Designer 2 */
      case SF_FORMAT_CAF:      /* Core Audio File format */
      case SF_FORMAT_WVE:      /* Psion WVE format */
      case SF_FORMAT_MPC2K:    /* Akai MPC 2000 sampler */
      default:
        fmt = NULL;
        GST_WARNING ("format 0x%x: '%s' is not mapped", format_info.format,
            format_info.name);
    }
    if (fmt != NULL) {
      gst_caps_append_structure (caps, gst_structure_new_empty (fmt));
    }
  }
  return gst_caps_simplify (caps);
}

void
sf_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;

  if (g_once_init_enter (&res)) {
#ifdef ENABLE_NLS
    GST_DEBUG ("binding text domain %s to locale dir %s", GETTEXT_PACKAGE,
        LOCALEDIR);
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */
    g_once_init_leave (&res, TRUE);
  }
}
