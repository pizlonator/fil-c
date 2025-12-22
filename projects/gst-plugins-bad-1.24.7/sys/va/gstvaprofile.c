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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvaprofile.h"

/* *INDENT-OFF* */
static const struct ProfileMap
{
  VAProfile profile;
  GstVaCodecs codec;
  const gchar *va_name;
  const gchar *name;
  const gchar *media_type;
  const gchar *caps_str;
  const gchar *decoder_compatible_caps_str;
} profile_map[] = {
#define O(codec, va_name, name, media_type, caps_str, all_caps_str)   \
    G_PASTE (G_PASTE (VAProfile, codec), va_name), codec,             \
    G_STRINGIFY (G_PASTE (G_PASTE (VAProfile, codec), va_name)),      \
    name, media_type, caps_str, all_caps_str
#define P(codec, va_name, name, media_type, caps_str) {               \
    O(codec, va_name, name, media_type, caps_str, NULL) }
#define Q(codec, va_name, name, media_type, caps_str, all_caps_str) { \
    O(codec, va_name, name, media_type, caps_str, all_caps_str) }
  P (MPEG2, Simple, "simple", "video/mpeg",
      "mpegversion = (int) 2, profile = (string) simple"),
  P (MPEG2, Main, "main", "video/mpeg",
      "mpegversion = (int) 2, profile = (string) main"),
  P (MPEG4, Simple, "simple", "video/mpeg",
      "mpegversion = (int) 4, profile = (string) simple"),
  P (MPEG4, AdvancedSimple, "advanced-simple", "video/mpeg",
      "mpegversion = (int) 4, profile = (string) advanced-simple"),
  P (MPEG4, Main, "main", "video/mpeg",
      "mpegversion = (int) 4, profile = (string) main, "),
  P (H264, Main, "main", "video/x-h264", "profile = (string) main"),
  P (H264, High, "high", "video/x-h264", "profile = (string) high"),
  P (VC1, Simple, "simple", "video/x-wmv",
      "wmvversion = (int) 3, profile = (string) simple"),
  P (VC1, Main, "main", "video/x-wmv",
      "wmvversion = (int) 3, profile = (string) main"),
  P (VC1, Advanced, "advanced", "video/x-wmv",
      "wmvversion = (int) 3, format = (string) WVC1, "
      "profile = (string) advanced"),
  P (H263, Baseline, "baseline", "video/x-h263",
      "variant = (string) itu, h263version = (string) h263, "
      "profile = (string) baseline"),
  P (JPEG, Baseline, "", "image/jpeg", "sof-marker = (int) 0"),
  P (H264, ConstrainedBaseline, "constrained-baseline", "video/x-h264",
      "profile = (string) constrained-baseline"),
  P (VP8, Version0_3, "", "video/x-vp8", NULL),
  /* Unsupported profiles by current GstH264Decoder */
  /* P (H264, MultiviewHigh, "video/x-h264", */
  /*     "profile = (string) {  multiview-high, stereo-high }"), */
  /* P (H264, StereoHigh, "video/x-h264", */
  /*     "profile = (string) {  multiview-high, stereo-high }"), */
  Q (HEVC, Main, "main", "video/x-h265", "profile = (string) main",
     "profile = (string) { main, main-intra, main-still-picture }"),
  Q (HEVC, Main10, "main-10", "video/x-h265", "profile = (string) main-10",
     "profile = (string) { main-10, main-10-intra }"),
  P (VP9, Profile0, "0", "video/x-vp9", "profile = (string) 0"),
  P (VP9, Profile1, "1", "video/x-vp9", "profile = (string) 1"),
  P (VP9, Profile2, "2", "video/x-vp9", "profile = (string) 2"),
  P (VP9, Profile3, "3", "video/x-vp9", "profile = (string) 3"),
  P (HEVC, Main12, "main-12", "video/x-h265", "profile = (string) main-12"),
  P (HEVC, Main422_10, "main-422-10", "video/x-h265",
      "profile = (string) main-422-10"),
  P (HEVC, Main422_12, "main-422-12", "video/x-h265",
      "profile = (string) main-422-12"),
  P (HEVC, Main444, "main-444", "video/x-h265", "profile = (string) main-444"),
  P (HEVC, Main444_10, "main-444-10", "video/x-h265",
      "profile = (string) main-444-10"),
  P (HEVC, Main444_12, "main-444-12", "video/x-h265",
      "profile = (string) main-444-12"),
  P (HEVC, SccMain, "screen-extended-main", "video/x-h265",
      "profile = (string) screen-extended-main"),
  P (HEVC, SccMain10, "screen-extended-main-10", "video/x-h265",
      "profile = (string) screen-extended-main-10"),
  P (HEVC, SccMain444, "screen-extended-main-444", "video/x-h265",
      "profile = (string) screen-extended-main-444"),
  /* Spec A.2:
     "Main" compliant decoders must be able to decode streams with
     seq_profile equal to 0.
     "High" compliant decoders must be able to decode streams with
     seq_profile less than or equal to 1.
     "Professional" compliant decoders must be able to decode streams
     with seq_profile less than or equal to 2.

     The correct relationship between profile "main" "high" "professional"
     and seq_profile "0" "1" "2" should be:
     main <------> { 0 }
     high <------> { main, 1 }
     professional <------> { high, 2 }

     So far, all va decoders can support "0" when they support "1",
     we just map "0" to "main" and "1" to "high".  */
  P (AV1, Profile0, "main", "video/x-av1", "profile = (string) main"),
  P (AV1, Profile1, "high", "video/x-av1", "profile = (string) high"),
  P (HEVC, SccMain444_10, "screen-extended-main-444-10", "video/x-h265",
      "profile = (string) screen-extended-main-444-10"),
#undef O
#undef P
#undef Q
};
/* *INDENT-ON* */

static const struct ProfileMap *
get_profile_map (VAProfile profile)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (profile_map); i++) {
    if (profile_map[i].profile == profile)
      return &profile_map[i];
  }

  return NULL;
}

guint32
gst_va_profile_codec (VAProfile profile)
{
  const struct ProfileMap *map = get_profile_map (profile);
  return map ? map->codec : GST_MAKE_FOURCC ('N', 'O', 'N', 'E');
}

const gchar *
gst_va_profile_name (VAProfile profile)
{
  const struct ProfileMap *map = get_profile_map (profile);
  return map ? map->va_name : NULL;
}

VAProfile
gst_va_profile_from_name (GstVaCodecs codec, const gchar * name)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (profile_map); i++) {
    if (profile_map[i].codec == codec &&
        g_strcmp0 (profile_map[i].name, name) == 0)
      return profile_map[i].profile;
  }

  return VAProfileNone;
}

GstCaps *
gst_va_profile_caps (VAProfile profile, VAEntrypoint entrypoint)
{
  const struct ProfileMap *map = get_profile_map (profile);
  GstCaps *caps;
  gchar *caps_str;

  if (!map)
    return NULL;

  if (entrypoint == VAEntrypointVLD && map->decoder_compatible_caps_str) {
    caps_str = g_strdup_printf ("%s, %s", map->media_type,
        map->decoder_compatible_caps_str);
  } else if (map->caps_str)
    caps_str = g_strdup_printf ("%s, %s", map->media_type, map->caps_str);
  else
    caps_str = g_strdup (map->media_type);

  caps = gst_caps_from_string (caps_str);
  g_free (caps_str);

  return caps;
}
