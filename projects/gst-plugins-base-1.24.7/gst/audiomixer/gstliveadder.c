/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2001 Thomas <thomas@apestaart.org>
 *               2005,2006 Wim Taymans <wim@fluendo.com>
 *                    2013 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * audiomixer.c: AudioMixer element, N in, one out, samples are added
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
 * SECTION:element-liveadder
 * @title: liveadder
 *
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstaudiomixerelements.h"
#include "gstaudiomixerorc.h"

#include "gstaudiointerleave.h"

/* Empty liveadder alias with non-zero latency */

typedef GstAudioMixer GstLiveAdder;
typedef GstAudioMixerClass GstLiveAdderClass;

static GType gst_live_adder_get_type (void);
#define GST_TYPE_LIVE_ADDER gst_live_adder_get_type ()

G_DEFINE_TYPE (GstLiveAdder, gst_live_adder, GST_TYPE_AUDIO_MIXER);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (liveadder, "liveadder",
    GST_RANK_NONE, GST_TYPE_LIVE_ADDER, audiomixer_element_init (plugin));

enum
{
  LIVEADDER_PROP_LATENCY = 1
};

static void
gst_live_adder_init (GstLiveAdder * self)
{
}

static void
gst_live_adder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case LIVEADDER_PROP_LATENCY:
    {
      GParamSpec *parent_spec =
          g_object_class_find_property (G_OBJECT_CLASS
          (gst_live_adder_parent_class), "latency");
      GObjectClass *pspec_class = g_type_class_peek (parent_spec->owner_type);
      GValue v = { 0 };

      g_value_init (&v, G_TYPE_UINT64);

      g_value_set_uint64 (&v, g_value_get_uint (value) * GST_MSECOND);

      G_OBJECT_CLASS (pspec_class)->set_property (object,
          parent_spec->param_id, &v, parent_spec);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_live_adder_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  switch (prop_id) {
    case LIVEADDER_PROP_LATENCY:
    {
      GParamSpec *parent_spec =
          g_object_class_find_property (G_OBJECT_CLASS
          (gst_live_adder_parent_class), "latency");
      GObjectClass *pspec_class = g_type_class_peek (parent_spec->owner_type);
      GValue v = { 0 };

      g_value_init (&v, G_TYPE_UINT64);

      G_OBJECT_CLASS (pspec_class)->get_property (object,
          parent_spec->param_id, &v, parent_spec);

      g_value_set_uint (value, g_value_get_uint64 (&v) / GST_MSECOND);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_live_adder_class_init (GstLiveAdderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_live_adder_set_property;
  gobject_class->get_property = gst_live_adder_get_property;

  g_object_class_install_property (gobject_class, LIVEADDER_PROP_LATENCY,
      g_param_spec_uint ("latency", "Buffer latency",
          "Additional latency in live mode to allow upstream "
          "to take longer to produce buffers for the current "
          "position (in milliseconds)", 0, G_MAXUINT,
          30, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
}
