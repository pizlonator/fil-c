/* GStreamer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 *               2001 Steve Baker <stevebaker_org@yahoo.co.uk>
 *               2003 Andy Wingo <wingo at pobox.com>
 *               2016 Thibault Saunier <thibault.saunier@collabora.com>
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
#include "gstlv2.h"
#include "gstlv2utils.h"

#include <string.h>
#include <math.h>
#include <glib.h>

#include <lilv/lilv.h>

#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>
#include <gst/audio/audio-channels.h>

GST_DEBUG_CATEGORY_EXTERN (lv2_debug);
#define GST_CAT_DEFAULT lv2_debug

typedef struct _lv2_control_info
{
  gchar *name;
  gchar *param_name;
  gfloat lowerbound, upperbound;
  gfloat def;
  gboolean lower, upper, samplerate;
  gboolean toggled, logarithmic, integer, writable;
} lv2_control_info;


typedef struct _GstLV2Filter GstLV2Filter;
typedef struct _GstLV2FilterClass GstLV2FilterClass;


struct _GstLV2Filter
{
  GstAudioFilter parent;

  GstLV2 lv2;
};

struct _GstLV2FilterClass
{
  GstAudioFilterClass parent_class;

  GstLV2Class lv2;
};

static GstAudioFilter *parent_class = NULL;

/* preset interface */

static gchar **
gst_lv2_filter_get_preset_names (GstPreset * preset)
{
  GstLV2Filter *self = (GstLV2Filter *) preset;

  return gst_lv2_get_preset_names (&self->lv2, (GstObject *) self);
}

static gboolean
gst_lv2_filter_load_preset (GstPreset * preset, const gchar * name)
{
  GstLV2Filter *self = (GstLV2Filter *) preset;

  return gst_lv2_load_preset (&self->lv2, (GstObject *) self, name);
}

static gboolean
gst_lv2_filter_save_preset (GstPreset * preset, const gchar * name)
{
  GstLV2Filter *self = (GstLV2Filter *) preset;

  return gst_lv2_save_preset (&self->lv2, (GstObject *) self, name);
}

static gboolean
gst_lv2_filter_rename_preset (GstPreset * preset, const gchar * old_name,
    const gchar * new_name)
{
  return FALSE;
}

static gboolean
gst_lv2_filter_delete_preset (GstPreset * preset, const gchar * name)
{
  GstLV2Filter *self = (GstLV2Filter *) preset;

  return gst_lv2_delete_preset (&self->lv2, (GstObject *) self, name);
}

static gboolean
gst_lv2_filter_set_meta (GstPreset * preset, const gchar * name,
    const gchar * tag, const gchar * value)
{
  return FALSE;
}

static gboolean
gst_lv2_filter_get_meta (GstPreset * preset, const gchar * name,
    const gchar * tag, gchar ** value)
{
  *value = NULL;
  return FALSE;
}

static void
gst_lv2_filter_preset_interface_init (gpointer g_iface, gpointer iface_data)
{
  GstPresetInterface *iface = g_iface;

  iface->get_preset_names = gst_lv2_filter_get_preset_names;
  iface->load_preset = gst_lv2_filter_load_preset;
  iface->save_preset = gst_lv2_filter_save_preset;
  iface->rename_preset = gst_lv2_filter_rename_preset;
  iface->delete_preset = gst_lv2_filter_delete_preset;
  iface->set_meta = gst_lv2_filter_set_meta;
  iface->get_meta = gst_lv2_filter_get_meta;
}


/* GObject vmethods implementation */
static void
gst_lv2_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLV2Filter *self = (GstLV2Filter *) object;

  gst_lv2_object_set_property (&self->lv2, object, prop_id, value, pspec);
}

static void
gst_lv2_filter_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstLV2Filter *self = (GstLV2Filter *) object;

  gst_lv2_object_get_property (&self->lv2, object, prop_id, value, pspec);
}

static void
gst_lv2_filter_finalize (GObject * object)
{
  GstLV2Filter *self = (GstLV2Filter *) object;

  gst_lv2_finalize (&self->lv2);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#if 0
/* Convert an LV2 port role to a Gst channel position
 * WARNING: If the group has only a single port,
 * GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER will be returned for pg:centerRole
 * (which is used by LV2 for mono groups), but this is not correct.  In this
 * case the value must be changed to GST_AUDIO_CHANNEL_POSITION_FRONT_MONO
 * (this can't be done by this function because this information isn't known
 * at the time it is used).
 */
static GstAudioChannelPosition
gst_lv2_filter_role_to_position (LilvNode * role)
{
  /* Front.  Mono and left/right are mututally exclusive */
  if (lilv_node_equals (role, gst_lv2_center_role_node)) {

    return GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER;
  } else if (lilv_node_equals (role, gst_lv2_left_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
  } else if (lilv_node_equals (role, gst_lv2_right_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;

    /* Rear. Left/right and center are mututally exclusive */
  } else if (lilv_node_equals (role, gst_lv2_rear_center_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_REAR_CENTER;
  } else if (lilv_node_equals (role, gst_lv2_rear_left_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_REAR_LEFT;
  } else if (lilv_node_equals (role, gst_lv2_rear_right_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT;

    /* Subwoofer/low-frequency-effects */
  } else if (lilv_node_equals (role, gst_lv2_lfe_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_LFE1;

    /* Center front speakers. Center and left/right_of_center
     * are mutually exclusive */
  } else if (lilv_node_equals (role, gst_lv2_center_left_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
  } else if (lilv_node_equals (role, gst_lv2_center_right_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;

    /* sides */
  } else if (lilv_node_equals (role, gst_lv2_side_left_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT;
  } else if (lilv_node_equals (role, gst_lv2_side_right_role_node)) {
    return GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT;
  }

  return GST_AUDIO_CHANNEL_POSITION_INVALID;
}

static GstAudioChannelPosition *
gst_lv2_filter_build_positions (GstLV2Group * group)
{
  GstAudioChannelPosition *positions = NULL;

  /* don't do anything for mono */
  if (group->ports->len > 1) {
    gint i;

    positions = g_new (GstAudioChannelPosition, group->ports->len);
    for (i = 0; i < group->ports->len; ++i)
      positions[i] = g_array_index (group->ports, GstLV2Port, i).position;
  }
  return positions;
}
#endif

/* Find and return the group @a uri in @a groups, or NULL if not found */
static void
gst_lv2_filter_type_class_add_pad_templates (GstLV2FilterClass * klass)
{
  GstCaps *srccaps, *sinkcaps;
  GstPadTemplate *pad_template;
  GstElementClass *elem_class = GST_ELEMENT_CLASS (klass);

  gint in_channels = 1, out_channels = 1;

  in_channels = klass->lv2.in_group.ports->len;

  out_channels = klass->lv2.out_group.ports->len;

  /* FIXME Implement deintereleaved audio support */
  sinkcaps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
      "channels", G_TYPE_INT, in_channels,
      "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "layout", G_TYPE_STRING, "interleaved", NULL);

  srccaps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
      "channels", G_TYPE_INT, out_channels,
      "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "layout", G_TYPE_STRING, "interleaved", NULL);

  pad_template =
      gst_pad_template_new (GST_BASE_TRANSFORM_SINK_NAME, GST_PAD_SINK,
      GST_PAD_ALWAYS, sinkcaps);
  gst_element_class_add_pad_template (elem_class, pad_template);

  pad_template =
      gst_pad_template_new (GST_BASE_TRANSFORM_SRC_NAME, GST_PAD_SRC,
      GST_PAD_ALWAYS, srccaps);
  gst_element_class_add_pad_template (elem_class, pad_template);

  gst_caps_unref (sinkcaps);
  gst_caps_unref (srccaps);
}

static gboolean
gst_lv2_filter_setup (GstAudioFilter * gsp, const GstAudioInfo * info)
{
  GstLV2Filter *self = (GstLV2Filter *) gsp;

  g_return_val_if_fail (self->lv2.activated == FALSE, FALSE);

  GST_DEBUG_OBJECT (self, "instantiating the plugin at %d Hz",
      GST_AUDIO_INFO_RATE (info));

  if (!gst_lv2_setup (&self->lv2, GST_AUDIO_INFO_RATE (info)))
    goto no_instance;

  /* FIXME Handle audio channel positioning while negotiating CAPS */
#if 0
  gint i;
  /* set input group pad audio channel position */
  for (i = 0; i < oclass->lv2.in_groups->len; ++i) {
    group = &g_array_index (oclass->lv2.in_groups, GstLV2Group, i);
    if (group->has_roles) {
      if ((positions = gst_lv2_filter_build_positions (group))) {
        if ((pad = gst_element_get_static_pad (GST_ELEMENT (gsp),
                    lilv_node_as_string (group->symbol)))) {
          GST_INFO_OBJECT (self, "set audio channel positions on sink pad %s",
              lilv_node_as_string (group->symbol));
          s = gst_caps_get_structure (caps, 0);
          gst_audio_set_channel_positions (s, positions);
          gst_object_unref (pad);
        }
        g_free (positions);
        positions = NULL;
      }
    }
  }
  /* set output group pad audio channel position */
  for (i = 0; i < oclass->lv2.out_groups->len; ++i) {
    group = &g_array_index (oclass->lv2.out_groups, GstLV2Group, i);
    if (group->has_roles) {
      if ((positions = gst_lv2_filter_build_positions (group))) {
        if ((pad = gst_element_get_static_pad (GST_ELEMENT (gsp),
                    lilv_node_as_string (group->symbol)))) {
          GST_INFO_OBJECT (self, "set audio channel positions on src pad %s",
              lilv_node_as_string (group->symbol));
          s = gst_caps_get_structure (caps, 0);
          gst_audio_set_channel_positions (s, positions);
          gst_object_unref (pad);
        }
        g_free (positions);
        positions = NULL;
      }
    }
  }
#endif

  return TRUE;

no_instance:
  {
    GST_ERROR_OBJECT (gsp, "could not create instance");
    return FALSE;
  }
}

static gboolean
gst_lv2_filter_stop (GstBaseTransform * transform)
{
  GstLV2Filter *lv2 = (GstLV2Filter *) transform;

  return gst_lv2_cleanup (&lv2->lv2, (GstObject *) lv2);
}

static inline void
gst_lv2_filter_deinterleave_data (guint n_channels, gfloat * outdata,
    guint samples, gfloat * indata)
{
  guint i, j;

  for (i = 0; i < n_channels; i++)
    for (j = 0; j < samples; j++)
      outdata[i * samples + j] = indata[j * n_channels + i];
}

static inline void
gst_lv2_filter_interleave_data (guint n_channels, gfloat * outdata,
    guint samples, gfloat * indata)
{
  guint i, j;

  for (i = 0; i < n_channels; i++)
    for (j = 0; j < samples; j++) {
      outdata[j * n_channels + i] = indata[i * samples + j];
    }
}

static GstFlowReturn
gst_lv2_filter_transform_data (GstLV2Filter * self,
    GstMapInfo * in_map, GstMapInfo * out_map)
{
  GstLV2FilterClass *klass =
      (GstLV2FilterClass *) GST_AUDIO_FILTER_GET_CLASS (self);
  GstLV2Class *lv2_class = &klass->lv2;
  GstLV2Group *lv2_group;
  GstLV2Port *lv2_port;
  guint j, k, l, nframes, samples, out_samples;
  gfloat *in = NULL, *out = NULL, *cv = NULL, *mem;
  gfloat val;

  nframes = in_map->size / sizeof (float);

  /* multi channel inputs */
  lv2_group = &lv2_class->in_group;
  samples = nframes / lv2_group->ports->len;
  GST_LOG_OBJECT (self, "in : samples=%u, nframes=%u, ports=%d", samples,
      nframes, lv2_group->ports->len);

  if (lv2_group->ports->len > 1) {
    in = g_new0 (gfloat, nframes);
    out = g_new0 (gfloat, samples * lv2_group->ports->len);
    gst_lv2_filter_deinterleave_data (lv2_group->ports->len, in,
        samples, (gfloat *) in_map->data);
  } else {
    in = (gfloat *) in_map->data;
    out = (gfloat *) out_map->data;
  }

  for (j = 0; j < lv2_group->ports->len; ++j) {
    lv2_port = &g_array_index (lv2_group->ports, GstLV2Port, j);
    lilv_instance_connect_port (self->lv2.instance, lv2_port->index,
        in + (j * samples));
  }

  /* multi channel outputs */
  lv2_group = &lv2_class->out_group;
  out_samples = nframes / lv2_group->ports->len;

  GST_LOG_OBJECT (self, "out: samples=%u, nframes=%u, ports=%d", out_samples,
      nframes, lv2_group->ports->len);
  for (j = 0; j < lv2_group->ports->len; ++j) {
    lv2_port = &g_array_index (lv2_group->ports, GstLV2Port, j);
    lilv_instance_connect_port (self->lv2.instance, lv2_port->index,
        out + (j * out_samples));
  }

  /* cv ports */
  cv = g_new (gfloat, samples * lv2_class->num_cv_in);
  for (j = k = 0; j < lv2_class->control_in_ports->len; j++) {
    lv2_port = &g_array_index (lv2_class->control_in_ports, GstLV2Port, j);
    if (lv2_port->type != GST_LV2_PORT_CV)
      continue;

    mem = cv + (k * samples);
    val = self->lv2.ports.control.in[j];
    /* FIXME: use gst_control_binding_get_value_array */
    for (l = 0; l < samples; l++)
      mem[l] = val;
    lilv_instance_connect_port (self->lv2.instance, lv2_port->index, mem);
    k++;
  }

  lilv_instance_run (self->lv2.instance, samples);

  if (lv2_group->ports->len > 1) {
    gst_lv2_filter_interleave_data (lv2_group->ports->len,
        (gfloat *) out_map->data, out_samples, out);
    g_free (out);
    g_free (in);
  }
  g_free (cv);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_lv2_filter_transform_ip (GstBaseTransform * transform, GstBuffer * buf)
{
  GstFlowReturn ret;
  GstMapInfo map;

  gst_buffer_map (buf, &map, GST_MAP_READWRITE);

  ret = gst_lv2_filter_transform_data ((GstLV2Filter *) transform, &map, &map);

  gst_buffer_unmap (buf, &map);

  return ret;
}

static GstFlowReturn
gst_lv2_filter_transform (GstBaseTransform * transform,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstMapInfo in_map, out_map;
  GstFlowReturn ret;

  gst_buffer_map (inbuf, &in_map, GST_MAP_READ);
  gst_buffer_map (outbuf, &out_map, GST_MAP_WRITE);

  ret = gst_lv2_filter_transform_data ((GstLV2Filter *) transform,
      &in_map, &out_map);

  gst_buffer_unmap (inbuf, &in_map);
  gst_buffer_unmap (outbuf, &out_map);

  return ret;
}

static void
gst_lv2_filter_base_init (gpointer g_class)
{
  GstLV2FilterClass *klass = (GstLV2FilterClass *) g_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_lv2_class_init (&klass->lv2, G_TYPE_FROM_CLASS (klass));

  gst_lv2_element_class_set_metadata (&klass->lv2, element_class,
      "Filter/Effect/Audio/LV2");

  gst_lv2_filter_type_class_add_pad_templates (klass);
}

static void
gst_lv2_filter_base_finalize (GstLV2FilterClass * lv2_class)
{
  gst_lv2_class_finalize (&lv2_class->lv2);
}

static void
gst_lv2_filter_class_init (GstLV2FilterClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstAudioFilterClass *audiofilter_class = GST_AUDIO_FILTER_CLASS (klass);

  GST_DEBUG ("class_init %p", klass);

  gobject_class->set_property = gst_lv2_filter_set_property;
  gobject_class->get_property = gst_lv2_filter_get_property;
  gobject_class->finalize = gst_lv2_filter_finalize;

  audiofilter_class->setup = gst_lv2_filter_setup;

  transform_class->stop = gst_lv2_filter_stop;
  transform_class->transform = gst_lv2_filter_transform;
  transform_class->transform_ip = gst_lv2_filter_transform_ip;

  gst_lv2_class_install_properties (&klass->lv2, gobject_class, 1);
}

static void
gst_lv2_filter_init (GstLV2Filter * self, GstLV2FilterClass * klass)
{
  gst_lv2_init (&self->lv2, &klass->lv2);

  if (!lilv_plugin_has_feature (klass->lv2.plugin,
          gst_lv2_in_place_broken_pred_node))
    gst_base_transform_set_in_place (GST_BASE_TRANSFORM (self), TRUE);
}

void
gst_lv2_filter_register_element (GstPlugin * plugin, GstStructure * lv2_meta)
{
  GTypeInfo info = {
    sizeof (GstLV2FilterClass),
    (GBaseInitFunc) gst_lv2_filter_base_init,
    (GBaseFinalizeFunc) gst_lv2_filter_base_finalize,
    (GClassInitFunc) gst_lv2_filter_class_init,
    NULL,
    NULL,
    sizeof (GstLV2Filter),
    0,
    (GInstanceInitFunc) gst_lv2_filter_init,
  };
  const gchar *type_name =
      gst_structure_get_string (lv2_meta, "element-type-name");
  GType element_type =
      g_type_register_static (GST_TYPE_AUDIO_FILTER, type_name, &info, 0);
  gboolean can_do_presets;

  /* register interfaces */
  gst_structure_get_boolean (lv2_meta, "can-do-presets", &can_do_presets);
  if (can_do_presets) {
    const GInterfaceInfo preset_interface_info = {
      (GInterfaceInitFunc) gst_lv2_filter_preset_interface_init,
      NULL,
      NULL
    };
    g_type_add_interface_static (element_type, GST_TYPE_PRESET,
        &preset_interface_info);
  }

  gst_element_register (plugin, type_name, GST_RANK_NONE, element_type);

  if (!parent_class)
    parent_class = g_type_class_ref (GST_TYPE_AUDIO_FILTER);
}
