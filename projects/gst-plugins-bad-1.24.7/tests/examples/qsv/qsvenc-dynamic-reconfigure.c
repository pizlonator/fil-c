/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
#include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <stdlib.h>
#include "../key-handler.h"

typedef enum
{
  RC_MODE_CBR,
  RC_MODE_VBR,
  RC_MODE_AVBR,
  RC_MODE_CQP,
} RcMode;

typedef enum
{
  CODEC_AVC,
  CODEC_HEVC,
  CODEC_VP9,
  CODEC_AV1,
} Codec;

static GMainLoop *loop = NULL;
static gint width = 640;
static gint height = 480;
static guint bitrate = 1000;
static guint max_bitrate = 2000;
static guint avbr_accuracy = 0;
static guint convergence = 0;
static RcMode rc_mode = RC_MODE_CBR;
static Codec codec = CODEC_AVC;
static guint qp_i = 24;
static guint qp_p = 24;
static guint qp_b = 24;
static guint max_qp = 51;

G_LOCK_DEFINE_STATIC (input_lock);

typedef struct
{
  GstElement *pipeline;
  GstElement *capsfilter;
  GstElement *encoder;
  gulong probe_id;

  gint prev_width;
  gint prev_height;
} TestCallbackData;


static void
print_keyboard_help (void)
{
  /* *INDENT-OFF* */
  static struct
  {
    const gchar *key_desc;
    const gchar *key_help;
  } key_controls[] = {
    {
    "q", "Quit"}, {
    "right arrow", "Increase Width"}, {
    "left arrow", "Decrease Width"}, {
    "up arrow", "Increase Height"}, {
    "down arrow", "Decrease Height"}, {
    ">", "Increase bitrate by 100 kbps"}, {
    "<", "Decrease bitrate by 100 kbps"}, {
    "]", "Increase max-bitrate by 100 kbps"}, {
    "[", "Decrease max-bitrate by 100 kbps"}, {
    "A", "Increase AVBR accuracy by 10 percent"}, {
    "a", "Decrease AVBR accuracy by 10 percent"}, {
    "C", "Increase AVBR convergence by 100 frame"}, {
    "c", "Decrease AVBR convergence by 100 frame"}, {
    "I", "Increase QP-I"}, {
    "i", "Decrease QP-I"}, {
    "P", "Increase QP-P"}, {
    "p", "Decrease QP-P"}, {
    "B", "Increase QP-B"}, {
    "b", "Decrease QP-B"}, {
    "k", "show keyboard shortcuts"}
  };
  /* *INDENT-ON* */

  guint i, chars_to_pad, desc_len, max_desc_len = 0;

  gst_print ("\n\n%s\n\n", "Keyboard controls:");

  for (i = 0; i < G_N_ELEMENTS (key_controls); ++i) {
    desc_len = g_utf8_strlen (key_controls[i].key_desc, -1);
    max_desc_len = MAX (max_desc_len, desc_len);
  }
  ++max_desc_len;

  for (i = 0; i < G_N_ELEMENTS (key_controls); ++i) {
    chars_to_pad = max_desc_len - g_utf8_strlen (key_controls[i].key_desc, -1);
    gst_print ("\t%s", key_controls[i].key_desc);
    gst_print ("%-*s: ", chars_to_pad, "");
    gst_print ("%s\n", key_controls[i].key_help);
  }
  gst_print ("\n");
}

static void
keyboard_cb (gchar input, gboolean is_ascii, gpointer user_data)
{
  TestCallbackData *data = (TestCallbackData *) user_data;

  G_LOCK (input_lock);

  if (!is_ascii) {
    switch (input) {
      case KB_ARROW_UP:
        height += 2;
        gst_println ("Increase height to %d", height);
        break;
      case KB_ARROW_DOWN:
        height -= 2;
        height = MAX (height, 16);
        gst_println ("Decrease height to %d", height);
        break;
      case KB_ARROW_LEFT:
        width -= 2;
        width = MAX (width, 16);
        gst_println ("Decrease width to %d", width);
        break;
      case KB_ARROW_RIGHT:
        width += 2;
        gst_println ("Increase width to %d", width);
        break;
      default:
        break;
    }
  } else {
    switch (input) {
      case 'k':
      case 'K':
        print_keyboard_help ();
        break;
      case 'q':
      case 'Q':
        gst_element_send_event (data->pipeline, gst_event_new_eos ());
        g_main_loop_quit (loop);
        break;
      case '>':
        if (rc_mode != RC_MODE_CQP) {
          bitrate += 100;
          bitrate = MIN (bitrate, 0xffff);

          if (rc_mode == RC_MODE_VBR)
            bitrate = MIN (bitrate, max_bitrate);
          gst_println ("Increase bitrate to %d", bitrate);
          g_object_set (data->encoder, "bitrate", bitrate, NULL);
        }
        break;
      case '<':
        if (rc_mode != RC_MODE_CQP) {
          bitrate -= 100;
          bitrate = MAX (bitrate, 100);

          if (rc_mode == RC_MODE_VBR)
            bitrate = MIN (bitrate, max_bitrate);
          gst_println ("Decrease bitrate to %d", bitrate);
          g_object_set (data->encoder, "bitrate", bitrate, NULL);
        }
        break;
      case ']':
        if (rc_mode == RC_MODE_VBR) {
          max_bitrate += 100;
          max_bitrate = MIN (max_bitrate, 0xffff);
          max_bitrate = MAX (max_bitrate, bitrate);
          gst_println ("Increase max-bitrate to %d", max_bitrate);
          g_object_set (data->encoder, "max-bitrate", max_bitrate, NULL);
        }
        break;
      case '[':
        if (rc_mode == RC_MODE_VBR) {
          max_bitrate -= 100;
          max_bitrate = MAX (max_bitrate, 100);
          max_bitrate = MAX (max_bitrate, bitrate);
          gst_println ("Decrease max-bitrate to %d", max_bitrate);
          g_object_set (data->encoder, "max-bitrate", max_bitrate, NULL);
        }
        break;
      case 'A':
        if (rc_mode == RC_MODE_AVBR && avbr_accuracy <= 900) {
          avbr_accuracy += 100;
          gst_println ("Increase AVBR accuracy to %d", avbr_accuracy);
          g_object_set (data->encoder, "avbr-accuracy", avbr_accuracy, NULL);
        }
        break;
      case 'a':
        if (rc_mode == RC_MODE_AVBR && avbr_accuracy >= 100) {
          avbr_accuracy -= 100;
          gst_println ("Decrease AVBR accuracy to %d", avbr_accuracy);
          g_object_set (data->encoder, "avbr-accuracy", avbr_accuracy, NULL);
        }
        break;
      case 'C':
        if (rc_mode == RC_MODE_AVBR && convergence < G_MAXINT16) {
          gst_println ("Increase AVBR Convergence to %d", convergence++);
          g_object_set (data->encoder, "avbr-convergence", convergence, NULL);
        }
        break;
      case 'c':
        if (rc_mode == RC_MODE_AVBR && convergence > 0) {
          gst_println ("Decrease AVBR Convergence to %d", convergence++);
          g_object_set (data->encoder, "avbr-convergence", convergence, NULL);
        }
        break;
      case 'I':
        if (rc_mode == RC_MODE_CQP && qp_i < max_qp) {
          gst_println ("Increase QP-I to %d", ++qp_i);
          g_object_set (data->encoder, "qp-i", qp_i, NULL);
        }
        break;
      case 'i':
        if (rc_mode == RC_MODE_CQP && qp_i > 0) {
          gst_println ("Decrease QP-I to %d", --qp_i);
          g_object_set (data->encoder, "qp-i", qp_i, NULL);
        }
        break;
      case 'P':
        if (rc_mode == RC_MODE_CQP && qp_p < max_qp) {
          gst_println ("Increase QP-P to %d", ++qp_p);
          g_object_set (data->encoder, "qp-p", qp_p, NULL);
        }
        break;
      case 'p':
        if (rc_mode == RC_MODE_CQP && qp_p > 0) {
          gst_println ("Decrease QP-P to %d", --qp_p);
          g_object_set (data->encoder, "qp-p", qp_p, NULL);
        }
        break;
      case 'B':
        if (rc_mode == RC_MODE_CQP && qp_b < max_qp && codec != CODEC_VP9) {
          gst_println ("Increase QP-B to %d", ++qp_b);
          g_object_set (data->encoder, "qp-b", qp_b, NULL);
        }
        break;
      case 'b':
        if (rc_mode == RC_MODE_CQP && qp_b > 0 && codec != CODEC_VP9) {
          gst_println ("Decrease QP-B to %d", --qp_b);
          g_object_set (data->encoder, "qp-b", qp_b, NULL);
        }
        break;
      default:
        break;
    }
  }

  G_UNLOCK (input_lock);
}

static gboolean
bus_msg (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      gst_printerrln ("ERROR %s", err->message);
      if (dbg != NULL)
        gst_printerrln ("ERROR debug information: %s", dbg);
      g_clear_error (&err);
      g_free (dbg);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static gboolean
check_qsvencoder_available (const gchar * encoder_name)
{
  gboolean ret = TRUE;
  GstElement *elem;

  elem = gst_element_factory_make (encoder_name, NULL);
  if (!elem) {
    gst_printerrln ("%s is not available", encoder_name);
    return FALSE;
  }

  if (gst_element_set_state (elem,
          GST_STATE_PAUSED) != GST_STATE_CHANGE_SUCCESS) {
    gst_printerrln ("cannot open device");
    ret = FALSE;
  }

  gst_element_set_state (elem, GST_STATE_NULL);
  gst_object_unref (elem);

  return ret;
}

static GstPadProbeReturn
resolution_change_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;
  TestCallbackData *data = (TestCallbackData *) user_data;

  G_LOCK (input_lock);

  if (GST_IS_BUFFER (GST_PAD_PROBE_INFO_DATA (info))) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);
    GstPad *peer = gst_pad_get_peer (pad);
    GstFlowReturn flow_ret = GST_FLOW_OK;

    ret = GST_PAD_PROBE_HANDLED;

    if (peer) {
      flow_ret = gst_pad_chain (peer, buffer);

      if (flow_ret != GST_FLOW_OK) {
        gst_pad_remove_probe (pad, data->probe_id);
        data->probe_id = 0;
      } else {
        if (data->prev_width != width || data->prev_height != height) {
          GstCaps *caps = NULL;
          gint next_width, next_height;

          next_width = width;
          next_height = height;

          g_object_get (data->capsfilter, "caps", &caps, NULL);
          caps = gst_caps_make_writable (caps);
          gst_caps_set_simple (caps,
              "width", G_TYPE_INT, next_width, "height", G_TYPE_INT,
              next_height, NULL);
          g_object_set (data->capsfilter, "caps", caps, NULL);
          gst_caps_unref (caps);

          data->prev_width = next_width;
          data->prev_height = next_height;
        }
      }
    }
  }

  G_UNLOCK (input_lock);

  return ret;
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *pipeline;
  GstElement *src, *capsfilter, *enc, *enc_queue, *dec, *parser, *queue, *sink;
  GstStateChangeReturn sret;
  GError *error = NULL;
  GOptionContext *option_ctx;
  GstCaps *caps;
  TestCallbackData data = { 0, };
  GstPad *pad;
  gchar *encoder_name = NULL;
  gchar *rate_control = NULL;
  gint bframes = 0;
  /* *INDENT-OFF* */
  GOptionEntry options[] = {
    {"encoder", 0, 0, G_OPTION_ARG_STRING, &encoder_name,
        "QSV video encoder element to test, default: qsvh264enc"},
    {"rate-control", 0, 0, G_OPTION_ARG_STRING, &rate_control,
        "Rate control method to test, default: cbr"},
    {"b-frames", 0, 0, G_OPTION_ARG_INT, &bframes,
        "Number of B frames between I and P frames, default: 0"},
    {NULL}
  };
  /* *INDENT-ON* */

#define MAKE_ELEMENT_AND_ADD(elem, name) G_STMT_START { \
  GstElement *_elem = gst_element_factory_make (name, NULL); \
  if (!_elem) { \
    gst_printerrln ("%s is not available", name); \
    exit (1); \
  } \
  gst_println ("Adding element %s", name); \
  elem = _elem; \
  gst_bin_add (GST_BIN (pipeline), elem); \
} G_STMT_END

  option_ctx =
      g_option_context_new ("QSV video encoder dynamic reconfigure example");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_set_help_enabled (option_ctx, TRUE);
  if (!g_option_context_parse (option_ctx, &argc, &argv, &error)) {
    gst_printerrln ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
    exit (1);
  }

  g_option_context_free (option_ctx);
  gst_init (NULL, NULL);

  if (!encoder_name)
    encoder_name = g_strdup ("qsvh264enc");
  if (!rate_control)
    rate_control = g_strdup ("cbr");

  if (g_strcmp0 (encoder_name, "qsvh264enc") == 0) {
    codec = CODEC_AVC;
  } else if (g_strcmp0 (encoder_name, "qsvh265enc") == 0) {
    codec = CODEC_HEVC;
  } else if (g_strcmp0 (encoder_name, "qsvvp9enc") == 0) {
    codec = CODEC_VP9;
    max_qp = 255;
    qp_i = 128;
    qp_p = 128;
  } else if (g_strcmp0 (encoder_name, "qsvav1enc") == 0) {
    codec = CODEC_AV1;
    max_qp = 255;
    qp_i = 128;
    qp_p = 128;
  } else {
    gst_printerrln ("Unexpected encoder %s", encoder_name);
    exit (1);
  }

  if (g_strcmp0 (rate_control, "cbr") == 0) {
    rc_mode = RC_MODE_CBR;
  } else if (g_strcmp0 (rate_control, "vbr") == 0) {
    rc_mode = RC_MODE_VBR;
  } else if (g_strcmp0 (rate_control, "avbr") == 0 && codec == CODEC_AVC) {
    rc_mode = RC_MODE_AVBR;
  } else if (g_strcmp0 (rate_control, "cqp") == 0) {
    rc_mode = RC_MODE_CQP;
  } else {
    gst_printerrln ("Unexpected rate-control method %s for encoder %s",
        rate_control, encoder_name);
    exit (1);
  }

  if (!check_qsvencoder_available (encoder_name)) {
    gst_printerrln ("Cannot load %s plugin", encoder_name);
    exit (1);
  }

  /* prepare the pipeline */
  loop = g_main_loop_new (NULL, FALSE);

  pipeline = gst_pipeline_new (NULL);

  MAKE_ELEMENT_AND_ADD (src, "videotestsrc");
  g_object_set (src, "pattern", 1, NULL);

  MAKE_ELEMENT_AND_ADD (capsfilter, "capsfilter");
  MAKE_ELEMENT_AND_ADD (enc, encoder_name);

  g_object_set (enc, "bitrate", bitrate, "max-bitrate", max_bitrate,
      "qp-i", qp_i, "qp-p", qp_p, "gop-size", 30, NULL);
  if (codec != CODEC_VP9 && codec != CODEC_AV1)
    g_object_set (enc, "qp-b", qp_b, NULL);

  gst_util_set_object_arg (G_OBJECT (enc), "rate-control", rate_control);

  MAKE_ELEMENT_AND_ADD (enc_queue, "queue");
  if (g_strrstr (encoder_name, "h265")) {
    if (bframes > 0)
      g_object_set (enc, "b-frames", bframes, NULL);
    if (rc_mode == RC_MODE_CBR || rc_mode == RC_MODE_VBR) {
      /* Disable HRD conformance for dynamic bitrate update */
      g_object_set (enc, "disable-hrd-conformance", TRUE, NULL);
    }

    MAKE_ELEMENT_AND_ADD (parser, "h265parse");
#ifdef G_OS_WIN32
    MAKE_ELEMENT_AND_ADD (dec, "d3d11h265dec");
#else
    MAKE_ELEMENT_AND_ADD (dec, "vah265dec");
#endif
  } else if (g_strrstr (encoder_name, "vp9")) {
    MAKE_ELEMENT_AND_ADD (parser, "vp9parse");
#ifdef G_OS_WIN32
    MAKE_ELEMENT_AND_ADD (dec, "d3d11vp9dec");
#else
    MAKE_ELEMENT_AND_ADD (dec, "vavp9dec");
#endif
  } else if (g_strrstr (encoder_name, "av1")) {
    MAKE_ELEMENT_AND_ADD (parser, "av1parse");
#ifdef G_OS_WIN32
    MAKE_ELEMENT_AND_ADD (dec, "d3d11av1dec");
#else
    MAKE_ELEMENT_AND_ADD (dec, "vaav1dec");
#endif
  } else {
    if (bframes > 0)
      g_object_set (enc, "b-frames", bframes, NULL);

    if (rc_mode == RC_MODE_CBR || rc_mode == RC_MODE_VBR) {
      /* Disable HRD conformance for dynamic bitrate update */
      g_object_set (enc, "disable-hrd-conformance", TRUE, NULL);
    }

    MAKE_ELEMENT_AND_ADD (parser, "h264parse");
#ifdef G_OS_WIN32
    MAKE_ELEMENT_AND_ADD (dec, "d3d11h264dec");
#else
    MAKE_ELEMENT_AND_ADD (dec, "vah264dec");
#endif
  }
  MAKE_ELEMENT_AND_ADD (queue, "queue");

#ifdef G_OS_WIN32
  MAKE_ELEMENT_AND_ADD (sink, "d3d11videosink");
#else
  MAKE_ELEMENT_AND_ADD (sink, "glimagesink");
#endif

  if (!gst_element_link_many (src, capsfilter, enc, enc_queue,
          parser, dec, queue, sink, NULL)) {
    gst_printerrln ("Failed to link element");
    exit (1);
  }

  caps = gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT,
      width, "height", G_TYPE_INT, height, NULL);
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);

  data.pipeline = pipeline;
  data.capsfilter = capsfilter;
  data.encoder = enc;

  pad = gst_element_get_static_pad (capsfilter, "src");
  data.probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) resolution_change_probe, &data, NULL);
  gst_object_unref (pad);
  data.prev_width = width;
  data.prev_height = height;

  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), bus_msg, &data);

  /* run the pipeline */
  sret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (sret == GST_STATE_CHANGE_FAILURE) {
    gst_printerrln ("Pipeline doesn't want to playing\n");
  } else {
    set_key_handler ((KeyInputCallback) keyboard_cb, &data);
    g_main_loop_run (loop);
    unset_key_handler ();
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));

  gst_object_unref (pipeline);
  g_main_loop_unref (loop);
  g_free (encoder_name);
  g_free (rate_control);

  return 0;
}
