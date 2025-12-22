/*
 * Copyright (C) 2008 Jan Schmidt <thaytan@noraisin.net>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/glib-compat-private.h>
#include <gst/video/video.h>
#include <string.h>

#include "rsnparsetter.h"

GST_DEBUG_CATEGORY_STATIC (rsn_parsetter_debug);
#define GST_CAT_DEFAULT rsn_parsetter_debug

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );

#define rsn_parsetter_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (RsnParSetter, rsn_parsetter, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (rsn_parsetter_debug, "rsnparsetter", 0,
        "Resin DVD aspect ratio adjuster"));

static void rsn_parsetter_finalize (GObject * object);
static GstFlowReturn rsn_parsetter_chain (GstPad * pad,
    RsnParSetter * parset, GstBuffer * buf);
static gboolean rsn_parsetter_sink_event (GstPad * pad,
    RsnParSetter * parset, GstEvent * event);

static gboolean rsn_parsetter_src_query (GstPad * pad, RsnParSetter * parset,
    GstQuery * query);
static GstCaps *rsn_parsetter_convert_caps (RsnParSetter * parset,
    GstCaps * caps, gboolean widescreen);
static gboolean rsn_parsetter_check_caps (RsnParSetter * parset,
    GstCaps * caps);
static void rsn_parsetter_update_caps (RsnParSetter * parset, GstCaps * caps);

static void
rsn_parsetter_class_init (RsnParSetterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = rsn_parsetter_finalize;

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_element_class_set_static_metadata (element_class,
      "Resin Aspect Ratio Setter", "Filter/Video",
      "Overrides caps on video buffers to force a particular display ratio",
      "Jan Schmidt <thaytan@noraisin.net>");
}

static void
rsn_parsetter_init (RsnParSetter * parset)
{
  parset->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_chain_function (parset->sinkpad,
      (GstPadChainFunction) GST_DEBUG_FUNCPTR (rsn_parsetter_chain));
  gst_pad_set_event_function (parset->sinkpad,
      (GstPadEventFunction) GST_DEBUG_FUNCPTR (rsn_parsetter_sink_event));
  GST_PAD_SET_PROXY_CAPS (parset->sinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (parset->sinkpad);
  gst_element_add_pad (GST_ELEMENT (parset), parset->sinkpad);

  parset->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_query_function (parset->srcpad,
      (GstPadQueryFunction) GST_DEBUG_FUNCPTR (rsn_parsetter_src_query));
  GST_PAD_SET_PROXY_CAPS (parset->srcpad);
  gst_element_add_pad (GST_ELEMENT (parset), parset->srcpad);
}

static void
rsn_parsetter_finalize (GObject * object)
{
  RsnParSetter *parset = RSN_PARSETTER (object);

  gst_caps_replace (&parset->outcaps, NULL);
  gst_caps_replace (&parset->in_caps_last, NULL);
  gst_caps_replace (&parset->in_caps_converted, NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstFlowReturn
rsn_parsetter_chain (GstPad * pad, RsnParSetter * parset, GstBuffer * buf)
{
  return gst_pad_push (parset->srcpad, buf);
}

static gboolean
rsn_parsetter_sink_event (GstPad * pad, RsnParSetter * parset, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:{
      const GstStructure *structure = gst_event_get_structure (event);

      if (structure != NULL &&
          gst_structure_has_name (structure, "application/x-gst-dvd")) {
        const char *type = gst_structure_get_string (structure, "event");
        GstEvent *caps_event = NULL;

        if (type == NULL)
          goto out;

        if (strcmp (type, "dvd-video-format") == 0) {
          gboolean is_widescreen;

          gst_structure_get_boolean (structure, "video-widescreen",
              &is_widescreen);

          GST_DEBUG_OBJECT (parset, "Video is %s",
              parset->is_widescreen ? "16:9" : "4:3");

          if (parset->in_caps_last && parset->is_widescreen != is_widescreen) {
            /* Force caps check */
            gst_caps_replace (&parset->in_caps_converted, NULL);
            rsn_parsetter_update_caps (parset, parset->in_caps_last);
            if (parset->override_outcaps)
              caps_event = gst_event_new_caps (parset->outcaps);
          }
          parset->is_widescreen = is_widescreen;

          /* FIXME: Added for testing: */
          // parset->is_widescreen = FALSE;

          if (caps_event)
            gst_pad_push_event (parset->srcpad, caps_event);
        }
      }
      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps = NULL;
      gst_event_parse_caps (event, &caps);
      rsn_parsetter_update_caps (parset, caps);
      if (parset->override_outcaps) {
        gst_event_unref (event);
        GST_DEBUG_OBJECT (parset,
            "Handling caps event. Overriding upstream caps"
            " with %" GST_PTR_FORMAT, parset->outcaps);
        event = gst_event_new_caps (parset->outcaps);
      } else {
        GST_DEBUG_OBJECT (parset,
            "Handling caps event. Upstream caps %" GST_PTR_FORMAT
            " acceptable", caps);
      }
      break;
    }
    default:
      break;
  }

out:
  return gst_pad_event_default (pad, (GstObject *) (parset), event);
}

static gboolean
rsn_parsetter_src_query (GstPad * pad, RsnParSetter * parset, GstQuery * query)
{
  GstCaps *caps = NULL;

  if (!gst_pad_peer_query (parset->sinkpad, query))
    return FALSE;

  if (GST_QUERY_TYPE (query) != GST_QUERY_CAPS)
    return TRUE;

  gst_query_parse_caps_result (query, &caps);

  GST_DEBUG_OBJECT (parset, "Handling caps query. Upstream caps %"
      GST_PTR_FORMAT, caps);

  if (caps == NULL) {
    GstCaps *templ_caps = gst_pad_get_pad_template_caps (pad);
    gst_query_set_caps_result (query, templ_caps);
    gst_caps_unref (templ_caps);
  } else {
    caps = rsn_parsetter_convert_caps (parset, caps, parset->is_widescreen);
    gst_query_set_caps_result (query, caps);
    gst_caps_unref (caps);
  }

  return TRUE;
}

/* Check if the DAR of the passed matches the required DAR */
static gboolean
rsn_parsetter_check_caps (RsnParSetter * parset, GstCaps * caps)
{
  GstStructure *s;
  gint width, height;
  gint par_n, par_d;
  guint dar_n, dar_d;
  gboolean ret = FALSE;

  if (parset->in_caps_last &&
      (caps == parset->in_caps_last ||
          gst_caps_is_equal (caps, parset->in_caps_last))) {
    ret = parset->in_caps_was_ok;
    goto out;
  }

  /* Calculate the DAR from the incoming caps, and return TRUE if it matches
   * the required DAR, FALSE if not */
  s = gst_caps_get_structure (caps, 0);
  if (s == NULL)
    goto out;

  if (!gst_structure_get_int (s, "width", &width) ||
      !gst_structure_get_int (s, "height", &height))
    goto out;

  if (!gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d))
    par_n = par_d = 1;

  if (!gst_video_calculate_display_ratio (&dar_n, &dar_d, width, height,
          par_n, par_d, 1, 1))
    goto out;

  GST_DEBUG_OBJECT (parset,
      "Incoming video caps now: w %d h %d PAR %d/%d = DAR %d/%d",
      width, height, par_n, par_d, dar_n, dar_d);

  if (parset->is_widescreen) {
    if (dar_n == 16 && dar_d == 9)
      ret = TRUE;
  } else {
    if (dar_n == 4 && dar_d == 3)
      ret = TRUE;
  }

  gst_caps_replace (&parset->in_caps_last, caps);
  gst_caps_replace (&parset->in_caps_converted, NULL);
  parset->in_caps_was_ok = ret;

out:
  return ret;
}

static GstCaps *
rsn_parsetter_convert_caps (RsnParSetter * parset, GstCaps * caps,
    gboolean widescreen)
{
  /* Duplicate the given caps, with a PAR that provides the desired DAR */
  GstCaps *outcaps;
  GstStructure *s;
  gint width, height;
  gint par_n, par_d;
  guint dar_n, dar_d;
  GValue par = { 0, };

  if (caps == parset->in_caps_last && parset->in_caps_converted) {
    outcaps = gst_caps_ref (parset->in_caps_converted);
    goto out;
  }

  outcaps = gst_caps_copy (caps);

  /* Calculate the DAR from the incoming caps, and return TRUE if it matches
   * the required DAR, FALSE if not */
  s = gst_caps_get_structure (outcaps, 0);
  if (s == NULL)
    goto out;

  if (!gst_structure_get_int (s, "width", &width) ||
      !gst_structure_get_int (s, "height", &height))
    goto out;

  if (widescreen) {
    dar_n = 16;
    dar_d = 9;
  } else {
    dar_n = 4;
    dar_d = 3;
  }

  par_n = dar_n * height;
  par_d = dar_d * width;

  g_value_init (&par, GST_TYPE_FRACTION);
  gst_value_set_fraction (&par, par_n, par_d);
  gst_structure_set_value (s, "pixel-aspect-ratio", &par);
  g_value_unset (&par);

  gst_caps_replace (&parset->in_caps_converted, outcaps);
out:
  return outcaps;
}

static void
rsn_parsetter_update_caps (RsnParSetter * parset, GstCaps * caps)
{
  /* Check the new incoming caps against our current DAR, and mark
   * whether the caps need adjusting */
  if (rsn_parsetter_check_caps (parset, caps)) {
    parset->override_outcaps = FALSE;
    gst_caps_replace (&parset->outcaps, caps);
  } else {
    GstCaps *override_caps = rsn_parsetter_convert_caps (parset, caps,
        parset->is_widescreen);
    if (parset->outcaps)
      gst_caps_unref (parset->outcaps);
    parset->outcaps = override_caps;

    parset->override_outcaps = TRUE;
  }

  GST_DEBUG_OBJECT (parset, "caps changed: need_override now = %d",
      parset->override_outcaps);
}
