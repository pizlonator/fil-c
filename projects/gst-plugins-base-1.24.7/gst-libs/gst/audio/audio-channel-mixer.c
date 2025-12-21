/* GStreamer
 * Copyright (C) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2008 Sebastian Dröge <slomo@circular-chaos.org>
 *
 * audio-channel-mixer.c: setup of channel conversion matrices
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

#include <math.h>
#include <string.h>

#include "audio-channel-mixer.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("audio-channel-mixer", 0,
        "audio-channel-mixer object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */


#define PRECISION_INT 10

typedef void (*MixerFunc) (GstAudioChannelMixer * mix, const gpointer src[],
    gpointer dst[], gint samples);

struct _GstAudioChannelMixer
{
  gint in_channels;
  gint out_channels;

  /* channel conversion matrix, m[in_channels][out_channels].
   * If identity matrix, passthrough applies. */
  gfloat **matrix;

  /* channel conversion matrix with int values, m[in_channels][out_channels].
   * this is matrix * (2^10) as integers */
  gint **matrix_int;

  MixerFunc func;
};

/**
 * gst_audio_channel_mixer_free:
 * @mix: a #GstAudioChannelMixer
 *
 * Free memory allocated by @mix.
 */
void
gst_audio_channel_mixer_free (GstAudioChannelMixer * mix)
{
  gint i;

  /* free */
  for (i = 0; i < mix->in_channels; i++)
    g_free (mix->matrix[i]);
  g_free (mix->matrix);
  mix->matrix = NULL;

  for (i = 0; i < mix->in_channels; i++)
    g_free (mix->matrix_int[i]);
  g_free (mix->matrix_int);
  mix->matrix_int = NULL;

  g_free (mix);
}

/*
 * Detect and fill in identical channels. E.g.
 * forward the left/right front channels in a
 * 5.1 to 2.0 conversion.
 */

static void
gst_audio_channel_mixer_fill_identical (gfloat ** matrix,
    gint in_channels, GstAudioChannelPosition * in_position, gint out_channels,
    GstAudioChannelPosition * out_position, GstAudioChannelMixerFlags flags)
{
  gint ci, co;

  /* Apart from the compatible channel assignments, we can also have
   * same channel assignments. This is much simpler, we simply copy
   * the value from source to dest! */
  for (co = 0; co < out_channels; co++) {
    /* find a channel in input with same position */
    for (ci = 0; ci < in_channels; ci++) {
      /* If the input was unpositioned, we're simply building
       * an identity matrix */
      if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_UNPOSITIONED_IN) {
        matrix[ci][co] = ci == co ? 1.0 : 0.0;
      } else if (in_position[ci] == out_position[co]) {
        matrix[ci][co] = 1.0;
      }
    }
  }
}

/*
 * Detect and fill in compatible channels. E.g.
 * forward left/right front to mono (or the other
 * way around) when going from 2.0 to 1.0.
 */

static void
gst_audio_channel_mixer_fill_compatible (gfloat ** matrix, gint in_channels,
    GstAudioChannelPosition * in_position, gint out_channels,
    GstAudioChannelPosition * out_position)
{
  /* Conversions from one-channel to compatible two-channel configs */
  struct
  {
    GstAudioChannelPosition pos1[2];
    GstAudioChannelPosition pos2[1];
  } conv[] = {
    /* front: mono <-> stereo */
    {{
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
            GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT}, {
            GST_AUDIO_CHANNEL_POSITION_MONO}},
    /* front center: 2 <-> 1 */
    {{
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
            GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER}, {
            GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER}},
    /* rear: 2 <-> 1 */
    {{
                GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
            GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT}, {
            GST_AUDIO_CHANNEL_POSITION_REAR_CENTER}}, {{
            GST_AUDIO_CHANNEL_POSITION_INVALID}}
  };
  gint c;

  /* conversions from compatible (but not the same) channel schemes */
  for (c = 0; conv[c].pos1[0] != GST_AUDIO_CHANNEL_POSITION_INVALID; c++) {
    gint pos1_0 = -1, pos1_1 = -1, pos1_2 = -1;
    gint pos2_0 = -1, pos2_1 = -1, pos2_2 = -1;
    gint n;

    for (n = 0; n < in_channels; n++) {
      if (in_position[n] == conv[c].pos1[0])
        pos1_0 = n;
      else if (in_position[n] == conv[c].pos1[1])
        pos1_1 = n;
      else if (in_position[n] == conv[c].pos2[0])
        pos1_2 = n;
    }
    for (n = 0; n < out_channels; n++) {
      if (out_position[n] == conv[c].pos1[0])
        pos2_0 = n;
      else if (out_position[n] == conv[c].pos1[1])
        pos2_1 = n;
      else if (out_position[n] == conv[c].pos2[0])
        pos2_2 = n;
    }

    /* The general idea here is to fill in channels from the same position
     * as good as possible. This means mixing left<->center and right<->center.
     */

    /* left -> center */
    if (pos1_0 != -1 && pos1_2 == -1 && pos2_0 == -1 && pos2_2 != -1)
      matrix[pos1_0][pos2_2] = 1.0;
    else if (pos1_0 != -1 && pos1_2 != -1 && pos2_0 == -1 && pos2_2 != -1)
      matrix[pos1_0][pos2_2] = 0.5;
    else if (pos1_0 != -1 && pos1_2 == -1 && pos2_0 != -1 && pos2_2 != -1)
      matrix[pos1_0][pos2_2] = 1.0;

    /* right -> center */
    if (pos1_1 != -1 && pos1_2 == -1 && pos2_1 == -1 && pos2_2 != -1)
      matrix[pos1_1][pos2_2] = 1.0;
    else if (pos1_1 != -1 && pos1_2 != -1 && pos2_1 == -1 && pos2_2 != -1)
      matrix[pos1_1][pos2_2] = 0.5;
    else if (pos1_1 != -1 && pos1_2 == -1 && pos2_1 != -1 && pos2_2 != -1)
      matrix[pos1_1][pos2_2] = 1.0;

    /* center -> left */
    if (pos1_2 != -1 && pos1_0 == -1 && pos2_2 == -1 && pos2_0 != -1)
      matrix[pos1_2][pos2_0] = 1.0;
    else if (pos1_2 != -1 && pos1_0 != -1 && pos2_2 == -1 && pos2_0 != -1)
      matrix[pos1_2][pos2_0] = 0.5;
    else if (pos1_2 != -1 && pos1_0 == -1 && pos2_2 != -1 && pos2_0 != -1)
      matrix[pos1_2][pos2_0] = 1.0;

    /* center -> right */
    if (pos1_2 != -1 && pos1_1 == -1 && pos2_2 == -1 && pos2_1 != -1)
      matrix[pos1_2][pos2_1] = 1.0;
    else if (pos1_2 != -1 && pos1_1 != -1 && pos2_2 == -1 && pos2_1 != -1)
      matrix[pos1_2][pos2_1] = 0.5;
    else if (pos1_2 != -1 && pos1_1 == -1 && pos2_2 != -1 && pos2_1 != -1)
      matrix[pos1_2][pos2_1] = 1.0;
  }
}

/*
 * Detect and fill in channels not handled by the
 * above two, e.g. center to left/right front in
 * 5.1 to 2.0 (or the other way around).
 *
 * Unfortunately, limited to static conversions
 * for now.
 */

static void
gst_audio_channel_mixer_detect_pos (gint channels,
    GstAudioChannelPosition position[64], gint * f, gboolean * has_f, gint * c,
    gboolean * has_c, gint * r, gboolean * has_r, gint * s, gboolean * has_s,
    gint * b, gboolean * has_b)
{
  gint n;

  for (n = 0; n < channels; n++) {
    switch (position[n]) {
      case GST_AUDIO_CHANNEL_POSITION_MONO:
        f[1] = n;
        *has_f = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT:
        f[0] = n;
        *has_f = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT:
        f[2] = n;
        *has_f = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER:
        c[1] = n;
        *has_c = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
        c[0] = n;
        *has_c = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
        c[2] = n;
        *has_c = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_REAR_CENTER:
        r[1] = n;
        *has_r = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_REAR_LEFT:
        r[0] = n;
        *has_r = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT:
        r[2] = n;
        *has_r = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT:
        s[0] = n;
        *has_s = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT:
        s[2] = n;
        *has_s = TRUE;
        break;
      case GST_AUDIO_CHANNEL_POSITION_LFE1:
        *has_b = TRUE;
        b[1] = n;
        break;
      default:
        break;
    }
  }
}

static void
gst_audio_channel_mixer_fill_one_other (gfloat ** matrix,
    gint * from_idx, gint * to_idx, gfloat ratio)
{

  /* src & dst have center => passthrough */
  if (from_idx[1] != -1 && to_idx[1] != -1) {
    matrix[from_idx[1]][to_idx[1]] = ratio;
  }

  /* src & dst have left => passthrough */
  if (from_idx[0] != -1 && to_idx[0] != -1) {
    matrix[from_idx[0]][to_idx[0]] = ratio;
  }

  /* src & dst have right => passthrough */
  if (from_idx[2] != -1 && to_idx[2] != -1) {
    matrix[from_idx[2]][to_idx[2]] = ratio;
  }

  /* src has left & dst has center => put into center */
  if (from_idx[0] != -1 && to_idx[1] != -1 && from_idx[1] != -1) {
    matrix[from_idx[0]][to_idx[1]] = 0.5 * ratio;
  } else if (from_idx[0] != -1 && to_idx[1] != -1 && from_idx[1] == -1) {
    matrix[from_idx[0]][to_idx[1]] = ratio;
  }

  /* src has right & dst has center => put into center */
  if (from_idx[2] != -1 && to_idx[1] != -1 && from_idx[1] != -1) {
    matrix[from_idx[2]][to_idx[1]] = 0.5 * ratio;
  } else if (from_idx[2] != -1 && to_idx[1] != -1 && from_idx[1] == -1) {
    matrix[from_idx[2]][to_idx[1]] = ratio;
  }

  /* src has center & dst has left => passthrough */
  if (from_idx[1] != -1 && to_idx[0] != -1 && from_idx[0] != -1) {
    matrix[from_idx[1]][to_idx[0]] = 0.5 * ratio;
  } else if (from_idx[1] != -1 && to_idx[0] != -1 && from_idx[0] == -1) {
    matrix[from_idx[1]][to_idx[0]] = ratio;
  }

  /* src has center & dst has right => passthrough */
  if (from_idx[1] != -1 && to_idx[2] != -1 && from_idx[2] != -1) {
    matrix[from_idx[1]][to_idx[2]] = 0.5 * ratio;
  } else if (from_idx[1] != -1 && to_idx[2] != -1 && from_idx[2] == -1) {
    matrix[from_idx[1]][to_idx[2]] = ratio;
  }
}

#define RATIO_CENTER_FRONT (1.0 / sqrt (2.0))
#define RATIO_CENTER_SIDE (1.0 / 2.0)
#define RATIO_CENTER_REAR (1.0 / sqrt (8.0))

#define RATIO_FRONT_CENTER (1.0 / sqrt (2.0))
#define RATIO_FRONT_SIDE (1.0 / sqrt (2.0))
#define RATIO_FRONT_REAR (1.0 / 2.0)

#define RATIO_SIDE_CENTER (1.0 / 2.0)
#define RATIO_SIDE_FRONT (1.0 / sqrt (2.0))
#define RATIO_SIDE_REAR (1.0 / sqrt (2.0))

#define RATIO_CENTER_BASS (1.0 / sqrt (2.0))
#define RATIO_FRONT_BASS (1.0)
#define RATIO_SIDE_BASS (1.0 / sqrt (2.0))
#define RATIO_REAR_BASS (1.0 / sqrt (2.0))

static void
gst_audio_channel_mixer_fill_others (gfloat ** matrix, gint in_channels,
    GstAudioChannelPosition * in_position, gint out_channels,
    GstAudioChannelPosition * out_position)
{
  gboolean in_has_front = FALSE, out_has_front = FALSE,
      in_has_center = FALSE, out_has_center = FALSE,
      in_has_rear = FALSE, out_has_rear = FALSE,
      in_has_side = FALSE, out_has_side = FALSE,
      in_has_bass = FALSE, out_has_bass = FALSE;
  /* LEFT, RIGHT, MONO */
  gint in_f[3] = { -1, -1, -1 };
  gint out_f[3] = { -1, -1, -1 };
  /* LOC, ROC, CENTER */
  gint in_c[3] = { -1, -1, -1 };
  gint out_c[3] = { -1, -1, -1 };
  /* RLEFT, RRIGHT, RCENTER */
  gint in_r[3] = { -1, -1, -1 };
  gint out_r[3] = { -1, -1, -1 };
  /* SLEFT, INVALID, SRIGHT */
  gint in_s[3] = { -1, -1, -1 };
  gint out_s[3] = { -1, -1, -1 };
  /* INVALID, LFE, INVALID */
  gint in_b[3] = { -1, -1, -1 };
  gint out_b[3] = { -1, -1, -1 };

  /* First see where (if at all) the various channels from/to
   * which we want to convert are located in our matrix/array. */
  gst_audio_channel_mixer_detect_pos (in_channels, in_position,
      in_f, &in_has_front,
      in_c, &in_has_center, in_r, &in_has_rear,
      in_s, &in_has_side, in_b, &in_has_bass);
  gst_audio_channel_mixer_detect_pos (out_channels, out_position,
      out_f, &out_has_front,
      out_c, &out_has_center, out_r, &out_has_rear,
      out_s, &out_has_side, out_b, &out_has_bass);

  /* The general idea here is:
   * - if the source has a channel that the destination doesn't have mix
   *   it into the nearest available destination channel
   * - if the destination has a channel that the source doesn't have mix
   *   the nearest source channel into the destination channel
   *
   * The ratio for the mixing becomes lower as the distance between the
   * channels gets larger
   */

  /* center <-> front/side/rear */
  if (!in_has_center && in_has_front && out_has_center) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_f, out_c,
        RATIO_CENTER_FRONT);
  } else if (!in_has_center && !in_has_front && in_has_side && out_has_center) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_c,
        RATIO_CENTER_SIDE);
  } else if (!in_has_center && !in_has_front && !in_has_side && in_has_rear
      && out_has_center) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_r, out_c,
        RATIO_CENTER_REAR);
  } else if (in_has_center && !out_has_center && out_has_front) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_c, out_f,
        RATIO_CENTER_FRONT);
  } else if (in_has_center && !out_has_center && !out_has_front && out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_c, out_s,
        RATIO_CENTER_SIDE);
  } else if (in_has_center && !out_has_center && !out_has_front && !out_has_side
      && out_has_rear) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_c, out_r,
        RATIO_CENTER_REAR);
  }

  /* front <-> center/side/rear */
  if (!in_has_front && in_has_center && !in_has_side && out_has_front) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_c, out_f,
        RATIO_CENTER_FRONT);
  } else if (!in_has_front && !in_has_center && in_has_side && out_has_front) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_f,
        RATIO_FRONT_SIDE);
  } else if (!in_has_front && in_has_center && in_has_side && out_has_front) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_c, out_f,
        0.5 * RATIO_CENTER_FRONT);
    gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_f,
        0.5 * RATIO_FRONT_SIDE);
  } else if (!in_has_front && !in_has_center && !in_has_side && in_has_rear
      && out_has_front) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_r, out_f,
        RATIO_FRONT_REAR);
  } else if (in_has_front && out_has_center && !out_has_side && !out_has_front) {
    gst_audio_channel_mixer_fill_one_other (matrix,
        in_f, out_c, RATIO_CENTER_FRONT);
  } else if (in_has_front && !out_has_center && out_has_side && !out_has_front) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_f, out_s,
        RATIO_FRONT_SIDE);
  } else if (in_has_front && out_has_center && out_has_side && !out_has_front) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_f, out_c,
        0.5 * RATIO_CENTER_FRONT);
    gst_audio_channel_mixer_fill_one_other (matrix, in_f, out_s,
        0.5 * RATIO_FRONT_SIDE);
  } else if (in_has_front && !out_has_center && !out_has_side && !out_has_front
      && out_has_rear) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_f, out_r,
        RATIO_FRONT_REAR);
  }

  /* side <-> center/front/rear */
  if (!in_has_side && in_has_front && !in_has_rear && out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_f, out_s,
        RATIO_FRONT_SIDE);
  } else if (!in_has_side && !in_has_front && in_has_rear && out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_r, out_s,
        RATIO_SIDE_REAR);
  } else if (!in_has_side && in_has_front && in_has_rear && out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_f, out_s,
        0.5 * RATIO_FRONT_SIDE);
    gst_audio_channel_mixer_fill_one_other (matrix, in_r, out_s,
        0.5 * RATIO_SIDE_REAR);
  } else if (!in_has_side && !in_has_front && !in_has_rear && in_has_center
      && out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_c, out_s,
        RATIO_CENTER_SIDE);
  } else if (in_has_side && out_has_front && !out_has_rear && !out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_f,
        RATIO_FRONT_SIDE);
  } else if (in_has_side && !out_has_front && out_has_rear && !out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_r,
        RATIO_SIDE_REAR);
  } else if (in_has_side && out_has_front && out_has_rear && !out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_f,
        0.5 * RATIO_FRONT_SIDE);
    gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_r,
        0.5 * RATIO_SIDE_REAR);
  } else if (in_has_side && !out_has_front && !out_has_rear && out_has_center
      && !out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_c,
        RATIO_CENTER_SIDE);
  }

  /* rear <-> center/front/side */
  if (!in_has_rear && in_has_side && out_has_rear) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_r,
        RATIO_SIDE_REAR);
  } else if (!in_has_rear && !in_has_side && in_has_front && out_has_rear) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_f, out_r,
        RATIO_FRONT_REAR);
  } else if (!in_has_rear && !in_has_side && !in_has_front && in_has_center
      && out_has_rear) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_c, out_r,
        RATIO_CENTER_REAR);
  } else if (in_has_rear && !out_has_rear && out_has_side) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_r, out_s,
        RATIO_SIDE_REAR);
  } else if (in_has_rear && !out_has_rear && !out_has_side && out_has_front) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_r, out_f,
        RATIO_FRONT_REAR);
  } else if (in_has_rear && !out_has_rear && !out_has_side && !out_has_front
      && out_has_center) {
    gst_audio_channel_mixer_fill_one_other (matrix, in_r, out_c,
        RATIO_CENTER_REAR);
  }

  /* bass <-> any */
  if (in_has_bass && !out_has_bass) {
    if (out_has_center) {
      gst_audio_channel_mixer_fill_one_other (matrix, in_b, out_c,
          RATIO_CENTER_BASS);
    }
    if (out_has_front) {
      gst_audio_channel_mixer_fill_one_other (matrix, in_b, out_f,
          RATIO_FRONT_BASS);
    }
    if (out_has_side) {
      gst_audio_channel_mixer_fill_one_other (matrix, in_b, out_s,
          RATIO_SIDE_BASS);
    }
    if (out_has_rear) {
      gst_audio_channel_mixer_fill_one_other (matrix, in_b, out_r,
          RATIO_REAR_BASS);
    }
  } else if (!in_has_bass && out_has_bass) {
    if (in_has_center) {
      gst_audio_channel_mixer_fill_one_other (matrix, in_c, out_b,
          RATIO_CENTER_BASS);
    }
    if (in_has_front) {
      gst_audio_channel_mixer_fill_one_other (matrix, in_f, out_b,
          RATIO_FRONT_BASS);
    }
    if (in_has_side) {
      gst_audio_channel_mixer_fill_one_other (matrix, in_s, out_b,
          RATIO_REAR_BASS);
    }
    if (in_has_rear) {
      gst_audio_channel_mixer_fill_one_other (matrix, in_r, out_b,
          RATIO_REAR_BASS);
    }
  }
}

/*
 * Normalize output values.
 */

static void
gst_audio_channel_mixer_fill_normalize (gfloat ** matrix, gint in_channels,
    gint out_channels)
{
  gfloat sum, top = 0;
  gint i, j;

  for (j = 0; j < out_channels; j++) {
    /* calculate sum */
    sum = 0.0;
    for (i = 0; i < in_channels; i++) {
      sum += fabs (matrix[i][j]);
    }
    if (sum > top) {
      top = sum;
    }
  }

  /* normalize to mix */
  if (top == 0.0)
    return;

  for (j = 0; j < out_channels; j++) {
    for (i = 0; i < in_channels; i++) {
      matrix[i][j] /= top;
    }
  }
}

static gboolean
gst_audio_channel_mixer_fill_special (gfloat ** matrix, gint in_channels,
    GstAudioChannelPosition * in_position, gint out_channels,
    GstAudioChannelPosition * out_position)
{
  /* Special, standard conversions here */

  /* Mono<->Stereo, just a fast-path */
  if (in_channels == 2 && out_channels == 1 &&
      ((in_position[0] == GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT &&
              in_position[1] == GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT) ||
          (in_position[0] == GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT &&
              in_position[1] == GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT)) &&
      out_position[0] == GST_AUDIO_CHANNEL_POSITION_MONO) {
    matrix[0][0] = 0.5;
    matrix[1][0] = 0.5;
    return TRUE;
  } else if (in_channels == 1 && out_channels == 2 &&
      ((out_position[0] == GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT &&
              out_position[1] == GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT) ||
          (out_position[0] == GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT &&
              out_position[1] == GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT)) &&
      in_position[0] == GST_AUDIO_CHANNEL_POSITION_MONO) {
    matrix[0][0] = 1.0;
    matrix[0][1] = 1.0;
    return TRUE;
  }

  /* TODO: 5.1 <-> Stereo and other standard conversions */

  return FALSE;
}

/*
 * Automagically generate conversion matrix.
 */

static void
gst_audio_channel_mixer_fill_matrix (gfloat ** matrix,
    GstAudioChannelMixerFlags flags, gint in_channels,
    GstAudioChannelPosition * in_position, gint out_channels,
    GstAudioChannelPosition * out_position)
{
  if (gst_audio_channel_mixer_fill_special (matrix, in_channels, in_position,
          out_channels, out_position))
    return;

  gst_audio_channel_mixer_fill_identical (matrix, in_channels, in_position,
      out_channels, out_position, flags);

  if (!(flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_UNPOSITIONED_IN)) {
    gst_audio_channel_mixer_fill_compatible (matrix, in_channels, in_position,
        out_channels, out_position);
    gst_audio_channel_mixer_fill_others (matrix, in_channels, in_position,
        out_channels, out_position);
    gst_audio_channel_mixer_fill_normalize (matrix, in_channels, out_channels);
  }
}

/* only call mix after mix->matrix is fully set up and normalized */
static void
gst_audio_channel_mixer_setup_matrix_int (GstAudioChannelMixer * mix)
{
  gint i, j;
  gfloat tmp;
  gfloat factor = (1 << PRECISION_INT);

  mix->matrix_int = g_new0 (gint *, mix->in_channels);

  for (i = 0; i < mix->in_channels; i++) {
    mix->matrix_int[i] = g_new (gint, mix->out_channels);

    for (j = 0; j < mix->out_channels; j++) {
      tmp = mix->matrix[i][j] * factor;
      mix->matrix_int[i][j] = (gint) tmp;
    }
  }
}

static gfloat **
gst_audio_channel_mixer_setup_matrix (GstAudioChannelMixerFlags flags,
    gint in_channels, GstAudioChannelPosition * in_position,
    gint out_channels, GstAudioChannelPosition * out_position)
{
  gint i, j;
  gfloat **matrix = g_new0 (gfloat *, in_channels);

  for (i = 0; i < in_channels; i++) {
    matrix[i] = g_new (gfloat, out_channels);
    for (j = 0; j < out_channels; j++)
      matrix[i][j] = 0.;
  }

  /* setup the matrix' internal values */
  gst_audio_channel_mixer_fill_matrix (matrix, flags, in_channels, in_position,
      out_channels, out_position);

  return matrix;
}

#define DEFINE_GET_DATA_FUNCS(type) \
static inline type \
_get_in_data_interleaved_##type (const type * in_data[], \
    gint sample, gint channel, gint total_channels) \
{ \
  return in_data[0][sample * total_channels + channel]; \
} \
\
static inline type * \
_get_out_data_interleaved_##type (type * out_data[], \
    gint sample, gint channel, gint total_channels) \
{ \
  return &out_data[0][sample * total_channels + channel]; \
} \
\
static inline type \
_get_in_data_planar_##type (const type * in_data[], \
    gint sample, gint channel, gint total_channels) \
{ \
  (void) total_channels; \
  return in_data[channel][sample]; \
} \
\
static inline type * \
_get_out_data_planar_##type (type * out_data[], \
    gint sample, gint channel, gint total_channels) \
{ \
  (void) total_channels; \
  return &out_data[channel][sample]; \
}

#define DEFINE_INTEGER_MIX_FUNC(bits, resbits, inlayout, outlayout) \
static void \
gst_audio_channel_mixer_mix_int##bits##_##inlayout##_##outlayout ( \
    GstAudioChannelMixer * mix, const gint##bits * in_data[], \
    gint##bits * out_data[], gint samples) \
{ \
  gint in, out, n; \
  gint##resbits res; \
  gint inchannels, outchannels; \
  \
  inchannels = mix->in_channels; \
  outchannels = mix->out_channels; \
  \
  for (n = 0; n < samples; n++) { \
    for (out = 0; out < outchannels; out++) { \
      /* convert */ \
      res = 0; \
      for (in = 0; in < inchannels; in++) \
        res += \
          _get_in_data_##inlayout##_gint##bits (in_data, n, in, inchannels) * \
          (gint##resbits) mix->matrix_int[in][out]; \
      \
      /* remove factor from int matrix */ \
      res = (res + (1 << (PRECISION_INT - 1))) >> PRECISION_INT; \
      *_get_out_data_##outlayout##_gint##bits (out_data, n, out, outchannels) = \
          CLAMP (res, G_MININT##bits, G_MAXINT##bits); \
    } \
  } \
}

#define DEFINE_FLOAT_MIX_FUNC(type, inlayout, outlayout) \
static void \
gst_audio_channel_mixer_mix_##type##_##inlayout##_##outlayout ( \
    GstAudioChannelMixer * mix, const g##type * in_data[], \
    g##type * out_data[], gint samples) \
{ \
  gint in, out, n; \
  g##type res; \
  gint inchannels, outchannels; \
  \
  inchannels = mix->in_channels; \
  outchannels = mix->out_channels; \
  \
  for (n = 0; n < samples; n++) { \
    for (out = 0; out < outchannels; out++) { \
      /* convert */ \
      res = 0.0; \
      for (in = 0; in < inchannels; in++) \
        res += \
          _get_in_data_##inlayout##_g##type (in_data, n, in, inchannels) * \
          mix->matrix[in][out]; \
      \
      *_get_out_data_##outlayout##_g##type (out_data, n, out, outchannels) = res; \
    } \
  } \
}

DEFINE_GET_DATA_FUNCS (gint16);
DEFINE_INTEGER_MIX_FUNC (16, 32, interleaved, interleaved);
DEFINE_INTEGER_MIX_FUNC (16, 32, interleaved, planar);
DEFINE_INTEGER_MIX_FUNC (16, 32, planar, interleaved);
DEFINE_INTEGER_MIX_FUNC (16, 32, planar, planar);

DEFINE_GET_DATA_FUNCS (gint32);
DEFINE_INTEGER_MIX_FUNC (32, 64, interleaved, interleaved);
DEFINE_INTEGER_MIX_FUNC (32, 64, interleaved, planar);
DEFINE_INTEGER_MIX_FUNC (32, 64, planar, interleaved);
DEFINE_INTEGER_MIX_FUNC (32, 64, planar, planar);

DEFINE_GET_DATA_FUNCS (gfloat);
DEFINE_FLOAT_MIX_FUNC (float, interleaved, interleaved);
DEFINE_FLOAT_MIX_FUNC (float, interleaved, planar);
DEFINE_FLOAT_MIX_FUNC (float, planar, interleaved);
DEFINE_FLOAT_MIX_FUNC (float, planar, planar);

DEFINE_GET_DATA_FUNCS (gdouble);
DEFINE_FLOAT_MIX_FUNC (double, interleaved, interleaved);
DEFINE_FLOAT_MIX_FUNC (double, interleaved, planar);
DEFINE_FLOAT_MIX_FUNC (double, planar, interleaved);
DEFINE_FLOAT_MIX_FUNC (double, planar, planar);

/**
 * gst_audio_channel_mixer_new_with_matrix: (skip):
 * @flags: #GstAudioChannelMixerFlags
 * @in_channels: number of input channels
 * @out_channels: number of output channels
 * @matrix: (transfer full) (nullable): channel conversion matrix, m[@in_channels][@out_channels].
 *   If identity matrix, passthrough applies. If %NULL, a (potentially truncated)
 *   identity matrix is generated.
 *
 * Create a new channel mixer object for the given parameters.
 *
 * Returns: a new #GstAudioChannelMixer object.
 *   Free with gst_audio_channel_mixer_free() after usage.
 *
 * Since: 1.14
 */
GstAudioChannelMixer *
gst_audio_channel_mixer_new_with_matrix (GstAudioChannelMixerFlags flags,
    GstAudioFormat format,
    gint in_channels, gint out_channels, gfloat ** matrix)
{
  GstAudioChannelMixer *mix;

  g_return_val_if_fail (format == GST_AUDIO_FORMAT_S16
      || format == GST_AUDIO_FORMAT_S32
      || format == GST_AUDIO_FORMAT_F32
      || format == GST_AUDIO_FORMAT_F64, NULL);

  mix = g_new0 (GstAudioChannelMixer, 1);
  mix->in_channels = in_channels;
  mix->out_channels = out_channels;

  if (!matrix) {
    /* Generate (potentially truncated) identity matrix */
    gint i, j;

    mix->matrix = g_new0 (gfloat *, in_channels);

    for (i = 0; i < in_channels; i++) {
      mix->matrix[i] = g_new (gfloat, out_channels);
      for (j = 0; j < out_channels; j++) {
        mix->matrix[i][j] = i == j ? 1.0 : 0.0;
      }
    }
  } else {
    mix->matrix = matrix;
  }

  gst_audio_channel_mixer_setup_matrix_int (mix);

#ifndef GST_DISABLE_GST_DEBUG
  /* debug */
  {
    GString *s;
    gint i, j;

    s = g_string_new ("Matrix for");
    g_string_append_printf (s, " %d -> %d: ",
        mix->in_channels, mix->out_channels);
    g_string_append (s, "{");
    for (i = 0; i < mix->in_channels; i++) {
      if (i != 0)
        g_string_append (s, ",");
      g_string_append (s, " {");
      for (j = 0; j < mix->out_channels; j++) {
        if (j != 0)
          g_string_append (s, ",");
        g_string_append_printf (s, " %f", mix->matrix[i][j]);
      }
      g_string_append (s, " }");
    }
    g_string_append (s, " }");
    GST_DEBUG ("%s", s->str);
    g_string_free (s, TRUE);
  }
#endif

  switch (format) {
    case GST_AUDIO_FORMAT_S16:
      if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_IN) {
        if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_OUT) {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_int16_planar_planar;
        } else {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_int16_planar_interleaved;
        }
      } else {
        if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_OUT) {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_int16_interleaved_planar;
        } else {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_int16_interleaved_interleaved;
        }
      }
      break;
    case GST_AUDIO_FORMAT_S32:
      if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_IN) {
        if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_OUT) {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_int32_planar_planar;
        } else {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_int32_planar_interleaved;
        }
      } else {
        if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_OUT) {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_int32_interleaved_planar;
        } else {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_int32_interleaved_interleaved;
        }
      }
      break;
    case GST_AUDIO_FORMAT_F32:
      if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_IN) {
        if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_OUT) {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_float_planar_planar;
        } else {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_float_planar_interleaved;
        }
      } else {
        if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_OUT) {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_float_interleaved_planar;
        } else {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_float_interleaved_interleaved;
        }
      }
      break;
    case GST_AUDIO_FORMAT_F64:
      if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_IN) {
        if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_OUT) {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_double_planar_planar;
        } else {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_double_planar_interleaved;
        }
      } else {
        if (flags & GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_OUT) {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_double_interleaved_planar;
        } else {
          mix->func = (MixerFunc)
              gst_audio_channel_mixer_mix_double_interleaved_interleaved;
        }
      }
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  return mix;
}

/**
 * gst_audio_channel_mixer_new: (skip):
 * @flags: #GstAudioChannelMixerFlags
 * @in_channels: number of input channels
 * @in_position: positions of input channels
 * @out_channels: number of output channels
 * @out_position: positions of output channels
 *
 * Create a new channel mixer object for the given parameters.
 *
 * Returns: a new #GstAudioChannelMixer object.
 *   Free with gst_audio_channel_mixer_free() after usage.
 */
GstAudioChannelMixer *
gst_audio_channel_mixer_new (GstAudioChannelMixerFlags flags,
    GstAudioFormat format,
    gint in_channels,
    GstAudioChannelPosition * in_position,
    gint out_channels, GstAudioChannelPosition * out_position)
{
  gfloat **matrix;

  g_return_val_if_fail (format == GST_AUDIO_FORMAT_S16
      || format == GST_AUDIO_FORMAT_S32
      || format == GST_AUDIO_FORMAT_F32
      || format == GST_AUDIO_FORMAT_F64, NULL);

  matrix =
      gst_audio_channel_mixer_setup_matrix (flags, in_channels, in_position,
      out_channels, out_position);
  return gst_audio_channel_mixer_new_with_matrix (flags, format, in_channels,
      out_channels, matrix);
}

/**
 * gst_audio_channel_mixer_is_passthrough:
 * @mix: a #GstAudioChannelMixer
 *
 * Check if @mix is in passthrough.
 *
 * Only N x N mix identity matrices are considered passthrough,
 * this is determined by comparing the contents of the matrix
 * with 0.0 and 1.0.
 *
 * As this is floating point comparisons, if the values have been
 * generated, they should be rounded up or down by explicit
 * assignment of 0.0 or 1.0 to values within a user-defined
 * epsilon, this code doesn't make assumptions as to what may
 * constitute an appropriate epsilon.
 *
 * Returns: %TRUE is @mix is passthrough.
 */
gboolean
gst_audio_channel_mixer_is_passthrough (GstAudioChannelMixer * mix)
{
  gint i, j;
  gboolean res;

  /* only NxN matrices can be identities */
  if (mix->in_channels != mix->out_channels)
    return FALSE;

  res = TRUE;

  for (i = 0; i < mix->in_channels; i++) {
    for (j = 0; j < mix->out_channels; j++) {
      if ((i == j && mix->matrix[i][j] != 1.0f) ||
          (i != j && mix->matrix[i][j] != 0.0f)) {
        res = FALSE;
        break;
      }
    }
  }

  return res;
}

/**
 * gst_audio_channel_mixer_samples:
 * @mix: a #GstAudioChannelMixer
 * @in: input samples
 * @out: output samples
 * @samples: number of samples
 *
 * In case the samples are interleaved, @in and @out must point to an
 * array with a single element pointing to a block of interleaved samples.
 *
 * If non-interleaved samples are used, @in and @out must point to an
 * array with pointers to memory blocks, one for each channel.
 *
 * Perform channel mixing on @in_data and write the result to @out_data.
 * @in_data and @out_data need to be in @format and @layout.
 */
void
gst_audio_channel_mixer_samples (GstAudioChannelMixer * mix,
    const gpointer in[], gpointer out[], gint samples)
{
  g_return_if_fail (mix != NULL);
  g_return_if_fail (mix->matrix != NULL);

  mix->func (mix, in, out, samples);
}
