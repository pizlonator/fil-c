/*
 * GStreamer
 * Copyright (C) 2010 Texas Instruments, Inc
 * Copyright (C) 2011 Thiago Santos <thiago.sousa.santos@collabora.com>
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
 * SECTION:element-wrappercamerabinsrc
 * @title: wrappercamerabinsrc
 *
 * A camera bin src element that wraps a default video source with a single
 * pad into the 3pad model that camerabin2 expects.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/interfaces/photography.h>
#include <glib/gi18n-lib.h>

#include "gstwrappercamerabinsrc.h"
#include "gstdigitalzoom.h"
#include "camerabingeneral.h"

enum
{
  PROP_0,
  PROP_VIDEO_SRC,
  PROP_VIDEO_SRC_FILTER
};

GST_DEBUG_CATEGORY (wrapper_camera_bin_src_debug);
#define GST_CAT_DEFAULT wrapper_camera_bin_src_debug

#define gst_wrapper_camera_bin_src_parent_class parent_class
G_DEFINE_TYPE (GstWrapperCameraBinSrc, gst_wrapper_camera_bin_src,
    GST_TYPE_BASE_CAMERA_SRC);
GST_ELEMENT_REGISTER_DEFINE (wrappercamerabinsrc, "wrappercamerabinsrc",
    GST_RANK_NONE, gst_wrapper_camera_bin_src_get_type ());

static GstStaticPadTemplate vfsrc_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate imgsrc_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate vidsrc_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void set_capsfilter_caps (GstWrapperCameraBinSrc * self,
    GstCaps * new_caps);

static void
gst_wrapper_camera_bin_src_dispose (GObject * object)
{
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (object);

  if (self->src_pad) {
    gst_object_unref (self->src_pad);
    self->src_pad = NULL;
  }
  if (self->video_tee_sink) {
    gst_object_unref (self->video_tee_sink);
    self->video_tee_sink = NULL;
  }
  if (self->video_tee_vf_pad) {
    gst_object_unref (self->video_tee_vf_pad);
    self->video_tee_vf_pad = NULL;
  }
  if (self->app_vid_src) {
    gst_object_unref (self->app_vid_src);
    self->app_vid_src = NULL;
  }
  if (self->app_vid_filter) {
    gst_object_unref (self->app_vid_filter);
    self->app_vid_filter = NULL;
  }
  if (self->srcfilter_pad) {
    gst_object_unref (self->srcfilter_pad);
    self->srcfilter_pad = NULL;
  }
  gst_caps_replace (&self->image_capture_caps, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_wrapper_camera_bin_src_finalize (GstWrapperCameraBinSrc * self)
{
  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (self));
}

static void
gst_wrapper_camera_bin_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (object);

  switch (prop_id) {
    case PROP_VIDEO_SRC:
      if (GST_STATE (self) != GST_STATE_NULL) {
        GST_ELEMENT_ERROR (self, CORE, FAILED,
            ("camerasrc must be in NULL state when setting the video source element"),
            (NULL));
      } else {
        if (self->app_vid_src)
          gst_object_unref (self->app_vid_src);
        self->app_vid_src = g_value_get_object (value);
        if (self->app_vid_src)
          gst_object_ref (self->app_vid_src);
      }
      break;
    case PROP_VIDEO_SRC_FILTER:
      if (GST_STATE (self) != GST_STATE_NULL) {
        GST_ELEMENT_ERROR (self, CORE, FAILED,
            ("camerasrc must be in NULL state when setting the video source filter element"),
            (NULL));
      } else {
        if (self->app_vid_filter)
          gst_object_unref (self->app_vid_filter);
        self->app_vid_filter = g_value_get_object (value);
        if (self->app_vid_filter)
          gst_object_ref (self->app_vid_filter);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static void
gst_wrapper_camera_bin_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (object);

  switch (prop_id) {
    case PROP_VIDEO_SRC:
      if (self->src_vid_src)
        g_value_set_object (value, self->src_vid_src);
      else
        g_value_set_object (value, self->app_vid_src);
      break;
    case PROP_VIDEO_SRC_FILTER:
      if (self->video_filter)
        g_value_set_object (value, self->video_filter);
      else
        g_value_set_object (value, self->app_vid_filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static void
gst_wrapper_camera_bin_src_reset_src_zoom (GstWrapperCameraBinSrc * self)
{
  if (self->src_crop) {
    g_object_set (self->src_crop, "top", 0, "left", 0, "bottom", 0, "right", 0,
        NULL);
  }
}

static void
gst_wrapper_camera_bin_reset_video_src_caps (GstWrapperCameraBinSrc * self,
    GstCaps * new_filter_caps)
{
  GST_DEBUG_OBJECT (self, "Resetting src caps to %" GST_PTR_FORMAT,
      new_filter_caps);
  if (self->src_vid_src) {
    GstCaps *src_neg_caps;      /* negotiated caps on src_filter */
    gboolean ret = FALSE;

    /* After pipe was negotiated src_filter do not have any filter caps.
     * In this situation we should compare negotiated caps on capsfilter pad
     * with requested range of caps. If one of this caps intersect,
     * then we can avoid resetting.
     */
    src_neg_caps = gst_pad_get_current_caps (self->srcfilter_pad);
    if (src_neg_caps && new_filter_caps && gst_caps_is_fixed (new_filter_caps))
      ret = gst_caps_can_intersect (src_neg_caps, new_filter_caps);
    else if (new_filter_caps == NULL) {
      /* If new_filter_caps = NULL, then some body wont to empty
       * capsfilter (set to ANY). In this case we will need to reset pipe,
       * but if capsfilter is actually empthy, then we can avoid
       * one more resetting.
       */
      GstCaps *old_filter_caps; /* range of caps on capsfilter */

      g_object_get (G_OBJECT (self->src_filter),
          "caps", &old_filter_caps, NULL);
      ret = gst_caps_is_any (old_filter_caps);
      gst_caps_unref (old_filter_caps);
    }
    if (src_neg_caps)
      gst_caps_unref (src_neg_caps);

    if (ret) {
      GST_DEBUG_OBJECT (self, "Negotiated caps on srcfilter intersect "
          "with requested caps, do not reset it.");
      return;
    }

    set_capsfilter_caps (self, new_filter_caps);
  }
}

static void
gst_wrapper_camera_bin_src_set_output (GstWrapperCameraBinSrc * self,
    GstPad * old_pad, GstPad * output_pad)
{
  if (old_pad)
    gst_ghost_pad_set_target (GST_GHOST_PAD (old_pad), NULL);
  if (output_pad)
    gst_ghost_pad_set_target (GST_GHOST_PAD (output_pad), self->src_pad);
}

/**
 * gst_wrapper_camera_bin_src_imgsrc_probe:
 *
 * Buffer probe called before sending each buffer to image queue.
 */
static GstPadProbeReturn
gst_wrapper_camera_bin_src_imgsrc_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer data)
{
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (data);
  GstBaseCameraSrc *camerasrc = GST_BASE_CAMERA_SRC (data);
  GstBuffer *buffer = GST_BUFFER (info->data);
  GstPadProbeReturn ret = GST_PAD_PROBE_DROP;

  GST_LOG_OBJECT (self, "Image probe, mode %d, capture count %d bufsize: %"
      G_GSIZE_FORMAT, camerasrc->mode, self->image_capture_count,
      gst_buffer_get_size (buffer));

  g_mutex_lock (&camerasrc->capturing_mutex);
  if (self->image_capture_count > 0) {
    GstSample *sample;
    GstCaps *caps;
    ret = GST_PAD_PROBE_OK;
    self->image_capture_count--;

    /* post preview */
    /* TODO This can likely be optimized if the viewfinder caps is the same as
     * the preview caps, avoiding another scaling of the same buffer. */
    GST_DEBUG_OBJECT (self, "Posting preview for image");
    caps = gst_pad_get_current_caps (pad);
    sample = gst_sample_new (buffer, caps, NULL, NULL);
    gst_base_camera_src_post_preview (camerasrc, sample);
    gst_caps_unref (caps);
    gst_sample_unref (sample);

    if (self->image_capture_count == 0) {
      GstCaps *anycaps = gst_caps_new_any ();

      /* Get back to viewfinder */
      gst_wrapper_camera_bin_src_reset_src_zoom (self);
      gst_wrapper_camera_bin_reset_video_src_caps (self, anycaps);
      gst_wrapper_camera_bin_src_set_output (self, self->imgsrc, self->vfsrc);
      gst_base_camera_src_finish_capture (camerasrc);

      gst_caps_unref (anycaps);
    }
  }
  g_mutex_unlock (&camerasrc->capturing_mutex);
  return ret;
}

/**
 * gst_wrapper_camera_bin_src_vidsrc_probe:
 *
 * Buffer probe called before sending each buffer to video queue.
 */
static GstPadProbeReturn
gst_wrapper_camera_bin_src_vidsrc_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer data)
{
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (data);
  GstBaseCameraSrc *camerasrc = GST_BASE_CAMERA_SRC_CAST (self);
  GstPadProbeReturn ret = GST_PAD_PROBE_DROP;
  GstBuffer *buffer = GST_BUFFER (info->data);

  GST_LOG_OBJECT (self, "Video probe, mode %d, capture status %d",
      camerasrc->mode, self->video_rec_status);

  /* TODO do we want to lock for every buffer? */
  /*
   * Note that we can use gst_pad_push_event here because we are a buffer
   * probe.
   */
  /* TODO shouldn't access this directly */
  g_mutex_lock (&camerasrc->capturing_mutex);
  if (self->video_rec_status == GST_VIDEO_RECORDING_STATUS_DONE) {
    /* NOP */
  } else if (self->video_rec_status == GST_VIDEO_RECORDING_STATUS_STARTING) {
    GstClockTime ts;
    GstSegment segment;
    GstCaps *caps;
    GstSample *sample;

    GST_DEBUG_OBJECT (self, "Starting video recording");
    self->video_rec_status = GST_VIDEO_RECORDING_STATUS_RUNNING;

    ts = GST_BUFFER_TIMESTAMP (buffer);
    if (!GST_CLOCK_TIME_IS_VALID (ts))
      ts = 0;
    gst_segment_init (&segment, GST_FORMAT_TIME);
    segment.start = ts;
    gst_pad_push_event (self->vidsrc, gst_event_new_segment (&segment));

    /* post preview */
    GST_DEBUG_OBJECT (self, "Posting preview for video");
    caps = gst_pad_get_current_caps (pad);
    sample = gst_sample_new (buffer, caps, NULL, NULL);
    gst_base_camera_src_post_preview (camerasrc, sample);
    gst_caps_unref (caps);
    gst_sample_unref (sample);

    ret = GST_PAD_PROBE_OK;
  } else if (self->video_rec_status == GST_VIDEO_RECORDING_STATUS_FINISHING) {
    GstPad *peer;

    /* send eos */
    GST_DEBUG_OBJECT (self, "Finishing video recording, pushing eos");

    peer = gst_pad_get_peer (self->vidsrc);

    if (peer) {
      /* send to the peer as we don't want our pads with eos flag */
      gst_pad_send_event (peer, gst_event_new_eos ());
      gst_object_unref (peer);
    } else {
      GST_WARNING_OBJECT (camerasrc, "No peer pad for vidsrc");
    }
    self->video_rec_status = GST_VIDEO_RECORDING_STATUS_DONE;

    gst_pad_unlink (self->src_pad, self->video_tee_sink);
    gst_wrapper_camera_bin_src_set_output (self, self->vfsrc, self->vfsrc);
    gst_base_camera_src_finish_capture (camerasrc);
  } else {
    ret = GST_PAD_PROBE_OK;
  }
  g_mutex_unlock (&camerasrc->capturing_mutex);
  return ret;
}

static void
gst_wrapper_camera_bin_src_caps_cb (GstPad * pad, GParamSpec * pspec,
    gpointer user_data)
{
  GstBaseCameraSrc *bcamsrc = GST_BASE_CAMERA_SRC (user_data);
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (user_data);
  GstCaps *caps;
  GstStructure *in_st = NULL;

  caps = gst_pad_get_current_caps (pad);

  GST_DEBUG_OBJECT (self, "src-filter caps changed to %" GST_PTR_FORMAT, caps);

  if (caps && gst_caps_get_size (caps)) {
    in_st = gst_caps_get_structure (caps, 0);
    if (in_st) {
      gst_structure_get_int (in_st, "width", &bcamsrc->width);
      gst_structure_get_int (in_st, "height", &bcamsrc->height);

      GST_DEBUG_OBJECT (self, "Source dimensions now: %dx%d", bcamsrc->width,
          bcamsrc->height);
    }
  }

  /* Update zoom */
  gst_base_camera_src_setup_zoom (bcamsrc);

  if (caps)
    gst_caps_unref (caps);
};

static void
gst_wrapper_camera_bin_src_max_zoom_cb (GObject * self, GParamSpec * pspec,
    gpointer user_data)
{
  GstBaseCameraSrc *bcamsrc = (GstBaseCameraSrc *) user_data;

  g_object_get (self, "max-zoom", &bcamsrc->max_zoom, NULL);
  g_object_notify (G_OBJECT (bcamsrc), "max-zoom");
}

static gboolean
gst_wrapper_camera_bin_src_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = TRUE;
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (parent);

  GST_DEBUG_OBJECT (self, "Handling event %p %" GST_PTR_FORMAT, event, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_RECONFIGURE:
      if (pad == self->imgsrc) {
        GST_DEBUG_OBJECT (self, "Image mode reconfigure event received");
        self->image_renegotiate = TRUE;
      } else if (pad == self->vidsrc) {
        GST_DEBUG_OBJECT (self, "Video mode reconfigure event received");
        self->video_renegotiate = TRUE;
      }
      if (pad == self->imgsrc || pad == self->vidsrc) {
        gst_event_unref (event);
        return ret;
      }
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

/**
 * check_and_replace_src
 * @self: #GstWrapperCamerabinSrcCameraSrc object
 *
 * Checks if the current videosrc needs to be replaced
 */
static gboolean
check_and_replace_src (GstWrapperCameraBinSrc * self)
{
  GstBin *cbin = GST_BIN_CAST (self);
  GstBaseCameraSrc *bcamsrc = GST_BASE_CAMERA_SRC_CAST (self);

  if (self->src_vid_src && self->src_vid_src == self->app_vid_src) {
    GST_DEBUG_OBJECT (self, "No need to change current videosrc");
    return TRUE;
  }

  if (self->src_vid_src) {
    GST_DEBUG_OBJECT (self, "Removing old video source");
    if (self->src_max_zoom_signal_id) {
      g_signal_handler_disconnect (self->src_vid_src,
          self->src_max_zoom_signal_id);
      self->src_max_zoom_signal_id = 0;
    }
    if (self->src_event_probe_id) {
      GstPad *pad;
      pad = gst_element_get_static_pad (self->src_vid_src, "src");
      gst_pad_remove_probe (pad, self->src_event_probe_id);
      gst_object_unref (pad);
      self->src_event_probe_id = 0;
    }
    gst_bin_remove (GST_BIN_CAST (self), self->src_vid_src);
    self->src_vid_src = NULL;
  }

  GST_DEBUG_OBJECT (self, "Adding new video source");

  /* Add application set or default video src element */
  if (!(self->src_vid_src = gst_camerabin_setup_default_element (cbin,
              self->app_vid_src, "autovideosrc", DEFAULT_VIDEOSRC,
              "camerasrc-real-src"))) {
    self->src_vid_src = NULL;
    goto fail;
  }

  if (!gst_bin_add (cbin, self->src_vid_src)) {
    goto fail;
  }

  /* check if we already have the next element to link to */
  if (self->src_crop) {
    if (!gst_element_link_pads (self->src_vid_src, "src", self->src_crop,
            "sink")) {
      goto fail;
    }
  }

  /* we listen for changes to max-zoom in the video src so that
   * we can proxy them to the basecamerasrc property */
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (bcamsrc), "max-zoom")) {
    self->src_max_zoom_signal_id =
        g_signal_connect (G_OBJECT (self->src_vid_src), "notify::max-zoom",
        (GCallback) gst_wrapper_camera_bin_src_max_zoom_cb, bcamsrc);
  }

  return TRUE;

fail:
  if (self->src_vid_src)
    gst_element_set_state (self->src_vid_src, GST_STATE_NULL);
  return FALSE;
}

/**
 * gst_wrapper_camera_bin_src_construct_pipeline:
 * @bcamsrc: camerasrc object
 *
 * This function creates and links the elements of the camerasrc bin
 * videosrc ! cspconv ! srcfilter ! cspconv ! capsfilter ! crop ! scale ! \
 * capsfilter
 *
 * Returns: TRUE, if elements were successfully created, FALSE otherwise
 */
static gboolean
gst_wrapper_camera_bin_src_construct_pipeline (GstBaseCameraSrc * bcamsrc)
{
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (bcamsrc);
  GstBin *cbin = GST_BIN (bcamsrc);
  GstElement *filter_csp;
  GstElement *src_csp;
  GstElement *capsfilter;
  GstElement *video_recording_tee;
  gboolean ret = FALSE;
  GstPad *tee_pad;

  /* checks and adds a new video src if needed */
  if (!check_and_replace_src (self))
    goto done;

  if (!self->elements_created) {

    GST_DEBUG_OBJECT (self, "constructing pipeline");

    if (!(self->src_crop =
            gst_camerabin_create_and_add_element (cbin, "videocrop",
                "src-crop")))
      goto done;

    if (!gst_camerabin_create_and_add_element (cbin, "videoconvert",
            "src-videoconvert"))
      goto done;

    if (self->app_vid_filter) {
      self->video_filter = gst_object_ref (self->app_vid_filter);

      if (!gst_camerabin_add_element (cbin, self->video_filter))
        goto done;
      if (!gst_camerabin_create_and_add_element (cbin, "videoconvert",
              "filter-videoconvert"))
        goto done;
    }

    if (!(self->src_filter =
            gst_camerabin_create_and_add_element (cbin, "capsfilter",
                "src-capsfilter")))
      goto done;

    /* attach to notify::caps on the first capsfilter and use a callback
     * to recalculate the zoom properties when these caps change and to
     * propagate the caps to the second capsfilter */
    self->srcfilter_pad = gst_element_get_static_pad (self->src_filter, "src");
    g_signal_connect (self->srcfilter_pad, "notify::caps",
        G_CALLBACK (gst_wrapper_camera_bin_src_caps_cb), self);

    if (!(self->digitalzoom = g_object_new (GST_TYPE_DIGITAL_ZOOM, NULL))) {
      GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN,
          (_("Digitalzoom element couldn't be created")), (NULL));

      goto done;
    }
    if (!gst_camerabin_add_element_full (GST_BIN_CAST (self), NULL,
            self->digitalzoom, "sink"))
      goto done;

    /* keep a 'tee' element that has 2 source pads, one is linked to the
     * vidsrc pad and the other is linked as needed to the viewfinder
     * when video recording is happening */
    video_recording_tee = gst_element_factory_make ("tee", "video_rec_tee");
    gst_bin_add (GST_BIN_CAST (self), video_recording_tee);     /* TODO check returns */
    self->video_tee_vf_pad =
        gst_element_request_pad_simple (video_recording_tee, "src_%u");
    self->video_tee_sink =
        gst_element_get_static_pad (video_recording_tee, "sink");
    tee_pad = gst_element_request_pad_simple (video_recording_tee, "src_%u");
    gst_ghost_pad_set_target (GST_GHOST_PAD (self->vidsrc), tee_pad);
    gst_object_unref (tee_pad);

    /* viewfinder pad */
    self->src_pad = gst_element_get_static_pad (self->digitalzoom, "src");
    gst_ghost_pad_set_target (GST_GHOST_PAD (self->vfsrc), self->src_pad);

    gst_pad_set_active (self->vfsrc, TRUE);
    gst_pad_set_active (self->imgsrc, TRUE);    /* XXX ??? */
    gst_pad_set_active (self->vidsrc, TRUE);    /* XXX ??? */

    gst_pad_add_probe (self->imgsrc, GST_PAD_PROBE_TYPE_BUFFER,
        gst_wrapper_camera_bin_src_imgsrc_probe, self, NULL);
    gst_pad_add_probe (self->video_tee_sink, GST_PAD_PROBE_TYPE_BUFFER,
        gst_wrapper_camera_bin_src_vidsrc_probe, self, NULL);
  }

  /* Do this even if pipeline is constructed */

  if (self->video_filter) {
    /* check if we need to replace the current one */
    if (self->video_filter != self->app_vid_filter) {
      gst_bin_remove (cbin, self->video_filter);
      gst_object_unref (self->video_filter);
      self->video_filter = NULL;
      filter_csp = gst_bin_get_by_name (cbin, "filter-videoconvert");
      gst_bin_remove (cbin, filter_csp);
      gst_object_unref (filter_csp);
      filter_csp = NULL;
    }
  }

  if (!self->video_filter) {
    if (self->app_vid_filter) {
      self->video_filter = gst_object_ref (self->app_vid_filter);
      filter_csp = gst_element_factory_make ("videoconvert",
          "filter-videoconvert");
      gst_bin_add_many (cbin, self->video_filter, filter_csp, NULL);
      src_csp = gst_bin_get_by_name (cbin, "src-videoconvert");
      capsfilter = gst_bin_get_by_name (cbin, "src-capsfilter");
      if (gst_pad_is_linked (gst_element_get_static_pad (src_csp, "src")))
        gst_element_unlink (src_csp, capsfilter);
      if (!gst_element_link_many (src_csp, self->video_filter, filter_csp,
              capsfilter, NULL)) {
        gst_object_unref (src_csp);
        gst_object_unref (capsfilter);
        goto done;
      }
      gst_object_unref (src_csp);
      gst_object_unref (capsfilter);
    }
  }
  ret = TRUE;
  self->elements_created = TRUE;
done:
  return ret;
}

/**
 * adapt_image_capture:
 * @self: camerasrc object
 * @in_caps: caps object that describes incoming image format
 *
 * Adjust capsfilters and crop according image capture caps if necessary.
 * The captured image format from video source might be different from
 * what application requested, so we can try to fix that in camerabin.
 *
 */
static void
adapt_image_capture (GstWrapperCameraBinSrc * self, GstCaps * in_caps)
{
  GstStructure *in_st, *req_st;
  gint in_width = 0, in_height = 0, req_width = 0, req_height = 0, crop = 0;
  gdouble ratio_w, ratio_h;

  GST_LOG_OBJECT (self, "in caps: %" GST_PTR_FORMAT, in_caps);
  GST_LOG_OBJECT (self, "requested caps: %" GST_PTR_FORMAT,
      self->image_capture_caps);

  in_st = gst_caps_get_structure (in_caps, 0);
  gst_structure_get_int (in_st, "width", &in_width);
  gst_structure_get_int (in_st, "height", &in_height);

  req_st = gst_caps_get_structure (self->image_capture_caps, 0);
  gst_structure_get_int (req_st, "width", &req_width);
  gst_structure_get_int (req_st, "height", &req_height);

  GST_INFO_OBJECT (self, "we requested %dx%d, and got %dx%d", req_width,
      req_height, in_width, in_height);

  /* Crop if requested aspect ratio differs from incoming frame aspect ratio */
  if (self->src_crop) {
    gint base_crop_top = 0, base_crop_bottom = 0;
    gint base_crop_left = 0, base_crop_right = 0;

    ratio_w = (gdouble) in_width / req_width;
    ratio_h = (gdouble) in_height / req_height;

    if (ratio_w < ratio_h) {
      crop = in_height - (req_height * ratio_w);
      base_crop_top = crop / 2;
      base_crop_bottom = crop / 2;
    } else {
      crop = in_width - (req_width * ratio_h);
      base_crop_left = crop / 2;
      base_crop_right += crop / 2;
    }

    GST_INFO_OBJECT (self,
        "setting base crop: left:%d, right:%d, top:%d, bottom:%d",
        base_crop_left, base_crop_right, base_crop_top, base_crop_bottom);
    g_object_set (G_OBJECT (self->src_crop),
        "top", base_crop_top, "bottom", base_crop_bottom,
        "left", base_crop_left, "right", base_crop_right, NULL);
  }

  /* Update capsfilters */
  set_capsfilter_caps (self, self->image_capture_caps);
}

/**
 * img_capture_prepared:
 * @data: camerasrc object
 * @caps: caps describing the prepared image format
 *
 * Callback which is called after image capture has been prepared.
 */
static void
img_capture_prepared (gpointer data, GstCaps * caps)
{
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (data);

  GST_INFO_OBJECT (self, "image capture prepared");

  /* It is possible we are about to get something else that we requested */
  if (!gst_caps_can_intersect (self->image_capture_caps, caps)) {
    adapt_image_capture (self, caps);
  } else {
    set_capsfilter_caps (self, self->image_capture_caps);
  }
}

static GstPadProbeReturn
start_image_capture (GstPad * pad, GstPadProbeInfo * info, gpointer udata)
{
  GstWrapperCameraBinSrc *self = udata;
  GstBaseCameraSrc *bcamsrc = GST_BASE_CAMERA_SRC (self);
  GstPhotography *photography =
      (GstPhotography *) gst_bin_get_by_interface (GST_BIN_CAST (bcamsrc),
      GST_TYPE_PHOTOGRAPHY);
  GstCaps *caps;

  GST_DEBUG_OBJECT (self, "Starting image capture");

  /* unlink from the viewfinder, link to the imagesrc pad to wait for
   * the buffer to pass */
  gst_wrapper_camera_bin_src_set_output (self, self->vfsrc, self->imgsrc);

  if (self->image_renegotiate) {
    self->image_renegotiate = FALSE;

    /* clean capsfilter caps so they don't interfere here */
    g_object_set (self->src_filter, "caps", NULL, NULL);

    caps = gst_pad_get_allowed_caps (self->imgsrc);
    gst_caps_replace (&self->image_capture_caps, caps);
    gst_caps_unref (caps);

    /* We caught this event in the src pad event handler and now we want to
     * actually push it upstream */
    gst_pad_mark_reconfigure (pad);
  }

  if (photography) {
    GST_DEBUG_OBJECT (self, "prepare image capture caps %" GST_PTR_FORMAT,
        self->image_capture_caps);
    if (!gst_photography_prepare_for_capture (photography,
            (GstPhotographyCapturePrepared) img_capture_prepared,
            self->image_capture_caps, self)) {
      GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
          ("Failed to prepare image capture"),
          ("Prepare capture call didn't succeed for the given caps"));
      self->image_capture_count = 0;
    }
    gst_object_unref (photography);
  } else {
    gst_wrapper_camera_bin_reset_video_src_caps (self,
        self->image_capture_caps);
  }

  self->image_capture_probe = 0;
  return GST_PAD_PROBE_REMOVE;
}

static GstPadProbeReturn
start_video_capture (GstPad * pad, GstPadProbeInfo * info, gpointer udata)
{
  GstWrapperCameraBinSrc *self = udata;
  GstCaps *caps;

  GST_DEBUG_OBJECT (self, "Starting video capture");

  if (self->video_renegotiate) {
    GstCaps *anycaps = gst_caps_new_any ();
    gst_wrapper_camera_bin_reset_video_src_caps (self, anycaps);
    gst_caps_unref (anycaps);

    /* clean capsfilter caps so they don't interfere here */
    g_object_set (self->src_filter, "caps", NULL, NULL);
  }

  /* unlink from the viewfinder, link to the imagesrc pad, wait for
   * the buffer to pass */
  gst_wrapper_camera_bin_src_set_output (self, self->vfsrc, NULL);
  gst_pad_link (self->src_pad, self->video_tee_sink);
  gst_ghost_pad_set_target (GST_GHOST_PAD (self->vfsrc),
      self->video_tee_vf_pad);

  if (self->video_renegotiate) {
    GST_DEBUG_OBJECT (self, "Getting allowed videosrc caps");
    caps = gst_pad_get_allowed_caps (self->vidsrc);
    GST_DEBUG_OBJECT (self, "Video src caps %" GST_PTR_FORMAT, caps);

    self->video_renegotiate = FALSE;
    gst_wrapper_camera_bin_reset_video_src_caps (self, caps);
    gst_caps_unref (caps);
  }
  self->video_capture_probe = 0;

  return GST_PAD_PROBE_REMOVE;
}

static gboolean
gst_wrapper_camera_bin_src_set_mode (GstBaseCameraSrc * bcamsrc,
    GstCameraBinMode mode)
{
  GstPhotography *photography =
      (GstPhotography *) gst_bin_get_by_interface (GST_BIN_CAST (bcamsrc),
      GST_TYPE_PHOTOGRAPHY);
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (bcamsrc);

  if (mode == MODE_IMAGE) {
    self->image_renegotiate = TRUE;
  } else {
    self->video_renegotiate = TRUE;
  }
  self->mode = mode;

  if (photography) {
    if (g_object_class_find_property (G_OBJECT_GET_CLASS (photography),
            "capture-mode")) {
      g_object_set (G_OBJECT (photography), "capture-mode", mode, NULL);
    }
    gst_object_unref (photography);
  } else {
    GstCaps *anycaps = gst_caps_new_any ();
    gst_wrapper_camera_bin_reset_video_src_caps (self, anycaps);
    gst_caps_unref (anycaps);
  }

  return TRUE;
}

static gboolean
set_videosrc_zoom (GstWrapperCameraBinSrc * self, gfloat zoom)
{
  gboolean ret = FALSE;

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (self->src_vid_src),
          "zoom")) {
    g_object_set (G_OBJECT (self->src_vid_src), "zoom", zoom, NULL);
    ret = TRUE;
  }
  return ret;
}

static void
gst_wrapper_camera_bin_src_set_zoom (GstBaseCameraSrc * bcamsrc, gfloat zoom)
{
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (bcamsrc);

  GST_INFO_OBJECT (self, "setting zoom %f", zoom);

  if (set_videosrc_zoom (self, zoom)) {
    g_object_set (self->digitalzoom, "zoom", (gfloat) 1.0, NULL);
    GST_INFO_OBJECT (self, "zoom set using videosrc");
  } else {
    GST_INFO_OBJECT (self, "zoom set using digitalzoom");
    g_object_set (self->digitalzoom, "zoom", zoom, NULL);
  }
}

/**
 * update_aspect_filter:
 * @self: camerasrc object
 * @new_caps: new caps of next buffers arriving to view finder sink element
 *
 * Updates aspect ratio capsfilter to maintain aspect ratio, if we need to
 * scale frames for showing them in view finder.
 */
static void
update_aspect_filter (GstWrapperCameraBinSrc * self, GstCaps * new_caps)
{
  /* XXX why not instead add a preserve-aspect-ratio property to videoscale? */
#if 0
  if (camera->flags & GST_CAMERABIN_FLAG_VIEWFINDER_SCALE) {
    GstCaps *sink_caps, *ar_caps;
    GstStructure *st;
    gint in_w = 0, in_h = 0, sink_w = 0, sink_h = 0, target_w = 0, target_h = 0;
    gdouble ratio_w, ratio_h;
    GstPad *sink_pad;
    const GValue *range;

    sink_pad = gst_element_get_static_pad (camera->view_sink, "sink");

    if (sink_pad) {
      sink_caps = gst_pad_get_caps (sink_pad);
      gst_object_unref (sink_pad);
      if (sink_caps) {
        if (!gst_caps_is_any (sink_caps)) {
          GST_DEBUG_OBJECT (camera, "sink element caps %" GST_PTR_FORMAT,
              sink_caps);
          /* Get maximum resolution that view finder sink accepts */
          st = gst_caps_get_structure (sink_caps, 0);
          if (gst_structure_has_field_typed (st, "width", GST_TYPE_INT_RANGE)) {
            range = gst_structure_get_value (st, "width");
            sink_w = gst_value_get_int_range_max (range);
          }
          if (gst_structure_has_field_typed (st, "height", GST_TYPE_INT_RANGE)) {
            range = gst_structure_get_value (st, "height");
            sink_h = gst_value_get_int_range_max (range);
          }
          GST_DEBUG_OBJECT (camera, "sink element accepts max %dx%d", sink_w,
              sink_h);

          /* Get incoming frames' resolution */
          if (sink_h && sink_w) {
            st = gst_caps_get_structure (new_caps, 0);
            gst_structure_get_int (st, "width", &in_w);
            gst_structure_get_int (st, "height", &in_h);
            GST_DEBUG_OBJECT (camera, "new caps with %dx%d", in_w, in_h);
          }
        }
        gst_caps_unref (sink_caps);
      }
    }

    /* If we get bigger frames than view finder sink accepts, then we scale.
       If we scale we need to adjust aspect ratio capsfilter caps in order
       to maintain aspect ratio while scaling. */
    if (in_w && in_h && (in_w > sink_w || in_h > sink_h)) {
      ratio_w = (gdouble) sink_w / in_w;
      ratio_h = (gdouble) sink_h / in_h;

      if (ratio_w < ratio_h) {
        target_w = sink_w;
        target_h = (gint) (ratio_w * in_h);
      } else {
        target_w = (gint) (ratio_h * in_w);
        target_h = sink_h;
      }

      GST_DEBUG_OBJECT (camera, "setting %dx%d filter to maintain aspect ratio",
          target_w, target_h);
      ar_caps = gst_caps_copy (new_caps);
      gst_caps_set_simple (ar_caps, "width", G_TYPE_INT, target_w, "height",
          G_TYPE_INT, target_h, NULL);
    } else {
      GST_DEBUG_OBJECT (camera, "no scaling");
      ar_caps = new_caps;
    }

    GST_DEBUG_OBJECT (camera, "aspect ratio filter caps %" GST_PTR_FORMAT,
        ar_caps);
    g_object_set (G_OBJECT (camera->aspect_filter), "caps", ar_caps, NULL);
    if (ar_caps != new_caps)
      gst_caps_unref (ar_caps);
  }
#endif
}


/**
 * set_capsfilter_caps:
 * @self: camerasrc object
 * @new_caps: pointer to caps object to set
 *
 * Set given caps to camerabin capsfilters.
 */
static void
set_capsfilter_caps (GstWrapperCameraBinSrc * self, GstCaps * new_caps)
{
  GST_INFO_OBJECT (self, "new_caps:%" GST_PTR_FORMAT, new_caps);

  /* Update zoom */
  gst_base_camera_src_setup_zoom (GST_BASE_CAMERA_SRC (self));

  /* Update capsfilters */
  g_object_set (G_OBJECT (self->src_filter), "caps", new_caps, NULL);
  update_aspect_filter (self, new_caps);
  GST_INFO_OBJECT (self, "updated");
}

static gboolean
gst_wrapper_camera_bin_src_start_capture (GstBaseCameraSrc * camerasrc)
{
  GstWrapperCameraBinSrc *src = GST_WRAPPER_CAMERA_BIN_SRC (camerasrc);
  GstPad *pad;
  gboolean ret = TRUE;

  pad = gst_element_get_static_pad (src->src_vid_src, "src");

  /* TODO should we access this directly? Maybe a macro is better? */
  if (src->mode == MODE_IMAGE) {
    src->image_capture_count = 1;

    src->image_capture_probe =
        gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_IDLE, start_image_capture,
        src, NULL);
  } else if (src->mode == MODE_VIDEO) {
    if (src->video_rec_status == GST_VIDEO_RECORDING_STATUS_DONE) {
      src->video_rec_status = GST_VIDEO_RECORDING_STATUS_STARTING;
      src->video_capture_probe =
          gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_IDLE, start_video_capture,
          src, NULL);
    }
  } else {
    g_assert_not_reached ();
    ret = FALSE;
  }
  gst_object_unref (pad);
  return ret;
}

static void
gst_wrapper_camera_bin_src_stop_capture (GstBaseCameraSrc * camerasrc)
{
  GstWrapperCameraBinSrc *src = GST_WRAPPER_CAMERA_BIN_SRC (camerasrc);

  /* TODO should we access this directly? Maybe a macro is better? */
  if (src->mode == MODE_VIDEO) {
    if (src->video_rec_status == GST_VIDEO_RECORDING_STATUS_STARTING) {
      GST_DEBUG_OBJECT (src, "Aborting, had not started recording");
      src->video_rec_status = GST_VIDEO_RECORDING_STATUS_DONE;

    } else if (src->video_rec_status == GST_VIDEO_RECORDING_STATUS_RUNNING) {
      GST_DEBUG_OBJECT (src, "Marking video recording as finishing");
      src->video_rec_status = GST_VIDEO_RECORDING_STATUS_FINISHING;
    }
  } else {
    /* TODO check what happens when we try to stop a image capture */
  }
}

static GstStateChangeReturn
gst_wrapper_camera_bin_src_change_state (GstElement * element,
    GstStateChange trans)
{
  GstStateChangeReturn ret;
  GstWrapperCameraBinSrc *self = GST_WRAPPER_CAMERA_BIN_SRC (element);

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);

  if (ret == GST_STATE_CHANGE_FAILURE)
    goto end;

  switch (trans) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->video_renegotiate = TRUE;
      self->image_renegotiate = TRUE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    default:
      break;
  }

end:
  return ret;
}

static void
gst_wrapper_camera_bin_src_class_init (GstWrapperCameraBinSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseCameraSrcClass *gstbasecamerasrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasecamerasrc_class = GST_BASE_CAMERA_SRC_CLASS (klass);

  gobject_class->dispose = gst_wrapper_camera_bin_src_dispose;
  gobject_class->finalize =
      (GObjectFinalizeFunc) gst_wrapper_camera_bin_src_finalize;
  gobject_class->set_property = gst_wrapper_camera_bin_src_set_property;
  gobject_class->get_property = gst_wrapper_camera_bin_src_get_property;

  /* g_object_class_install_property .... */
  g_object_class_install_property (gobject_class, PROP_VIDEO_SRC,
      g_param_spec_object ("video-source", "Video source",
          "The video source element to be used",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_VIDEO_SRC_FILTER,
      g_param_spec_object ("video-source-filter", "Video source filter",
          "Optional video source filter element",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state = gst_wrapper_camera_bin_src_change_state;

  gstbasecamerasrc_class->construct_pipeline =
      gst_wrapper_camera_bin_src_construct_pipeline;
  gstbasecamerasrc_class->set_zoom = gst_wrapper_camera_bin_src_set_zoom;
  gstbasecamerasrc_class->set_mode = gst_wrapper_camera_bin_src_set_mode;
  gstbasecamerasrc_class->start_capture =
      gst_wrapper_camera_bin_src_start_capture;
  gstbasecamerasrc_class->stop_capture =
      gst_wrapper_camera_bin_src_stop_capture;

  GST_DEBUG_CATEGORY_INIT (wrapper_camera_bin_src_debug, "wrappercamerabinsrc",
      0, "wrapper camera src");

  gst_element_class_add_static_pad_template (gstelement_class, &vfsrc_template);

  gst_element_class_add_static_pad_template (gstelement_class,
      &imgsrc_template);

  gst_element_class_add_static_pad_template (gstelement_class,
      &vidsrc_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "Wrapper camera src element for camerabin2", "Source/Video",
      "Wrapper camera src element for camerabin2",
      "Thiago Santos <thiago.sousa.santos@collabora.com>");
}

static void
gst_wrapper_camera_bin_src_init (GstWrapperCameraBinSrc * self)
{
  self->vfsrc =
      gst_ghost_pad_new_no_target (GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME,
      GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (self), self->vfsrc);

  self->imgsrc =
      gst_ghost_pad_new_no_target (GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME,
      GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (self), self->imgsrc);

  self->vidsrc =
      gst_ghost_pad_new_no_target (GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME,
      GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (self), self->vidsrc);

  gst_pad_set_event_function (self->imgsrc,
      GST_DEBUG_FUNCPTR (gst_wrapper_camera_bin_src_src_event));
  gst_pad_set_event_function (self->vidsrc,
      GST_DEBUG_FUNCPTR (gst_wrapper_camera_bin_src_src_event));

  /* TODO where are variables reset? */
  self->image_capture_count = 0;
  self->video_rec_status = GST_VIDEO_RECORDING_STATUS_DONE;
  self->video_renegotiate = TRUE;
  self->image_renegotiate = TRUE;
  self->mode = GST_BASE_CAMERA_SRC_CAST (self)->mode;
  self->app_vid_filter = NULL;
}
