/* GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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
 * SECTION:element-viewfinderbin
 * @title: gstviewfinderbin
 *
 * The gstviewfinderbin element is a displaying element for camerabin2.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! viewfinderbin
 * ]|
 * Feeds the viewfinderbin with video test data.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstviewfinderbin.h"
#include "camerabingeneral.h"
#include <gst/pbutils/pbutils.h>

#include <glib/gi18n-lib.h>

GST_DEBUG_CATEGORY_STATIC (gst_viewfinder_bin_debug);
#define GST_CAT_DEFAULT gst_viewfinder_bin_debug

enum
{
  PROP_0,
  PROP_VIDEO_SINK,
  PROP_DISABLE_CONVERTERS
};

#define DEFAULT_DISABLE_CONVERTERS FALSE

/* pad templates */

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)")
    );

/* class initialization */
#define gst_viewfinder_bin_parent_class parent_class
G_DEFINE_TYPE (GstViewfinderBin, gst_viewfinder_bin, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (viewfinderbin, "viewfinderbin",
    GST_RANK_NONE, gst_viewfinder_bin_get_type (),
    GST_DEBUG_CATEGORY_INIT (gst_viewfinder_bin_debug, "viewfinderbin", 0,
        "ViewFinderBin");
    );

static void gst_viewfinder_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * spec);
static void gst_viewfinder_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * spec);

static void
gst_viewfinder_bin_set_video_sink (GstViewfinderBin * vfbin, GstElement * sink);


/* Element class functions */
static GstStateChangeReturn
gst_viewfinder_bin_change_state (GstElement * element, GstStateChange trans);

static void
gst_viewfinder_bin_dispose (GObject * object)
{
  GstViewfinderBin *viewfinderbin = GST_VIEWFINDER_BIN_CAST (object);

  if (viewfinderbin->user_video_sink) {
    gst_object_unref (viewfinderbin->user_video_sink);
    viewfinderbin->user_video_sink = NULL;
  }

  if (viewfinderbin->video_sink) {
    gst_object_unref (viewfinderbin->video_sink);
    viewfinderbin->video_sink = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose ((GObject *) viewfinderbin);
}

static void
gst_viewfinder_bin_class_init (GstViewfinderBinClass * klass)
{
  GObjectClass *gobject_klass;
  GstElementClass *element_class;

  gobject_klass = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_viewfinder_bin_change_state);

  gobject_klass->dispose = gst_viewfinder_bin_dispose;
  gobject_klass->set_property = gst_viewfinder_bin_set_property;
  gobject_klass->get_property = gst_viewfinder_bin_get_property;

  g_object_class_install_property (gobject_klass, PROP_VIDEO_SINK,
      g_param_spec_object ("video-sink", "Video Sink",
          "the video output element to use (NULL = default)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_DISABLE_CONVERTERS,
      g_param_spec_boolean ("disable-converters", "Disable conversion elements",
          "If video converters should be disabled (must be set on NULL)",
          DEFAULT_DISABLE_CONVERTERS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class, "Viewfinder Bin",
      "Sink/Video", "Viewfinder Bin used in camerabin2",
      "Thiago Santos <thiago.sousa.santos@collabora.com>");
}

static void
gst_viewfinder_bin_init (GstViewfinderBin * viewfinderbin)
{
  GstPadTemplate *templ = gst_static_pad_template_get (&sink_template);
  viewfinderbin->ghostpad = gst_ghost_pad_new_no_target_from_template ("sink",
      templ);
  gst_object_unref (templ);
  gst_element_add_pad (GST_ELEMENT_CAST (viewfinderbin),
      viewfinderbin->ghostpad);

  viewfinderbin->disable_converters = DEFAULT_DISABLE_CONVERTERS;
}

static gboolean
gst_viewfinder_bin_create_elements (GstViewfinderBin * vfbin)
{
  GstElement *csp = NULL;
  GstElement *videoscale = NULL;
  GstPad *firstpad = NULL;
  const gchar *missing_element_name;
  gboolean newsink = FALSE;
  gboolean updated_converters = FALSE;

  GST_DEBUG_OBJECT (vfbin, "Creating internal elements");

  /* First check if we need to add/replace the internal sink */
  if (vfbin->video_sink) {
    if (vfbin->user_video_sink && vfbin->video_sink != vfbin->user_video_sink) {
      gst_bin_remove (GST_BIN_CAST (vfbin), vfbin->video_sink);
      gst_object_unref (vfbin->video_sink);
      vfbin->video_sink = NULL;
    }
  }

  if (!vfbin->video_sink) {
    if (vfbin->user_video_sink)
      vfbin->video_sink = gst_object_ref (vfbin->user_video_sink);
    else {
      vfbin->video_sink = gst_element_factory_make ("autovideosink",
          "vfbin-sink");
      if (!vfbin->video_sink) {
        missing_element_name = "autovideosink";
        goto missing_element;
      }
    }

    gst_bin_add (GST_BIN_CAST (vfbin), gst_object_ref (vfbin->video_sink));
    newsink = TRUE;
  }

  /* check if we want add/remove the conversion elements */
  if (vfbin->elements_created && vfbin->disable_converters) {
    /* remove the elements, user doesn't want them */

    gst_ghost_pad_set_target (GST_GHOST_PAD (vfbin->ghostpad), NULL);
    csp = gst_bin_get_by_name (GST_BIN_CAST (vfbin), "vfbin-csp");
    videoscale = gst_bin_get_by_name (GST_BIN_CAST (vfbin), "vfbin-videoscale");

    gst_bin_remove (GST_BIN_CAST (vfbin), csp);
    gst_bin_remove (GST_BIN_CAST (vfbin), videoscale);

    gst_object_unref (csp);
    gst_object_unref (videoscale);

    updated_converters = TRUE;
  } else if (!vfbin->elements_created && !vfbin->disable_converters) {
    gst_ghost_pad_set_target (GST_GHOST_PAD (vfbin->ghostpad), NULL);

    /* add the elements, user wants them */
    csp = gst_element_factory_make ("videoconvert", "vfbin-csp");
    if (!csp) {
      missing_element_name = "videoconvert";
      goto missing_element;
    }
    gst_bin_add (GST_BIN_CAST (vfbin), csp);

    videoscale = gst_element_factory_make ("videoscale", "vfbin-videoscale");
    if (!videoscale) {
      missing_element_name = "videoscale";
      goto missing_element;
    }
    gst_bin_add (GST_BIN_CAST (vfbin), videoscale);

    gst_element_link_pads_full (csp, "src", videoscale, "sink",
        GST_PAD_LINK_CHECK_NOTHING);

    vfbin->elements_created = TRUE;
    GST_DEBUG_OBJECT (vfbin, "Elements successfully created and linked");

    updated_converters = TRUE;
  }
  /* otherwise, just leave it as is */

  /* if sink was replaced -> link it to the internal converters */
  if (newsink && !vfbin->disable_converters) {
    gboolean unref = FALSE;
    if (!videoscale) {
      videoscale = gst_bin_get_by_name (GST_BIN_CAST (vfbin),
          "vfbin-videoscale");
      unref = TRUE;
    }

    if (!gst_element_link_pads_full (videoscale, "src", vfbin->video_sink,
            "sink", GST_PAD_LINK_CHECK_CAPS)) {
      GST_ELEMENT_ERROR (vfbin, CORE, NEGOTIATION, (NULL),
          ("linking videoscale and viewfindersink failed"));
    }

    if (unref)
      gst_object_unref (videoscale);
    videoscale = NULL;
  }

  /* Check if we need a new ghostpad target */
  if (updated_converters || (newsink && vfbin->disable_converters)) {
    if (vfbin->disable_converters) {
      firstpad = gst_element_get_static_pad (vfbin->video_sink, "sink");
    } else {
      /* csp should always exist at this point */
      firstpad = gst_element_get_static_pad (csp, "sink");
    }
  }

  /* need to change the ghostpad target if firstpad is set */
  if (firstpad) {
    if (!gst_ghost_pad_set_target (GST_GHOST_PAD (vfbin->ghostpad), firstpad))
      goto error;
    gst_object_unref (firstpad);
    firstpad = NULL;
  }

  return TRUE;

missing_element:
  gst_element_post_message (GST_ELEMENT_CAST (vfbin),
      gst_missing_element_message_new (GST_ELEMENT_CAST (vfbin),
          missing_element_name));
  GST_ELEMENT_ERROR (vfbin, CORE, MISSING_PLUGIN,
      (_("Missing element '%s' - check your GStreamer installation."),
          missing_element_name), (NULL));
  goto error;

error:
  GST_WARNING_OBJECT (vfbin, "Creating internal elements failed");
  if (firstpad)
    gst_object_unref (firstpad);
  return FALSE;
}

static GstStateChangeReturn
gst_viewfinder_bin_change_state (GstElement * element, GstStateChange trans)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstViewfinderBin *vfbin = GST_VIEWFINDER_BIN_CAST (element);

  switch (trans) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_viewfinder_bin_create_elements (vfbin)) {
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);

  switch (trans) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_viewfinder_bin_set_video_sink (GstViewfinderBin * vfbin, GstElement * sink)
{
  GST_INFO_OBJECT (vfbin, "Setting video sink to %" GST_PTR_FORMAT, sink);

  if (vfbin->user_video_sink != sink) {
    if (vfbin->user_video_sink) {
      gst_object_unref (vfbin->user_video_sink);
    }
    vfbin->user_video_sink = sink;
    if (sink)
      gst_object_ref (sink);
  }
}

static void
gst_viewfinder_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstViewfinderBin *vfbin = GST_VIEWFINDER_BIN_CAST (object);

  switch (prop_id) {
    case PROP_VIDEO_SINK:
      gst_viewfinder_bin_set_video_sink (vfbin, g_value_get_object (value));
      break;
    case PROP_DISABLE_CONVERTERS:
      vfbin->disable_converters = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_viewfinder_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstViewfinderBin *vfbin = GST_VIEWFINDER_BIN_CAST (object);

  switch (prop_id) {
    case PROP_VIDEO_SINK:
      g_value_set_object (value, vfbin->video_sink);
      break;
    case PROP_DISABLE_CONVERTERS:
      g_value_set_boolean (value, vfbin->disable_converters);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
