/*
 * GStreamer
 * Copyright (C) 2010 Nokia Corporation <multimedia@maemo.org>
 * Copyright (C) 2011 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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

 /*
    TODO review
    Examples:
    ./gst-camerabin2-test --image-width=2048 --image-height=1536
    ./gst-camerabin2-test --mode=2 --capture-time=10 --image-width=848 --image-height=480 --view-framerate-num=2825 \
    --view-framerate-den=100

    gst-camerabin2-test --help
    Usage:
    gst-camerabin2-test [OPTION...]

    camerabin command line test application.

    Help Options:
    -h, --help                        Show help options
    --help-all                        Show all help options
    --help-gst                        Show GStreamer Options

    Application Options:
    --ev-compensation                 EV compensation (-2.5..2.5, default = 0)
    --aperture                        Aperture (size of lens opening, default = 0 (auto))
    --flash-mode                      Flash mode (default = 0 (auto))
    --scene-mode                      Scene mode (default = 6 (auto))
    --exposure                        Exposure (default = 0 (auto))
    --iso-speed                       ISO speed (default = 0 (auto))
    --white-balance-mode              White balance mode (default = 0 (auto))
    --colour-tone-mode                Colour tone mode (default = 0 (auto))
    --directory                       Directory for capture file(s) (default is current directory)
    --mode                            Capture mode (default = 0 (image), 1 = video)
    --capture-time                    Time to capture video in seconds (default = 10)
    --capture-total                   Total number of captures to be done (default = 1)
    --zoom                            Zoom (100 = 1x (default), 200 = 2x etc.)
    --wrapper-source                  Camera source wrapper used for setting the video source
    --video-source                    Video source used in still capture and video recording
    --video-device                    Video device to be set on the video source (e.g. /dev/video0)
    --audio-source                    Audio source used in video recording
    --image-pp                        List of image post-processing elements separated with comma
    --viewfinder-sink                 Viewfinder sink (default = fakesink)
    --image-width                     Width for capture (only used if the caps
    arguments aren't set)
    --image-height                    Height for capture (only used if the caps
    arguments aren't set)
    --view-framerate-num              Framerate numerator for viewfinder
    --view-framerate-den              Framerate denominator for viewfinder
    --preview-caps                    Preview caps (e.g. video/x-raw-rgb,width=320,height=240)
    --viewfinder-filter               Filter to process all frames going to viewfinder sink
    --x-width                         X window width (default = 320)
    --x-height                        X window height (default = 240)
    --no-xwindow                      Do not create XWindow
    --encoding-target                 Video encoding target name
    --encoding-profile                Video encoding profile name
    --encoding-profile-filename       Video encoding profile filename
    --image-capture-caps              Image capture caps (e.g. video/x-raw-rgb,width=640,height=480)
    --viewfinder-caps                 Viewfinder caps (e.g. video/x-raw-rgb,width=640,height=480)
    --video-capture-caps              Video capture caps (e.g. video/x-raw-rgb,width=640,height=480)
    --performance-measure             Collect timing information about the
    captures and provides performance statistics at the end
    --performance-targets             A list of doubles that are the performance target
    times for each of the measured timestamps. The order is
    startup time, change mode time, shot to save, shot to snapshot,
    shot to shot, preview to precapture, shot to buffer.
    e.g. 3.5,1.0,5.0,2.5,5.0,1.5,1.0
    * Startup time -> time it takes for camerabin to reach playing
    * Change mode time -> time it takes for camerabin to change to the selected
    mode in playing
    * Shot to save -> time it takes from start-capture to having the image saved
    to disk
    * Shot to snapshot -> time it takes from start-capture to getting a snapshot
    * Shot to shot -> time from one start-capture to the next one
    * Preview to precapture -> time it takes from getting the snapshot to the
    next buffer that reaches the viewfinder
    * Shot to buffer -> time it takes from start-capture to the moment a buffer
    is pushed out of the camera source

  */

/*
 * Includes
 */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define GST_USE_UNSTABLE_API 1

#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/interfaces/photography.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/encoding-target.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
/*
 * debug logging
 */
GST_DEBUG_CATEGORY_STATIC (camerabin_test);
#define GST_CAT_DEFAULT camerabin_test

#define TIME_DIFF(a,b) ((((gint64)(a)) - ((gint64)(b))) / (gdouble) GST_SECOND)

#define TIME_FORMAT "02d.%09u"
#define TIMEDIFF_FORMAT "0.6lf"

#define TIME_ARGS(t) \
        (GST_CLOCK_TIME_IS_VALID (t) && (t) < 99 * GST_SECOND) ? \
        (gint) ((((GstClockTime)(t)) / GST_SECOND) % 60) : 99, \
        (GST_CLOCK_TIME_IS_VALID (t) && ((t) < 99 * GST_SECOND)) ? \
        (guint) (((GstClockTime)(t)) % GST_SECOND) : 999999999

#define TIMEDIFF_ARGS(t) (t)

typedef struct _CaptureTiming
{
  GstClockTime start_capture;
  GstClockTime got_preview;
  GstClockTime capture_done;
  GstClockTime precapture;
  GstClockTime camera_capture;
} CaptureTiming;

typedef struct _CaptureTimingStats
{
  GstClockTime shot_to_shot;
  GstClockTime shot_to_save;
  GstClockTime shot_to_snapshot;
  GstClockTime preview_to_precapture;
  GstClockTime shot_to_buffer;
} CaptureTimingStats;

static void
capture_timing_stats_add (CaptureTimingStats * a, CaptureTimingStats * b)
{
  a->shot_to_shot += b->shot_to_shot;
  a->shot_to_snapshot += b->shot_to_snapshot;
  a->shot_to_save += b->shot_to_save;
  a->preview_to_precapture += b->preview_to_precapture;
  a->shot_to_buffer += b->shot_to_buffer;
}

static void
capture_timing_stats_div (CaptureTimingStats * stats, gint div)
{
  stats->shot_to_shot /= div;
  stats->shot_to_snapshot /= div;
  stats->shot_to_save /= div;
  stats->preview_to_precapture /= div;
  stats->shot_to_buffer /= div;
}

#define PRINT_STATS(d,s) g_print ("%02d | %" TIME_FORMAT " | %" \
    TIME_FORMAT "   | %" TIME_FORMAT " | %" TIME_FORMAT \
    "    | %" TIME_FORMAT "\n", d, \
    TIME_ARGS ((s)->shot_to_save), TIME_ARGS ((s)->shot_to_snapshot), \
    TIME_ARGS ((s)->shot_to_shot), \
    TIME_ARGS ((s)->preview_to_precapture), \
    TIME_ARGS ((s)->shot_to_buffer))

#define SHOT_TO_SAVE(t) ((t)->capture_done - (t)->start_capture)
#define SHOT_TO_SNAPSHOT(t) ((t)->got_preview - (t)->start_capture)
#define PREVIEW_TO_PRECAPTURE(t) ((t)->precapture - (t)->got_preview)
#define SHOT_TO_BUFFER(t) ((t)->camera_capture - (t)->start_capture)

/*
 * Global vars
 */
static GstElement *camerabin = NULL;
static GstElement *viewfinder_sink = NULL;
static gulong camera_probe_id = 0;
static gulong viewfinder_probe_id = 0;
static GMainLoop *loop = NULL;

/* commandline options */
static gchar *videosrc_name = NULL;
static gchar *videodevice_name = NULL;
static gchar *audiosrc_name = NULL;
static gchar *wrappersrc_name = NULL;
static gchar *imagepp_name = NULL;
static gchar *vfsink_name = NULL;
static gint image_width = 0;
static gint image_height = 0;
static gint view_framerate_num = 0;
static gint view_framerate_den = 0;
static gboolean no_xwindow = FALSE;
static gchar *gep_targetname = NULL;
static gchar *gep_profilename = NULL;
static gchar *gep_filename = NULL;
static gchar *image_capture_caps_str = NULL;
static gchar *viewfinder_caps_str = NULL;
static gchar *video_capture_caps_str = NULL;
static gchar *audio_capture_caps_str = NULL;
static gboolean performance_measure = FALSE;
static gchar *performance_targets_str = NULL;
static gchar *camerabin_flags = NULL;


#define MODE_VIDEO 2
#define MODE_IMAGE 1
static gint mode = MODE_IMAGE;
static gint zoom = 100;

static gint capture_time = 10;
static gint capture_count = 0;
static gint capture_total = 1;
static gulong stop_capture_cb_id = 0;

/* photography interface command line options */
#define EV_COMPENSATION_NONE -G_MAXFLOAT
#define APERTURE_NONE -G_MAXINT
#define FLASH_MODE_NONE -G_MAXINT
#define SCENE_MODE_NONE -G_MAXINT
#define EXPOSURE_NONE -G_MAXINT64
#define ISO_SPEED_NONE -G_MAXINT
#define WHITE_BALANCE_MODE_NONE -G_MAXINT
#define COLOR_TONE_MODE_NONE -G_MAXINT
static gfloat ev_compensation = EV_COMPENSATION_NONE;
static gint aperture = APERTURE_NONE;
static gint flash_mode = FLASH_MODE_NONE;
static gint scene_mode = SCENE_MODE_NONE;
static gint64 exposure = EXPOSURE_NONE;
static gint iso_speed = ISO_SPEED_NONE;
static gint wb_mode = WHITE_BALANCE_MODE_NONE;
static gint color_mode = COLOR_TONE_MODE_NONE;

static gchar *viewfinder_filter = NULL;

static int x_width = 320;
static int x_height = 240;

/* test configuration for common callbacks */
static GString *filename = NULL;

static gchar *preview_caps_name = NULL;

/* X window variables */
static Display *display = NULL;
static Window window = 0;

/* timing data */
static GstClockTime initial_time = 0;
static GstClockTime startup_time = 0;
static GstClockTime change_mode_before = 0;
static GstClockTime change_mode_after = 0;
static GList *capture_times = NULL;

static GstClockTime target_startup;
static GstClockTime target_change_mode;
static GstClockTime target_shot_to_shot;
static GstClockTime target_shot_to_save;
static GstClockTime target_shot_to_snapshot;
static GstClockTime target_preview_to_precapture;
static GstClockTime target_shot_to_buffer;


/*
 * Prototypes
 */
static gboolean run_pipeline (gpointer user_data);
static void set_metadata (GstElement * camera);

static void
create_host_window (void)
{
  unsigned long valuemask;
  XSetWindowAttributes attributes;

  display = XOpenDisplay (NULL);
  if (display) {
    window =
        XCreateSimpleWindow (display, DefaultRootWindow (display), 0, 0,
        x_width, x_height, 0, 0, 0);
    if (window) {
      valuemask = CWOverrideRedirect;
      attributes.override_redirect = True;
      XChangeWindowAttributes (display, window, valuemask, &attributes);
      XSetWindowBackgroundPixmap (display, window, None);
      XMapRaised (display, window);
      XSync (display, FALSE);
    } else {
      GST_DEBUG ("could not create X window!");
    }
  } else {
    GST_DEBUG ("could not open display!");
  }
}

static GstPadProbeReturn
camera_src_get_timestamp_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer udata)
{
  CaptureTiming *timing;

  timing = (CaptureTiming *) g_list_first (capture_times)->data;
  timing->camera_capture = gst_util_get_timestamp ();

  return GST_PAD_PROBE_REMOVE;
}

static GstPadProbeReturn
viewfinder_get_timestamp_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer udata)
{
  CaptureTiming *timing;

  timing = (CaptureTiming *) g_list_first (capture_times)->data;
  timing->precapture = gst_util_get_timestamp ();

  return GST_PAD_PROBE_REMOVE;
}

static GstBusSyncReply
sync_bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  const GstStructure *st;
  const GValue *image;
  GstBuffer *buf = NULL;
  gchar *preview_filename = NULL;
  FILE *f = NULL;
  size_t written;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ELEMENT:{
      st = gst_message_get_structure (message);
      if (st) {
        if (gst_message_has_name (message, "prepare-xwindow-id")) {
          if (!no_xwindow && window) {
            gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY
                (GST_MESSAGE_SRC (message)), window);
            gst_message_unref (message);
            message = NULL;
            return GST_BUS_DROP;
          }
        } else if (gst_structure_has_name (st, "preview-image")) {
          CaptureTiming *timing;

          GST_DEBUG ("preview-image");

          timing = (CaptureTiming *) g_list_first (capture_times)->data;
          timing->got_preview = gst_util_get_timestamp ();

          {
            /* set up probe to check when the viewfinder gets data */
            GstPad *pad = gst_element_get_static_pad (viewfinder_sink, "sink");

            viewfinder_probe_id =
                gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
                viewfinder_get_timestamp_probe, NULL, NULL);

            gst_object_unref (pad);
          }

          /* extract preview-image from msg */
          image = gst_structure_get_value (st, "buffer");
          if (image) {
            buf = gst_value_get_buffer (image);
            preview_filename = g_strdup_printf ("test_vga.rgb");
            f = g_fopen (preview_filename, "w");
            if (f) {
              GstMapInfo map;

              gst_buffer_map (buf, &map, GST_MAP_READ);
              written = fwrite (map.data, map.size, 1, f);
              gst_buffer_unmap (buf, &map);
              if (!written) {
                g_print ("error writing file\n");
              }
              fclose (f);
            } else {
              g_print ("error opening file for raw image writing\n");
            }
            g_free (preview_filename);
          }
        }
      }
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC (message) == (GstObject *) camerabin) {
        GstState newstate;

        gst_message_parse_state_changed (message, NULL, &newstate, NULL);
        if (newstate == GST_STATE_PLAYING) {
          startup_time = gst_util_get_timestamp ();
        }
      }
      break;
    default:
      /* unhandled message */
      break;
  }
  return GST_BUS_PASS;
}

static gboolean
bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_clear_error (&err);
      g_free (debug);

      /* Write debug graph to file */
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (camerabin),
          GST_DEBUG_GRAPH_SHOW_ALL, "camerabin.error");

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_IS_BIN (GST_MESSAGE_SRC (message))) {
        GstState oldstate, newstate;

        gst_message_parse_state_changed (message, &oldstate, &newstate, NULL);
        GST_DEBUG_OBJECT (GST_MESSAGE_SRC (message), "state-changed: %s -> %s",
            gst_element_state_get_name (oldstate),
            gst_element_state_get_name (newstate));
      }
      break;
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      GST_INFO ("got eos() - should not happen");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ELEMENT:
      if (GST_MESSAGE_SRC (message) == (GstObject *) camerabin) {
        const GstStructure *structure = gst_message_get_structure (message);

        if (gst_structure_has_name (structure, "image-done")) {
          CaptureTiming *timing;
#ifndef GST_DISABLE_GST_DEBUG
          const gchar *fname = gst_structure_get_string (structure, "filename");

          GST_DEBUG ("image done: %s", fname);
#endif
          timing = (CaptureTiming *) g_list_first (capture_times)->data;
          timing->capture_done = gst_util_get_timestamp ();

          if (capture_count < capture_total) {
            g_idle_add ((GSourceFunc) run_pipeline, NULL);
          } else {
            g_main_loop_quit (loop);
          }
        }
      }
      break;
    default:
      /* unhandled message */
      break;
  }
  return TRUE;
}

/*
 * Helpers
 */

static void
cleanup_pipeline (void)
{
  if (camerabin) {
    GST_INFO_OBJECT (camerabin, "stopping and destroying");
    gst_element_set_state (camerabin, GST_STATE_NULL);
    gst_object_unref (camerabin);
    camerabin = NULL;
  }
}

static GstElement *
create_ipp_bin (void)
{
  GstElement *bin = NULL, *element = NULL;
  GstPad *pad = NULL;
  gchar **elements;
  GList *element_list = NULL, *current = NULL, *next = NULL;
  int i;

  bin = gst_bin_new ("ippbin");

  elements = g_strsplit (imagepp_name, ",", 0);

  for (i = 0; elements[i] != NULL; i++) {
    element = gst_element_factory_make (elements[i], NULL);
    if (element) {
      element_list = g_list_append (element_list, element);
      gst_bin_add (GST_BIN (bin), element);
    } else
      GST_WARNING ("Could create element %s for ippbin", elements[i]);
  }

  for (i = 1; i < g_list_length (element_list); i++) {
    current = g_list_nth (element_list, i - 1);
    next = g_list_nth (element_list, i);
    gst_element_link (current->data, next->data);
  }

  current = g_list_first (element_list);
  pad = gst_element_get_static_pad (current->data, "sink");
  gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (GST_OBJECT (pad));

  current = g_list_last (element_list);
  pad = gst_element_get_static_pad (current->data, "src");
  gst_element_add_pad (bin, gst_ghost_pad_new ("src", pad));
  gst_object_unref (GST_OBJECT (pad));

  g_list_free (element_list);
  g_strfreev (elements);

  return bin;
}

static GstEncodingProfile *
load_encoding_profile (void)
{
  GstEncodingProfile *prof = NULL;
  GstEncodingTarget *target = NULL;
  GError *error = NULL;

  /* if profile file was given, try to load profile from there */
  if (gep_filename && gep_profilename) {
    target = gst_encoding_target_load_from_file (gep_filename, &error);
    if (!target) {
      GST_WARNING ("Could not load target %s from file %s", gep_targetname,
          gep_filename);
      if (error) {
        GST_WARNING ("Error from file loading: %s", error->message);
        g_clear_error (&error);
      }
    } else {
      prof = gst_encoding_target_get_profile (target, gep_profilename);
      if (prof)
        GST_DEBUG ("Loaded encoding profile %s from %s", gep_profilename,
            gep_filename);
      else
        GST_WARNING
            ("Could not load specified encoding profile %s from file %s",
            gep_profilename, gep_filename);
    }
    /* if we could not load profile from file then try to find one from system */
  } else if (gep_profilename && gep_targetname) {
    prof = gst_encoding_profile_find (gep_targetname, gep_profilename, NULL);
    if (prof)
      GST_DEBUG ("Loaded encoding profile %s from target %s", gep_profilename,
          gep_targetname);
  } else
    GST_DEBUG
        ("Encoding profile not set, using camerabin default encoding profile");

  return prof;
}

static gboolean
setup_pipeline_element (GstElement * element, const gchar * property_name,
    const gchar * element_name, GstElement ** res_elem)
{
  gboolean res = TRUE;
  GstElement *elem = NULL;

  if (element_name) {
    GError *error = NULL;

    elem = gst_parse_launch (element_name, &error);
    if (elem) {
      g_object_set (element, property_name, elem, NULL);
      g_object_unref (elem);
    } else {
      GST_WARNING ("can't create element '%s' for property '%s'", element_name,
          property_name);
      if (error) {
        GST_ERROR ("%s", error->message);
        g_clear_error (&error);
      }
      res = FALSE;
    }
  } else {
    GST_DEBUG ("no element for property '%s' given", property_name);
  }
  if (res_elem)
    *res_elem = elem;
  return res;
}

static void
set_camerabin_caps_from_string (void)
{
  GstCaps *caps = NULL;
  if (image_capture_caps_str != NULL) {
    caps = gst_caps_from_string (image_capture_caps_str);
    if (GST_CAPS_IS_SIMPLE (caps) && image_width > 0 && image_height > 0) {
      gst_caps_set_simple (caps, "width", G_TYPE_INT, image_width, "height",
          G_TYPE_INT, image_height, NULL);
    }
    GST_DEBUG ("setting image-capture-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (camerabin, "image-capture-caps", caps, NULL);
    gst_caps_unref (caps);
  }

  if (viewfinder_caps_str != NULL) {
    caps = gst_caps_from_string (viewfinder_caps_str);
    if (GST_CAPS_IS_SIMPLE (caps) && view_framerate_num > 0
        && view_framerate_den > 0) {
      gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION,
          view_framerate_num, view_framerate_den, NULL);
    }
    GST_DEBUG ("setting viewfinder-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (camerabin, "viewfinder-caps", caps, NULL);
    gst_caps_unref (caps);
  }

  if (video_capture_caps_str != NULL) {
    caps = gst_caps_from_string (video_capture_caps_str);
    GST_DEBUG ("setting video-capture-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (camerabin, "video-capture-caps", caps, NULL);
    gst_caps_unref (caps);
  }

  if (audio_capture_caps_str != NULL) {
    caps = gst_caps_from_string (audio_capture_caps_str);
    GST_DEBUG ("setting audio-capture-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (camerabin, "audio-capture-caps", caps, NULL);
    gst_caps_unref (caps);
  }
}

static gboolean
setup_pipeline (void)
{
  gboolean res = TRUE;
  GstBus *bus;
  GstElement *sink = NULL, *ipp = NULL;
  GstEncodingProfile *prof = NULL;

  initial_time = gst_util_get_timestamp ();

  camerabin = gst_element_factory_make ("camerabin", NULL);
  if (NULL == camerabin) {
    g_warning ("can't create camerabin element\n");
    goto error;
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (camerabin));
  /* Add sync handler for time critical messages that need to be handled fast */
  gst_bus_set_sync_handler (bus, sync_bus_callback, NULL, NULL);
  /* Handle normal messages asynchronously */
  gst_bus_add_watch (bus, bus_callback, NULL);
  gst_object_unref (bus);

  GST_INFO_OBJECT (camerabin, "camerabin created");

  if (camerabin_flags)
    gst_util_set_object_arg (G_OBJECT (camerabin), "flags", camerabin_flags);
  else
    gst_util_set_object_arg (G_OBJECT (camerabin), "flags", "");

  if (videosrc_name) {
    GstElement *wrapper;
    GstElement *videosrc;

    if (wrappersrc_name)
      wrapper = gst_element_factory_make (wrappersrc_name, NULL);
    else
      wrapper = gst_element_factory_make ("wrappercamerabinsrc", NULL);

    if (setup_pipeline_element (wrapper, "video-source", videosrc_name, NULL)) {
      g_object_set (camerabin, "camera-source", wrapper, NULL);
      g_object_unref (wrapper);
    } else {
      GST_WARNING ("Failed to set videosrc to %s", videosrc_name);
    }

    g_object_get (wrapper, "video-source", &videosrc, NULL);
    if (videosrc && videodevice_name &&
        g_object_class_find_property (G_OBJECT_GET_CLASS (videosrc),
            "device")) {
      g_object_set (videosrc, "device", videodevice_name, NULL);
    }
  }

  /* configure used elements */
  res &=
      setup_pipeline_element (camerabin, "audio-source", audiosrc_name, NULL);
  res &=
      setup_pipeline_element (camerabin, "viewfinder-sink", vfsink_name, &sink);
  res &=
      setup_pipeline_element (camerabin, "viewfinder-filter", viewfinder_filter,
      NULL);

  if (imagepp_name) {
    ipp = create_ipp_bin ();
    if (ipp) {
      g_object_set (camerabin, "image-filter", ipp, NULL);
      g_object_unref (ipp);
    } else
      GST_WARNING ("Could not create ipp elements");
  }

  prof = load_encoding_profile ();
  if (prof) {
    g_object_set (G_OBJECT (camerabin), "video-profile", prof, NULL);
    gst_encoding_profile_unref (prof);
  }

  GST_INFO_OBJECT (camerabin, "elements created");

  if (sink) {
    g_object_set (sink, "sync", TRUE, NULL);
  } else {
    /* Get the inner viewfinder sink, this uses fixed names given
     * by default in camerabin */
    sink = gst_bin_get_by_name (GST_BIN (camerabin), "vf-bin");
    g_assert (sink);
    gst_object_unref (sink);

    sink = gst_bin_get_by_name (GST_BIN (sink), "vfbin-sink");
    g_assert (sink);
    gst_object_unref (sink);
  }
  viewfinder_sink = sink;

  GST_INFO_OBJECT (camerabin, "elements configured");

  /* configure a resolution and framerate */
  if (image_width > 0 && image_height > 0) {
    if (mode == MODE_VIDEO) {
      GstCaps *caps = NULL;
      if (view_framerate_num > 0)
        caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
                "width", G_TYPE_INT, image_width,
                "height", G_TYPE_INT, image_height,
                "framerate", GST_TYPE_FRACTION, view_framerate_num,
                view_framerate_den, NULL), NULL);
      else
        caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
                "width", G_TYPE_INT, image_width,
                "height", G_TYPE_INT, image_height, NULL), NULL);

      g_object_set (camerabin, "video-capture-caps", caps, NULL);
      gst_caps_unref (caps);
    } else {
      GstCaps *caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
              "width", G_TYPE_INT, image_width,
              "height", G_TYPE_INT, image_height, NULL), NULL);

      g_object_set (camerabin, "image-capture-caps", caps, NULL);
      gst_caps_unref (caps);
    }
  }

  set_camerabin_caps_from_string ();

  /* change to the wrong mode if timestamping if performance mode is on so
   * we can change it back and measure the time after in playing */
  if (performance_measure) {
    g_object_set (camerabin, "mode",
        mode == MODE_VIDEO ? MODE_IMAGE : MODE_VIDEO, NULL);
  }

  if (GST_STATE_CHANGE_FAILURE ==
      gst_element_set_state (camerabin, GST_STATE_READY)) {
    g_warning ("can't set camerabin to ready\n");
    goto error;
  }
  GST_INFO_OBJECT (camerabin, "camera ready");

  if (GST_STATE_CHANGE_FAILURE ==
      gst_element_set_state (camerabin, GST_STATE_PLAYING)) {
    g_warning ("can't set camerabin to playing\n");
    goto error;
  }

  GST_INFO_OBJECT (camerabin, "camera started");

  /* do the mode change timestamping if performance mode is on */
  if (performance_measure) {
    change_mode_before = gst_util_get_timestamp ();
    g_object_set (camerabin, "mode", mode, NULL);
    change_mode_after = gst_util_get_timestamp ();
  }

  return res;
error:
  cleanup_pipeline ();
  return FALSE;
}

static void
stop_capture_cb (GObject * self, GParamSpec * pspec, gpointer user_data)
{
  gboolean idle = FALSE;

  g_object_get (camerabin, "idle", &idle, NULL);

  if (idle) {
    if (capture_count < capture_total) {
      g_idle_add ((GSourceFunc) run_pipeline, NULL);
    } else {
      g_main_loop_quit (loop);
    }
  }

  g_signal_handler_disconnect (camerabin, stop_capture_cb_id);
}

static gboolean
stop_capture (gpointer user_data)
{
  stop_capture_cb_id = g_signal_connect (camerabin, "notify::idle",
      (GCallback) stop_capture_cb, camerabin);
  g_signal_emit_by_name (camerabin, "stop-capture", 0);
  return FALSE;
}

static void
set_metadata (GstElement * camera)
{
  GstTagSetter *setter = GST_TAG_SETTER (camera);
  GstDateTime *datetime;
  gchar *desc_str;

  datetime = gst_date_time_new_now_local_time ();

  desc_str = g_strdup_printf ("captured by %s", g_get_real_name ());

  gst_tag_setter_add_tags (setter, GST_TAG_MERGE_REPLACE,
      GST_TAG_DATE_TIME, datetime,
      GST_TAG_DESCRIPTION, desc_str,
      GST_TAG_TITLE, "gst-camerabin-test capture",
      GST_TAG_GEO_LOCATION_LONGITUDE, 1.0,
      GST_TAG_GEO_LOCATION_LATITUDE, 2.0,
      GST_TAG_GEO_LOCATION_ELEVATION, 3.0,
      GST_TAG_DEVICE_MANUFACTURER, "gst-camerabin-test manufacturer",
      GST_TAG_DEVICE_MODEL, "gst-camerabin-test model", NULL);

  g_free (desc_str);
  gst_date_time_unref (datetime);
}

static gboolean
run_pipeline (gpointer user_data)
{
  GstCaps *preview_caps = NULL;
  gchar *filename_str = NULL;
  GstElement *video_source = NULL;
  const gchar *filename_suffix;
  CaptureTiming *timing;

  g_object_set (camerabin, "mode", mode, NULL);

  if (preview_caps_name != NULL) {
    preview_caps = gst_caps_from_string (preview_caps_name);
    if (preview_caps) {
      g_object_set (camerabin, "preview-caps", preview_caps, NULL);
      GST_DEBUG ("Preview caps set");
    } else
      GST_DEBUG ("Preview caps set but could not create caps from string");
  }

  set_metadata (camerabin);

  /* Construct filename */
  if (mode == MODE_VIDEO)
    filename_suffix = ".mp4";
  else
    filename_suffix = ".jpg";
  filename_str =
      g_strdup_printf ("%s/test_%04u%s", filename->str, capture_count,
      filename_suffix);
  GST_DEBUG ("Setting filename: %s", filename_str);
  g_object_set (camerabin, "location", filename_str, NULL);
  g_free (filename_str);

  g_object_get (camerabin, "camera-source", &video_source, NULL);
  if (video_source) {
    if (GST_IS_ELEMENT (video_source) && GST_IS_PHOTOGRAPHY (video_source)) {
      /* Set GstPhotography interface options. If option not given as
         command-line parameter use default of the source element. */
      if (scene_mode != SCENE_MODE_NONE)
        g_object_set (video_source, "scene-mode", scene_mode, NULL);
      if (ev_compensation != EV_COMPENSATION_NONE)
        g_object_set (video_source, "ev-compensation", ev_compensation, NULL);
      if (aperture != APERTURE_NONE)
        g_object_set (video_source, "aperture", aperture, NULL);
      if (flash_mode != FLASH_MODE_NONE)
        g_object_set (video_source, "flash-mode", flash_mode, NULL);
      if (exposure != EXPOSURE_NONE)
        g_object_set (video_source, "exposure", exposure, NULL);
      if (iso_speed != ISO_SPEED_NONE)
        g_object_set (video_source, "iso-speed", iso_speed, NULL);
      if (wb_mode != WHITE_BALANCE_MODE_NONE)
        g_object_set (video_source, "white-balance-mode", wb_mode, NULL);
      if (color_mode != COLOR_TONE_MODE_NONE)
        g_object_set (video_source, "colour-tone-mode", color_mode, NULL);
    }
    g_object_unref (video_source);
  } else {
    video_source = gst_bin_get_by_name (GST_BIN (camerabin), "camerasrc");
    gst_object_unref (video_source);
  }
  g_object_set (camerabin, "zoom", zoom / 100.0f, NULL);

  capture_count++;

  timing = g_slice_new0 (CaptureTiming);
  capture_times = g_list_prepend (capture_times, timing);

  /* set pad probe to check when buffer leaves the camera source */
  if (mode == MODE_IMAGE) {
    GstPad *pad;

    pad = gst_element_get_static_pad (video_source, "imgsrc");
    camera_probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
        camera_src_get_timestamp_probe, NULL, NULL);

    gst_object_unref (pad);
  }
  timing->start_capture = gst_util_get_timestamp ();
  g_signal_emit_by_name (camerabin, "start-capture", 0);

  if (mode == MODE_VIDEO) {
    g_timeout_add ((capture_time * 1000), (GSourceFunc) stop_capture, NULL);
  }

  return FALSE;
}

static void
parse_target_values (void)
{
  gdouble startup = 0, change_mode = 0, shot_to_save = 0, shot_to_snapshot = 0;
  gdouble shot_to_shot = 0, preview_to_precapture = 0, shot_to_buffer = 0;

  if (performance_targets_str == NULL)
    return;

  /*
     startup time, change mode time, shot to save, shot to snapshot,
     shot to shot, preview to precapture, shot to buffer.
   */
  sscanf (performance_targets_str, "%lf,%lf,%lf,%lf,%lf,%lf,%lf",
      &startup, &change_mode, &shot_to_save,
      &shot_to_snapshot, &shot_to_shot, &preview_to_precapture,
      &shot_to_buffer);

  target_startup = (GstClockTime) (startup * GST_SECOND);
  target_change_mode = (GstClockTime) (change_mode * GST_SECOND);
  target_shot_to_save = (GstClockTime) (shot_to_save * GST_SECOND);
  target_shot_to_snapshot = (GstClockTime) (shot_to_snapshot * GST_SECOND);
  target_shot_to_shot = (GstClockTime) (shot_to_shot * GST_SECOND);
  target_preview_to_precapture =
      (GstClockTime) (preview_to_precapture * GST_SECOND);
  target_shot_to_buffer = (GstClockTime) (shot_to_buffer * GST_SECOND);
}

static void
print_performance_data (void)
{
  GList *iter;
  gint i = 0;
  GstClockTime last_start = 0;
  CaptureTimingStats max;
  CaptureTimingStats min;
  CaptureTimingStats avg;
  CaptureTimingStats avg_wo_first;
  GstClockTime shot_to_shot;

  if (!performance_measure)
    return;

  parse_target_values ();

  /* Initialize stats */
  min.shot_to_shot = -1;
  min.shot_to_save = -1;
  min.shot_to_snapshot = -1;
  min.preview_to_precapture = -1;
  min.shot_to_buffer = -1;
  memset (&avg, 0, sizeof (CaptureTimingStats));
  memset (&avg_wo_first, 0, sizeof (CaptureTimingStats));
  memset (&max, 0, sizeof (CaptureTimingStats));

  g_print ("-- Performance results --\n");
  g_print ("Startup time: %" TIME_FORMAT "; Target: %" TIME_FORMAT "\n",
      TIME_ARGS (startup_time - initial_time), TIME_ARGS (target_startup));
  g_print ("Change mode time: %" TIME_FORMAT "; Target: %" TIME_FORMAT "\n",
      TIME_ARGS (change_mode_after - change_mode_before),
      TIME_ARGS (target_change_mode));

  g_print
      ("\n   | Shot to save |Shot to snapshot| Shot to shot |"
      "Preview to precap| Shot to buffer\n");
  capture_times = g_list_reverse (capture_times);
  for (iter = capture_times; iter; iter = g_list_next (iter)) {
    CaptureTiming *t = (CaptureTiming *) iter->data;
    CaptureTimingStats stats;

    stats.shot_to_save = SHOT_TO_SAVE (t);
    stats.shot_to_snapshot = SHOT_TO_SNAPSHOT (t);
    stats.shot_to_shot = i == 0 ? 0 : t->start_capture - last_start;
    stats.preview_to_precapture = PREVIEW_TO_PRECAPTURE (t);
    stats.shot_to_buffer = SHOT_TO_BUFFER (t);

    PRINT_STATS (i, &stats);

    if (i != 0) {
      capture_timing_stats_add (&avg_wo_first, &stats);
    }
    capture_timing_stats_add (&avg, &stats);

    if (stats.shot_to_save < min.shot_to_save) {
      min.shot_to_save = stats.shot_to_save;
    }
    if (stats.shot_to_snapshot < min.shot_to_snapshot) {
      min.shot_to_snapshot = stats.shot_to_snapshot;
    }
    if (stats.shot_to_shot < min.shot_to_shot && stats.shot_to_shot > 0) {
      min.shot_to_shot = stats.shot_to_shot;
    }
    if (stats.preview_to_precapture < min.preview_to_precapture) {
      min.preview_to_precapture = stats.preview_to_precapture;
    }
    if (stats.shot_to_buffer < min.shot_to_buffer) {
      min.shot_to_buffer = stats.shot_to_buffer;
    }


    if (stats.shot_to_save > max.shot_to_save) {
      max.shot_to_save = stats.shot_to_save;
    }
    if (stats.shot_to_snapshot > max.shot_to_snapshot) {
      max.shot_to_snapshot = stats.shot_to_snapshot;
    }
    if (stats.shot_to_shot > max.shot_to_shot) {
      max.shot_to_shot = stats.shot_to_shot;
    }
    if (stats.preview_to_precapture > max.preview_to_precapture) {
      max.preview_to_precapture = stats.preview_to_precapture;
    }
    if (stats.shot_to_buffer > max.shot_to_buffer) {
      max.shot_to_buffer = stats.shot_to_buffer;
    }

    last_start = t->start_capture;
    i++;
  }

  if (i == 0)
    return;

  if (i > 1)
    shot_to_shot = avg.shot_to_shot / (i - 1);
  else
    shot_to_shot = GST_CLOCK_TIME_NONE;
  capture_timing_stats_div (&avg, i);
  avg.shot_to_shot = shot_to_shot;
  if (i > 1)
    capture_timing_stats_div (&avg_wo_first, i - 1);
  else {
    memset (&avg_wo_first, 0, sizeof (CaptureTimingStats));
  }

  g_print ("\n    Stats             |     MIN      |     MAX      |"
      "     AVG      | AVG wo First |   Target     | Diff \n");
  g_print ("Shot to shot          | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIMEDIFF_FORMAT "\n",
      TIME_ARGS (min.shot_to_shot), TIME_ARGS (max.shot_to_shot),
      TIME_ARGS (avg.shot_to_shot),
      TIME_ARGS (avg_wo_first.shot_to_shot),
      TIME_ARGS (target_shot_to_shot),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_shot, target_shot_to_shot)));
  g_print ("Shot to save          | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIMEDIFF_FORMAT "\n",
      TIME_ARGS (min.shot_to_save), TIME_ARGS (max.shot_to_save),
      TIME_ARGS (avg.shot_to_save),
      TIME_ARGS (avg_wo_first.shot_to_save),
      TIME_ARGS (target_shot_to_save),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_save, target_shot_to_save)));
  g_print ("Shot to snapshot      | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIMEDIFF_FORMAT "\n",
      TIME_ARGS (min.shot_to_snapshot),
      TIME_ARGS (max.shot_to_snapshot),
      TIME_ARGS (avg.shot_to_snapshot),
      TIME_ARGS (avg_wo_first.shot_to_snapshot),
      TIME_ARGS (target_shot_to_snapshot),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_snapshot,
              target_shot_to_snapshot)));
  g_print ("Preview to precapture | %" TIME_FORMAT " | %" TIME_FORMAT " | %"
      TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIMEDIFF_FORMAT
      "\n", TIME_ARGS (min.preview_to_precapture),
      TIME_ARGS (max.preview_to_precapture),
      TIME_ARGS (avg.preview_to_precapture),
      TIME_ARGS (avg_wo_first.preview_to_precapture),
      TIME_ARGS (target_preview_to_precapture),
      TIMEDIFF_ARGS (TIME_DIFF (avg.preview_to_precapture,
              target_preview_to_precapture)));
  g_print ("Shot to buffer        | %" TIME_FORMAT " | %" TIME_FORMAT " | %"
      TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIMEDIFF_FORMAT
      "\n", TIME_ARGS (min.shot_to_buffer), TIME_ARGS (max.shot_to_buffer),
      TIME_ARGS (avg.shot_to_buffer), TIME_ARGS (avg_wo_first.shot_to_buffer),
      TIME_ARGS (target_shot_to_buffer),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_buffer, target_shot_to_buffer)));
}

int
main (int argc, char *argv[])
{
  gchar *target_times = NULL;
  gchar *ev_option = NULL;
  gchar *fn_option = NULL;

  GOptionEntry options[] = {
    {"ev-compensation", '\0', 0, G_OPTION_ARG_STRING, &ev_option,
        "EV compensation for source element GstPhotography interface", NULL},
    {"aperture", '\0', 0, G_OPTION_ARG_INT, &aperture,
          "Aperture (size of lens opening) for source element GstPhotography interface",
        NULL},
    {"flash-mode", '\0', 0, G_OPTION_ARG_INT,
          &flash_mode,
        "Flash mode for source element GstPhotography interface", NULL},
    {"scene-mode", '\0', 0, G_OPTION_ARG_INT,
          &scene_mode,
        "Scene mode for source element GstPhotography interface", NULL},
    {"exposure", '\0', 0, G_OPTION_ARG_INT64,
          &exposure,
          "Exposure time (in ms) for source element GstPhotography interface",
        NULL},
    {"iso-speed", '\0', 0, G_OPTION_ARG_INT,
          &iso_speed,
        "ISO speed for source element GstPhotography interface", NULL},
    {"white-balance-mode", '\0', 0, G_OPTION_ARG_INT,
          &wb_mode,
        "White balance mode for source element GstPhotography interface", NULL},
    {"colour-tone-mode", '\0', 0, G_OPTION_ARG_INT,
          &color_mode,
        "Colour tone mode for source element GstPhotography interface", NULL},
    {"directory", '\0', 0, G_OPTION_ARG_STRING, &fn_option,
        "Directory for capture file(s) (default is current directory)", NULL},
    {"mode", '\0', 0, G_OPTION_ARG_INT, &mode,
        "Capture mode (default = 1 (image), 2 = video)", NULL},
    {"capture-time", '\0', 0, G_OPTION_ARG_INT,
          &capture_time,
        "Time to capture video in seconds (default = 10)", NULL},
    {"capture-total", '\0', 0, G_OPTION_ARG_INT, &capture_total,
        "Total number of captures to be done (default = 1)", NULL},
    {"zoom", '\0', 0, G_OPTION_ARG_INT, &zoom,
        "Zoom (100 = 1x (default), 200 = 2x etc.)", NULL},
    {"wrapper-source", '\0', 0, G_OPTION_ARG_STRING, &wrappersrc_name,
          "Camera source wrapper used for setting the video source (default is wrappercamerabinsrc)",
        NULL},
    {"video-source", '\0', 0, G_OPTION_ARG_STRING, &videosrc_name,
        "Video source used in still capture and video recording", NULL},
    {"video-device", '\0', 0, G_OPTION_ARG_STRING, &videodevice_name,
        "Video device to be set on the video source", NULL},
    {"audio-source", '\0', 0, G_OPTION_ARG_STRING, &audiosrc_name,
        "Audio source used in video recording", NULL},
    {"image-pp", '\0', 0, G_OPTION_ARG_STRING, &imagepp_name,
        "List of image post-processing elements separated with comma", NULL},
    {"viewfinder-sink", '\0', 0, G_OPTION_ARG_STRING, &vfsink_name,
        "Viewfinder sink (default = fakesink)", NULL},
    {"image-width", '\0', 0, G_OPTION_ARG_INT, &image_width,
        "Width for image capture", NULL},
    {"image-height", '\0', 0, G_OPTION_ARG_INT, &image_height,
        "Height for image capture", NULL},
    {"view-framerate-num", '\0', 0, G_OPTION_ARG_INT, &view_framerate_num,
        "Framerate numerator for viewfinder", NULL},
    {"view-framerate-den", '\0', 0, G_OPTION_ARG_INT, &view_framerate_den,
        "Framerate denominator for viewfinder", NULL},
    {"preview-caps", '\0', 0, G_OPTION_ARG_STRING, &preview_caps_name,
        "Preview caps (e.g. video/x-raw-rgb,width=320,height=240)", NULL},
    {"viewfinder-filter", '\0', 0, G_OPTION_ARG_STRING, &viewfinder_filter,
        "Filter to process all frames going to viewfinder sink", NULL},
    {"x-width", '\0', 0, G_OPTION_ARG_INT, &x_width,
        "X window width (default = 320)", NULL},
    {"x-height", '\0', 0, G_OPTION_ARG_INT, &x_height,
        "X window height (default = 240)", NULL},
    {"no-xwindow", '\0', 0, G_OPTION_ARG_NONE, &no_xwindow,
        "Do not create XWindow", NULL},
    {"encoding-target", '\0', 0, G_OPTION_ARG_STRING, &gep_targetname,
        "Video encoding target name", NULL},
    {"encoding-profile", '\0', 0, G_OPTION_ARG_STRING, &gep_profilename,
        "Video encoding profile name", NULL},
    {"encoding-profile-filename", '\0', 0, G_OPTION_ARG_STRING, &gep_filename,
        "Video encoding profile filename", NULL},
    {"image-capture-caps", '\0', 0,
          G_OPTION_ARG_STRING, &image_capture_caps_str,
        "Image capture caps (e.g. video/x-raw-rgb,width=640,height=480)", NULL},
    {"viewfinder-caps", '\0', 0, G_OPTION_ARG_STRING,
          &viewfinder_caps_str,
        "Viewfinder caps (e.g. video/x-raw-rgb,width=640,height=480)", NULL},
    {"video-capture-caps", '\0', 0,
          G_OPTION_ARG_STRING, &video_capture_caps_str,
        "Video capture caps (e.g. video/x-raw-rgb,width=640,height=480)", NULL},
    {"audio-capture-caps", '\0', 0,
          G_OPTION_ARG_STRING, &audio_capture_caps_str,
          "Audio capture caps (e.g. audio/x-raw-int,width=16,depth=16,rate=44100,channels=2)",
        NULL},
    {"performance-measure", '\0', 0,
          G_OPTION_ARG_NONE, &performance_measure,
          "If performance information should be printed at the end of execution",
        NULL},
    {"performance-targets", '\0', 0,
          G_OPTION_ARG_STRING, &performance_targets_str,
          "Comma separated list of doubles representing the target values in "
          "seconds. The order is: startup time, change mode time, shot to save"
          ", shot to snapshot, shot to shot, preview to shot, shot to buffer. "
          "e.g. 3.5,1.0,5.0,2.5,5.0,1.5,1.0",
        NULL},
    {"flags", '\0', 0, G_OPTION_ARG_STRING, &camerabin_flags,
        "camerabin element flags (default = 0)", NULL},
    {NULL}
  };

  GOptionContext *ctx;
  GError *err = NULL;

  ctx = g_option_context_new ("\n\ncamerabin command line test application.");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    g_clear_error (&err);
    exit (1);
  }
  g_option_context_free (ctx);

  GST_DEBUG_CATEGORY_INIT (camerabin_test, "camerabin-test", 0,
      "camerabin test");

  /* if we fail to create xwindow should we care? */
  if (!no_xwindow)
    create_host_window ();

  /* FIXME: error handling */
  if (ev_option != NULL)
    ev_compensation = strtod (ev_option, (char **) NULL);

  if (vfsink_name == NULL)
    vfsink_name = g_strdup ("fakesink");

  filename = g_string_new (fn_option);
  if (filename->len == 0)
    filename = g_string_append (filename, ".");

  /* init */
  if (setup_pipeline ()) {
    loop = g_main_loop_new (NULL, FALSE);
    g_idle_add ((GSourceFunc) run_pipeline, NULL);
    g_main_loop_run (loop);
    cleanup_pipeline ();
    g_main_loop_unref (loop);
  }

  /* performance */
  if (performance_measure) {
    print_performance_data ();
  }

  /* free */
  {
    GList *iter;

    for (iter = capture_times; iter; iter = g_list_next (iter)) {
      g_slice_free (CaptureTiming, iter->data);
    }
    g_list_free (capture_times);
  }

  g_string_free (filename, TRUE);
  g_free (ev_option);
  g_free (wrappersrc_name);
  g_free (videosrc_name);
  g_free (videodevice_name);
  g_free (audiosrc_name);
  g_free (imagepp_name);
  g_free (vfsink_name);
  g_free (target_times);
  g_free (gep_targetname);
  g_free (gep_profilename);
  g_free (gep_filename);
  g_free (performance_targets_str);

  if (window)
    XDestroyWindow (display, window);

  if (display)
    XCloseDisplay (display);

  return 0;
}
