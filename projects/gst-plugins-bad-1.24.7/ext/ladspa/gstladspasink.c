/* GStreamer LADSPA sink category
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *               2001 Steve Baker <stevebaker_org@yahoo.co.uk>
 *               2003 Andy Wingo <wingo at pobox.com>
 * Copyright (C) 2005 Wim Taymans <wim@fluendo.com> (fakesink)
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
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

#include "gstladspasink.h"
#include "gstladspa.h"
#include "gstladspautils.h"
#include <gst/base/gstbasetransform.h>

#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (ladspa_debug);
#define GST_CAT_DEFAULT ladspa_debug

#define GST_LADSPA_SINK_CLASS_TAGS                   "Sink/Audio/LADSPA"
#define GST_LADSPA_SINK_DEFAULT_SYNC                 TRUE
#define GST_LADSPA_SINK_DEFAULT_CAN_ACTIVATE_PUSH    TRUE
#define GST_LADSPA_SINK_DEFAULT_CAN_ACTIVATE_PULL    FALSE
#define GST_LADSPA_SINK_DEFAULT_NUM_BUFFERS          -1

enum
{
  GST_LADSPA_SINK_PROP_0,
  GST_LADSPA_SINK_PROP_CAN_ACTIVATE_PUSH,
  GST_LADSPA_SINK_PROP_CAN_ACTIVATE_PULL,
  GST_LADSPA_SINK_PROP_NUM_BUFFERS,
  GST_LADSPA_SINK_PROP_LAST
};

static GstLADSPASinkClass *gst_ladspa_sink_type_parent_class = NULL;

/*
 * Boilerplates BaseSink add pad.
 */
void
gst_my_base_sink_class_add_pad_template (GstBaseSinkClass * base_class,
    GstCaps * sinkcaps)
{
  GstElementClass *elem_class = GST_ELEMENT_CLASS (base_class);
  GstPadTemplate *pad_template;

  g_return_if_fail (GST_IS_CAPS (sinkcaps));

  pad_template =
      gst_pad_template_new (GST_BASE_TRANSFORM_SINK_NAME, GST_PAD_SINK,
      GST_PAD_ALWAYS, sinkcaps);
  gst_element_class_add_pad_template (elem_class, pad_template);
}

static gboolean
gst_ladspa_sink_type_set_caps (GstBaseSink * base, GstCaps * caps)
{
  GstLADSPASink *ladspa = GST_LADSPA_SINK (base);
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (base, "received invalid caps");
    return FALSE;
  }

  GST_DEBUG_OBJECT (ladspa, "negotiated to caps %" GST_PTR_FORMAT, caps);

  ladspa->info = info;

  return gst_ladspa_setup (&ladspa->ladspa, GST_AUDIO_INFO_RATE (&info));
}

static gboolean
gst_ladspa_sink_type_query (GstBaseSink * base, GstQuery * query)
{
  gboolean ret;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_SEEKING:{
      GstFormat fmt;

      /* we don't supporting seeking */
      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      gst_query_set_seeking (query, fmt, FALSE, 0, -1);
      ret = TRUE;
      break;
    }
    default:
      ret =
          GST_BASE_SINK_CLASS (gst_ladspa_sink_type_parent_class)->query
          (base, query);
      break;
  }

  return ret;
}

static GstFlowReturn
gst_ladspa_sink_type_preroll (GstBaseSink * base, GstBuffer * buffer)
{
  GstLADSPASink *ladspa = GST_LADSPA_SINK (base);

  if (ladspa->num_buffers_left == 0) {
    GST_DEBUG_OBJECT (ladspa, "we are EOS");
    return GST_FLOW_EOS;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_ladspa_sink_type_render (GstBaseSink * base, GstBuffer * buf)
{
  GstLADSPASink *ladspa = GST_LADSPA_SINK (base);
  GstMapInfo info;

  if (ladspa->num_buffers_left == 0)
    goto eos;

  if (ladspa->num_buffers_left != -1)
    ladspa->num_buffers_left--;

  gst_object_sync_values (GST_OBJECT (ladspa), GST_BUFFER_TIMESTAMP (buf));

  gst_buffer_map (buf, &info, GST_MAP_READ);
  gst_ladspa_transform (&ladspa->ladspa, NULL,
      info.size / sizeof (LADSPA_Data) / ladspa->ladspa.klass->count.audio.in,
      info.data);
  gst_buffer_unmap (buf, &info);

  if (ladspa->num_buffers_left == 0)
    goto eos;

  return GST_FLOW_OK;

  /* ERRORS */
eos:
  {
    GST_DEBUG_OBJECT (ladspa, "we are EOS");
    return GST_FLOW_EOS;
  }
}

static GstStateChangeReturn
gst_ladspa_sink_type_change_state (GstElement * element,
    GstStateChange transition)
{
  GstLADSPASink *ladspa = GST_LADSPA_SINK (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ladspa->num_buffers_left = ladspa->num_buffers;
      break;
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_ladspa_sink_type_parent_class)->change_state
      (element, transition);

  return ret;
}


static void
gst_ladspa_sink_type_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLADSPASink *ladspa = GST_LADSPA_SINK (object);

  switch (prop_id) {
    case GST_LADSPA_SINK_PROP_CAN_ACTIVATE_PUSH:
      GST_BASE_SINK (ladspa)->can_activate_push = g_value_get_boolean (value);
      break;
    case GST_LADSPA_SINK_PROP_CAN_ACTIVATE_PULL:
      GST_BASE_SINK (ladspa)->can_activate_pull = g_value_get_boolean (value);
      break;
    case GST_LADSPA_SINK_PROP_NUM_BUFFERS:
      ladspa->num_buffers = g_value_get_int (value);
      break;
    default:
      gst_ladspa_object_set_property (&ladspa->ladspa, object, prop_id, value,
          pspec);
      break;
  }
}

static void
gst_ladspa_sink_type_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstLADSPASink *ladspa = GST_LADSPA_SINK (object);

  switch (prop_id) {
    case GST_LADSPA_SINK_PROP_CAN_ACTIVATE_PUSH:
      g_value_set_boolean (value, GST_BASE_SINK (ladspa)->can_activate_push);
      break;
    case GST_LADSPA_SINK_PROP_CAN_ACTIVATE_PULL:
      g_value_set_boolean (value, GST_BASE_SINK (ladspa)->can_activate_pull);
      break;
    case GST_LADSPA_SINK_PROP_NUM_BUFFERS:
      g_value_set_int (value, ladspa->num_buffers);
      break;
    default:
      gst_ladspa_object_get_property (&ladspa->ladspa, object, prop_id, value,
          pspec);
      break;
  }
}

static void
gst_ladspa_sink_type_init (GstLADSPASink * ladspa, LADSPA_Descriptor * desc)
{
  GstLADSPASinkClass *ladspa_class = GST_LADSPA_SINK_GET_CLASS (ladspa);
  GstBaseSink *base = GST_BASE_SINK (ladspa);

  gst_ladspa_init (&ladspa->ladspa, &ladspa_class->ladspa);

  ladspa->num_buffers = GST_LADSPA_SINK_DEFAULT_NUM_BUFFERS;

  gst_base_sink_set_sync (base, GST_LADSPA_SINK_DEFAULT_SYNC);
}

static void
gst_ladspa_sink_type_dispose (GObject * object)
{
  GstLADSPASink *ladspa = GST_LADSPA_SINK (object);

  gst_ladspa_cleanup (&ladspa->ladspa);

  G_OBJECT_CLASS (gst_ladspa_sink_type_parent_class)->dispose (object);
}

static void
gst_ladspa_sink_type_finalize (GObject * object)
{
  GstLADSPASink *ladspa = GST_LADSPA_SINK (object);

  gst_ladspa_finalize (&ladspa->ladspa);

  G_OBJECT_CLASS (gst_ladspa_sink_type_parent_class)->finalize (object);
}

/*
 * It is okay for plugins to 'leak' a one-time allocation. This will be freed when
 * the application exits. When the plugins are scanned for the first time, this is
 * done from a separate process to not impose the memory overhead on the calling
 * application (among other reasons). Hence no need for class_finalize.
 */
static void
gst_ladspa_sink_type_base_init (GstLADSPASinkClass * ladspa_class)
{
  GstElementClass *elem_class = GST_ELEMENT_CLASS (ladspa_class);
  GstBaseSinkClass *base_class = GST_BASE_SINK_CLASS (ladspa_class);

  gst_ladspa_class_init (&ladspa_class->ladspa,
      G_TYPE_FROM_CLASS (ladspa_class));

  gst_ladspa_element_class_set_metadata (&ladspa_class->ladspa, elem_class,
      GST_LADSPA_SINK_CLASS_TAGS);

  gst_ladspa_sink_type_class_add_pad_template (&ladspa_class->ladspa,
      base_class);
}


static void
gst_ladspa_sink_type_base_finalize (GstLADSPASinkClass * ladspa_class)
{
  gst_ladspa_class_finalize (&ladspa_class->ladspa);
}

static void
gst_ladspa_sink_type_class_init (GstLADSPASinkClass * ladspa_class,
    LADSPA_Descriptor * desc)
{
  GObjectClass *object_class = G_OBJECT_CLASS (ladspa_class);
  GstElementClass *elem_class = GST_ELEMENT_CLASS (ladspa_class);
  GstBaseSinkClass *base_class = GST_BASE_SINK_CLASS (ladspa_class);

  gst_ladspa_sink_type_parent_class = g_type_class_peek_parent (ladspa_class);

  object_class->dispose = GST_DEBUG_FUNCPTR (gst_ladspa_sink_type_dispose);
  object_class->finalize = GST_DEBUG_FUNCPTR (gst_ladspa_sink_type_finalize);
  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_ladspa_sink_type_set_property);
  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_ladspa_sink_type_get_property);

  elem_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ladspa_sink_type_change_state);

  base_class->set_caps = GST_DEBUG_FUNCPTR (gst_ladspa_sink_type_set_caps);
  base_class->preroll = GST_DEBUG_FUNCPTR (gst_ladspa_sink_type_preroll);
  base_class->render = GST_DEBUG_FUNCPTR (gst_ladspa_sink_type_render);
  base_class->query = GST_DEBUG_FUNCPTR (gst_ladspa_sink_type_query);

  g_object_class_install_property (object_class,
      GST_LADSPA_SINK_PROP_CAN_ACTIVATE_PUSH,
      g_param_spec_boolean ("can-activate-push", "Can activate push",
          "Can activate in push mode",
          GST_LADSPA_SINK_DEFAULT_CAN_ACTIVATE_PUSH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      GST_LADSPA_SINK_PROP_CAN_ACTIVATE_PULL,
      g_param_spec_boolean ("can-activate-pull", "Can activate pull",
          "Can activate in pull mode",
          GST_LADSPA_SINK_DEFAULT_CAN_ACTIVATE_PULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      GST_LADSPA_SINK_PROP_NUM_BUFFERS, g_param_spec_int ("num-buffers",
          "num-buffers", "Number of buffers to accept going EOS", -1, G_MAXINT,
          GST_LADSPA_SINK_DEFAULT_NUM_BUFFERS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_ladspa_object_class_install_properties (&ladspa_class->ladspa,
      object_class, GST_LADSPA_SINK_PROP_LAST);

}

G_DEFINE_ABSTRACT_TYPE (GstLADSPASink, gst_ladspa_sink, GST_TYPE_BASE_SINK);

static void
gst_ladspa_sink_init (GstLADSPASink * ladspa)
{
}

static void
gst_ladspa_sink_class_init (GstLADSPASinkClass * ladspa_class)
{
}

/*
 * Construct the type.
 */
void
ladspa_register_sink_element (GstPlugin * plugin, GstStructure * ladspa_meta)
{
  GTypeInfo info = {
    sizeof (GstLADSPASinkClass),
    (GBaseInitFunc) gst_ladspa_sink_type_base_init,
    (GBaseFinalizeFunc) gst_ladspa_sink_type_base_finalize,
    (GClassInitFunc) gst_ladspa_sink_type_class_init,
    NULL,
    NULL,
    sizeof (GstLADSPASink),
    0,
    (GInstanceInitFunc) gst_ladspa_sink_type_init,
    NULL
  };
  ladspa_register_element (plugin, GST_TYPE_LADSPA_SINK, &info, ladspa_meta);
}
