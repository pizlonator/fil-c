/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
 *
 * gstrtppayloads.h: various helper functions to deal with RTP payload
 *     types.
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
 * SECTION:gstrtppayloads
 * @title: GstRTPPayloadInfo
 * @short_description: Helper methods for dealing with RTP payloads
 * @see_also: gstrtpbuffer
 *
 * The GstRTPPayloads helper functions makes it easy to deal with static and dynamic
 * payloads. Its main purpose is to retrieve properties such as the default clock-rate
 * and get session bandwidth information.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstrtppayloads.h"

/* pt, encoding_name, media, rate, params, bitrate */
static const GstRTPPayloadInfo info[] = {
  /* static audio */
  {0, "audio", "PCMU", 8000, "1", 64000},
  /* { 1, "audio", "reserved", 0, NULL, 0 }, */
  /* { 2, "audio", "reserved", 0, NULL, 0 }, */
  {3, "audio", "GSM", 8000, "1", 0},
  {4, "audio", "G723", 8000, "1", 0},
  {5, "audio", "DVI4", 8000, "1", 32000},
  {6, "audio", "DVI4", 16000, "1", 64000},
  {7, "audio", "LPC", 8000, "1", 0},
  {8, "audio", "PCMA", 8000, "1", 64000},
  {9, "audio", "G722", 8000, "1", 64000},
  {10, "audio", "L16", 44100, "2", 1411200},
  {11, "audio", "L16", 44100, "1", 705600},
  {12, "audio", "QCELP", 8000, "1", 0},
  {13, "audio", "CN", 8000, "1", 0},
  {14, "audio", "MPA", 90000, NULL, 0},
  {15, "audio", "G728", 8000, "1", 0},
  {16, "audio", "DVI4", 11025, "1", 44100},
  {17, "audio", "DVI4", 22050, "1", 88200},
  {18, "audio", "G729", 8000, "1", 0},
  /* { 19, "audio", "reserved", 0, NULL, 0 }, */
  /* { 20, "audio", "unassigned", 0, NULL, 0 }, */
  /* { 21, "audio", "unassigned", 0, NULL, 0 }, */
  /* { 22, "audio", "unassigned", 0, NULL, 0 }, */
  /* { 23, "audio", "unassigned", 0, NULL, 0 }, */

  /* video and video/audio */
  /* { 24, "video", "unassigned", 0, NULL, 0 }, */
  {25, "video", "CelB", 90000, NULL, 0},
  {26, "video", "JPEG", 90000, NULL, 0},
  /* { 27, "video", "unassigned", 0, NULL, 0 }, */
  {28, "video", "nv", 90000, NULL, 0},
  /* { 29, "video", "unassigned", 0, NULL, 0 }, */
  /* { 30, "video", "unassigned", 0, NULL, 0 }, */
  {31, "video", "H261", 90000, NULL, 0},
  {32, "video", "MPV", 90000, NULL, 0},
  {33, "video", "MP2T", 90000, NULL, 0},
  {34, "video", "H263", 90000, NULL, 0},
  /* { 35-71, "unassigned", 0, 0, NULL, 0 }, */
  /* { 72-76, "reserved", 0, 0, NULL, 0 }, */
  /* { 77-95, "unassigned", 0, 0, NULL, 0 }, */
  /* { 96-127, "dynamic", 0, 0, NULL, 0 }, */

  /* dynamic stuff */
  {G_MAXUINT8, "application", "parityfec", 0, NULL, 0}, /* [RFC3009] */
  {G_MAXUINT8, "application", "rtx", 0, NULL, 0},       /* [RFC4588] */
  {G_MAXUINT8, "audio", "AMR", 8000, NULL, 0},  /* [RFC4867][RFC3267] */
  {G_MAXUINT8, "audio", "AMR-WB", 16000, NULL, 0},      /* [RFC4867][RFC3267] */
  {G_MAXUINT8, "audio", "DAT12", 0, NULL, 0},   /* [RFC3190] */
  {G_MAXUINT8, "audio", "dsr-es201108", 0, NULL, 0},    /* [RFC3557] */
  {G_MAXUINT8, "audio", "EVRC", 8000, "1", 0},  /* [RFC4788]  */
  {G_MAXUINT8, "audio", "EVRC0", 8000, "1", 0}, /* [RFC4788]  */
  {G_MAXUINT8, "audio", "EVRC1", 8000, "1", 0}, /* [RFC4788]  */
  {G_MAXUINT8, "audio", "EVRCB", 8000, "1", 0}, /* [RFC4788]  */
  {G_MAXUINT8, "audio", "EVRCB0", 8000, "1", 0},        /* [RFC4788]  */
  {G_MAXUINT8, "audio", "EVRCB1", 8000, "1", 0},        /* [RFC4788]  */
  {G_MAXUINT8, "audio", "G7221", 16000, "1", 0},        /* [RFC3047] */
  {G_MAXUINT8, "audio", "G726-16", 8000, "1", 0},       /* [RFC3551][RFC4856] */
  {G_MAXUINT8, "audio", "G726-24", 8000, "1", 0},       /* [RFC3551][RFC4856] */
  {G_MAXUINT8, "audio", "G726-32", 8000, "1", 0},       /* [RFC3551][RFC4856] */
  {G_MAXUINT8, "audio", "G726-40", 8000, "1", 0},       /* [RFC3551][RFC4856] */
  {G_MAXUINT8, "audio", "G729D", 8000, "1", 0}, /* [RFC3551][RFC4856] */
  {G_MAXUINT8, "audio", "G729E", 8000, "1", 0}, /* [RFC3551][RFC4856] */
  {G_MAXUINT8, "audio", "GSM-EFR", 8000, "1", 0},       /* [RFC3551][RFC4856] */
  {G_MAXUINT8, "audio", "L8", 0, NULL, 0},      /* [RFC3551][RFC4856] */
  {G_MAXUINT8, "audio", "RED", 0, NULL, 0},     /* [RFC2198][RFC3555] */
  {G_MAXUINT8, "audio", "rtx", 0, NULL, 0},     /* [RFC4588] */
  {G_MAXUINT8, "audio", "VDVI", 0, "1", 0},     /* [RFC3551][RFC4856] */
  {G_MAXUINT8, "audio", "L20", 0, NULL, 0},     /* [RFC3190] */
  {G_MAXUINT8, "audio", "L24", 0, NULL, 0},     /* [RFC3190] */
  {G_MAXUINT8, "audio", "MP4A-LATM", 0, NULL, 0},       /* [RFC3016] */
  {G_MAXUINT8, "audio", "mpa-robust", 90000, NULL, 0},  /* [RFC3119] */
  {G_MAXUINT8, "audio", "parityfec", 0, NULL, 0},       /* [RFC3009] */
  {G_MAXUINT8, "audio", "SMV", 8000, "1", 0},   /* [RFC3558] */
  {G_MAXUINT8, "audio", "SMV0", 8000, "1", 0},  /* [RFC3558] */
  {G_MAXUINT8, "audio", "t140c", 0, NULL, 0},   /* [RFC4351] */
  {G_MAXUINT8, "audio", "t38", 0, NULL, 0},     /* [RFC4612] */
  {G_MAXUINT8, "audio", "telephone-event", 0, NULL, 0}, /* [RFC4733] */
  {G_MAXUINT8, "audio", "tone", 0, NULL, 0},    /* [RFC4733] */
  {G_MAXUINT8, "audio", "DVI4", 0, NULL, 0},    /* [RFC4856] */
  {G_MAXUINT8, "audio", "G722", 0, NULL, 0},    /* [RFC4856] */
  {G_MAXUINT8, "audio", "G723", 0, NULL, 0},    /* [RFC4856] */
  {G_MAXUINT8, "audio", "G728", 0, NULL, 0},    /* [RFC4856] */
  {G_MAXUINT8, "audio", "G729", 0, NULL, 0},    /* [RFC4856] */
  {G_MAXUINT8, "audio", "GSM", 0, NULL, 0},     /* [RFC4856] */
  {G_MAXUINT8, "audio", "L16", 0, NULL, 0},     /* [RFC4856] */
  {G_MAXUINT8, "audio", "LPC", 0, NULL, 0},     /* [RFC4856] */
  {G_MAXUINT8, "audio", "PCMA", 0, NULL, 0},    /* [RFC4856] */
  {G_MAXUINT8, "audio", "PCMU", 0, NULL, 0},    /* [RFC4856] */
  {G_MAXUINT8, "text", "parityfec", 0, NULL, 0},        /* [RFC3009] */
  {G_MAXUINT8, "text", "red", 1000, NULL, 0},   /* [RFC4102] */
  {G_MAXUINT8, "text", "rtx", 0, NULL, 0},      /* [RFC4588] */
  {G_MAXUINT8, "text", "t140", 1000, NULL, 0},  /* [RFC4103] */
  {G_MAXUINT8, "video", "BMPEG", 90000, NULL, 0},       /* [RFC2343][RFC3555] */
  {G_MAXUINT8, "video", "BT656", 90000, NULL, 0},       /* [RFC2431][RFC3555] */
  {G_MAXUINT8, "video", "DV", 90000, NULL, 0},  /* [RFC3189] */
  {G_MAXUINT8, "video", "H263-1998", 90000, NULL, 0},   /* [RFC2429][RFC3555] */
  {G_MAXUINT8, "video", "H263-2000", 90000, NULL, 0},   /* [RFC2429][RFC3555] */
  {G_MAXUINT8, "video", "MP1S", 90000, NULL, 0},        /* [RFC2250][RFC3555] */
  {G_MAXUINT8, "video", "MP2P", 90000, NULL, 0},        /* [RFC2250][RFC3555] */
  {G_MAXUINT8, "video", "MP4V-ES", 90000, NULL, 0},     /* [RFC3016] */
  {G_MAXUINT8, "video", "parityfec", 0, NULL, 0},       /* [RFC3009] */
  {G_MAXUINT8, "video", "pointer", 90000, NULL, 0},     /* [RFC2862] */
  {G_MAXUINT8, "video", "raw", 90000, NULL, 0}, /* [RFC4175] */
  {G_MAXUINT8, "video", "rtx", 0, NULL, 0},     /* [RFC4588] */
  {G_MAXUINT8, "video", "SMPTE292M", 0, NULL, 0},       /* [RFC3497] */
  {G_MAXUINT8, "video", "vc1", 90000, NULL, 0}, /* [RFC4425] */

  /* not in http://www.iana.org/assignments/rtp-parameters */
  {G_MAXUINT8, "audio", "AC3", 0, NULL, 0},
  {G_MAXUINT8, "audio", "ILBC", 8000, NULL, 0},
  {G_MAXUINT8, "audio", "MPEG4-GENERIC", 0, NULL, 0},
  {G_MAXUINT8, "audio", "SPEEX", 0, NULL, 0},
  {G_MAXUINT8, "audio", "OPUS", 48000, NULL, 0},

  {G_MAXUINT8, "application", "MPEG4-GENERIC", 0, NULL, 0},

  {G_MAXUINT8, "video", "H264", 90000, NULL, 0},
  {G_MAXUINT8, "video", "H265", 90000, NULL, 0},
  {G_MAXUINT8, "video", "MPEG4-GENERIC", 90000, NULL, 0},
  {G_MAXUINT8, "video", "THEORA", 0, NULL, 0},
  {G_MAXUINT8, "video", "VORBIS", 0, NULL, 0},
  {G_MAXUINT8, "video", "X-SV3V-ES", 90000, NULL, 0},
  {G_MAXUINT8, "video", "X-SORENSON-VIDEO", 90000, NULL, 0},
  {G_MAXUINT8, "video", "VP8", 90000, NULL, 0},
  {G_MAXUINT8, "video", "VP9", 90000, NULL, 0},

  /* real stuff */
  {G_MAXUINT8, "video", "x-pn-realvideo", 1000, NULL, 0},
  {G_MAXUINT8, "audio", "x-pn-realaudio", 1000, NULL, 0},
  {G_MAXUINT8, "application", "x-pn-realmedia", 1000, NULL, 0},

  /* terminator */
  {G_MAXUINT8, NULL, NULL, 0, NULL, 0}
};

/**
 * gst_rtp_payload_info_for_pt:
 * @payload_type: the payload_type to find
 *
 * Get the #GstRTPPayloadInfo for @payload_type. This function is
 * mostly used to get the default clock-rate and bandwidth for static payload
 * types specified with @payload_type.
 *
 * Returns: (nullable): a #GstRTPPayloadInfo or NULL when no info could be found.
 */
const GstRTPPayloadInfo *
gst_rtp_payload_info_for_pt (guint8 payload_type)
{
  const GstRTPPayloadInfo *result = NULL;
  gint i;

  for (i = 0; info[i].media; i++) {
    if (info[i].payload_type == payload_type) {
      result = &info[i];
      break;
    }
  }
  return result;
}

/**
 * gst_rtp_payload_info_for_name:
 * @media: the media to find
 * @encoding_name: the encoding name to find
 *
 * Get the #GstRTPPayloadInfo for @media and @encoding_name. This function is
 * mostly used to get the default clock-rate and bandwidth for dynamic payload
 * types specified with @media and @encoding name.
 *
 * The search for @encoding_name will be performed in a case insensitive way.
 *
 * Returns: (nullable): a #GstRTPPayloadInfo or NULL when no info could be found.
 */
const GstRTPPayloadInfo *
gst_rtp_payload_info_for_name (const gchar * media, const gchar * encoding_name)
{
  const GstRTPPayloadInfo *result = NULL;
  gint i;

  for (i = 0; info[i].media; i++) {
    if (strcmp (media, info[i].media) == 0
        && g_ascii_strcasecmp (encoding_name, info[i].encoding_name) == 0) {
      result = &info[i];
      break;
    }
  }
  return result;
}
