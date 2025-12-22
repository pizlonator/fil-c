#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>

#include <string.h>

static GMainLoop *loop;
static GstElement *pipe1, *webrtc1, *webrtc2, *extra_src;
static GstBus *bus1;

#define SEND_SRC(pattern, pt) "videotestsrc is-live=true pattern=" pattern \
    " ! timeoverlay ! queue ! vp8enc ! rtpvp8pay ! queue ! capsfilter " \
    " caps=application/x-rtp,media=video,payload=" pt ",encoding-name=VP8"

static void
_element_message (GstElement * parent, GstMessage * msg)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:{
      GstElement *receive;
      GstPad *pad, *peer;

      g_print ("Got element EOS message from %s parent %s\n",
          GST_OBJECT_NAME (msg->src), GST_OBJECT_NAME (parent));

      receive = GST_ELEMENT (msg->src);

      pad = gst_element_get_static_pad (receive, "sink");
      peer = gst_pad_get_peer (pad);

      gst_bin_remove (GST_BIN (pipe1), receive);

      gst_pad_unlink (peer, pad);

      gst_object_unref (pad);
      gst_object_unref (peer);

      gst_element_set_state (receive, GST_STATE_NULL);
      break;
    }
    default:
      break;
  }
}

static gboolean
_bus_watch (GstBus * bus, GstMessage * msg, GstElement * pipe)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_ELEMENT (msg->src) == pipe) {
        GstState old, new, pending;

        gst_message_parse_state_changed (msg, &old, &new, &pending);

        {
          gchar *dump_name = g_strconcat ("state_changed-",
              gst_element_state_get_name (old), "_",
              gst_element_state_get_name (new), NULL);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (msg->src),
              GST_DEBUG_GRAPH_SHOW_ALL, dump_name);
          g_free (dump_name);
        }
      }
      break;
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg_info = NULL;

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");

      gst_message_parse_error (msg, &err, &dbg_info);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), err->message);
      g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
      g_error_free (err);
      g_free (dbg_info);
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_EOS:{
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "eos");
      g_print ("EOS received\n");
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_ELEMENT:{
      const GstStructure *s = gst_message_get_structure (msg);
      if (g_strcmp0 (gst_structure_get_name (s), "GstBinForwarded") == 0) {
        GstMessage *sub_msg;

        gst_structure_get (s, "message", GST_TYPE_MESSAGE, &sub_msg, NULL);
        _element_message (GST_ELEMENT (msg->src), sub_msg);
        gst_message_unref (sub_msg);
      }
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static void
_webrtc_pad_added (GstElement * webrtc, GstPad * new_pad, GstElement * pipe)
{
  GstElement *out;
  GstPad *sink;

  if (GST_PAD_DIRECTION (new_pad) != GST_PAD_SRC)
    return;

  out = gst_parse_bin_from_description ("queue ! rtpvp8depay ! vp8dec ! "
      "videoconvert ! queue ! xvimagesink", TRUE, NULL);
  gst_bin_add (GST_BIN (pipe), out);
  gst_element_sync_state_with_parent (out);

  sink = out->sinkpads->data;

  gst_pad_link (new_pad, sink);
}

static void
_on_answer_received (GstPromise * promise, gpointer user_data)
{
  GstWebRTCSessionDescription *answer = NULL;
  const GstStructure *reply;
  gchar *desc;

  g_assert (gst_promise_wait (promise) == GST_PROMISE_RESULT_REPLIED);
  reply = gst_promise_get_reply (promise);
  gst_structure_get (reply, "answer",
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
  gst_promise_unref (promise);
  desc = gst_sdp_message_as_text (answer->sdp);
  g_print ("Created answer:\n%s\n", desc);
  g_free (desc);

  /* this is one way to tell webrtcbin that we don't want to be notified when
   * this task is complete: set a NULL promise */
  g_signal_emit_by_name (webrtc1, "set-remote-description", answer, NULL);
  /* this is another way to tell webrtcbin that we don't want to be notified
   * when this task is complete: interrupt the promise */
  promise = gst_promise_new ();
  g_signal_emit_by_name (webrtc2, "set-local-description", answer, promise);
  gst_promise_interrupt (promise);
  gst_promise_unref (promise);

  gst_webrtc_session_description_free (answer);
}

static void
_on_offer_received (GstPromise * promise, gpointer user_data)
{
  GstWebRTCSessionDescription *offer = NULL;
  const GstStructure *reply;
  gchar *desc;

  g_assert (gst_promise_wait (promise) == GST_PROMISE_RESULT_REPLIED);
  reply = gst_promise_get_reply (promise);
  gst_structure_get (reply, "offer",
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
  gst_promise_unref (promise);
  desc = gst_sdp_message_as_text (offer->sdp);
  g_print ("Created offer:\n%s\n", desc);
  g_free (desc);

  g_signal_emit_by_name (webrtc1, "set-local-description", offer, NULL);
  g_signal_emit_by_name (webrtc2, "set-remote-description", offer, NULL);

  promise = gst_promise_new_with_change_func (_on_answer_received, user_data,
      NULL);
  g_signal_emit_by_name (webrtc2, "create-answer", NULL, promise);

  gst_webrtc_session_description_free (offer);
}

static void
_on_negotiation_needed (GstElement * element, gpointer user_data)
{
  GstPromise *promise;

  promise = gst_promise_new_with_change_func (_on_offer_received, user_data,
      NULL);
  g_signal_emit_by_name (webrtc1, "create-offer", NULL, promise);
}

static void
_on_ice_candidate (GstElement * webrtc, guint mlineindex, gchar * candidate,
    GstElement * other)
{
  g_signal_emit_by_name (other, "add-ice-candidate", mlineindex, candidate);
}

static gboolean
stream_change (gpointer data)
{
  if (!extra_src) {
    g_print ("Adding extra stream\n");
    extra_src = gst_parse_bin_from_description (SEND_SRC ("circular", "97"),
        TRUE, NULL);

    gst_element_set_locked_state (extra_src, TRUE);
    gst_bin_add (GST_BIN (pipe1), extra_src);
    gst_element_link (extra_src, webrtc1);
    gst_element_set_locked_state (extra_src, FALSE);
    gst_element_sync_state_with_parent (extra_src);
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe1),
        GST_DEBUG_GRAPH_SHOW_ALL, "add");
  } else {
    GstPad *pad, *peer;
    GstWebRTCRTPTransceiver *transceiver;

    g_print ("Removing extra stream\n");
    pad = gst_element_get_static_pad (extra_src, "src");
    peer = gst_pad_get_peer (pad);

    g_object_get (peer, "transceiver", &transceiver, NULL);
    /* Instead of removing the source, you can add a pad probe to block data
     * flow, and you can set this to SENDONLY later to switch this track from
     * inactive to sendonly, but this only works with non-gstreamer receivers
     * at present. */
    g_object_set (transceiver, "direction",
        GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, NULL);

    gst_element_set_locked_state (extra_src, TRUE);
    gst_element_set_state (extra_src, GST_STATE_NULL);
    gst_pad_unlink (pad, peer);
    gst_element_release_request_pad (webrtc1, peer);

    gst_object_unref (transceiver);
    gst_object_unref (peer);
    gst_object_unref (pad);

    gst_bin_remove (GST_BIN (pipe1), extra_src);
    extra_src = NULL;
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe1),
        GST_DEBUG_GRAPH_SHOW_ALL, "remove");
  }

  return G_SOURCE_CONTINUE;
}

int
main (int argc, char *argv[])
{
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  pipe1 = gst_parse_launch (SEND_SRC ("smpte", "96")
      " ! webrtcbin name=smpte bundle-policy=max-bundle "
      SEND_SRC ("ball", "96")
      " ! webrtcbin name=ball bundle-policy=max-bundle", NULL);
  g_object_set (pipe1, "message-forward", TRUE, NULL);
  bus1 = gst_pipeline_get_bus (GST_PIPELINE (pipe1));
  gst_bus_add_watch (bus1, (GstBusFunc) _bus_watch, pipe1);

  webrtc1 = gst_bin_get_by_name (GST_BIN (pipe1), "smpte");
  g_signal_connect (webrtc1, "on-negotiation-needed",
      G_CALLBACK (_on_negotiation_needed), NULL);
  g_signal_connect (webrtc1, "pad-added", G_CALLBACK (_webrtc_pad_added),
      pipe1);
  webrtc2 = gst_bin_get_by_name (GST_BIN (pipe1), "ball");
  g_signal_connect (webrtc2, "pad-added", G_CALLBACK (_webrtc_pad_added),
      pipe1);
  g_signal_connect (webrtc1, "on-ice-candidate",
      G_CALLBACK (_on_ice_candidate), webrtc2);
  g_signal_connect (webrtc2, "on-ice-candidate",
      G_CALLBACK (_on_ice_candidate), webrtc1);

  g_print ("Starting pipeline\n");
  gst_element_set_state (GST_ELEMENT (pipe1), GST_STATE_PLAYING);

  g_timeout_add_seconds (5, stream_change, NULL);

  g_main_loop_run (loop);

  gst_element_set_state (GST_ELEMENT (pipe1), GST_STATE_NULL);
  g_print ("Pipeline stopped\n");

  gst_object_unref (webrtc1);
  gst_object_unref (webrtc2);
  gst_bus_remove_watch (bus1);
  gst_object_unref (bus1);
  gst_object_unref (pipe1);

  gst_deinit ();

  return 0;
}
