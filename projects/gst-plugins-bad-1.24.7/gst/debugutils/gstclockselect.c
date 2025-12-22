/* GStreamer
 * Copyright (C) 2019 Intel Corporation
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-clockselect
 *
 * The clockselect element is a pipeline that enables one to choose its
 * clock. By default, pipelines chose a clock depending on its elements,
 * however the clockselect pipeline has some properties to force an
 * arbitrary clock on it.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v clockselect. \( clock-id=ptp domain=1 fakesrc ! fakesink \)
 * ]|
 * This example will create a pipeline and use the PTP clock with domain 1 on it.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/net/net.h>
#include "gstdebugutilsbadelements.h"
#include "gstclockselect.h"

GST_DEBUG_CATEGORY_STATIC (gst_clock_select_debug_category);
#define GST_CAT_DEFAULT gst_clock_select_debug_category

#define GST_TYPE_CLOCK_SELECT_CLOCK_ID (gst_clock_select_clock_id_get_type())
static GType
gst_clock_select_clock_id_get_type (void)
{
  static GType clock_id_type = 0;

  if (g_once_init_enter (&clock_id_type)) {
    GType type;
    static const GEnumValue clock_id_types[] = {
      {GST_CLOCK_SELECT_CLOCK_ID_DEFAULT,
          "Default (elected from elements) pipeline clock", "default"},
      {GST_CLOCK_SELECT_CLOCK_ID_MONOTONIC, "System monotonic clock",
          "monotonic"},
      {GST_CLOCK_SELECT_CLOCK_ID_REALTIME, "System realtime clock", "realtime"},
      {GST_CLOCK_SELECT_CLOCK_ID_PTP, "PTP clock", "ptp"},
      {GST_CLOCK_SELECT_CLOCK_ID_TAI, "System TAI clock", "tai"},
      {0, NULL, NULL},
    };

    type = g_enum_register_static ("GstClockSelectClockId", clock_id_types);
    g_once_init_leave (&clock_id_type, type);
  }

  return clock_id_type;
}

/* prototypes */
static void gst_clock_select_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_clock_select_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static GstClock *gst_clock_select_provide_clock (GstElement * element);

enum
{
  PROP_0,
  PROP_CLOCK_ID,
  PROP_PTP_DOMAIN
};

#define DEFAULT_CLOCK_ID GST_CLOCK_SELECT_CLOCK_ID_DEFAULT
#define DEFAULT_PTP_DOMAIN 0

/* class initialization */

#define gst_clock_select_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstClockSelect, gst_clock_select, GST_TYPE_PIPELINE,
    GST_DEBUG_CATEGORY_INIT (gst_clock_select_debug_category, "clockselect", 0,
        "debug category for clockselect element"));
GST_ELEMENT_REGISTER_DEFINE (clockselect, "clockselect",
    GST_RANK_NONE, gst_clock_select_get_type ());

static void
gst_clock_select_class_init (GstClockSelectClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_clock_select_set_property;
  gobject_class->get_property = gst_clock_select_get_property;

  g_object_class_install_property (gobject_class, PROP_CLOCK_ID,
      g_param_spec_enum ("clock-id", "Clock ID", "ID of pipeline clock",
          GST_TYPE_CLOCK_SELECT_CLOCK_ID, DEFAULT_CLOCK_ID,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PTP_DOMAIN,
      g_param_spec_uint ("ptp-domain", "PTP domain",
          "PTP clock domain (meaningful only when Clock ID is PTP)",
          0, G_MAXUINT8, DEFAULT_PTP_DOMAIN,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Clock select", "Generic/Bin", "Pipeline that enables different clocks",
      "Ederson de Souza <ederson.desouza@intel.com>");

  gstelement_class->provide_clock =
      GST_DEBUG_FUNCPTR (gst_clock_select_provide_clock);

  gst_type_mark_as_plugin_api (GST_TYPE_CLOCK_SELECT_CLOCK_ID, 0);
}

static void
gst_clock_select_init (GstClockSelect * clock_select)
{
  clock_select->clock_id = DEFAULT_CLOCK_ID;
  clock_select->ptp_domain = DEFAULT_PTP_DOMAIN;
}

static void
gst_clock_select_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstClockSelect *clock_select = GST_CLOCK_SELECT (object);

  GST_DEBUG_OBJECT (clock_select, "set_property");

  switch (property_id) {
    case PROP_CLOCK_ID:
      clock_select->clock_id = g_value_get_enum (value);
      break;
    case PROP_PTP_DOMAIN:
      clock_select->ptp_domain = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_clock_select_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstClockSelect *clock_select = GST_CLOCK_SELECT (object);

  GST_DEBUG_OBJECT (clock_select, "get_property");

  switch (property_id) {
    case PROP_CLOCK_ID:
      g_value_set_enum (value, clock_select->clock_id);
      break;
    case PROP_PTP_DOMAIN:
      g_value_set_uint (value, clock_select->ptp_domain);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static GstClock *
gst_clock_select_provide_clock (GstElement * element)
{
  GstClock *clock;
  GstClockSelect *clock_select = GST_CLOCK_SELECT (element);

  switch (clock_select->clock_id) {
    case GST_CLOCK_SELECT_CLOCK_ID_MONOTONIC:
      clock =
          g_object_new (GST_TYPE_SYSTEM_CLOCK, "name", "DebugGstSystemClock",
          NULL);
      gst_object_ref_sink (clock);
      gst_util_set_object_arg (G_OBJECT (clock), "clock-type", "monotonic");
      break;
    case GST_CLOCK_SELECT_CLOCK_ID_REALTIME:
      clock =
          g_object_new (GST_TYPE_SYSTEM_CLOCK, "name", "DebugGstSystemClock",
          NULL);
      gst_object_ref_sink (clock);
      gst_util_set_object_arg (G_OBJECT (clock), "clock-type", "realtime");
      break;
    case GST_CLOCK_SELECT_CLOCK_ID_PTP:
      clock = gst_ptp_clock_new ("ptp-clock", clock_select->ptp_domain);
      if (!clock) {
        GST_WARNING_OBJECT (clock_select,
            "Failed to get PTP clock, falling back to pipeline default clock");
      }
      break;
    case GST_CLOCK_SELECT_CLOCK_ID_TAI:
      clock =
          g_object_new (GST_TYPE_SYSTEM_CLOCK, "name", "DebugGstSystemClock",
          NULL);
      gst_object_ref_sink (clock);
      gst_util_set_object_arg (G_OBJECT (clock), "clock-type", "tai");
      break;
    case GST_CLOCK_SELECT_CLOCK_ID_DEFAULT:
    default:
      clock = NULL;
  }

  if (clock) {
    GST_INFO_OBJECT (clock_select, "Waiting clock sync...");
    gst_clock_wait_for_sync (clock, GST_CLOCK_TIME_NONE);
    gst_pipeline_use_clock (GST_PIPELINE (clock_select), clock);
    /* gst_pipeline_use_clock above ref's clock, as well as parent call
     * below, so we don't need our reference anymore */
    gst_object_unref (clock);
  }

  clock = GST_ELEMENT_CLASS (parent_class)->provide_clock (element);

  return clock;
}
