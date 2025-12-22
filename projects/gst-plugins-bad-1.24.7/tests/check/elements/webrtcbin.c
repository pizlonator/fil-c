/* GStreamer
 *
 * Unit tests for webrtcbin
 *
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
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

#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/webrtc/webrtc.h>
#include <gst/rtp/rtp.h>
#include "../../../ext/webrtc/webrtcsdp.h"
#include "../../../ext/webrtc/webrtcsdp.c"
#include "../../../ext/webrtc/utils.h"
#include "../../../ext/webrtc/utils.c"

#define OPUS_RTP_CAPS(pt) "application/x-rtp,payload=" G_STRINGIFY(pt) ",encoding-name=OPUS,media=audio,clock-rate=48000,ssrc=(uint)3384078950"
#define VP8_RTP_CAPS(pt) "application/x-rtp,payload=" G_STRINGIFY(pt) ",encoding-name=VP8,media=video,clock-rate=90000,ssrc=(uint)3484078951"
#define H264_RTP_CAPS(pt) "application/x-rtp,payload=" G_STRINGIFY(pt) ",encoding-name=H264,media=video,clock-rate=90000,ssrc=(uint)3484078952"

#define TEST_IS_OFFER_ELEMENT(t, e) ((((t)->offerror == 1 && (e) == (t)->webrtc1) || ((t)->offerror == 2 && (e) == (t)->webrtc2)) ? TRUE : FALSE)
#define TEST_GET_OFFEROR(t) (TEST_IS_OFFER_ELEMENT(t, t->webrtc1) ? (t)->webrtc1 : t->webrtc2)
#define TEST_GET_ANSWERER(t) (TEST_IS_OFFER_ELEMENT(t, t->webrtc1) ? (t)->webrtc2 : t->webrtc1)

#define TEST_SDP_IS_LOCAL(t, e, d) ((TEST_IS_OFFER_ELEMENT (t, e) ^ ((d)->type == GST_WEBRTC_SDP_TYPE_OFFER)) == 0)

typedef enum
{
  STATE_NEW = 1,
  STATE_NEGOTIATION_NEEDED,
  STATE_OFFER_CREATED,
  STATE_OFFER_SET,
  STATE_ANSWER_CREATED,
  STATE_ANSWER_SET,
  STATE_EOS,
  STATE_ERROR,
  STATE_CUSTOM,
} TestState;

struct test_webrtc;
typedef void (*OnPadAdded) (struct test_webrtc * t, GstElement * element,
    GstPad * pad, gpointer user_data);

/* basic premise of this is that webrtc1 and webrtc2 are attempting to connect
 * to each other in various configurations */
struct test_webrtc
{
  GList *harnesses;
  GstTestClock *test_clock;
  GThread *thread;
  GMainLoop *loop;
  GstBus *bus1;
  GstBus *bus2;
  GstElement *webrtc1;
  GstElement *webrtc2;
  GMutex lock;
  GCond cond;
  GArray *states;
  guint offerror;
  gulong error_signal_handler_id;
  gpointer user_data;
  GDestroyNotify data_notify;
/* *INDENT-OFF* */
  void      (*on_negotiation_needed)    (struct test_webrtc * t,
                                         GstElement * element,
                                         gpointer user_data);
  gpointer negotiation_data;
  GDestroyNotify negotiation_notify;
  void      (*on_ice_candidate)         (struct test_webrtc * t,
                                         GstElement * element,
                                         guint mlineindex,
                                         gchar * candidate,
                                         GstElement * other,
                                         gpointer user_data);
  gpointer ice_candidate_data;
  GDestroyNotify ice_candidate_notify;
  void      (*on_offer_created)         (struct test_webrtc * t,
                                         GstElement * element,
                                         GstPromise * promise,
                                         gpointer user_data);
  GstWebRTCSessionDescription *offer_desc;
  guint offer_set_count;
  gpointer offer_data;
  GDestroyNotify offer_notify;
  void      (*on_offer_set)             (struct test_webrtc * t,
                                         GstElement * element,
                                         GstPromise * promise,
                                         gpointer user_data);
  gpointer offer_set_data;
  GDestroyNotify offer_set_notify;
  void      (*on_answer_created)        (struct test_webrtc * t,
                                         GstElement * element,
                                         GstPromise * promise,
                                         gpointer user_data);
  GstWebRTCSessionDescription *answer_desc;
  guint answer_set_count;
  gpointer answer_data;
  GDestroyNotify answer_notify;
  void      (*on_answer_set)            (struct test_webrtc * t,
                                         GstElement * element,
                                         GstPromise * promise,
                                         gpointer user_data);
  gpointer answer_set_data;
  GDestroyNotify answer_set_notify;
  void      (*on_prepare_data_channel)  (struct test_webrtc * t,
                                         GstElement * element,
                                         GObject * data_channel,
                                         gboolean is_local,
                                         gpointer user_data);

  void      (*on_data_channel)          (struct test_webrtc * t,
                                         GstElement * element,
                                         GObject *data_channel,
                                         gpointer user_data);
  gpointer data_channel_data;
  GDestroyNotify data_channel_notify;

  OnPadAdded on_pad_added;
  gpointer pad_added_data;
  GDestroyNotify pad_added_notify;
  void      (*bus_message)              (struct test_webrtc * t,
                                         GstBus * bus,
                                         GstMessage * msg,
                                         gpointer user_data);
  gpointer bus_data;
  GDestroyNotify bus_notify;
/* *INDENT-ON* */
};

static void
test_webrtc_signal_state_unlocked (struct test_webrtc *t, TestState state)
{
  GST_TRACE ("signal state 0x%x", state);
  g_array_append_val (t->states, state);
  g_cond_broadcast (&t->cond);
}

static void
test_webrtc_signal_state (struct test_webrtc *t, TestState state)
{
  g_mutex_lock (&t->lock);
  test_webrtc_signal_state_unlocked (t, state);
  g_mutex_unlock (&t->lock);
}

#if 0
static gboolean
test_webrtc_state_find_unlocked (struct test_webrtc *t, TestState state,
    guint * idx)
{
  guint i;
  for (i = 0; i < t->states->len; i++) {
    if (state == g_array_index (t->states, TestState, i))
      return TRUE;
  }

  return FALSE;
}
#endif
static void
_on_answer_set (GstPromise * promise, gpointer user_data)
{
  struct test_webrtc *t = user_data;
  GstElement *answerer = TEST_GET_ANSWERER (t);

  g_mutex_lock (&t->lock);
  if (++t->answer_set_count >= 2) {
    if (t->on_answer_set)
      t->on_answer_set (t, answerer, promise, t->answer_set_data);
    test_webrtc_signal_state_unlocked (t, STATE_ANSWER_SET);
    g_cond_broadcast (&t->cond);
  }
  gst_promise_unref (promise);
  g_mutex_unlock (&t->lock);
}

static void
_on_answer_received (GstPromise * promise, gpointer user_data)
{
  struct test_webrtc *t = user_data;
  GstElement *offeror = TEST_GET_OFFEROR (t);
  GstElement *answerer = TEST_GET_ANSWERER (t);
  const GstStructure *reply;
  GstWebRTCSessionDescription *answer = NULL;
  GError *error = NULL;

  reply = gst_promise_get_reply (promise);
  if (gst_structure_get (reply, "answer",
          GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL)) {
    gchar *desc = gst_sdp_message_as_text (answer->sdp);
    GST_INFO ("Created Answer: %s", desc);
    g_free (desc);
  } else if (gst_structure_get (reply, "error", G_TYPE_ERROR, &error, NULL)) {
    GST_INFO ("Creating answer resulted in error: %s", error->message);
  } else {
    g_assert_not_reached ();
  }

  g_mutex_lock (&t->lock);

  g_assert (t->answer_desc == NULL);
  t->answer_desc = answer;

  if (t->on_answer_created) {
    t->on_answer_created (t, answerer, promise, t->answer_data);
  }
  gst_promise_unref (promise);

  if (error)
    goto error;

  if (t->answer_desc) {
    promise = gst_promise_new_with_change_func (_on_answer_set, t, NULL);
    g_signal_emit_by_name (answerer, "set-local-description", t->answer_desc,
        promise);
    promise = gst_promise_new_with_change_func (_on_answer_set, t, NULL);
    g_signal_emit_by_name (offeror, "set-remote-description", t->answer_desc,
        promise);
  }

  test_webrtc_signal_state_unlocked (t, STATE_ANSWER_CREATED);
  g_mutex_unlock (&t->lock);
  return;

error:
  g_clear_error (&error);
  test_webrtc_signal_state_unlocked (t, STATE_ERROR);
  g_mutex_unlock (&t->lock);
  return;
}

static void
_on_offer_set (GstPromise * promise, gpointer user_data)
{
  struct test_webrtc *t = user_data;
  GstElement *offeror = TEST_GET_OFFEROR (t);

  g_mutex_lock (&t->lock);
  if (++t->offer_set_count >= 2) {
    if (t->on_offer_set)
      t->on_offer_set (t, offeror, promise, t->offer_set_data);
    test_webrtc_signal_state_unlocked (t, STATE_OFFER_SET);
    g_cond_broadcast (&t->cond);
  }
  gst_promise_unref (promise);
  g_mutex_unlock (&t->lock);
}

static void
_on_offer_received (GstPromise * promise, gpointer user_data)
{
  struct test_webrtc *t = user_data;
  GstElement *offeror = TEST_GET_OFFEROR (t);
  GstElement *answerer = TEST_GET_ANSWERER (t);
  const GstStructure *reply;
  GstWebRTCSessionDescription *offer = NULL;
  GError *error = NULL;

  reply = gst_promise_get_reply (promise);
  if (gst_structure_get (reply, "offer",
          GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL)) {
    gchar *desc = gst_sdp_message_as_text (offer->sdp);
    GST_INFO ("Created offer: %s", desc);
    g_free (desc);
  } else if (gst_structure_get (reply, "error", G_TYPE_ERROR, &error, NULL)) {
    GST_INFO ("Creating offer resulted in error: %s", error->message);
  } else {
    g_assert_not_reached ();
  }

  g_mutex_lock (&t->lock);

  g_assert (t->offer_desc == NULL);
  t->offer_desc = offer;

  if (t->on_offer_created) {
    t->on_offer_created (t, offeror, promise, t->offer_data);
  }
  gst_promise_unref (promise);

  if (error)
    goto error;

  test_webrtc_signal_state_unlocked (t, STATE_OFFER_CREATED);

  gst_object_ref (offeror);
  gst_object_ref (answerer);
  g_mutex_unlock (&t->lock);

  if (t->offer_desc) {
    promise = gst_promise_new_with_change_func (_on_offer_set, t, NULL);
    g_signal_emit_by_name (offeror, "set-local-description", t->offer_desc,
        promise);
    promise = gst_promise_new_with_change_func (_on_offer_set, t, NULL);
    g_signal_emit_by_name (answerer, "set-remote-description", t->offer_desc,
        promise);

    promise = gst_promise_new_with_change_func (_on_answer_received, t, NULL);
    g_signal_emit_by_name (answerer, "create-answer", NULL, promise);
  }

  gst_clear_object (&offeror);
  gst_clear_object (&answerer);

  return;

error:
  g_clear_error (&error);
  test_webrtc_signal_state_unlocked (t, STATE_ERROR);
  g_mutex_unlock (&t->lock);
  return;
}

static gboolean
_bus_watch (GstBus * bus, GstMessage * msg, struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_ELEMENT (msg->src) == t->webrtc1
          || GST_ELEMENT (msg->src) == t->webrtc2) {
        GstState old, new, pending;

        gst_message_parse_state_changed (msg, &old, &new, &pending);

        {
          gchar *dump_name =
              g_strconcat (GST_OBJECT_NAME (msg->src), "-state_changed-",
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

      {
        gchar *dump_name;
        dump_name = g_strconcat (GST_OBJECT_NAME (t->webrtc1), "-error", NULL);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (t->webrtc1),
            GST_DEBUG_GRAPH_SHOW_ALL, dump_name);
        g_free (dump_name);
        dump_name = g_strconcat (GST_OBJECT_NAME (t->webrtc2), "-error", NULL);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (t->webrtc2),
            GST_DEBUG_GRAPH_SHOW_ALL, dump_name);
        g_free (dump_name);
      }

      gst_message_parse_error (msg, &err, &dbg_info);
      GST_WARNING ("ERROR from element %s: %s",
          GST_OBJECT_NAME (msg->src), err->message);
      GST_WARNING ("Debugging info: %s", (dbg_info) ? dbg_info : "none");
      g_error_free (err);
      g_free (dbg_info);
      test_webrtc_signal_state_unlocked (t, STATE_ERROR);
      break;
    }
    case GST_MESSAGE_EOS:{
      {
        gchar *dump_name;
        dump_name = g_strconcat ("%s-eos", GST_OBJECT_NAME (t->webrtc1), NULL);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (t->webrtc1),
            GST_DEBUG_GRAPH_SHOW_ALL, dump_name);
        g_free (dump_name);
        dump_name = g_strconcat ("%s-eos", GST_OBJECT_NAME (t->webrtc2), NULL);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (t->webrtc2),
            GST_DEBUG_GRAPH_SHOW_ALL, dump_name);
        g_free (dump_name);
      }
      GST_INFO ("EOS received");
      test_webrtc_signal_state_unlocked (t, STATE_EOS);
      break;
    }
    default:
      break;
  }

  if (t->bus_message)
    t->bus_message (t, bus, msg, t->bus_data);
  g_mutex_unlock (&t->lock);

  return TRUE;
}


static void
on_channel_error_not_reached (GObject * channel, GError * error,
    gpointer user_data)
{
  g_assert_not_reached ();
}

static void on_message_string (GObject * channel, const gchar * str,
    struct test_webrtc *t);
static void on_message_data (GObject * channel, GBytes * data,
    struct test_webrtc *t);

static void
have_prepare_data_channel (struct test_webrtc *t,
    GstElement * element,
    GObject * data_channel, gboolean is_local, gpointer user_data)
{
  t->error_signal_handler_id =
      g_signal_connect (data_channel, "on-error",
      G_CALLBACK (on_channel_error_not_reached), NULL);
  g_signal_connect (data_channel, "on-message-string",
      G_CALLBACK (on_message_string), t);
  g_signal_connect (data_channel, "on-message-data",
      G_CALLBACK (on_message_data), t);
}

static void
_on_prepare_data_channel (GstElement * webrtc, GObject * data_channel,
    gboolean is_local, struct test_webrtc *t)
{
  /* We can't lock the test_webrtc mutex here because this callback might be
   * called from an already locked _on_data_channel thread. This is the case for
   * the test_data_channel_create_after_negotiate test. */
  if (t->on_prepare_data_channel)
    t->on_prepare_data_channel (t, webrtc, data_channel, is_local,
        t->data_channel_data);

}

static void
_on_negotiation_needed (GstElement * webrtc, struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  if (t->on_negotiation_needed)
    t->on_negotiation_needed (t, webrtc, t->negotiation_data);
  test_webrtc_signal_state_unlocked (t, STATE_NEGOTIATION_NEEDED);
  g_cond_broadcast (&t->cond);
  g_mutex_unlock (&t->lock);
}

static void
_on_ice_candidate (GstElement * webrtc, guint mlineindex, gchar * candidate,
    struct test_webrtc *t)
{
  GstElement *other;

  g_mutex_lock (&t->lock);
  other = webrtc == t->webrtc1 ? t->webrtc2 : t->webrtc1;

  if (t->on_ice_candidate)
    t->on_ice_candidate (t, webrtc, mlineindex, candidate, other,
        t->ice_candidate_data);

  g_signal_emit_by_name (other, "add-ice-candidate", mlineindex, candidate);
  g_mutex_unlock (&t->lock);
}

static void
_on_pad_added (GstElement * webrtc, GstPad * new_pad, struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  if (t->on_pad_added)
    t->on_pad_added (t, webrtc, new_pad, t->pad_added_data);
  g_mutex_unlock (&t->lock);
}

static void
_on_data_channel (GstElement * webrtc, GObject * data_channel,
    struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  if (t->on_data_channel)
    t->on_data_channel (t, webrtc, data_channel, t->data_channel_data);
  g_mutex_unlock (&t->lock);
}

static void
_pad_added_not_reached (struct test_webrtc *t, GstElement * element,
    GstPad * pad, gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_ice_candidate_not_reached (struct test_webrtc *t, GstElement * element,
    guint mlineindex, gchar * candidate, GstElement * other, gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_negotiation_not_reached (struct test_webrtc *t, GstElement * element,
    gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_bus_no_errors (struct test_webrtc *t, GstBus * bus, GstMessage * msg,
    gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg = NULL;

      gst_message_parse_error (msg, &err, &dbg);
      g_error ("ERROR from element %s: %s (Debugging info: %s)",
          GST_OBJECT_NAME (msg->src), err->message, (dbg) ? dbg : "none");
      g_error_free (err);
      g_free (dbg);
      g_assert_not_reached ();
      break;
    }
    default:
      break;
  }
}

static void
_offer_answer_not_reached (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_on_prepare_data_channel_not_reached (struct test_webrtc *t,
    GstElement * element, GObject * data_channel, gboolean is_local,
    gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_on_data_channel_not_reached (struct test_webrtc *t,
    GstElement * element, GObject * data_channel, gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_broadcast (struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  g_cond_broadcast (&t->cond);
  g_mutex_unlock (&t->lock);
}

static gboolean
_unlock_create_thread (GMutex * lock)
{
  g_mutex_unlock (lock);
  return G_SOURCE_REMOVE;
}

static gpointer
_bus_thread (struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  t->loop = g_main_loop_new (NULL, FALSE);
  g_idle_add ((GSourceFunc) _unlock_create_thread, &t->lock);
  g_cond_broadcast (&t->cond);

  g_main_loop_run (t->loop);

  g_mutex_lock (&t->lock);
  g_main_loop_unref (t->loop);
  t->loop = NULL;
  g_cond_broadcast (&t->cond);
  g_mutex_unlock (&t->lock);

  return NULL;
}

static void
element_added_disable_sync (GstBin * bin, GstBin * sub_bin,
    GstElement * element, gpointer user_data)
{
  GObjectClass *class = G_OBJECT_GET_CLASS (element);
  if (g_object_class_find_property (class, "async"))
    g_object_set (element, "async", FALSE, NULL);
  if (g_object_class_find_property (class, "sync"))
    g_object_set (element, "sync", FALSE, NULL);
}

static struct test_webrtc *
test_webrtc_new (void)
{
  struct test_webrtc *ret = g_new0 (struct test_webrtc, 1);

  ret->on_negotiation_needed = _negotiation_not_reached;
  ret->on_ice_candidate = _ice_candidate_not_reached;
  ret->on_pad_added = _pad_added_not_reached;
  ret->on_offer_created = _offer_answer_not_reached;
  ret->on_answer_created = _offer_answer_not_reached;
  ret->on_prepare_data_channel = _on_prepare_data_channel_not_reached;
  ret->on_data_channel = _on_data_channel_not_reached;
  ret->bus_message = _bus_no_errors;
  ret->offerror = 1;
  ret->error_signal_handler_id = -1;

  g_mutex_init (&ret->lock);
  g_cond_init (&ret->cond);

  ret->states = g_array_new (FALSE, TRUE, sizeof (TestState));

  ret->test_clock = GST_TEST_CLOCK (gst_test_clock_new ());

  ret->thread = g_thread_new ("test-webrtc", (GThreadFunc) _bus_thread, ret);

  g_mutex_lock (&ret->lock);
  while (!ret->loop)
    g_cond_wait (&ret->cond, &ret->lock);
  g_mutex_unlock (&ret->lock);

  ret->bus1 = gst_bus_new ();
  ret->bus2 = gst_bus_new ();
  gst_bus_add_watch (ret->bus1, (GstBusFunc) _bus_watch, ret);
  gst_bus_add_watch (ret->bus2, (GstBusFunc) _bus_watch, ret);
  ret->webrtc1 = gst_element_factory_make ("webrtcbin", NULL);
  ret->webrtc2 = gst_element_factory_make ("webrtcbin", NULL);
  fail_unless (ret->webrtc1 != NULL && ret->webrtc2 != NULL);

  gst_element_set_clock (ret->webrtc1, GST_CLOCK (ret->test_clock));
  gst_element_set_clock (ret->webrtc2, GST_CLOCK (ret->test_clock));

  gst_element_set_bus (ret->webrtc1, ret->bus1);
  gst_element_set_bus (ret->webrtc2, ret->bus2);

  g_signal_connect (ret->webrtc1, "deep-element-added",
      G_CALLBACK (element_added_disable_sync), NULL);
  g_signal_connect (ret->webrtc2, "deep-element-added",
      G_CALLBACK (element_added_disable_sync), NULL);
  g_signal_connect (ret->webrtc1, "on-negotiation-needed",
      G_CALLBACK (_on_negotiation_needed), ret);
  g_signal_connect (ret->webrtc2, "on-negotiation-needed",
      G_CALLBACK (_on_negotiation_needed), ret);
  g_signal_connect (ret->webrtc1, "on-ice-candidate",
      G_CALLBACK (_on_ice_candidate), ret);
  g_signal_connect (ret->webrtc2, "on-ice-candidate",
      G_CALLBACK (_on_ice_candidate), ret);
  g_signal_connect (ret->webrtc1, "on-data-channel",
      G_CALLBACK (_on_data_channel), ret);
  g_signal_connect (ret->webrtc2, "on-data-channel",
      G_CALLBACK (_on_data_channel), ret);
  g_signal_connect (ret->webrtc1, "prepare-data-channel",
      G_CALLBACK (_on_prepare_data_channel), ret);
  g_signal_connect (ret->webrtc2, "prepare-data-channel",
      G_CALLBACK (_on_prepare_data_channel), ret);
  g_signal_connect (ret->webrtc1, "pad-added", G_CALLBACK (_on_pad_added), ret);
  g_signal_connect (ret->webrtc2, "pad-added", G_CALLBACK (_on_pad_added), ret);
  g_signal_connect_swapped (ret->webrtc1, "notify::ice-gathering-state",
      G_CALLBACK (_broadcast), ret);
  g_signal_connect_swapped (ret->webrtc2, "notify::ice-gathering-state",
      G_CALLBACK (_broadcast), ret);
  g_signal_connect_swapped (ret->webrtc1, "notify::ice-connection-state",
      G_CALLBACK (_broadcast), ret);
  g_signal_connect_swapped (ret->webrtc2, "notify::ice-connection-state",
      G_CALLBACK (_broadcast), ret);

  return ret;
}

static void
test_webrtc_reset_negotiation (struct test_webrtc *t)
{
  GST_DEBUG ("resetting negotiation");
  if (t->offer_desc)
    gst_webrtc_session_description_free (t->offer_desc);
  t->offer_desc = NULL;
  t->offer_set_count = 0;
  if (t->answer_desc)
    gst_webrtc_session_description_free (t->answer_desc);
  t->answer_desc = NULL;
  t->answer_set_count = 0;

  test_webrtc_signal_state (t, STATE_NEGOTIATION_NEEDED);
}

static void
test_webrtc_free (struct test_webrtc *t)
{
  /* Otherwise while one webrtcbin is being destroyed, the other could
   * generate a signal that calls into the destroyed webrtcbin */
  g_signal_handlers_disconnect_by_data (t->webrtc1, t);
  g_signal_handlers_disconnect_by_data (t->webrtc2, t);

  g_main_loop_quit (t->loop);
  g_mutex_lock (&t->lock);
  while (t->loop)
    g_cond_wait (&t->cond, &t->lock);
  g_mutex_unlock (&t->lock);

  g_thread_join (t->thread);

  g_object_unref (t->test_clock);

  gst_bus_remove_watch (t->bus1);
  gst_bus_remove_watch (t->bus2);

  gst_bus_set_flushing (t->bus1, TRUE);
  gst_bus_set_flushing (t->bus2, TRUE);

  gst_object_unref (t->bus1);
  gst_object_unref (t->bus2);

  g_list_free_full (t->harnesses, (GDestroyNotify) gst_harness_teardown);

  if (t->data_notify)
    t->data_notify (t->user_data);
  if (t->negotiation_notify)
    t->negotiation_notify (t->negotiation_data);
  if (t->ice_candidate_notify)
    t->ice_candidate_notify (t->ice_candidate_data);
  if (t->offer_notify)
    t->offer_notify (t->offer_data);
  if (t->offer_set_notify)
    t->offer_set_notify (t->offer_set_data);
  if (t->answer_notify)
    t->answer_notify (t->answer_data);
  if (t->answer_set_notify)
    t->answer_set_notify (t->answer_set_data);
  if (t->pad_added_notify)
    t->pad_added_notify (t->pad_added_data);
  if (t->data_channel_notify)
    t->data_channel_notify (t->data_channel_data);

  fail_unless_equals_int (GST_STATE_CHANGE_SUCCESS,
      gst_element_set_state (t->webrtc1, GST_STATE_NULL));
  fail_unless_equals_int (GST_STATE_CHANGE_SUCCESS,
      gst_element_set_state (t->webrtc2, GST_STATE_NULL));

  test_webrtc_reset_negotiation (t);

  gst_object_unref (t->webrtc1);
  gst_object_unref (t->webrtc2);

  g_mutex_clear (&t->lock);
  g_cond_clear (&t->cond);

  g_array_free (t->states, TRUE);
  t->states = NULL;

  g_free (t);
}

static void
test_webrtc_create_offer (struct test_webrtc *t)
{
  GstPromise *promise;
  GstElement *offeror = TEST_GET_OFFEROR (t);

  promise = gst_promise_new_with_change_func (_on_offer_received, t, NULL);
  g_signal_emit_by_name (offeror, "create-offer", NULL, promise);
}

static TestState
test_webrtc_check_for_state_mask_unlocked (struct test_webrtc *t,
    TestState state)
{
  guint i;

  GST_LOG ("attempting to check for state mask 0x%x", state);
  for (i = 0; i < t->states->len; i++) {
    TestState val = g_array_index (t->states, TestState, i);

    if (((1 << val) & state) != 0) {
      GST_DEBUG ("found state 0x%x in wait mask 0x%x at idx %u", val, state, i);
      g_array_remove_range (t->states, 0, i + 1);
      return val;
    }
  }

  return 0;
}

static TestState
test_webrtc_check_for_state_mask (struct test_webrtc *t, TestState state)
{
  TestState ret;

  g_mutex_lock (&t->lock);
  ret = test_webrtc_check_for_state_mask_unlocked (t, state);
  g_mutex_unlock (&t->lock);

  return ret;
}

static TestState
test_webrtc_wait_for_state_mask (struct test_webrtc *t, TestState state)
{
  TestState ret = 0;

  g_mutex_lock (&t->lock);

  GST_LOG ("attempting to wait for state mask 0x%x", state);
  while (TRUE) {
    ret = test_webrtc_check_for_state_mask_unlocked (t, state);

    if (ret)
      break;

    g_cond_wait (&t->cond, &t->lock);
  }
  g_mutex_unlock (&t->lock);

  return ret;
}

static TestState
test_webrtc_wait_for_answer_error_eos (struct test_webrtc *t)
{
  TestState states = 0;
  states |= (1 << STATE_ANSWER_SET);
  states |= (1 << STATE_EOS);
  states |= (1 << STATE_ERROR);
  return test_webrtc_wait_for_state_mask (t, states);
}

static void
test_webrtc_wait_for_ice_gathering_complete (struct test_webrtc *t)
{
  GstWebRTCICEGatheringState ice_state1, ice_state2;
  g_mutex_lock (&t->lock);
  g_object_get (t->webrtc1, "ice-gathering-state", &ice_state1, NULL);
  g_object_get (t->webrtc2, "ice-gathering-state", &ice_state2, NULL);
  while (ice_state1 != GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE ||
      ice_state2 != GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE) {
    g_cond_wait (&t->cond, &t->lock);
    g_object_get (t->webrtc1, "ice-gathering-state", &ice_state1, NULL);
    g_object_get (t->webrtc2, "ice-gathering-state", &ice_state2, NULL);
  }
  g_mutex_unlock (&t->lock);
}

#if 0
static void
test_webrtc_wait_for_ice_connection (struct test_webrtc *t,
    GstWebRTCICEConnectionState states)
{
  GstWebRTCICEConnectionState ice_state1, ice_state2, current;
  g_mutex_lock (&t->lock);
  g_object_get (t->webrtc1, "ice-connection-state", &ice_state1, NULL);
  g_object_get (t->webrtc2, "ice-connection-state", &ice_state2, NULL);
  current = (1 << ice_state1) | (1 << ice_state2);
  while ((current & states) == 0 || (current & ~states)) {
    g_cond_wait (&t->cond, &t->lock);
    g_object_get (t->webrtc1, "ice-connection-state", &ice_state1, NULL);
    g_object_get (t->webrtc2, "ice-connection-state", &ice_state2, NULL);
    current = (1 << ice_state1) | (1 << ice_state2);
  }
  g_mutex_unlock (&t->lock);
}
#endif

static void
_pad_added_fakesink (struct test_webrtc *t, GstElement * element,
    GstPad * pad, gpointer user_data)
{
  GstHarness *h;

  if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC)
    return;

  h = gst_harness_new_with_element (element, NULL, "src_%u");
  gst_harness_add_sink_parse (h, "fakesink async=false sync=false");

  t->harnesses = g_list_prepend (t->harnesses, h);
}

static void
on_negotiation_needed_hit (struct test_webrtc *t, GstElement * element,
    gpointer user_data)
{
  guint *flag = (guint *) user_data;

  *flag |= 1 << ((element == t->webrtc1) ? 1 : 2);
}

typedef void (*ValidateSDPFunc) (struct test_webrtc * t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data);

struct validate_sdp;
struct validate_sdp
{
  ValidateSDPFunc validate;
  gpointer user_data;
  struct validate_sdp *next;
};

#define VAL_SDP_INIT(name,func,data,next) \
    struct validate_sdp name = { func, data, next }

static void
_check_validate_sdp (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  struct validate_sdp *validate = user_data;
  GstWebRTCSessionDescription *desc = NULL;

  if (TEST_IS_OFFER_ELEMENT (t, element))
    desc = t->offer_desc;
  else
    desc = t->answer_desc;

  while (validate) {
    validate->validate (t, element, desc, validate->user_data);
    validate = validate->next;
  }
}

static void
test_validate_sdp_full (struct test_webrtc *t, struct validate_sdp *offer,
    struct validate_sdp *answer, TestState wait_mask,
    gboolean perform_state_change)
{
  if (offer) {
    t->offer_data = offer;
    t->on_offer_created = _check_validate_sdp;
  } else {
    t->offer_data = NULL;
    t->on_offer_created = NULL;
  }
  if (answer) {
    t->answer_data = answer;
    t->on_answer_created = _check_validate_sdp;
  } else {
    t->answer_data = NULL;
    t->on_answer_created = NULL;
  }

  if (perform_state_change) {
    fail_if (gst_element_set_state (t->webrtc1,
            GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
    fail_if (gst_element_set_state (t->webrtc2,
            GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  }

  test_webrtc_create_offer (t);

  if (wait_mask == 0) {
    fail_unless_equals_int (test_webrtc_wait_for_answer_error_eos (t),
        STATE_ANSWER_SET);
  } else {
    test_webrtc_wait_for_state_mask (t, wait_mask);
  }
}

static void
test_validate_sdp (struct test_webrtc *t, struct validate_sdp *offer,
    struct validate_sdp *answer)
{
  test_validate_sdp_full (t, offer, answer, 0, TRUE);
}

static void
_count_num_sdp_media (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  guint expected = GPOINTER_TO_UINT (user_data);

  fail_unless_equals_int (gst_sdp_message_medias_len (desc->sdp), expected);
}

GST_START_TEST (test_sdp_no_media)
{
  struct test_webrtc *t = test_webrtc_new ();
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (0), NULL);

  /* check that a no stream connection creates 0 media sections */

  t->on_negotiation_needed = NULL;
  test_validate_sdp (t, &count, &count);

  test_webrtc_free (t);
}

GST_END_TEST;

static void
on_sdp_media_direction (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  gchar **expected_directions = user_data;
  int i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);

    if (g_strcmp0 (gst_sdp_media_get_media (media), "audio") == 0
        || g_strcmp0 (gst_sdp_media_get_media (media), "video") == 0) {
      gboolean have_direction = FALSE;
      int j;

      for (j = 0; j < gst_sdp_media_attributes_len (media); j++) {
        const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, j);

        if (g_strcmp0 (attr->key, "inactive") == 0) {
          fail_unless (have_direction == FALSE,
              "duplicate/multiple directions for media %u", i);
          have_direction = TRUE;
          fail_unless_equals_string (attr->key, expected_directions[i]);
        } else if (g_strcmp0 (attr->key, "sendonly") == 0) {
          fail_unless (have_direction == FALSE,
              "duplicate/multiple directions for media %u", i);
          have_direction = TRUE;
          fail_unless_equals_string (attr->key, expected_directions[i]);
        } else if (g_strcmp0 (attr->key, "recvonly") == 0) {
          fail_unless (have_direction == FALSE,
              "duplicate/multiple directions for media %u", i);
          have_direction = TRUE;
          fail_unless_equals_string (attr->key, expected_directions[i]);
        } else if (g_strcmp0 (attr->key, "sendrecv") == 0) {
          fail_unless (have_direction == FALSE,
              "duplicate/multiple directions for media %u", i);
          have_direction = TRUE;
          fail_unless_equals_string (attr->key, expected_directions[i]);
        }
      }
      fail_unless (have_direction, "no direction attribute in media %u", i);
    }
  }
}

static void
on_sdp_media_no_duplicate_payloads (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  int i, j, k;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);

    GArray *media_formats = g_array_new (FALSE, FALSE, sizeof (int));
    for (j = 0; j < gst_sdp_media_formats_len (media); j++) {
      int pt = atoi (gst_sdp_media_get_format (media, j));
      for (k = 0; k < media_formats->len; k++) {
        int val = g_array_index (media_formats, int, k);
        if (pt == val)
          fail ("found an unexpected duplicate payload type %u within media %u",
              pt, i);
      }
      g_array_append_val (media_formats, pt);
    }
    g_array_free (media_formats, TRUE);
  }
}

static void
on_sdp_media_count_formats (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  guint *expected_n_media_formats = user_data;
  int i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);
    fail_unless_equals_int (gst_sdp_media_formats_len (media),
        expected_n_media_formats[i]);
  }
}

static void
on_sdp_media_setup (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  gchar **expected_setup = user_data;
  int i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);
    gboolean have_setup = FALSE;
    int j;

    for (j = 0; j < gst_sdp_media_attributes_len (media); j++) {
      const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, j);

      if (g_strcmp0 (attr->key, "setup") == 0) {
        fail_unless (have_setup == FALSE,
            "duplicate/multiple setup for media %u", i);
        have_setup = TRUE;
        fail_unless_equals_string (attr->value, expected_setup[i]);
      }
    }
    fail_unless (have_setup, "no setup attribute in media %u", i);
  }
}

static void
add_fake_audio_src_harness (GstHarness * h, gint pt, guint ssrc)
{
  GstCaps *caps = gst_caps_from_string (OPUS_RTP_CAPS (pt));
  GstStructure *s = gst_caps_get_structure (caps, 0);
  if (ssrc != 0)
    gst_structure_set (s, "ssrc", G_TYPE_UINT, ssrc, NULL);
  gst_structure_set (s, "payload", G_TYPE_INT, pt, NULL);
  gst_harness_set_src_caps (h, caps);
  gst_harness_add_src_parse (h, "fakesrc is-live=true", TRUE);
}

static void
add_fake_video_src_harness (GstHarness * h, gint pt, guint ssrc)
{
  GstCaps *caps = gst_caps_from_string (VP8_RTP_CAPS (pt));
  GstStructure *s = gst_caps_get_structure (caps, 0);
  if (ssrc != 0)
    gst_structure_set (s, "ssrc", G_TYPE_UINT, ssrc, NULL);
  gst_structure_set (s, "payload", G_TYPE_INT, pt, NULL);
  gst_harness_set_src_caps (h, caps);
  gst_harness_add_src_parse (h, "fakesrc is-live=true", TRUE);
}

static struct test_webrtc *
create_audio_test (void)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstHarness *h;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;
  t->on_prepare_data_channel = have_prepare_data_channel;

  h = gst_harness_new_with_element (t->webrtc1, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  return t;
}

static void
on_new_transceiver_expected_kind (GstWebRTCBin * webrtc,
    GstWebRTCRTPTransceiver * trans, gpointer user_data)
{
  GstWebRTCKind kind, expected = GPOINTER_TO_UINT (user_data);

  g_object_get (trans, "kind", &kind, NULL);
  fail_unless_equals_int (kind, expected);
}

GST_START_TEST (test_audio)
{
  struct test_webrtc *t = create_audio_test ();
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &media_formats);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup, &count);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &count);
  const gchar *expected_offer_direction[] = { "sendrecv", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstWebRTCKind expected_kind = GST_WEBRTC_KIND_AUDIO;

  /* check that a single stream connection creates the associated number
   * of media sections */

  g_signal_connect (t->webrtc1, "on-new-transceiver",
      G_CALLBACK (on_new_transceiver_expected_kind),
      GUINT_TO_POINTER (expected_kind));
  g_signal_connect (t->webrtc2, "on-new-transceiver",
      G_CALLBACK (on_new_transceiver_expected_kind),
      GUINT_TO_POINTER (expected_kind));

  test_validate_sdp (t, &offer, &answer);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
_check_ice_port_restriction (struct test_webrtc *t, GstElement * element,
    guint mlineindex, gchar * candidate, GstElement * other, gpointer user_data)
{
  GRegex *regex;
  GMatchInfo *match_info;

  gchar *candidate_port;
  gchar *candidate_protocol;
  gchar *candidate_typ;
  guint port_as_int;
  guint peer_number;

  if (!candidate || candidate[0] == '\0')
    return;

  regex =
      g_regex_new ("candidate:(\\d+) (1) (UDP|TCP) (\\d+) ([0-9.]+|[0-9a-f:]+)"
      " (\\d+) typ ([a-z]+)", 0, 0, NULL);

  g_regex_match (regex, candidate, 0, &match_info);
  fail_unless (g_match_info_get_match_count (match_info) == 8, candidate);

  candidate_protocol = g_match_info_fetch (match_info, 2);
  candidate_port = g_match_info_fetch (match_info, 6);
  candidate_typ = g_match_info_fetch (match_info, 7);

  peer_number = t->webrtc1 == element ? 1 : 2;

  port_as_int = atoi (candidate_port);

  if (!g_strcmp0 (candidate_typ, "host") && port_as_int != 9) {
    guint expected_min = peer_number * 10000 + 1000;
    guint expected_max = expected_min + 999;

    fail_unless (port_as_int >= expected_min);
    fail_unless (port_as_int <= expected_max);
  }

  g_free (candidate_port);
  g_free (candidate_protocol);
  g_free (candidate_typ);
  g_match_info_free (match_info);
  g_regex_unref (regex);
}

GST_START_TEST (test_ice_port_restriction)
{
  struct test_webrtc *t = create_audio_test ();
  GObject *webrtcice;

  VAL_SDP_INIT (offer, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (answer, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);

  /*
   *  Ports are defined as follows "{peer}{protocol}000"
   *  - peer number: "1" for t->webrtc1, "2" for t->webrtc2
   */
  g_object_get (t->webrtc1, "ice-agent", &webrtcice, NULL);
  g_object_set (webrtcice, "min-rtp-port", 11000, "max-rtp-port", 11999, NULL);
  g_object_unref (webrtcice);

  g_object_get (t->webrtc2, "ice-agent", &webrtcice, NULL);
  g_object_set (webrtcice, "min-rtp-port", 21000, "max-rtp-port", 21999, NULL);
  g_object_unref (webrtcice);

  t->on_ice_candidate = _check_ice_port_restriction;
  test_validate_sdp (t, &offer, &answer);

  test_webrtc_wait_for_ice_gathering_complete (t);
  test_webrtc_free (t);
}

GST_END_TEST;

static struct test_webrtc *
create_audio_video_test (void)
{
  struct test_webrtc *t = create_audio_test ();
  GstHarness *h;

  h = gst_harness_new_with_element (t->webrtc1, "sink_1", NULL);
  add_fake_video_src_harness (h, 97, 0xBEEFDEAD);
  t->harnesses = g_list_prepend (t->harnesses, h);

  return t;
}

GST_START_TEST (test_audio_video)
{
  struct test_webrtc *t = create_audio_video_test ();
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup, &count);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &count);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);

  /* check that a dual stream connection creates the associated number
   * of media sections */

  test_validate_sdp (t, &offer, &answer);
  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_media_direction)
{
  struct test_webrtc *t = create_audio_video_test ();
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup, &count);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &count);

  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendrecv", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstHarness *h;

  /* check the default media directions for transceivers */

  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
on_sdp_media_payload_types (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  const GstSDPMedia *vmedia;
  guint video_mline = GPOINTER_TO_UINT (user_data);
  guint j;

  vmedia = gst_sdp_message_get_media (desc->sdp, video_mline);

  for (j = 0; j < gst_sdp_media_attributes_len (vmedia); j++) {
    const GstSDPAttribute *attr = gst_sdp_media_get_attribute (vmedia, j);

    if (!g_strcmp0 (attr->key, "rtpmap")) {
      if (g_str_has_prefix (attr->value, "97")) {
        fail_unless_equals_string (attr->value, "97 VP8/90000");
      } else if (g_str_has_prefix (attr->value, "96")) {
        fail_unless_equals_string (attr->value, "96 red/90000");
      } else if (g_str_has_prefix (attr->value, "98")) {
        fail_unless_equals_string (attr->value, "98 ulpfec/90000");
      } else if (g_str_has_prefix (attr->value, "99")) {
        fail_unless_equals_string (attr->value, "99 rtx/90000");
      } else if (g_str_has_prefix (attr->value, "100")) {
        fail_unless_equals_string (attr->value, "100 rtx/90000");
      } else if (g_str_has_prefix (attr->value, "101")) {
        fail_unless_equals_string (attr->value, "101 H264/90000");
      }
    }
  }
}

/* In this test we verify that webrtcbin will pick available payload
 * types when it needs to, in that example for RTX and FEC */
GST_START_TEST (test_payload_types)
{
  struct test_webrtc *t = create_audio_video_test ();
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1, 5, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (payloads, on_sdp_media_payload_types, GUINT_TO_POINTER (1),
      &media_formats);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2), &payloads);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup, &count);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  GstWebRTCRTPTransceiver *trans;
  GArray *transceivers;

  g_signal_emit_by_name (t->webrtc1, "get-transceivers", &transceivers);
  fail_unless_equals_int (transceivers->len, 2);
  trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 1);
  g_object_set (trans, "fec-type", GST_WEBRTC_FEC_TYPE_ULP_RED, "do-nack", TRUE,
      NULL);
  g_array_unref (transceivers);

  /* We don't really care about the answer here */
  test_validate_sdp (t, &offer, NULL);
  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_no_nice_elements_request_pad)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstPluginFeature *nicesrc, *nicesink;
  GstRegistry *registry;
  GstPad *pad;

  /* check that the absence of libnice elements posts an error on the bus
   * when requesting a pad */

  registry = gst_registry_get ();
  nicesrc = gst_registry_lookup_feature (registry, "nicesrc");
  nicesink = gst_registry_lookup_feature (registry, "nicesink");

  if (nicesrc)
    gst_registry_remove_feature (registry, nicesrc);
  if (nicesink)
    gst_registry_remove_feature (registry, nicesink);

  t->bus_message = NULL;

  pad = gst_element_request_pad_simple (t->webrtc1, "sink_0");
  fail_unless (pad == NULL);

  fail_unless_equals_int (STATE_ERROR,
      test_webrtc_wait_for_answer_error_eos (t));
  test_webrtc_free (t);

  if (nicesrc)
    gst_registry_add_feature (registry, nicesrc);
  if (nicesink)
    gst_registry_add_feature (registry, nicesink);
}

GST_END_TEST;

GST_START_TEST (test_no_nice_elements_state_change)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstPluginFeature *nicesrc, *nicesink;
  GstRegistry *registry;

  /* check that the absence of libnice elements posts an error on the bus */

  registry = gst_registry_get ();
  nicesrc = gst_registry_lookup_feature (registry, "nicesrc");
  nicesink = gst_registry_lookup_feature (registry, "nicesink");

  if (nicesrc)
    gst_registry_remove_feature (registry, nicesrc);
  if (nicesink)
    gst_registry_remove_feature (registry, nicesink);

  t->bus_message = NULL;
  gst_element_set_state (t->webrtc1, GST_STATE_READY);

  fail_unless_equals_int (STATE_ERROR,
      test_webrtc_wait_for_answer_error_eos (t));
  test_webrtc_free (t);

  if (nicesrc)
    gst_registry_add_feature (registry, nicesrc);
  if (nicesink)
    gst_registry_add_feature (registry, nicesink);
}

GST_END_TEST;

static void
validate_rtc_stats (const GstStructure * s)
{
  GstWebRTCStatsType type = 0;
  double ts = 0.;
  gchar *id = NULL;

  fail_unless (gst_structure_get (s, "type", GST_TYPE_WEBRTC_STATS_TYPE, &type,
          NULL));
  fail_unless (gst_structure_get (s, "id", G_TYPE_STRING, &id, NULL));
  fail_unless (gst_structure_get (s, "timestamp", G_TYPE_DOUBLE, &ts, NULL));
  fail_unless (type != 0);
  fail_unless (ts != 0.);
  fail_unless (id != NULL);

  g_free (id);
}

static void
validate_codec_stats (const GstStructure * s)
{
  guint pt = 0, clock_rate = 0;

  fail_unless (gst_structure_get (s, "payload-type", G_TYPE_UINT, &pt, NULL));
  fail_unless (gst_structure_get (s, "clock-rate", G_TYPE_UINT, &clock_rate,
          NULL));
  fail_unless (pt >= 0 && pt <= 127);
  fail_unless (clock_rate >= 0);
}

static void
validate_rtc_stream_stats (const GstStructure * s, const GstStructure * stats)
{
  gchar *codec_id, *transport_id, *kind;
  GstStructure *codec, *transport;

  fail_unless (gst_structure_get (s, "codec-id", G_TYPE_STRING, &codec_id,
          NULL));
  fail_unless (gst_structure_get (s, "transport-id", G_TYPE_STRING,
          &transport_id, NULL));

  fail_unless (gst_structure_get (stats, codec_id, GST_TYPE_STRUCTURE, &codec,
          NULL));
  fail_unless (gst_structure_get (stats, transport_id, GST_TYPE_STRUCTURE,
          &transport, NULL));

  fail_unless (codec != NULL);
  fail_unless (transport != NULL);

  gst_structure_free (transport);
  gst_structure_free (codec);

  fail_unless (gst_structure_get (s, "kind", G_TYPE_STRING, &kind, NULL));
  fail_unless (g_str_equal (kind, "audio") || g_str_equal (kind, "video"));

  g_free (codec_id);
  g_free (transport_id);
  g_free (kind);
}

static void
validate_inbound_rtp_stats (const GstStructure * s, const GstStructure * stats)
{
  guint ssrc, fir, pli, nack;
  gint64 packets_lost;
  guint64 packets_received, bytes_received;
  double jitter;
  gchar *remote_id;
  GstStructure *remote;

  validate_rtc_stream_stats (s, stats);

  fail_unless (gst_structure_get (s, "ssrc", G_TYPE_UINT, &ssrc, NULL));
  fail_unless (gst_structure_get (s, "fir-count", G_TYPE_UINT, &fir, NULL));
  fail_unless (gst_structure_get (s, "pli-count", G_TYPE_UINT, &pli, NULL));
  fail_unless (gst_structure_get (s, "nack-count", G_TYPE_UINT, &nack, NULL));
  fail_unless (gst_structure_get (s, "packets-received", G_TYPE_UINT64,
          &packets_received, NULL));
  fail_unless (gst_structure_get (s, "bytes-received", G_TYPE_UINT64,
          &bytes_received, NULL));
  fail_unless (gst_structure_get (s, "jitter", G_TYPE_DOUBLE, &jitter, NULL));
  fail_unless (gst_structure_get (s, "packets-lost", G_TYPE_INT64,
          &packets_lost, NULL));
  fail_unless (gst_structure_get (s, "remote-id", G_TYPE_STRING, &remote_id,
          NULL));
  fail_unless (gst_structure_get (stats, remote_id, GST_TYPE_STRUCTURE, &remote,
          NULL));
  fail_unless (remote != NULL);

  gst_structure_free (remote);
  g_free (remote_id);
}

static void
validate_remote_inbound_rtp_stats (const GstStructure * s,
    const GstStructure * stats)
{
  guint ssrc;
  gint64 packets_lost;
  double jitter, rtt;
  gchar *local_id;
  GstStructure *local;

  validate_rtc_stream_stats (s, stats);

  fail_unless (gst_structure_get (s, "ssrc", G_TYPE_UINT, &ssrc, NULL));
  fail_unless (gst_structure_get (s, "jitter", G_TYPE_DOUBLE, &jitter, NULL));
  fail_unless (gst_structure_get (s, "packets-lost", G_TYPE_INT64,
          &packets_lost, NULL));
  fail_unless (gst_structure_get (s, "round-trip-time", G_TYPE_DOUBLE, &rtt,
          NULL));
  fail_unless (gst_structure_get (s, "local-id", G_TYPE_STRING, &local_id,
          NULL));
  fail_unless (gst_structure_get (stats, local_id, GST_TYPE_STRUCTURE, &local,
          NULL));
  fail_unless (local != NULL);

  gst_structure_free (local);
  g_free (local_id);
}

static void
validate_outbound_rtp_stats (const GstStructure * s, const GstStructure * stats)
{
  guint ssrc, fir, pli, nack;
  guint64 packets_sent, bytes_sent;
  gchar *remote_id;
  GstStructure *remote;

  validate_rtc_stream_stats (s, stats);

  fail_unless (gst_structure_get (s, "ssrc", G_TYPE_UINT, &ssrc, NULL));
  fail_unless (gst_structure_get (s, "fir-count", G_TYPE_UINT, &fir, NULL));
  fail_unless (gst_structure_get (s, "pli-count", G_TYPE_UINT, &pli, NULL));
  fail_unless (gst_structure_get (s, "nack-count", G_TYPE_UINT, &nack, NULL));
  fail_unless (gst_structure_get (s, "packets-sent", G_TYPE_UINT64,
          &packets_sent, NULL));
  fail_unless (gst_structure_get (s, "bytes-sent", G_TYPE_UINT64, &bytes_sent,
          NULL));
  if (gst_structure_get (s, "remote-id", G_TYPE_STRING, &remote_id, NULL)) {
    fail_unless (gst_structure_get (stats, remote_id, GST_TYPE_STRUCTURE,
            &remote, NULL));
    fail_unless (remote != NULL);

    gst_structure_free (remote);
    g_free (remote_id);
  }
}

static void
validate_remote_outbound_rtp_stats (const GstStructure * s,
    const GstStructure * stats)
{
  guint ssrc;
  gchar *local_id;
  GstStructure *local;

  validate_rtc_stream_stats (s, stats);

  fail_unless (gst_structure_get (s, "ssrc", G_TYPE_UINT, &ssrc, NULL));
  fail_unless (gst_structure_get (s, "local-id", G_TYPE_STRING, &local_id,
          NULL));
  fail_unless (gst_structure_get (stats, local_id, GST_TYPE_STRUCTURE, &local,
          NULL));
  fail_unless (local != NULL);

  gst_structure_free (local);
  g_free (local_id);
}

static void
validate_candidate_stats (const GstStructure * s, const GstStructure * stats)
{
  guint port;
  guint64 priority;
  gchar *address, *candidateType, *protocol;

  fail_unless (gst_structure_get (s, "address", G_TYPE_STRING, &address, NULL));
  fail_unless (gst_structure_get (s, "port", G_TYPE_UINT, &port, NULL));
  fail_unless (gst_structure_get (s, "candidate-type", G_TYPE_STRING,
          &candidateType, NULL));
  fail_unless (gst_structure_get (s, "priority", G_TYPE_UINT64, &priority,
          NULL));
  fail_unless (gst_structure_get (s, "protocol", G_TYPE_STRING, &protocol,
          NULL));

  fail_unless (strcmp (protocol, "udp") || strcmp (protocol, "tcp"));

  g_free (address);
  g_free (candidateType);
  g_free (protocol);
}

static void
validate_peer_connection_stats (const GstStructure * s)
{
  guint opened, closed;

  fail_unless (gst_structure_get (s, "data-channels-opened", G_TYPE_UINT,
          &opened, NULL));
  fail_unless (gst_structure_get (s, "data-channels-closed", G_TYPE_UINT,
          &closed, NULL));
  fail_unless (opened >= closed);
}

static gboolean
validate_stats_foreach (GQuark field_id, const GValue * value,
    const GstStructure * stats)
{
  const gchar *field = g_quark_to_string (field_id);
  GstWebRTCStatsType type;
  const GstStructure *s;

  fail_unless (GST_VALUE_HOLDS_STRUCTURE (value));

  s = gst_value_get_structure (value);

  GST_INFO ("validating field %s %" GST_PTR_FORMAT, field, s);

  validate_rtc_stats (s);
  gst_structure_get (s, "type", GST_TYPE_WEBRTC_STATS_TYPE, &type, NULL);
  if (type == GST_WEBRTC_STATS_CODEC) {
    validate_codec_stats (s);
  } else if (type == GST_WEBRTC_STATS_INBOUND_RTP) {
    validate_inbound_rtp_stats (s, stats);
  } else if (type == GST_WEBRTC_STATS_OUTBOUND_RTP) {
    validate_outbound_rtp_stats (s, stats);
  } else if (type == GST_WEBRTC_STATS_REMOTE_INBOUND_RTP) {
    validate_remote_inbound_rtp_stats (s, stats);
  } else if (type == GST_WEBRTC_STATS_REMOTE_OUTBOUND_RTP) {
    validate_remote_outbound_rtp_stats (s, stats);
  } else if (type == GST_WEBRTC_STATS_CSRC) {
  } else if (type == GST_WEBRTC_STATS_PEER_CONNECTION) {
    validate_peer_connection_stats (s);
  } else if (type == GST_WEBRTC_STATS_DATA_CHANNEL) {
  } else if (type == GST_WEBRTC_STATS_STREAM) {
  } else if (type == GST_WEBRTC_STATS_TRANSPORT) {
  } else if (type == GST_WEBRTC_STATS_CANDIDATE_PAIR) {
  } else if (type == GST_WEBRTC_STATS_LOCAL_CANDIDATE) {
    validate_candidate_stats (s, stats);
  } else if (type == GST_WEBRTC_STATS_REMOTE_CANDIDATE) {
    validate_candidate_stats (s, stats);
  } else if (type == GST_WEBRTC_STATS_CERTIFICATE) {
  } else {
    g_assert_not_reached ();
  }

  return TRUE;
}

static void
validate_stats (const GstStructure * stats)
{
  gst_structure_foreach (stats,
      (GstStructureForeachFunc) validate_stats_foreach, (gpointer) stats);
}

static void
_on_stats (GstPromise * promise, gpointer user_data)
{
  struct test_webrtc *t = user_data;
  const GstStructure *reply = gst_promise_get_reply (promise);
  int i;

  validate_stats (reply);
  i = GPOINTER_TO_INT (t->user_data);
  i++;
  t->user_data = GINT_TO_POINTER (i);
  if (i >= 2)
    test_webrtc_signal_state (t, STATE_CUSTOM);

  gst_promise_unref (promise);
}

GST_START_TEST (test_session_stats)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstPromise *p;

  /* test that the stats generated without any streams are sane */
  t->on_negotiation_needed = NULL;
  test_validate_sdp (t, NULL, NULL);

  p = gst_promise_new_with_change_func (_on_stats, t, NULL);
  g_signal_emit_by_name (t->webrtc1, "get-stats", NULL, p);
  p = gst_promise_new_with_change_func (_on_stats, t, NULL);
  g_signal_emit_by_name (t->webrtc2, "get-stats", NULL, p);

  test_webrtc_wait_for_state_mask (t, 1 << STATE_CUSTOM);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_stats_with_stream)
{
  struct test_webrtc *t = create_audio_test ();
  GstPromise *p;
  GstCaps *caps;
  GstPad *pad;

  /* test that the stats generated with stream are sane */

  t->on_offer_created = NULL;
  t->on_answer_created = NULL;
  t->on_negotiation_needed = NULL;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  test_webrtc_create_offer (t);

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  /* set caps for webrtcbin sink to validate codec stats */
  caps = gst_caps_from_string (OPUS_RTP_CAPS (96));
  pad = gst_element_get_static_pad (t->webrtc1, "sink_0");
  gst_pad_set_caps (pad, caps);
  gst_caps_unref (caps);

  test_webrtc_wait_for_answer_error_eos (t);
  test_webrtc_signal_state (t, STATE_ANSWER_SET);

  p = gst_promise_new_with_change_func (_on_stats, t, NULL);
  g_signal_emit_by_name (t->webrtc1, "get-stats", NULL, p);
  p = gst_promise_new_with_change_func (_on_stats, t, NULL);
  g_signal_emit_by_name (t->webrtc2, "get-stats", NULL, p);

  test_webrtc_wait_for_state_mask (t, 1 << STATE_CUSTOM);

  gst_object_unref (pad);
  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_add_transceiver)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiverDirection direction, trans_direction;
  GstWebRTCRTPTransceiver *trans;

  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, NULL,
      &trans);
  fail_unless (trans != NULL);
  g_object_get (trans, "direction", &trans_direction, NULL);
  fail_unless_equals_int (direction, trans_direction);

  gst_object_unref (trans);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_get_transceivers)
{
  struct test_webrtc *t = create_audio_test ();
  GstWebRTCRTPTransceiver *trans;
  GArray *transceivers;

  g_signal_emit_by_name (t->webrtc1, "get-transceivers", &transceivers);
  fail_unless (transceivers != NULL);
  fail_unless_equals_int (1, transceivers->len);

  trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 0);
  fail_unless (trans != NULL);

  g_array_unref (transceivers);

  test_webrtc_free (t);
}

GST_END_TEST;

static void
on_sdp_media_check_mid (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  const char **mid = user_data;
  guint i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);
    gboolean seen_mid = FALSE;
    guint j;

    for (j = 0; j < gst_sdp_media_attributes_len (media); j++) {
      const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, j);

      if (g_strcmp0 (attr->key, "mid") == 0) {
        fail_unless (!seen_mid);
        seen_mid = TRUE;
        fail_unless_equals_string (attr->value, mid[i]);
      }
    }
  }
}

GST_START_TEST (test_add_recvonly_transceiver)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiverDirection direction;
  GstWebRTCRTPTransceiver *trans;
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1, 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &media_formats);
  const char *expected_mid[] = { "gst", };
  VAL_SDP_INIT (mid, on_sdp_media_check_mid, expected_mid, &count);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup, &mid);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup, &mid);
  const gchar *expected_offer_direction[] = { "recvonly", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstCaps *caps;
  GstHarness *h;

  /* add a transceiver that will only receive an opus stream and check that
   * the created offer is marked as recvonly */
  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  /* setup recvonly transceiver */
  caps = gst_caps_from_string (OPUS_RTP_CAPS (96) ", a-mid=(string)gst");
  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  gst_caps_unref (caps);
  fail_unless (trans != NULL);
  gst_object_unref (trans);

  /* setup sendonly peer */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);
  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_recvonly_sendonly)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiverDirection direction;
  GstWebRTCRTPTransceiver *trans;
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1, 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup, &count);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &count);
  const gchar *expected_offer_direction[] = { "recvonly", "sendonly" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstCaps *caps;
  GstHarness *h;
  GArray *transceivers;

  /* add a transceiver that will only receive an opus stream and check that
   * the created offer is marked as recvonly */
  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  /* setup recvonly transceiver */
  caps = gst_caps_from_string (OPUS_RTP_CAPS (96));
  gst_caps_set_simple (caps, "ssrc", G_TYPE_UINT, 0xDEADBEEF, NULL);
  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  gst_caps_unref (caps);
  fail_unless (trans != NULL);
  gst_object_unref (trans);

  /* setup sendonly stream */
  h = gst_harness_new_with_element (t->webrtc1, "sink_1", NULL);
  add_fake_audio_src_harness (h, 96, 0xBEEFDEAD);
  t->harnesses = g_list_prepend (t->harnesses, h);
  g_signal_emit_by_name (t->webrtc1, "get-transceivers", &transceivers);
  fail_unless (transceivers != NULL);
  fail_unless_equals_int (transceivers->len, 2);
  trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 1);
  g_object_set (trans, "direction",
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, NULL);

  g_array_unref (transceivers);

  /* setup sendonly peer */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

static void
on_sdp_has_datachannel (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  gboolean have_data_channel = FALSE;
  int i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    if (_message_media_is_datachannel (desc->sdp, i)) {
      /* there should only be one data channel m= section */
      fail_unless_equals_int (FALSE, have_data_channel);
      have_data_channel = TRUE;
    }
  }

  fail_unless_equals_int (TRUE, have_data_channel);
}

GST_START_TEST (test_data_channel_create)
{
  struct test_webrtc *t = test_webrtc_new ();
  GObject *channel = NULL;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);
  gchar *label;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = have_prepare_data_channel;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);
  g_assert_nonnull (channel);
  g_object_get (channel, "label", &label, NULL);
  g_assert_cmpstr (label, ==, "label");

  test_validate_sdp (t, &offer, &offer);

  g_object_unref (channel);
  g_free (label);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
signal_data_channel (struct test_webrtc *t,
    GstElement * element, GObject * our, gpointer user_data)
{
  test_webrtc_signal_state_unlocked (t, STATE_CUSTOM);
}

GST_START_TEST (test_data_channel_create_two_channels)
{
  struct test_webrtc *t = test_webrtc_new ();
  GObject *channel = NULL;
  GObject *channel2 = NULL;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);
  gchar *label;
  GstStructure *options = NULL;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = have_prepare_data_channel;
  t->on_data_channel = signal_data_channel;

  fail_if (gst_element_set_state (t->webrtc1, GST_STATE_READY) ==
      GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2, GST_STATE_READY) ==
      GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);
  g_assert_nonnull (channel);
  g_object_get (channel, "label", &label, NULL);
  g_assert_cmpstr (label, ==, "label");
  g_free (label);
  g_object_unref (channel);

  fail_if (gst_element_set_state (t->webrtc1, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE);

  /* Wait SCTP transport creation */
  test_validate_sdp_full (t, &offer, &offer, 1 << STATE_CUSTOM, FALSE);

  /* Create another channel on an existing SCTP transport, forcing an ID that
     should comply with the max-channels requiremennt, this should not raise a
     critical warning, the id is beneath the required limits. */
  options =
      gst_structure_new ("options", "id", G_TYPE_INT, 2, "negotiated",
      G_TYPE_BOOLEAN, TRUE, NULL);
  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label2", options,
      &channel2);
  gst_structure_free (options);
  g_assert_nonnull (channel2);
  g_object_get (channel2, "label", &label, NULL);
  g_assert_cmpstr (label, ==, "label2");
  g_free (label);
  g_object_unref (channel2);

  test_webrtc_free (t);
}

GST_END_TEST;

static void
have_data_channel (struct test_webrtc *t, GstElement * element,
    GObject * our, gpointer user_data)
{
  GObject *other = user_data;
  gchar *our_label, *other_label;

  g_assert_true (t->error_signal_handler_id > 0);

  g_object_get (our, "label", &our_label, NULL);
  g_object_get (other, "label", &other_label, NULL);

  g_assert_cmpstr (our_label, ==, other_label);

  g_free (our_label);
  g_free (other_label);

  test_webrtc_signal_state_unlocked (t, STATE_CUSTOM);
}

GST_START_TEST (test_data_channel_remote_notify)
{
  struct test_webrtc *t = test_webrtc_new ();
  GObject *channel = NULL;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = have_prepare_data_channel;
  t->on_data_channel = have_data_channel;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);
  g_assert_nonnull (channel);
  t->data_channel_data = channel;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  test_validate_sdp_full (t, &offer, &offer, 1 << STATE_CUSTOM, FALSE);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;

static const gchar *test_string = "GStreamer WebRTC is awesome!";

static void
on_message_string (GObject * channel, const gchar * str, struct test_webrtc *t)
{
  GstWebRTCDataChannelState state;
  gchar *expected;

  g_object_get (channel, "ready-state", &state, NULL);
  fail_unless_equals_int (GST_WEBRTC_DATA_CHANNEL_STATE_OPEN, state);

  expected = g_object_steal_data (channel, "expected");
  g_assert_cmpstr (expected, ==, str);
  g_free (expected);

  test_webrtc_signal_state (t, STATE_CUSTOM);
}

static void
have_data_channel_transfer_string (struct test_webrtc *t, GstElement * element,
    GObject * our, gpointer user_data)
{
  GObject *other = user_data;
  GstWebRTCDataChannelState state;
  GError *error = NULL;

  g_object_get (our, "ready-state", &state, NULL);
  fail_unless_equals_int (GST_WEBRTC_DATA_CHANNEL_STATE_OPEN, state);

  g_object_set_data_full (our, "expected", g_strdup (test_string), g_free);

  fail_unless (gst_webrtc_data_channel_send_string_full (GST_WEBRTC_DATA_CHANNEL
          (other), test_string, &error));
  g_assert_null (error);
}

GST_START_TEST (test_data_channel_transfer_string)
{
  struct test_webrtc *t = test_webrtc_new ();
  GObject *channel = NULL;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = have_prepare_data_channel;
  t->on_data_channel = have_data_channel_transfer_string;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);
  g_assert_nonnull (channel);
  t->data_channel_data = channel;
  g_signal_connect (channel, "on-error",
      G_CALLBACK (on_channel_error_not_reached), NULL);

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  test_validate_sdp_full (t, &offer, &offer, 1 << STATE_CUSTOM, FALSE);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;

#define g_assert_cmpbytes(b1, b2)                       \
    G_STMT_START {                                      \
      gsize l1, l2;                                     \
      const guint8 *d1 = g_bytes_get_data (b1, &l1);    \
      const guint8 *d2 = g_bytes_get_data (b2, &l2);    \
      g_assert_cmpmem (d1, l1, d2, l2);                 \
    } G_STMT_END;

static void
on_message_data (GObject * channel, GBytes * data, struct test_webrtc *t)
{
  GstWebRTCDataChannelState state;
  GBytes *expected;

  g_object_get (channel, "ready-state", &state, NULL);
  fail_unless_equals_int (GST_WEBRTC_DATA_CHANNEL_STATE_OPEN, state);

  expected = g_object_steal_data (channel, "expected");
  g_assert_cmpbytes (data, expected);
  g_bytes_unref (expected);

  test_webrtc_signal_state (t, STATE_CUSTOM);
}

static void
have_data_channel_transfer_data (struct test_webrtc *t, GstElement * element,
    GObject * our, gpointer user_data)
{
  GObject *other = user_data;
  GBytes *data = g_bytes_new_static (test_string, strlen (test_string));
  GstWebRTCDataChannelState state;
  GError *error = NULL;

  g_object_get (our, "ready-state", &state, NULL);
  fail_unless_equals_int (GST_WEBRTC_DATA_CHANNEL_STATE_OPEN, state);

  g_object_set_data_full (our, "expected", g_bytes_ref (data),
      (GDestroyNotify) g_bytes_unref);

  fail_unless (gst_webrtc_data_channel_send_data_full (GST_WEBRTC_DATA_CHANNEL
          (other), data, &error));
  g_assert_null (error);
  g_bytes_unref (data);
}

GST_START_TEST (test_data_channel_transfer_data)
{
  struct test_webrtc *t = test_webrtc_new ();
  GObject *channel = NULL;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = have_prepare_data_channel;
  t->on_data_channel = have_data_channel_transfer_data;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);
  g_assert_nonnull (channel);
  t->data_channel_data = channel;
  g_signal_connect (channel, "on-error",
      G_CALLBACK (on_channel_error_not_reached), NULL);

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  test_validate_sdp_full (t, &offer, &offer, 1 << STATE_CUSTOM, FALSE);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
have_data_channel_create_data_channel (struct test_webrtc *t,
    GstElement * element, GObject * our, gpointer user_data)
{
  GObject *another;

  t->on_prepare_data_channel = have_prepare_data_channel;
  t->on_data_channel = have_data_channel_transfer_string;

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &another);
  g_assert_nonnull (another);
  t->data_channel_data = another;
  t->data_channel_notify = (GDestroyNotify) g_object_unref;
}

GST_START_TEST (test_data_channel_create_after_negotiate)
{
  struct test_webrtc *t = test_webrtc_new ();
  GObject *channel = NULL;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = have_prepare_data_channel;
  t->on_data_channel = have_data_channel_create_data_channel;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "prev-label", NULL,
      &channel);
  g_assert_nonnull (channel);
  t->data_channel_data = channel;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  test_validate_sdp_full (t, &offer, &offer, 1 << STATE_CUSTOM, FALSE);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;

struct test_data_channel
{
  GObject *dc1;
  GObject *dc2;
  gint n_open;
  gint n_closed;
};

static void
have_data_channel_mark_open (struct test_webrtc *t,
    GstElement * element, GObject * our, gpointer user_data)
{
  struct test_data_channel *tdc = t->data_channel_data;

  tdc->dc2 = g_object_ref (our);
  if (g_atomic_int_add (&tdc->n_open, 1) == 1) {
    test_webrtc_signal_state_unlocked (t, STATE_CUSTOM);
  }
}

static gboolean
is_data_channel_open (GObject * channel)
{
  GstWebRTCDataChannelState ready_state = GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED;

  if (channel) {
    g_object_get (channel, "ready-state", &ready_state, NULL);
  }

  return ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_OPEN;
}

static void
on_data_channel_open (GObject * channel, GParamSpec * pspec,
    struct test_webrtc *t)
{
  struct test_data_channel *tdc = t->data_channel_data;

  if (is_data_channel_open (channel)) {
    if (g_atomic_int_add (&tdc->n_open, 1) == 1) {
      test_webrtc_signal_state (t, STATE_CUSTOM);
    }
  }
}

static void
on_data_channel_close (GObject * channel, GParamSpec * pspec,
    struct test_webrtc *t)
{
  struct test_data_channel *tdc = t->data_channel_data;
  GstWebRTCDataChannelState ready_state;

  g_object_get (channel, "ready-state", &ready_state, NULL);

  if (ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED) {
    g_atomic_int_add (&tdc->n_closed, 1);
  }
}

GST_START_TEST (test_data_channel_close)
{
#define NUM_CHANNELS 3
  struct test_webrtc *t = test_webrtc_new ();
  struct test_data_channel tdc = { NULL, };
  guint channel_id[NUM_CHANNELS] = { 0, 1, 2 };
  gulong sigid = 0;
  int i;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = have_prepare_data_channel;
  t->on_data_channel = have_data_channel_mark_open;
  t->data_channel_data = &tdc;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  /* open and close NUM_CHANNELS data channels to verify that we can reuse the
   * stream id of a previously closed data channel and that we have the same
   * behaviour no matter if we create the channel in READY or PLAYING state */
  for (i = 0; i < NUM_CHANNELS; i++) {
    GWeakRef dc1_ref, dc2_ref;
    tdc.n_open = 0;
    tdc.n_closed = 0;

    g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
        &tdc.dc1);
    g_assert_nonnull (tdc.dc1);
    g_weak_ref_init (&dc1_ref, tdc.dc1);
    sigid = g_signal_connect (tdc.dc1, "notify::ready-state",
        G_CALLBACK (on_data_channel_open), t);

    if (i == 0) {
      fail_if (gst_element_set_state (t->webrtc1,
              GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
      fail_if (gst_element_set_state (t->webrtc2,
              GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

      test_validate_sdp_full (t, &offer, &offer, 1 << STATE_CUSTOM, FALSE);
    }
    /* FIXME: Creating a data channel may result in "on-open" being sent
     * before we even had a chance to register the signal. For this test we
     * want to make sure that the channel is actually open before we try to
     * close it. So if we didn't receive the signal we fall back to a 1s
     * timeout where we explicitly check if both channels are open. */
    while (!is_data_channel_open (tdc.dc1)
        || !is_data_channel_open (tdc.dc2))
      g_usleep (100 * 1000);

    g_object_get (tdc.dc1, "id", &channel_id[i], NULL);

    g_signal_handler_disconnect (tdc.dc1, sigid);
    g_weak_ref_init (&dc2_ref, tdc.dc2);
    g_signal_connect (tdc.dc1, "notify::ready-state",
        G_CALLBACK (on_data_channel_close), t);
    g_signal_connect (tdc.dc2, "notify::ready-state",
        G_CALLBACK (on_data_channel_close), t);
    test_webrtc_signal_state (t, STATE_NEW);

    /* currently we assume there is no renegotiation if the last data channel is
     * removed but if it changes this test could be extended to verify both
     * the behaviour of removing the last channel as well as the behaviour when
     * there are still data channels remaining */
    t->on_negotiation_needed = _negotiation_not_reached;
    g_signal_emit_by_name (tdc.dc1, "close");

    /* XXX: try to do something better here */
    while (g_atomic_int_get (&tdc.n_closed) != 2)
      g_usleep (100 * 1000);

    g_clear_object (&tdc.dc1);
    g_clear_object (&tdc.dc2);

    /* XXX: try to do something better here */
    while (g_weak_ref_get (&dc1_ref) != NULL
        || g_weak_ref_get (&dc2_ref) != NULL)
      g_usleep (100 * 1000);

    g_weak_ref_clear (&dc1_ref);
    g_weak_ref_clear (&dc2_ref);

    test_webrtc_signal_state (t, STATE_NEW);
    test_webrtc_wait_for_state_mask (t, 1 << STATE_NEW);
  }

  /* verify the same stream id has been reused for each data channel */
  assert_equals_int (channel_id[0], channel_id[1]);
  assert_equals_int (channel_id[0], channel_id[2]);

  test_webrtc_free (t);
#undef NUM_CHANNELS
}

GST_END_TEST;

static void
on_buffered_amount_low_emitted (GObject * channel, struct test_webrtc *t)
{
  test_webrtc_signal_state (t, STATE_CUSTOM);
}

static void
have_data_channel_check_low_threshold_emitted (struct test_webrtc *t,
    GstElement * element, GObject * our, gpointer user_data)
{
  g_signal_connect (our, "on-buffered-amount-low",
      G_CALLBACK (on_buffered_amount_low_emitted), t);
  g_object_set (our, "buffered-amount-low-threshold", 1, NULL);

  g_signal_connect (our, "on-error", G_CALLBACK (on_channel_error_not_reached),
      NULL);
  gst_webrtc_data_channel_send_string_full (GST_WEBRTC_DATA_CHANNEL (our), "A",
      NULL);
}

GST_START_TEST (test_data_channel_low_threshold)
{
  struct test_webrtc *t = test_webrtc_new ();
  GObject *channel = NULL;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = NULL;
  t->on_data_channel = have_data_channel_check_low_threshold_emitted;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);
  g_assert_nonnull (channel);
  t->data_channel_data = channel;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  test_validate_sdp_full (t, &offer, &offer, 1 << STATE_CUSTOM, FALSE);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
have_data_channel_transfer_large_data (struct test_webrtc *t,
    GstElement * element, GObject * our, gpointer user_data)
{
  GObject *other = user_data;
  const gsize size = 1024 * 1024;
  guint8 *random_data = g_new (guint8, size);
  GBytes *data;
  gsize i;
  GError *error = NULL;

  for (i = 0; i < size; i++)
    random_data[i] = (guint8) (i & 0xff);

  data = g_bytes_new_with_free_func (random_data, size,
      (GDestroyNotify) g_free, random_data);

  g_object_set_data_full (our, "expected", g_bytes_ref (data),
      (GDestroyNotify) g_bytes_unref);
  g_signal_connect (our, "on-message-data", G_CALLBACK (on_message_data), t);

  g_signal_connect (other, "on-error",
      G_CALLBACK (on_channel_error_not_reached), NULL);
  fail_if (gst_webrtc_data_channel_send_data_full (GST_WEBRTC_DATA_CHANNEL
          (other), data, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  g_bytes_unref (data);

  test_webrtc_signal_state_unlocked (t, STATE_CUSTOM);
}

GST_START_TEST (test_data_channel_max_message_size)
{
  struct test_webrtc *t = test_webrtc_new ();
  GObject *channel = NULL;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = NULL;
  t->on_data_channel = have_data_channel_transfer_large_data;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);
  g_assert_nonnull (channel);
  t->data_channel_data = channel;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  test_validate_sdp_full (t, &offer, &offer, 1 << STATE_CUSTOM, FALSE);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
_on_ready_state_notify (GObject * channel, GParamSpec * pspec,
    struct test_webrtc *t)
{
  gint *n_ready = t->data_channel_data;
  GstWebRTCDataChannelState ready_state;

  g_object_get (channel, "ready-state", &ready_state, NULL);

  if (ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_OPEN) {
    if (g_atomic_int_add (n_ready, 1) >= 1) {
      test_webrtc_signal_state (t, STATE_CUSTOM);
    }
  }
}

GST_START_TEST (test_data_channel_pre_negotiated)
{
  struct test_webrtc *t = test_webrtc_new ();
  GObject *channel1 = NULL, *channel2 = NULL;
  VAL_SDP_INIT (media_count, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (offer, on_sdp_has_datachannel, NULL, &media_count);
  GstStructure *s;
  gint n_ready = 0;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_prepare_data_channel = have_prepare_data_channel;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  s = gst_structure_new ("application/data-channel", "negotiated",
      G_TYPE_BOOLEAN, TRUE, "id", G_TYPE_INT, 1, NULL);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", s,
      &channel1);
  g_assert_nonnull (channel1);
  g_signal_emit_by_name (t->webrtc2, "create-data-channel", "label", s,
      &channel2);
  g_assert_nonnull (channel2);

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  test_validate_sdp_full (t, &offer, &offer, 0, FALSE);

  t->data_channel_data = &n_ready;

  g_signal_connect (channel1, "notify::ready-state",
      G_CALLBACK (_on_ready_state_notify), t);
  g_signal_connect (channel2, "notify::ready-state",
      G_CALLBACK (_on_ready_state_notify), t);

  test_webrtc_wait_for_state_mask (t, 1 << STATE_CUSTOM);
  test_webrtc_signal_state (t, STATE_NEW);

  have_data_channel_transfer_string (t, t->webrtc1, channel1, channel2);

  test_webrtc_wait_for_state_mask (t, 1 << STATE_CUSTOM);

  g_object_unref (channel1);
  g_object_unref (channel2);
  gst_structure_free (s);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
_count_non_rejected_media (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * sd, gpointer user_data)
{
  guint expected = GPOINTER_TO_UINT (user_data);
  guint non_rejected_media;
  guint i;

  non_rejected_media = 0;

  for (i = 0; i < gst_sdp_message_medias_len (sd->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (sd->sdp, i);

    if (gst_sdp_media_get_port (media) != 0)
      non_rejected_media += 1;
  }

  fail_unless_equals_int (non_rejected_media, expected);
}

static void
_check_bundle_tag (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * sd, gpointer user_data)
{
  gchar **bundled = NULL;
  GStrv expected = user_data;
  guint i;

  fail_unless (_parse_bundle (sd->sdp, &bundled, NULL));

  if (!bundled) {
    fail_unless_equals_int (g_strv_length (expected), 0);
  } else {
    fail_unless_equals_int (g_strv_length (bundled), g_strv_length (expected));
  }

  for (i = 0; i < g_strv_length (expected); i++) {
    fail_unless (g_strv_contains ((const gchar **) bundled, expected[i]));
  }

  g_strfreev (bundled);
}

static void
_check_bundle_only_media (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * sd, gpointer user_data)
{
  gchar **expected_bundle_only = user_data;
  guint i;

  for (i = 0; i < gst_sdp_message_medias_len (sd->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (sd->sdp, i);
    const gchar *mid = gst_sdp_media_get_attribute_val (media, "mid");

    if (g_strv_contains ((const gchar **) expected_bundle_only, mid))
      fail_unless (_media_has_attribute_key (media, "bundle-only"));
  }
}

GST_START_TEST (test_bundle_audio_video_max_bundle_max_bundle)
{
  struct test_webrtc *t = create_audio_video_test ();
  const gchar *bundle[] = { "audio0", "video1", NULL };
  const gchar *offer_bundle_only[] = { "video1", NULL };
  const gchar *answer_bundle_only[] = { NULL };

  guint media_format_count[] = { 1, 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  VAL_SDP_INIT (bundle_tag, _check_bundle_tag, bundle, &payloads);
  VAL_SDP_INIT (offer_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (1), &bundle_tag);
  VAL_SDP_INIT (answer_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (2), &bundle_tag);
  VAL_SDP_INIT (offer_bundle, _check_bundle_only_media, &offer_bundle_only,
      &offer_non_reject);
  VAL_SDP_INIT (answer_bundle, _check_bundle_only_media, &answer_bundle_only,
      &answer_non_reject);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &offer_bundle);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &answer_bundle);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);

  /* We set a max-bundle policy on the offering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, and they should be marked
   * as bundle-only
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-bundle");
  /* We also set a max-bundle policy on the answering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, but need not be marked
   * as bundle-only.
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_bundle_audio_video_max_compat_max_bundle)
{
  struct test_webrtc *t = create_audio_video_test ();
  const gchar *bundle[] = { "audio0", "video1", NULL };
  const gchar *bundle_only[] = { NULL };

  guint media_format_count[] = { 1, 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (bundle_tag, _check_bundle_tag, bundle, &count);
  VAL_SDP_INIT (count_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (2), &bundle_tag);
  VAL_SDP_INIT (bundle_sdp, _check_bundle_only_media, &bundle_only,
      &count_non_reject);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &bundle_sdp);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &bundle_sdp);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);

  /* We set a max-compat policy on the offering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, and they should *not* be marked
   * as bundle-only
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-compat");
  /* We set a max-bundle policy on the answering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, but need not be marked
   * as bundle-only.
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_bundle_audio_video_max_bundle_none)
{
  struct test_webrtc *t = create_audio_video_test ();
  const gchar *offer_mid[] = { "audio0", "video1", NULL };
  const gchar *offer_bundle_only[] = { "video1", NULL };
  const gchar *answer_mid[] = { NULL };
  const gchar *answer_bundle_only[] = { NULL };

  guint media_format_count[] = { 1, 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  VAL_SDP_INIT (count_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (1), &payloads);
  VAL_SDP_INIT (offer_bundle_tag, _check_bundle_tag, offer_mid,
      &count_non_reject);
  VAL_SDP_INIT (answer_bundle_tag, _check_bundle_tag, answer_mid,
      &count_non_reject);
  VAL_SDP_INIT (offer_bundle, _check_bundle_only_media, &offer_bundle_only,
      &offer_bundle_tag);
  VAL_SDP_INIT (answer_bundle, _check_bundle_only_media, &answer_bundle_only,
      &answer_bundle_tag);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &offer_bundle);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &answer_bundle);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);

  /* We set a max-bundle policy on the offering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, and they should be marked
   * as bundle-only
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-bundle");
  /* We set a none policy on the answering webrtcbin,
   * this means that the answer should contain no bundled
   * medias, and as the bundle-policy of the offering webrtcbin
   * is set to max-bundle, only one media should be active.
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy", "none");

  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_bundle_audio_video_data)
{
  struct test_webrtc *t = create_audio_video_test ();
  const gchar *mids[] = { "audio0", "video1", "application2", NULL };
  const gchar *offer_bundle_only[] = { "video1", "application2", NULL };
  const gchar *answer_bundle_only[] = { NULL };
  GObject *channel = NULL;

  guint media_format_count[] = { 1, 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (3),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  VAL_SDP_INIT (bundle_tag, _check_bundle_tag, mids, &payloads);
  VAL_SDP_INIT (offer_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (1), &bundle_tag);
  VAL_SDP_INIT (answer_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (3), &bundle_tag);
  VAL_SDP_INIT (offer_bundle, _check_bundle_only_media, &offer_bundle_only,
      &offer_non_reject);
  VAL_SDP_INIT (answer_bundle, _check_bundle_only_media, &answer_bundle_only,
      &answer_non_reject);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &offer_bundle);
  const gchar *expected_answer_setup[] = { "active", "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &answer_bundle);
  const gchar *expected_offer_direction[] =
      { "sendrecv", "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] =
      { "recvonly", "recvonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);

  /* We set a max-bundle policy on the offering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, and they should be marked
   * as bundle-only
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-bundle");
  /* We also set a max-bundle policy on the answering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, but need not be marked
   * as bundle-only.
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);

  test_validate_sdp (t, &offer, &answer);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_duplicate_nego)
{
  struct test_webrtc *t = create_audio_video_test ();
  guint media_format_count[] = { 1, 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendrecv", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstHarness *h;
  guint negotiation_flag = 0;

  /* check that negotiating twice succeeds */

  t->on_negotiation_needed = on_negotiation_needed_hit;
  t->negotiation_data = &negotiation_flag;

  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);
  fail_unless (negotiation_flag & (1 << 2));

  test_webrtc_reset_negotiation (t);
  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_dual_audio)
{
  struct test_webrtc *t = create_audio_test ();
  guint media_format_count[] = { 1, 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendrecv", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstHarness *h;
  GstWebRTCRTPTransceiver *trans;
  GArray *transceivers;
  guint mline;

  /* test that each mline gets a unique transceiver even with the same caps */

  h = gst_harness_new_with_element (t->webrtc1, "sink_1", NULL);
  add_fake_audio_src_harness (h, 96, 0xBEEFDEAD);
  t->harnesses = g_list_prepend (t->harnesses, h);

  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  t->on_negotiation_needed = NULL;
  test_validate_sdp (t, &offer, &answer);

  g_signal_emit_by_name (t->webrtc1, "get-transceivers", &transceivers);
  fail_unless (transceivers != NULL);
  fail_unless_equals_int (2, transceivers->len);

  trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 0);
  fail_unless (trans != NULL);
  g_object_get (trans, "mlineindex", &mline, NULL);
  fail_unless_equals_int (mline, 0);

  trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 1);
  fail_unless (trans != NULL);
  g_object_get (trans, "mlineindex", &mline, NULL);
  fail_unless_equals_int (mline, 1);

  g_array_unref (transceivers);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
sdp_increasing_session_version (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  GstWebRTCSessionDescription *previous;
  const GstSDPOrigin *our_origin, *previous_origin;
  const gchar *prop;
  guint64 our_v, previous_v;

  prop =
      TEST_SDP_IS_LOCAL (t, element,
      desc) ? "current-local-description" : "current-remote-description";
  g_object_get (element, prop, &previous, NULL);

  our_origin = gst_sdp_message_get_origin (desc->sdp);
  previous_origin = gst_sdp_message_get_origin (previous->sdp);

  our_v = g_ascii_strtoull (our_origin->sess_version, NULL, 10);
  previous_v = g_ascii_strtoull (previous_origin->sess_version, NULL, 10);

  ck_assert_int_lt (previous_v, our_v);

  gst_webrtc_session_description_free (previous);
}

static void
sdp_equal_session_id (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  GstWebRTCSessionDescription *previous;
  const GstSDPOrigin *our_origin, *previous_origin;
  const gchar *prop;

  prop =
      TEST_SDP_IS_LOCAL (t, element,
      desc) ? "current-local-description" : "current-remote-description";
  g_object_get (element, prop, &previous, NULL);

  our_origin = gst_sdp_message_get_origin (desc->sdp);
  previous_origin = gst_sdp_message_get_origin (previous->sdp);

  fail_unless_equals_string (previous_origin->sess_id, our_origin->sess_id);
  gst_webrtc_session_description_free (previous);
}

static void
sdp_media_equal_attribute (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, GstWebRTCSessionDescription * previous,
    const gchar * attr)
{
  guint i, n;

  n = MIN (gst_sdp_message_medias_len (previous->sdp),
      gst_sdp_message_medias_len (desc->sdp));

  for (i = 0; i < n; i++) {
    const GstSDPMedia *our_media, *other_media;
    const gchar *our_mid, *other_mid;

    our_media = gst_sdp_message_get_media (desc->sdp, i);
    other_media = gst_sdp_message_get_media (previous->sdp, i);

    our_mid = gst_sdp_media_get_attribute_val (our_media, attr);
    other_mid = gst_sdp_media_get_attribute_val (other_media, attr);

    fail_unless_equals_string (our_mid, other_mid);
  }
}

static void
sdp_media_equal_mid (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  GstWebRTCSessionDescription *previous;
  const gchar *prop;

  prop =
      TEST_SDP_IS_LOCAL (t, element,
      desc) ? "current-local-description" : "current-remote-description";
  g_object_get (element, prop, &previous, NULL);

  sdp_media_equal_attribute (t, element, desc, previous, "mid");

  gst_webrtc_session_description_free (previous);
}

static void
sdp_media_equal_ice_params (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  GstWebRTCSessionDescription *previous;
  const gchar *prop;

  prop =
      TEST_SDP_IS_LOCAL (t, element,
      desc) ? "current-local-description" : "current-remote-description";
  g_object_get (element, prop, &previous, NULL);

  sdp_media_equal_attribute (t, element, desc, previous, "ice-ufrag");
  sdp_media_equal_attribute (t, element, desc, previous, "ice-pwd");

  gst_webrtc_session_description_free (previous);
}

static void
sdp_media_equal_fingerprint (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  GstWebRTCSessionDescription *previous;
  const gchar *prop;

  prop =
      TEST_SDP_IS_LOCAL (t, element,
      desc) ? "current-local-description" : "current-remote-description";
  g_object_get (element, prop, &previous, NULL);

  sdp_media_equal_attribute (t, element, desc, previous, "fingerprint");

  gst_webrtc_session_description_free (previous);
}

GST_START_TEST (test_renego_add_stream)
{
  struct test_webrtc *t = create_audio_video_test ();
  guint media_format_count[] = { 1, 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] =
      { "sendrecv", "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] =
      { "sendrecv", "recvonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  VAL_SDP_INIT (renego_mid, sdp_media_equal_mid, NULL, NULL);
  VAL_SDP_INIT (renego_ice_params, sdp_media_equal_ice_params, NULL,
      &renego_mid);
  VAL_SDP_INIT (renego_sess_id, sdp_equal_session_id, NULL, &renego_ice_params);
  VAL_SDP_INIT (renego_sess_ver, sdp_increasing_session_version, NULL,
      &renego_sess_id);
  VAL_SDP_INIT (renego_fingerprint, sdp_media_equal_fingerprint, NULL,
      &renego_sess_ver);
  GstHarness *h;

  /* negotiate an AV stream and then renegotiate an extra stream */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);

  h = gst_harness_new_with_element (t->webrtc1, "sink_2", NULL);
  add_fake_audio_src_harness (h, 98, 0xBEEFFFFF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  media_formats.next = &renego_fingerprint;
  count.user_data = GUINT_TO_POINTER (3);

  /* renegotiate! */
  test_webrtc_reset_negotiation (t);
  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_renego_stream_add_data_channel)
{
  struct test_webrtc *t = create_audio_video_test ();

  guint media_format_count[] = { 1, 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv", NULL };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendrecv", "recvonly", NULL };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  VAL_SDP_INIT (renego_mid, sdp_media_equal_mid, NULL, NULL);
  VAL_SDP_INIT (renego_ice_params, sdp_media_equal_ice_params, NULL,
      &renego_mid);
  VAL_SDP_INIT (renego_sess_id, sdp_equal_session_id, NULL, &renego_ice_params);
  VAL_SDP_INIT (renego_sess_ver, sdp_increasing_session_version, NULL,
      &renego_sess_id);
  VAL_SDP_INIT (renego_fingerprint, sdp_media_equal_fingerprint, NULL,
      &renego_sess_ver);
  GObject *channel;
  GstHarness *h;

  /* negotiate an AV stream and then renegotiate a data channel */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);

  media_formats.next = &renego_fingerprint;
  count.user_data = GUINT_TO_POINTER (3);

  /* renegotiate! */
  test_webrtc_reset_negotiation (t);
  test_validate_sdp (t, &offer, &answer);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_renego_data_channel_add_stream)
{
  struct test_webrtc *t = test_webrtc_new ();
  guint media_format_count[] = { 1, 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] = { NULL, "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { NULL, "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  VAL_SDP_INIT (renego_mid, sdp_media_equal_mid, NULL, NULL);
  VAL_SDP_INIT (renego_ice_params, sdp_media_equal_ice_params, NULL,
      &renego_mid);
  VAL_SDP_INIT (renego_sess_id, sdp_equal_session_id, NULL, &renego_ice_params);
  VAL_SDP_INIT (renego_sess_ver, sdp_increasing_session_version, NULL,
      &renego_sess_id);
  VAL_SDP_INIT (renego_fingerprint, sdp_media_equal_fingerprint, NULL,
      &renego_sess_ver);
  GObject *channel;
  GstHarness *h;

  /* negotiate an data channel and then renegotiate to add a av stream */
  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_data_channel = NULL;
  t->on_pad_added = _pad_added_fakesink;
  t->on_prepare_data_channel = NULL;

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);

  test_validate_sdp_full (t, &offer, &answer, 0, FALSE);

  h = gst_harness_new_with_element (t->webrtc1, "sink_1", NULL);
  add_fake_audio_src_harness (h, 97, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  media_formats.next = &renego_fingerprint;
  count.user_data = GUINT_TO_POINTER (2);

  /* renegotiate! */
  test_webrtc_reset_negotiation (t);
  test_validate_sdp_full (t, &offer, &answer, 0, FALSE);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;


GST_START_TEST (test_renego_stream_data_channel_add_stream)
{
  struct test_webrtc *t = test_webrtc_new ();
  guint media_format_count[] = { 1, 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] = { "sendrecv", NULL, "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", NULL, "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  VAL_SDP_INIT (renego_mid, sdp_media_equal_mid, NULL, NULL);
  VAL_SDP_INIT (renego_ice_params, sdp_media_equal_ice_params, NULL,
      &renego_mid);
  VAL_SDP_INIT (renego_sess_id, sdp_equal_session_id, NULL, &renego_ice_params);
  VAL_SDP_INIT (renego_sess_ver, sdp_increasing_session_version, NULL,
      &renego_sess_id);
  VAL_SDP_INIT (renego_fingerprint, sdp_media_equal_fingerprint, NULL,
      &renego_sess_ver);
  GObject *channel;
  GstHarness *h;

  /* Negotiate a stream and a data channel, then renogotiate with a new stream */
  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_data_channel = NULL;
  t->on_prepare_data_channel = NULL;
  t->on_pad_added = _pad_added_fakesink;

  h = gst_harness_new_with_element (t->webrtc1, "sink_0", NULL);
  add_fake_audio_src_harness (h, 97, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  g_signal_emit_by_name (t->webrtc1, "create-data-channel", "label", NULL,
      &channel);

  test_validate_sdp_full (t, &offer, &answer, 0, FALSE);

  h = gst_harness_new_with_element (t->webrtc1, "sink_2", NULL);
  add_fake_audio_src_harness (h, 97, 0xBEEFDEAD);
  t->harnesses = g_list_prepend (t->harnesses, h);

  media_formats.next = &renego_fingerprint;
  count.user_data = GUINT_TO_POINTER (3);

  /* renegotiate! */
  test_webrtc_reset_negotiation (t);
  test_validate_sdp_full (t, &offer, &answer, 0, FALSE);

  g_object_unref (channel);
  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_bundle_renego_add_stream)
{
  struct test_webrtc *t = create_audio_video_test ();
  const gchar *bundle[] = { "audio0", "video1", "audio2", NULL };
  const gchar *offer_bundle_only[] = { "video1", "audio2", NULL };
  const gchar *answer_bundle_only[] = { NULL };
  guint media_format_count[] = { 1, 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] =
      { "sendrecv", "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] =
      { "sendrecv", "recvonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);

  VAL_SDP_INIT (renego_mid, sdp_media_equal_mid, NULL, &payloads);
  VAL_SDP_INIT (renego_ice_params, sdp_media_equal_ice_params, NULL,
      &renego_mid);
  VAL_SDP_INIT (renego_sess_id, sdp_equal_session_id, NULL, &renego_ice_params);
  VAL_SDP_INIT (renego_sess_ver, sdp_increasing_session_version, NULL,
      &renego_sess_id);
  VAL_SDP_INIT (renego_fingerprint, sdp_media_equal_fingerprint, NULL,
      &renego_sess_ver);
  VAL_SDP_INIT (bundle_tag, _check_bundle_tag, bundle, &renego_fingerprint);
  VAL_SDP_INIT (offer_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (1), &bundle_tag);
  VAL_SDP_INIT (answer_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (3), &bundle_tag);
  VAL_SDP_INIT (offer_bundle_only_sdp, _check_bundle_only_media,
      &offer_bundle_only, &offer_non_reject);
  VAL_SDP_INIT (answer_bundle_only_sdp, _check_bundle_only_media,
      &answer_bundle_only, &answer_non_reject);
  GstHarness *h;

  /* We set a max-bundle policy on the offering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, and they should be marked
   * as bundle-only
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-bundle");
  /* We also set a max-bundle policy on the answering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, but need not be marked
   * as bundle-only.
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  /* negotiate an AV stream and then renegotiate an extra stream */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);

  h = gst_harness_new_with_element (t->webrtc1, "sink_2", NULL);
  add_fake_audio_src_harness (h, 98, 0xBEEFFFFF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  offer_setup.next = &offer_bundle_only_sdp;
  answer_setup.next = &answer_bundle_only_sdp;
  count.user_data = GUINT_TO_POINTER (3);

  /* renegotiate! */
  test_webrtc_reset_negotiation (t);
  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_bundle_max_compat_max_bundle_renego_add_stream)
{
  struct test_webrtc *t = create_audio_video_test ();
  const gchar *bundle[] = { "audio0", "video1", "audio2", NULL };
  const gchar *bundle_only[] = { NULL };
  guint media_format_count[] = { 1, 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] =
      { "sendrecv", "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] =
      { "sendrecv", "recvonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);

  VAL_SDP_INIT (renego_mid, sdp_media_equal_mid, NULL, NULL);
  VAL_SDP_INIT (renego_ice_params, sdp_media_equal_ice_params, NULL,
      &renego_mid);
  VAL_SDP_INIT (renego_sess_id, sdp_equal_session_id, NULL, &renego_ice_params);
  VAL_SDP_INIT (renego_sess_ver, sdp_increasing_session_version, NULL,
      &renego_sess_id);
  VAL_SDP_INIT (renego_fingerprint, sdp_media_equal_fingerprint, NULL,
      &renego_sess_ver);
  VAL_SDP_INIT (bundle_tag, _check_bundle_tag, bundle, &renego_fingerprint);
  VAL_SDP_INIT (count_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (3), &bundle_tag);
  VAL_SDP_INIT (bundle_sdp, _check_bundle_only_media, &bundle_only,
      &count_non_reject);
  GstHarness *h;

  /* We set a max-compat policy on the offering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, and they should *not* be marked
   * as bundle-only
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-compat");
  /* We set a max-bundle policy on the answering webrtcbin,
   * this means that all the offered medias should be part
   * of the group:BUNDLE attribute, but need not be marked
   * as bundle-only.
   */
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  /* negotiate an AV stream and then renegotiate an extra stream */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);

  h = gst_harness_new_with_element (t->webrtc1, "sink_2", NULL);
  add_fake_audio_src_harness (h, 98, 0xBEEFFFFF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  media_formats.next = &bundle_sdp;
  count.user_data = GUINT_TO_POINTER (3);

  /* renegotiate! */
  test_webrtc_reset_negotiation (t);
  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_renego_transceiver_set_direction)
{
  struct test_webrtc *t = create_audio_test ();
  guint media_format_count[] = { 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] = { "sendrecv", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendrecv", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstWebRTCRTPTransceiver *transceiver;
  GstHarness *h;
  GstPad *pad;

  /* negotiate an AV stream and then change the transceiver direction */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);

  /* renegotiate an inactive transceiver! */
  pad = gst_element_get_static_pad (t->webrtc1, "sink_0");
  g_object_get (pad, "transceiver", &transceiver, NULL);
  fail_unless (transceiver != NULL);
  g_object_set (transceiver, "direction",
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, NULL);
  expected_offer_direction[0] = "inactive";
  expected_answer_direction[0] = "inactive";

  /* TODO: also validate EOS events from the inactive change */

  test_webrtc_reset_negotiation (t);
  test_validate_sdp (t, &offer, &answer);

  gst_object_unref (pad);
  gst_object_unref (transceiver);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
offer_remove_last_media (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  guint i, n;
  GstSDPMessage *new, *old;
  const GstSDPOrigin *origin;
  const GstSDPConnection *conn;

  old = t->offer_desc->sdp;
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_message_new (&new));

  origin = gst_sdp_message_get_origin (old);
  conn = gst_sdp_message_get_connection (old);
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_message_set_version (new,
          gst_sdp_message_get_version (old)));
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_message_set_origin (new,
          origin->username, origin->sess_id, origin->sess_version,
          origin->nettype, origin->addrtype, origin->addr));
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_message_set_session_name (new,
          gst_sdp_message_get_session_name (old)));
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_message_set_information (new,
          gst_sdp_message_get_information (old)));
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_message_set_uri (new,
          gst_sdp_message_get_uri (old)));
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_message_set_connection (new,
          conn->nettype, conn->addrtype, conn->address, conn->ttl,
          conn->addr_number));

  n = gst_sdp_message_attributes_len (old);
  for (i = 0; i < n; i++) {
    const GstSDPAttribute *a = gst_sdp_message_get_attribute (old, i);
    fail_unless_equals_int (GST_SDP_OK, gst_sdp_message_add_attribute (new,
            a->key, a->value));
  }

  n = gst_sdp_message_medias_len (old);
  fail_unless (n > 0);
  for (i = 0; i < n - 1; i++) {
    const GstSDPMedia *m = gst_sdp_message_get_media (old, i);
    GstSDPMedia *new_m;

    fail_unless_equals_int (GST_SDP_OK, gst_sdp_media_copy (m, &new_m));
    fail_unless_equals_int (GST_SDP_OK, gst_sdp_message_add_media (new, new_m));
    gst_sdp_media_init (new_m);
    gst_sdp_media_free (new_m);
  }

  gst_webrtc_session_description_free (t->offer_desc);
  t->offer_desc = gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_OFFER,
      new);
}

static void
offer_set_produced_error (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  const GstStructure *reply;
  GError *error = NULL;

  reply = gst_promise_get_reply (promise);
  fail_unless (gst_structure_get (reply, "error", G_TYPE_ERROR, &error, NULL));
  GST_INFO ("error produced: %s", error->message);
  g_clear_error (&error);

  test_webrtc_signal_state_unlocked (t, STATE_CUSTOM);
}

static void
offer_created_produced_error (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  const GstStructure *reply;
  GError *error = NULL;

  reply = gst_promise_get_reply (promise);
  fail_unless (gst_structure_get (reply, "error", G_TYPE_ERROR, &error, NULL));
  GST_INFO ("error produced: %s", error->message);
  g_clear_error (&error);
}

GST_START_TEST (test_renego_lose_media_fails)
{
  struct test_webrtc *t = create_audio_video_test ();
  VAL_SDP_INIT (offer, _count_num_sdp_media, GUINT_TO_POINTER (2), NULL);
  VAL_SDP_INIT (answer, _count_num_sdp_media, GUINT_TO_POINTER (2), NULL);

  /* check that removing an m=line will produce an error */

  test_validate_sdp (t, &offer, &answer);

  test_webrtc_reset_negotiation (t);

  t->on_offer_created = offer_remove_last_media;
  t->on_offer_set = offer_set_produced_error;
  t->on_answer_created = NULL;

  test_webrtc_create_offer (t);
  test_webrtc_wait_for_state_mask (t, 1 << STATE_CUSTOM);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_bundle_codec_preferences_rtx_no_duplicate_payloads)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiverDirection direction;
  GstWebRTCRTPTransceiver *trans;
  guint offer_media_format_count[] = { 2, };
  guint answer_media_format_count[] = { 1, };
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, NULL);
  VAL_SDP_INIT (offer_media_formats, on_sdp_media_count_formats,
      offer_media_format_count, &payloads);
  VAL_SDP_INIT (answer_media_formats, on_sdp_media_count_formats,
      answer_media_format_count, &payloads);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &offer_media_formats);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &answer_media_formats);
  const gchar *expected_offer_direction[] = { "recvonly", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstCaps *caps;
  GstHarness *h;

  /* add a transceiver that will only receive an opus stream and check that
   * the created offer is marked as recvonly */
  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-bundle");
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  /* setup recvonly transceiver */
  caps = gst_caps_from_string (VP8_RTP_CAPS (96));
  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  g_object_set (GST_OBJECT (trans), "do-nack", TRUE, NULL);
  gst_caps_unref (caps);
  fail_unless (trans != NULL);
  gst_object_unref (trans);

  /* setup sendonly peer */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_video_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);
  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

static void
on_sdp_media_no_duplicate_extmaps (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, 0);

  fail_unless (media != NULL);

  fail_unless_equals_string (gst_sdp_media_get_attribute_val_n (media, "extmap",
          0), "1 foobar");

  fail_unless (gst_sdp_media_get_attribute_val_n (media, "extmap", 1) == NULL);
}

/* In this test, we validate that identical extmaps for multiple formats
 * in the caps of a single transceiver are deduplicated. This is necessary
 * because Firefox will complain about duplicate extmap ids and fail negotiation
 * otherwise. */
GST_START_TEST (test_codec_preferences_no_duplicate_extmaps)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiver *trans;
  GstWebRTCRTPTransceiverDirection direction;
  VAL_SDP_INIT (extmaps, on_sdp_media_no_duplicate_extmaps, NULL, NULL);
  GstCaps *caps;
  GstStructure *s;

  caps = gst_caps_new_empty ();

  s = gst_structure_from_string (VP8_RTP_CAPS (96), NULL);
  gst_structure_set (s, "extmap-1", G_TYPE_STRING, "foobar", NULL);
  gst_caps_append_structure (caps, s);
  s = gst_structure_from_string (H264_RTP_CAPS (97), NULL);
  gst_structure_set (s, "extmap-1", G_TYPE_STRING, "foobar", NULL);
  gst_caps_append_structure (caps, s);

  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  gst_caps_unref (caps);
  fail_unless (trans != NULL);

  t->on_negotiation_needed = NULL;
  t->on_pad_added = NULL;
  t->on_ice_candidate = NULL;

  test_validate_sdp (t, &extmaps, NULL);

  test_webrtc_free (t);
}

GST_END_TEST;

/* In this test, we validate that trying to use different values
 * for the same extmap id in multiple formats in the caps of a
 * single transceiver errors out when creating the offer. */
GST_START_TEST (test_codec_preferences_incompatible_extmaps)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiver *trans;
  GstWebRTCRTPTransceiverDirection direction;
  GstCaps *caps;
  GstStructure *s;

  caps = gst_caps_new_empty ();

  s = gst_structure_from_string (VP8_RTP_CAPS (96), NULL);
  gst_structure_set (s, "extmap-1", G_TYPE_STRING, "foobar", NULL);
  gst_caps_append_structure (caps, s);
  s = gst_structure_from_string (H264_RTP_CAPS (97), NULL);
  gst_structure_set (s, "extmap-1", G_TYPE_STRING, "foobaz", NULL);
  gst_caps_append_structure (caps, s);

  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  gst_caps_unref (caps);
  fail_unless (trans != NULL);

  t->on_negotiation_needed = NULL;
  t->on_pad_added = NULL;
  t->on_ice_candidate = NULL;
  t->on_offer_created = offer_created_produced_error;

  test_validate_sdp_full (t, NULL, NULL, 1 << STATE_ERROR, TRUE);

  test_webrtc_free (t);
}

GST_END_TEST;

/* In this test, we validate that extmap values must be of the correct type */
GST_START_TEST (test_codec_preferences_invalid_extmap)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiver *trans;
  GstWebRTCRTPTransceiverDirection direction;
  GstCaps *caps;
  GstStructure *s;

  caps = gst_caps_new_empty ();

  s = gst_structure_from_string (VP8_RTP_CAPS (96), NULL);
  gst_structure_set (s, "extmap-1", G_TYPE_INT, 42, NULL);
  gst_caps_append_structure (caps, s);

  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  gst_caps_unref (caps);
  fail_unless (trans != NULL);

  t->on_negotiation_needed = NULL;
  t->on_pad_added = NULL;
  t->on_ice_candidate = NULL;
  t->on_offer_created = offer_created_produced_error;

  test_validate_sdp_full (t, NULL, NULL, 1 << STATE_ERROR, TRUE);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_reject_request_pad)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiverDirection direction;
  GstWebRTCRTPTransceiver *trans, *trans2;
  guint offer_media_format_count[] = { 1, };
  guint answer_media_format_count[] = { 1, };
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, NULL);
  VAL_SDP_INIT (offer_media_formats, on_sdp_media_count_formats,
      offer_media_format_count, &payloads);
  VAL_SDP_INIT (answer_media_formats, on_sdp_media_count_formats,
      answer_media_format_count, &payloads);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &offer_media_formats);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &answer_media_formats);
  const gchar *expected_offer_direction[] = { "recvonly", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstCaps *caps;
  GstHarness *h;
  GstPad *pad;
  GstPadTemplate *templ;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-bundle");
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  /* setup recvonly transceiver */
  caps = gst_caps_from_string (VP8_RTP_CAPS (96));
  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  gst_caps_unref (caps);
  fail_unless (trans != NULL);

  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_video_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);

  /* This should fail because the direction is wrong */
  pad = gst_element_request_pad_simple (t->webrtc1, "sink_0");
  fail_unless (pad == NULL);

  g_object_set (trans, "direction",
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, NULL);

  templ = gst_element_get_pad_template (t->webrtc1, "sink_%u");
  fail_unless (templ != NULL);

  /* This should fail because the caps are wrong */
  caps = gst_caps_from_string (OPUS_RTP_CAPS (96));
  pad = gst_element_request_pad (t->webrtc1, templ, "sink_0", caps);
  fail_unless (pad == NULL);

  g_object_set (trans, "codec-preferences", NULL, NULL);

  /* This should fail because the kind doesn't match */
  pad = gst_element_request_pad (t->webrtc1, templ, "sink_0", caps);
  fail_unless (pad == NULL);
  gst_caps_unref (caps);

  /* This should succeed and give us sink_0 */
  pad = gst_element_request_pad_simple (t->webrtc1, "sink_0");
  fail_unless (pad != NULL);

  g_object_get (pad, "transceiver", &trans2, NULL);

  fail_unless (trans == trans2);

  gst_object_unref (pad);
  gst_object_unref (trans);
  gst_object_unref (trans2);

  test_webrtc_free (t);
}

GST_END_TEST;

static void
_verify_media_types (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  gchar **media_types = user_data;
  int i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);

    fail_unless_equals_string (gst_sdp_media_get_media (media), media_types[i]);
  }
}

GST_START_TEST (test_reject_create_offer)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstHarness *h;
  GstPromise *promise;
  GstPromiseResult res;
  const GstStructure *s;
  GError *error = NULL;

  const gchar *media_types[] = { "video", "audio" };
  VAL_SDP_INIT (media_type, _verify_media_types, &media_types, NULL);
  guint media_format_count[] = { 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &media_type);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass" };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", "active" };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv" };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", "recvonly" };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  /* setup sendonly peer */
  h = gst_harness_new_with_element (t->webrtc1, "sink_1", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  /* Check that if there is no 0, we can't create an offer with a hole */
  promise = gst_promise_new ();
  g_signal_emit_by_name (t->webrtc1, "create-offer", NULL, promise);
  res = gst_promise_wait (promise);
  fail_unless_equals_int (res, GST_PROMISE_RESULT_REPLIED);
  s = gst_promise_get_reply (promise);
  fail_unless (s != NULL);
  gst_structure_get (s, "error", G_TYPE_ERROR, &error, NULL);
  fail_unless (g_error_matches (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_INTERNAL_FAILURE));
  fail_unless_equals_string (error->message,
      "Tranceiver <webrtctransceiver0> with mid (null) has locked mline 1 but the offer only has 0 sections");
  g_clear_error (&error);
  gst_promise_unref (promise);

  h = gst_harness_new_with_element (t->webrtc1, "sink_%u", NULL);
  add_fake_video_src_harness (h, 97, 0xBEEFDEAD);
  t->harnesses = g_list_prepend (t->harnesses, h);

  /* Adding a second sink, which will fill m-line 0, should fix it */
  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_reject_create_offer_mline_locked_no_caps)
{
  GstHarness *h;
  GstPromise *promise;
  const GstStructure *s;
  GstPromiseResult res;
  GError *error = NULL;

  h = gst_harness_new_with_padnames ("webrtcbin", "sink_0", NULL);

  promise = gst_promise_new ();
  g_signal_emit_by_name (h->element, "create-offer", NULL, promise);
  res = gst_promise_wait (promise);
  fail_unless_equals_int (res, GST_PROMISE_RESULT_REPLIED);
  s = gst_promise_get_reply (promise);
  fail_unless (s != NULL);
  gst_structure_get (s, "error", G_TYPE_ERROR, &error, NULL);
  fail_unless (g_error_matches (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_INTERNAL_FAILURE));
  fail_unless_equals_string (error->message,
      "Trying to add transceiver at line 0 but there is a transceiver with a"
      " locked mline for this line which doesn't have caps");
  g_clear_error (&error);
  gst_promise_unref (promise);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_reject_set_description)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstHarness *h;
  GstPromise *promise;
  GstPromiseResult res;
  const GstStructure *s;
  GError *error = NULL;
  GstWebRTCSessionDescription *desc = NULL;
  GstPadTemplate *templ;
  GstCaps *caps;
  GstPad *pad;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  /* setup peer 1 */
  h = gst_harness_new_with_element (t->webrtc1, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  /* Create a second side with specific video caps */
  templ = gst_element_get_pad_template (t->webrtc2, "sink_%u");
  fail_unless (templ != NULL);
  caps = gst_caps_from_string (VP8_RTP_CAPS (97));
  pad = gst_element_request_pad (t->webrtc2, templ, "sink_0", caps);
  fail_unless (pad != NULL);
  gst_caps_unref (caps);
  gst_object_unref (pad);

  /* Create an offer */
  promise = gst_promise_new ();
  g_signal_emit_by_name (t->webrtc1, "create-offer", NULL, promise);
  res = gst_promise_wait (promise);
  fail_unless_equals_int (res, GST_PROMISE_RESULT_REPLIED);
  s = gst_promise_get_reply (promise);
  fail_unless (s != NULL);
  gst_structure_get (s, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &desc,
      NULL);
  fail_unless (desc != NULL);
  gst_promise_unref (promise);

  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_READY) == GST_STATE_CHANGE_FAILURE);

  /* Verify that setting an offer where there is a forced m-line with
     a different kind fails. */
  promise = gst_promise_new ();
  g_signal_emit_by_name (t->webrtc2, "set-remote-description", desc, promise);
  res = gst_promise_wait (promise);
  fail_unless_equals_int (res, GST_PROMISE_RESULT_REPLIED);
  s = gst_promise_get_reply (promise);
  gst_structure_get (s, "error", G_TYPE_ERROR, &error, NULL);
  fail_unless (g_error_matches (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_INTERNAL_FAILURE));
  fail_unless_equals_string
      (error->message,
      "m-line 0 with transceiver <webrtctransceiver1> was locked to video, but SDP has audio media");

  g_clear_error (&error);
  fail_unless (s != NULL);
  gst_promise_unref (promise);
  gst_webrtc_session_description_free (desc);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_force_second_media)
{
  struct test_webrtc *t = test_webrtc_new ();
  const gchar *media_types[] = { "audio" };
  VAL_SDP_INIT (media_type, _verify_media_types, &media_types, NULL);
  guint media_format_count[] = { 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &media_type);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &media_formats);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &media_formats);
  const gchar *expected_offer_direction[] = { "sendrecv", };
  VAL_SDP_INIT (offer_direction, on_sdp_media_direction,
      expected_offer_direction, &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", };
  VAL_SDP_INIT (answer_direction, on_sdp_media_direction,
      expected_answer_direction, &answer_setup);
  VAL_SDP_INIT (answer_count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &answer_direction);
  VAL_SDP_INIT (offer_count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &offer_direction);

  const gchar *second_media_types[] = { "audio", "video" };
  VAL_SDP_INIT (second_media_type, _verify_media_types, &second_media_types,
      NULL);
  guint second_media_format_count[] = { 1, 1 };
  VAL_SDP_INIT (second_media_formats, on_sdp_media_count_formats,
      second_media_format_count, &second_media_type);
  const gchar *second_expected_offer_setup[] = { "active", "actpass" };
  VAL_SDP_INIT (second_offer_setup, on_sdp_media_setup,
      second_expected_offer_setup, &second_media_formats);
  const gchar *second_expected_answer_setup[] = { "passive", "active" };
  VAL_SDP_INIT (second_answer_setup, on_sdp_media_setup,
      second_expected_answer_setup, &second_media_formats);
  const gchar *second_expected_answer_direction[] = { "sendonly", "recvonly" };
  VAL_SDP_INIT (second_answer_direction, on_sdp_media_direction,
      second_expected_answer_direction, &second_answer_setup);
  const gchar *second_expected_offer_direction[] = { "recvonly", "sendrecv" };
  VAL_SDP_INIT (second_offer_direction, on_sdp_media_direction,
      second_expected_offer_direction, &second_offer_setup);
  VAL_SDP_INIT (second_answer_count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &second_answer_direction);
  VAL_SDP_INIT (second_offer_count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &second_offer_direction);

  GstHarness *h;
  guint negotiation_flag = 0;
  GstPadTemplate *templ;
  GstCaps *caps;
  GstPad *pad;

  /* add a transceiver that will only receive an opus stream and check that
   * the created offer is marked as recvonly */
  t->on_negotiation_needed = on_negotiation_needed_hit;
  t->negotiation_data = &negotiation_flag;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  /* setup peer */
  h = gst_harness_new_with_element (t->webrtc1, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  /* Create a second side with specific video caps */
  templ = gst_element_get_pad_template (t->webrtc2, "sink_%u");
  fail_unless (templ != NULL);
  caps = gst_caps_from_string (VP8_RTP_CAPS (97));
  pad = gst_element_request_pad (t->webrtc2, templ, NULL, caps);
  gst_caps_unref (caps);
  fail_unless (pad != NULL);
  h = gst_harness_new_with_element (t->webrtc2, GST_PAD_NAME (pad), NULL);
  gst_object_unref (pad);
  add_fake_video_src_harness (h, 97, 0xBEEFDEAD);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer_count, &answer_count);
  fail_unless (negotiation_flag & 1 << 2);

  test_webrtc_reset_negotiation (t);

  t->offerror = 2;
  test_validate_sdp (t, &second_offer_count, &second_answer_count);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_codec_preferences_caps)
{
  GstHarness *h;
  GstPad *pad;
  GstWebRTCRTPTransceiver *trans;
  GstCaps *caps, *caps2;

  h = gst_harness_new_with_padnames ("webrtcbin", "sink_0", NULL);
  pad = gst_element_get_static_pad (h->element, "sink_0");

  g_object_get (pad, "transceiver", &trans, NULL);

  caps = gst_caps_from_string ("application/x-rtp, media=video,"
      "encoding-name=VP8, payload=115; application/x-rtp, media=video,"
      " encoding-name=H264, payload=104");
  g_object_set (trans, "codec-preferences", caps, NULL);

  caps2 = gst_pad_query_caps (pad, NULL);
  fail_unless (gst_caps_is_equal (caps, caps2));
  gst_caps_unref (caps2);
  gst_caps_unref (caps);

  caps = gst_caps_from_string (VP8_RTP_CAPS (115));
  fail_unless (gst_pad_query_accept_caps (pad, caps));
  gst_harness_set_src_caps (h, g_steal_pointer (&caps));

  caps = gst_caps_from_string (VP8_RTP_CAPS (99));
  fail_unless (!gst_pad_query_accept_caps (pad, caps));
  gst_caps_unref (caps);

  gst_object_unref (pad);
  gst_object_unref (trans);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_codec_preferences_negotiation_sinkpad)
{
  struct test_webrtc *t = test_webrtc_new ();
  guint media_format_count[] = { 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &media_formats);
  VAL_SDP_INIT (payloads2, on_sdp_media_payload_types, GUINT_TO_POINTER (0),
      &count);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &payloads2);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] = { "sendrecv", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);

  GstPad *pad;
  GstWebRTCRTPTransceiver *transceiver;
  GstHarness *h;
  GstCaps *caps;
  GstPromise *promise;
  GstPromiseResult res;
  const GstStructure *s;
  GError *error = NULL;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  h = gst_harness_new_with_element (t->webrtc1, "sink_0", NULL);
  pad = gst_element_get_static_pad (t->webrtc1, "sink_0");
  g_object_get (pad, "transceiver", &transceiver, NULL);
  caps = gst_caps_from_string (VP8_RTP_CAPS (115) ";" VP8_RTP_CAPS (97));
  g_object_set (transceiver, "codec-preferences", caps, NULL);
  gst_caps_unref (caps);
  gst_object_unref (transceiver);
  gst_object_unref (pad);

  add_fake_video_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  promise = gst_promise_new ();
  g_signal_emit_by_name (t->webrtc1, "create-offer", NULL, promise);
  res = gst_promise_wait (promise);
  fail_unless_equals_int (res, GST_PROMISE_RESULT_REPLIED);
  s = gst_promise_get_reply (promise);
  fail_unless (s != NULL);
  gst_structure_get (s, "error", G_TYPE_ERROR, &error, NULL);
  fail_unless (g_error_matches (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_INTERNAL_FAILURE));
  fail_unless_equals_string
      ("Caps negotiation on pad sink_0 failed against codec preferences",
      error->message);
  g_clear_error (&error);
  gst_promise_unref (promise);

  caps = gst_caps_from_string (VP8_RTP_CAPS (97));
  gst_harness_set_src_caps (h, caps);

  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;


static void
add_audio_test_src_harness (GstHarness * h, guint ssrc)
{
#define L16_CAPS "application/x-rtp, payload=11, media=audio," \
      " encoding-name=L16, clock-rate=44100, ssrc=(uint)3484078952"
  GstCaps *caps = gst_caps_from_string (L16_CAPS);
  GstElement *capsfilter;
  if (ssrc != 0) {
    gst_caps_set_simple (caps, "ssrc", G_TYPE_UINT, ssrc, NULL);
  }
  gst_harness_add_src_parse (h, "audiotestsrc is-live=true ! rtpL16pay ! "
      "capsfilter name=capsfilter ! identity", TRUE);
  capsfilter =
      gst_bin_get_by_name (GST_BIN (h->src_harness->element), "capsfilter");
  g_object_set (G_OBJECT (capsfilter), "caps", caps, NULL);
  gst_harness_set_src_caps (h, caps);
  caps = NULL;
  gst_clear_object (&capsfilter);
#undef L16_CAPS
}

struct pad_added_harness_data
{
  GList *sink_harnesses;
  OnPadAdded on_pad_added;
  gpointer on_pad_added_data;
};

static void
_pad_added_harness (struct test_webrtc *t, GstElement * element,
    GstPad * pad, gpointer user_data)
{
  struct pad_added_harness_data *data = user_data;
  GstHarness *h;

  if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC)
    return;

  h = gst_harness_new_with_element (element, NULL, GST_OBJECT_NAME (pad));
  t->harnesses = g_list_prepend (t->harnesses, h);

  if (data) {
    data->sink_harnesses = g_list_prepend (data->sink_harnesses, h);
    g_cond_broadcast (&t->cond);

    if (data->on_pad_added)
      data->on_pad_added (t, element, pad, data->on_pad_added_data);
  }
}

static void
new_jitterbuffer_set_fast_start (GstElement * rtpbin,
    GstElement * rtpjitterbuffer, guint session_id, guint ssrc,
    gpointer user_data)
{
  g_object_set (rtpjitterbuffer, "faststart-min-packets", 1, NULL);
}

GST_START_TEST (test_codec_preferences_negotiation_srcpad)
{
  struct test_webrtc *t = test_webrtc_new ();
  guint media_format_count[] = { 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL, &count);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &payloads);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &payloads);
  const gchar *expected_offer_direction[] = { "sendrecv", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  VAL_SDP_INIT (answer_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (0), &count);
  GstHarness *h;
  struct pad_added_harness_data pad_added_data = { NULL, };
  GstHarness *sink_harness = NULL;
  guint i;
  GstElement *rtpbin2;
  GstBuffer *buf;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_harness;
  t->pad_added_data = &pad_added_data;

  rtpbin2 = gst_bin_get_by_name (GST_BIN (t->webrtc2), "rtpbin");
  fail_unless (rtpbin2 != NULL);
  g_signal_connect (rtpbin2, "new-jitterbuffer",
      G_CALLBACK (new_jitterbuffer_set_fast_start), NULL);
  g_object_unref (rtpbin2);

  h = gst_harness_new_with_element (t->webrtc1, "sink_0", NULL);
  add_audio_test_src_harness (h, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  for (i = 0; i < 10; i++)
    gst_harness_push_from_src (h);

  g_mutex_lock (&t->lock);
  while (pad_added_data.sink_harnesses == NULL) {
    gst_harness_push_from_src (h);
    g_cond_wait_until (&t->cond, &t->lock, g_get_monotonic_time () + 5000);
  }
  fail_unless_equals_int (1, g_list_length (pad_added_data.sink_harnesses));
  sink_harness = (GstHarness *) pad_added_data.sink_harnesses->data;
  g_mutex_unlock (&t->lock);
  fail_unless (sink_harness->element == t->webrtc2);

  /* Get one buffer out, this makes sure the capsfilter is primed and
   * avoids races.
   */
  buf = gst_harness_pull (sink_harness);
  fail_unless (buf != NULL);
  gst_buffer_unref (buf);

  gst_harness_set_sink_caps_str (sink_harness, OPUS_RTP_CAPS (100));

  test_webrtc_reset_negotiation (t);
  test_validate_sdp_full (t, &offer, &answer_non_reject, 0, FALSE);

  /* check that the mid/mline is correct */
  {
    GstWebRTCRTPTransceiver *rtp_trans;
    GstPad *srcpad;
    guint mline;

    srcpad = gst_pad_get_peer (sink_harness->sinkpad);
    fail_unless (srcpad != NULL);
    g_object_get (srcpad, "transceiver", &rtp_trans, NULL);
    gst_clear_object (&srcpad);
    fail_unless (rtp_trans != NULL);
    g_object_get (rtp_trans, "mlineindex", &mline, NULL);
    gst_clear_object (&rtp_trans);
    fail_unless_equals_int (mline, 0);
  }

  test_webrtc_free (t);

  g_list_free (pad_added_data.sink_harnesses);
}

GST_END_TEST;

static void
_on_new_transceiver_codec_preferences_h264 (GstElement * webrtcbin,
    GstWebRTCRTPTransceiver * trans, gpointer * user_data)
{
  GstCaps *caps;

  caps = gst_caps_from_string ("application/x-rtp,encoding-name=(string)H264");
  g_object_set (trans, "codec-preferences", caps, NULL);
  gst_caps_unref (caps);
}

static void
on_sdp_media_payload_types_only_h264 (struct test_webrtc *t,
    GstElement * element, GstWebRTCSessionDescription * desc,
    gpointer user_data)
{
  const GstSDPMedia *vmedia;
  guint video_mline = GPOINTER_TO_UINT (user_data);
  guint j;

  vmedia = gst_sdp_message_get_media (desc->sdp, video_mline);

  for (j = 0; j < gst_sdp_media_attributes_len (vmedia); j++) {
    const GstSDPAttribute *attr = gst_sdp_media_get_attribute (vmedia, j);

    if (!g_strcmp0 (attr->key, "rtpmap")) {
      fail_unless_equals_string (attr->value, "101 H264/90000");
    }
  }
}


GST_START_TEST (test_codec_preferences_in_on_new_transceiver)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiverDirection direction;
  GstWebRTCRTPTransceiver *trans;
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint offer_media_format_count[] = { 2 };
  guint answer_media_format_count[] = { 1 };
  VAL_SDP_INIT (offer_media_formats, on_sdp_media_count_formats,
      offer_media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (answer_media_formats, on_sdp_media_count_formats,
      answer_media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (offer_count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &offer_media_formats);
  VAL_SDP_INIT (answer_count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &answer_media_formats);
  VAL_SDP_INIT (offer_payloads, on_sdp_media_payload_types,
      GUINT_TO_POINTER (0), &offer_count);
  VAL_SDP_INIT (answer_payloads, on_sdp_media_payload_types_only_h264,
      GUINT_TO_POINTER (0), &answer_count);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &offer_payloads);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &answer_payloads);
  const gchar *expected_offer_direction[] = { "sendonly", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstCaps *caps;
  GstHarness *h;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  /* setup sendonly transceiver with VP8 and H264 */
  caps = gst_caps_from_string (VP8_RTP_CAPS (97) ";" H264_RTP_CAPS (101));
  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  gst_caps_unref (caps);
  fail_unless (trans != NULL);
  gst_object_unref (trans);

  /* setup recvonly peer */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_video_src_harness (h, 101, 0);
  t->harnesses = g_list_prepend (t->harnesses, h);

  /* connect to "on-new-transceiver" to set codec-preferences to H264 */
  g_signal_connect (t->webrtc2, "on-new-transceiver",
      G_CALLBACK (_on_new_transceiver_codec_preferences_h264), NULL);

  /* Answer SDP should now have H264 only. Without the codec-preferences it
   * would only have VP8 because that comes first in the SDP */

  test_validate_sdp (t, &offer, &answer);
  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_renego_rtx)
{
  struct test_webrtc *t = create_audio_video_test ();
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1, 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (count_media, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  VAL_SDP_INIT (payloads, on_sdp_media_payload_types,
      GUINT_TO_POINTER (1), &count_media);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv", };
  VAL_SDP_INIT (offer_direction, on_sdp_media_direction,
      expected_offer_direction, &payloads);
  const gchar *expected_answer_direction[] = { "recvonly", "recvonly", };
  VAL_SDP_INIT (answer_direction, on_sdp_media_direction,
      expected_answer_direction, &payloads);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", };
  VAL_SDP_INIT (offer, on_sdp_media_setup, expected_offer_setup,
      &offer_direction);
  const gchar *expected_answer_setup[] = { "active", "active", };
  VAL_SDP_INIT (answer, on_sdp_media_setup, expected_answer_setup,
      &answer_direction);
  GstWebRTCRTPTransceiver *trans;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  test_validate_sdp (t, &offer, &answer);

  test_webrtc_reset_negotiation (t);

  g_signal_emit_by_name (t->webrtc1, "get-transceiver", 1, &trans);
  g_object_set (trans, "do-nack", TRUE, "fec-type",
      GST_WEBRTC_FEC_TYPE_ULP_RED, NULL);
  g_clear_object (&trans);

  g_signal_emit_by_name (t->webrtc2, "get-transceiver", 1, &trans);
  g_object_set (trans, "do-nack", TRUE, "fec-type",
      GST_WEBRTC_FEC_TYPE_ULP_RED, NULL);
  g_clear_object (&trans);

  /* adding RTX/RED/FEC increases the number of media formats */
  media_format_count[1] = 5;

  test_validate_sdp (t, &offer, &answer);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_bundle_mid_header_extension)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstWebRTCRTPTransceiverDirection direction;
  GstWebRTCRTPTransceiver *trans;
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1, 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &media_formats);
  const char *expected_mid[] = { "gst", };
  VAL_SDP_INIT (mid, on_sdp_media_check_mid, expected_mid, &count);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup, &mid);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup, &mid);
  const gchar *expected_offer_direction[] = { "recvonly", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "sendonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstCaps *caps;
  GstHarness *h;
  guint mline;
  char *trans_mid;

  /* add a transceiver that will only receive an opus stream and check that
   * the created offer is marked as recvonly */
  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  /* setup recvonly transceiver */
  caps = gst_caps_from_string (OPUS_RTP_CAPS (96) ", a-mid=(string)gst");
  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  gst_caps_unref (caps);
  fail_unless (trans != NULL);
  g_object_get (trans, "mlineindex", &mline, NULL);
  fail_unless_equals_int (mline, -1);

  /* setup sendonly peer */
  h = gst_harness_new_with_element (t->webrtc2, "sink_0", NULL);
  add_fake_audio_src_harness (h, 96, 0xDEADBEEF);
  t->harnesses = g_list_prepend (t->harnesses, h);

  test_validate_sdp (t, &offer, &answer);

  g_object_get (trans, "mlineindex", &mline, "mid", &trans_mid, NULL);
  fail_unless_equals_int (mline, 0);
  fail_unless_equals_string (trans_mid, "gst");
  g_clear_pointer (&trans_mid, g_free);
  gst_object_unref (trans);

  test_webrtc_free (t);
}

GST_END_TEST;

static void
on_new_transceiver_set_rtx_fec (GstElement * webrtcbin, GObject * trans,
    gpointer user_data)
{
  g_object_set (trans, "fec-type", GST_WEBRTC_FEC_TYPE_ULP_RED,
      "fec-percentage", 100, "do-nack", TRUE, NULL);
}

struct pad_properties
{
  const char *mid;
  guint mlineindex;
};

struct new_pad_validate_properties
{
  guint n_props;
  struct pad_properties *props;
};

static void
on_pad_added_validate_props (struct test_webrtc *t, GstElement * element,
    GstPad * pad, gpointer user_data)
{
  GstWebRTCRTPTransceiver *rtp_trans;
  struct new_pad_validate_properties *pad_props = user_data;
  char *trans_mid;
  guint mlineindex;
  int i;

  g_object_get (pad, "transceiver", &rtp_trans, NULL);
  fail_unless (rtp_trans);
  g_object_get (rtp_trans, "mid", &trans_mid, "mlineindex", &mlineindex, NULL);
  fail_unless (trans_mid != NULL);
  fail_unless (mlineindex != -1);
  for (i = 0; i < pad_props->n_props; i++) {
    struct pad_properties *expected = &pad_props->props[i];

    if (g_strcmp0 (expected->mid, trans_mid) == 0) {
      if (expected->mlineindex != -1) {
        fail_unless_equals_int (mlineindex, expected->mlineindex);
        break;
      }
    }
  }
  if (i == pad_props->n_props)
    fail ("could not find a matching expected output pad for mid %s and mline %u", trans_mid, mlineindex);

  g_clear_pointer (&trans_mid, g_free);
  gst_clear_object (&rtp_trans);
}

GST_START_TEST (test_max_bundle_fec)
{
  struct test_webrtc *t = test_webrtc_new ();
  guint media_format_count[] = { 5, 5, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL,
      &media_formats);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2), &payloads);
  VAL_SDP_INIT (offer_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (1), &count);
  VAL_SDP_INIT (answer_non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (2), &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &offer_non_reject);
  const gchar *expected_answer_setup[] = { "active", "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &answer_non_reject);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", "recvonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstHarness *src0, *src1;
  struct pad_properties pad_prop[] = {
    {"audio0", 0},
    {"audio1", 1},
  };
  struct new_pad_validate_properties validate_pad = { 2, pad_prop };
  struct pad_added_harness_data pad_added_data =
      { NULL, on_pad_added_validate_props, &validate_pad };
  guint i;
  GstElement *rtpbin2;
  GstBuffer *buf;
  guint ssrcs[] = { 123456789, 987654321 };
  GArray *ssrcs_received;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_harness;
  t->pad_added_data = &pad_added_data;

  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-bundle");
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  rtpbin2 = gst_bin_get_by_name (GST_BIN (t->webrtc2), "rtpbin");
  fail_unless (rtpbin2 != NULL);
  g_signal_connect (rtpbin2, "new-jitterbuffer",
      G_CALLBACK (new_jitterbuffer_set_fast_start), NULL);
  g_object_unref (rtpbin2);

  g_signal_connect (t->webrtc1, "on-new-transceiver",
      G_CALLBACK (on_new_transceiver_set_rtx_fec), NULL);
  g_signal_connect (t->webrtc2, "on-new-transceiver",
      G_CALLBACK (on_new_transceiver_set_rtx_fec), NULL);

  src0 = gst_harness_new_with_element (t->webrtc1, "sink_0", NULL);
  add_audio_test_src_harness (src0, ssrcs[0]);
  t->harnesses = g_list_prepend (t->harnesses, src0);

  src1 = gst_harness_new_with_element (t->webrtc1, "sink_1", NULL);
  add_audio_test_src_harness (src1, ssrcs[1]);
  t->harnesses = g_list_prepend (t->harnesses, src1);

  test_validate_sdp (t, &offer, &answer);

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  for (i = 0; i < 10; i++) {
    gst_harness_push_from_src (src0);
    gst_harness_push_from_src (src1);
  }

  ssrcs_received = g_array_new (FALSE, TRUE, sizeof (guint32));

  /* Get one buffer out for each ssrc sent.
   */
  g_mutex_lock (&t->lock);
  while (ssrcs_received->len < G_N_ELEMENTS (ssrcs)) {
    GList *sink_harnesses = pad_added_data.sink_harnesses;
    GList *l;
    guint i;

    gst_harness_push_from_src (src0);
    gst_harness_push_from_src (src1);
    if (g_list_length (sink_harnesses) < 2) {
      g_cond_wait_until (&t->cond, &t->lock, g_get_monotonic_time () + 5000);
      if (g_list_length (sink_harnesses) < 2)
        continue;
    }

    g_mutex_unlock (&t->lock);

    for (l = sink_harnesses; l; l = l->next) {
      GstHarness *sink_harness = (GstHarness *) l->data;
      GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
      guint ssrc;

      fail_unless (sink_harness->element == t->webrtc2);

      buf = gst_harness_try_pull (sink_harness);
      if (!buf)
        continue;

      fail_unless (gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp));

      ssrc = gst_rtp_buffer_get_ssrc (&rtp);
      for (i = 0; i < ssrcs_received->len; i++) {
        if (g_array_index (ssrcs_received, guint, i) == ssrc)
          break;
      }
      if (i == ssrcs_received->len) {
        g_array_append_val (ssrcs_received, ssrc);
      }

      gst_rtp_buffer_unmap (&rtp);

      gst_buffer_unref (buf);
    }
    g_mutex_lock (&t->lock);
  }
  g_mutex_unlock (&t->lock);

  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (t->webrtc1), GST_DEBUG_GRAPH_SHOW_ALL,
      "webrtc1-fec-final");
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (t->webrtc2), GST_DEBUG_GRAPH_SHOW_ALL,
      "webrtc2-fec-final");

  test_webrtc_free (t);
  g_list_free (pad_added_data.sink_harnesses);

  g_array_unref (ssrcs_received);
}

GST_END_TEST;

#define RTPHDREXT_MID GST_RTP_HDREXT_BASE "sdes:mid"
#define RTPHDREXT_STREAM_ID GST_RTP_HDREXT_BASE "sdes:rtp-stream-id"
#define RTPHDREXT_REPAIRED_STREAM_ID GST_RTP_HDREXT_BASE "sdes:repaired-rtp-stream-id"

#define L16_CAPS "application/x-rtp, payload=11, media=audio," \
      " encoding-name=L16, clock-rate=44100"

static GstCaps *
create_simulcast_audio_caps (GstWebRTCRTPTransceiverDirection direction,
    guint n_rid, guint ssrc[], const char *mid, guint mid_ext_id,
    const char *const *rids, guint stream_ext_id, guint repaired_ext_id)
{
  GstStructure *s;
  GstCaps *caps;
  const char *dir_str;

  if (direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY)
    dir_str = "recv";
  else if (direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY)
    dir_str = "send";
  else
    g_assert_not_reached ();

  caps = gst_caps_from_string (L16_CAPS);
  s = gst_caps_get_structure (caps, 0);
  if (mid && mid_ext_id != G_MAXUINT) {
    char *extmap_key = g_strdup_printf ("extmap-%u", mid_ext_id);
    gst_structure_set (s, "a-mid", G_TYPE_STRING, mid, extmap_key,
        G_TYPE_STRING, RTPHDREXT_MID, NULL);
    g_free (extmap_key);
  }
  if (rids && n_rid > 0 && stream_ext_id != G_MAXUINT) {
    GString *simulcast_value = g_string_new (dir_str);
    char *extmap_key, *value;
    int i;

    g_string_append_c (simulcast_value, ' ');

    for (i = 0; i < n_rid; i++) {
      char *rid_key = g_strdup_printf ("rid-%s", rids[i]);
      gst_structure_set (s, rid_key, G_TYPE_STRING, dir_str, NULL);
      if (i > 0)
        g_string_append_c (simulcast_value, ';');
      g_string_append (simulcast_value, rids[i]);
      g_free (rid_key);
    }
    value = g_string_free (simulcast_value, FALSE);
    simulcast_value = NULL;
    extmap_key = g_strdup_printf ("extmap-%u", stream_ext_id);
    gst_structure_set (s, extmap_key, G_TYPE_STRING, RTPHDREXT_STREAM_ID,
        "a-simulcast", G_TYPE_STRING, value, NULL);
    g_clear_pointer (&extmap_key, g_free);
    g_clear_pointer (&value, g_free);

    if (repaired_ext_id != G_MAXUINT) {
      extmap_key = g_strdup_printf ("extmap-%u", repaired_ext_id);
      gst_structure_set (s, extmap_key, G_TYPE_STRING,
          RTPHDREXT_REPAIRED_STREAM_ID, NULL);
      g_clear_pointer (&extmap_key, g_free);
    }
  }

  return caps;
}

static void
add_simulcast_audio_test_src_harness (GstHarness * h, guint n_rid,
    guint ssrc[], const char *mid, guint mid_ext_id,
    const char *const *rids, guint stream_ext_id, guint repaired_ext_id)
{
  GstRTPHeaderExtension *ext;
  GstElement *capsfilter;
  char *launch_str;
  GstCaps *caps;
  int i;

  caps =
      create_simulcast_audio_caps
      (GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, n_rid, ssrc, mid,
      mid_ext_id, rids, stream_ext_id, repaired_ext_id);

  gst_harness_set_src_caps (h, gst_caps_ref (caps));

  if (n_rid == 0) {
    launch_str =
        g_strdup ("audiotestsrc is-live=true ! " L16_CAPS
        ",ssrc=(uint)3384078954 ! rtpL16pay name=payloader0");
  } else {
    GString *launch = g_string_new (NULL);

    for (i = 0; i < n_rid; i++) {
      const char *rtpfunnel = "funnel.";
      if (i == 0)
        rtpfunnel = "rtpfunnel name=funnel ! capsfilter name=capsfilter";

      g_string_append_printf (launch, "audiotestsrc is-live=true ! "
          "rtpL16pay name=payloader%u ! " L16_CAPS ", ssrc=(uint)%u ! %s ", i,
          ssrc[i], rtpfunnel);
    }

    launch_str = g_string_free (launch, FALSE);
  }
  GST_INFO ("generated launch string %s", launch_str);
  gst_harness_add_src_parse (h, launch_str, TRUE);
  g_clear_pointer (&launch_str, g_free);
  capsfilter =
      gst_bin_get_by_name (GST_BIN (h->src_harness->element), "capsfilter");
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_clear_object (&capsfilter);
  gst_clear_caps (&caps);

  for (i = 0; i == 0 || i < n_rid; i++) {
    const char *rid = n_rid > 0 ? rids[i] : NULL;
    char *pay_name = g_strdup_printf ("payloader%u", i);
    GstElement *payloader =
        gst_bin_get_by_name (GST_BIN (h->src_harness->element), pay_name);
    fail_unless (payloader);
    g_clear_pointer (&pay_name, g_free);

    if (mid_ext_id != G_MAXUINT) {
      ext = gst_rtp_header_extension_create_from_uri (RTPHDREXT_MID);
      fail_unless (ext);
      gst_rtp_header_extension_set_id (ext, mid_ext_id);
      g_object_set (ext, "mid", mid, NULL);
      g_signal_emit_by_name (payloader, "add-extension", ext);
      gst_clear_object (&ext);
    }
    if (n_rid > 0 && stream_ext_id != G_MAXUINT) {
      ext = gst_rtp_header_extension_create_from_uri (RTPHDREXT_STREAM_ID);
      fail_unless (ext);
      gst_rtp_header_extension_set_id (ext, stream_ext_id);
      g_object_set (ext, "rid", rid, NULL);
      g_signal_emit_by_name (payloader, "add-extension", ext);
      gst_clear_object (&ext);
    }
    if (n_rid > 0 && stream_ext_id != G_MAXUINT) {
      ext = gst_rtp_header_extension_create_from_uri (RTPHDREXT_STREAM_ID);
      fail_unless (ext);
      gst_rtp_header_extension_set_id (ext, stream_ext_id);
      g_object_set (ext, "rid", rid, NULL);
      g_signal_emit_by_name (payloader, "add-extension", ext);
      gst_clear_object (&ext);
    }
    gst_clear_object (&payloader);
  }
}

#undef L16_CAPS

static gboolean
gst_g_ptr_array_find_str (GPtrArray * ptr, const char *needle, guint * index)
{
  guint i;

  for (i = 0; i < ptr->len; i++) {
    const char *test = g_ptr_array_index (ptr, i);
    if (g_strcmp0 (test, needle) == 0) {
      if (index)
        *index = i;
      return TRUE;
    }
  }

  return FALSE;
}

struct ExpectedRid
{
  guint n_rid;
  const char *const *rid;
};

static void
on_sdp_media_rid (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  struct ExpectedRid *expected_rids = user_data;
  int i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);
    struct ExpectedRid *expected_rid = &expected_rids[i];
    GPtrArray *seen_rid = g_ptr_array_new_with_free_func (g_free);
    int j;

    for (j = 0; j < gst_sdp_media_attributes_len (media); j++) {
      const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, j);

      if (g_strcmp0 (attr->key, "rid") == 0) {
        const char *p;
        char *v;
        guint k;

        p = attr->value;
        /* take up to either space or nul-terminator */
        while (p && *p && *p == ' ')
          p++;
        v = (char *) p;
        /* take up to either space or nul-terminator */
        while (p && *p && *p != ' ')
          p++;
        g_assert (v != p);
        v = g_strndup (v, p - v);
        GST_INFO ("rid = %s", v);

        fail_unless (FALSE == gst_g_ptr_array_find_str (seen_rid, v, NULL),
            "duplicate/multiple rid for media %u", i);
        for (k = 0; k < expected_rid->n_rid; k++) {
          GST_LOG ("expected %u = %s", k, expected_rid->rid[k]);
          if (g_strcmp0 (v, expected_rid->rid[k]) == 0)
            break;
        }
        fail_unless (k < expected_rid->n_rid, "rid %s not found in media %u",
            v, i);
        g_ptr_array_add (seen_rid, v);
      }
    }
    fail_unless (seen_rid->len == expected_rid->n_rid,
        "mismatch in number of rid's in media %u, seen %u, expected %u", i,
        seen_rid->len, expected_rid->n_rid);
    g_ptr_array_unref (seen_rid);
  }
}

static void
do_test_simulcast (gboolean enable_fec_rtx)
{
  struct test_webrtc *t = test_webrtc_new ();
  guint media_format_count[] = { enable_fec_rtx ? 5 : 1, };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, NULL);
  VAL_SDP_INIT (payloads, on_sdp_media_no_duplicate_payloads, NULL,
      &media_formats);
  const char *expected_rids0[] = { "a", "z" };
  struct ExpectedRid expected_rids = { G_N_ELEMENTS (expected_rids0),
    expected_rids0
  };
  VAL_SDP_INIT (rids, on_sdp_media_rid, &expected_rids, &payloads);
  VAL_SDP_INIT (non_reject, _count_non_rejected_media,
      GUINT_TO_POINTER (1), &rids);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (1), &non_reject);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup, &count);
  const gchar *expected_answer_setup[] = { "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &count);
  const gchar *expected_offer_direction[] = { "sendrecv", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_direction[] = { "recvonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstHarness *h;
  GObject *trans;
  guint i;
  GstElement *rtpbin2;
  GstBuffer *buf;
  guint mid_ext_id = 1;
  guint stream_ext_id = 2;
  guint repaired_ext_id = 3;
  const char *mid = "5";
  guint ssrcs[] = { 123456789, 987654321 };
  GArray *ssrcs_received;
  GstCaps *caps;
  struct pad_properties pad_prop = { mid, 0 };
  struct new_pad_validate_properties validate_pad = { 1, &pad_prop };
  struct pad_added_harness_data pad_added_data =
      { NULL, on_pad_added_validate_props, &validate_pad };

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_harness;
  t->pad_added_data = &pad_added_data;

  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-bundle");
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  if (enable_fec_rtx) {
    g_signal_connect (t->webrtc1, "on-new-transceiver",
        G_CALLBACK (on_new_transceiver_set_rtx_fec), NULL);
    g_signal_connect (t->webrtc2, "on-new-transceiver",
        G_CALLBACK (on_new_transceiver_set_rtx_fec), NULL);
  }

  rtpbin2 = gst_bin_get_by_name (GST_BIN (t->webrtc2), "rtpbin");
  fail_unless (rtpbin2 != NULL);
  g_signal_connect (rtpbin2, "new-jitterbuffer",
      G_CALLBACK (new_jitterbuffer_set_fast_start), NULL);
  g_object_unref (rtpbin2);

  h = gst_harness_new_with_element (t->webrtc1, "sink_0", NULL);
  add_simulcast_audio_test_src_harness (h, expected_rids.n_rid, ssrcs, mid,
      mid_ext_id, expected_rids.rid, stream_ext_id, repaired_ext_id);
  t->harnesses = g_list_prepend (t->harnesses, h);

  /* setup recvonly transceiver as answer */
  caps =
      create_simulcast_audio_caps
      (GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, expected_rids.n_rid,
      ssrcs, mid, mid_ext_id, expected_rids.rid, stream_ext_id,
      repaired_ext_id);
  g_signal_emit_by_name (t->webrtc2, "add-transceiver",
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, caps, &trans);
  gst_clear_caps (&caps);
  fail_unless (trans != NULL);
  g_clear_object (&trans);

  test_validate_sdp (t, &offer, &answer);

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  for (i = 0; i < 10; i++) {
    gst_harness_push_from_src (h);
  }

  ssrcs_received = g_array_new (FALSE, TRUE, sizeof (guint32));

  /* Get one buffer out for each ssrc sent.
   */
  g_mutex_lock (&t->lock);
  while (ssrcs_received->len < G_N_ELEMENTS (ssrcs)) {
    GList *sink_harnesses = pad_added_data.sink_harnesses;
    GList *l;
    guint i;

    gst_harness_push_from_src (h);
    if (g_list_length (pad_added_data.sink_harnesses) < 2) {
      g_cond_wait_until (&t->cond, &t->lock, g_get_monotonic_time () + 5000);
      if (g_list_length (pad_added_data.sink_harnesses) < 2)
        continue;
    }

    g_mutex_unlock (&t->lock);

    for (l = sink_harnesses; l; l = l->next) {
      GstHarness *sink_harness = (GstHarness *) l->data;
      GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
      guint ssrc;

      fail_unless (sink_harness->element == t->webrtc2);

      buf = gst_harness_try_pull (sink_harness);
      if (!buf)
        continue;

      fail_unless (gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp));

      ssrc = gst_rtp_buffer_get_ssrc (&rtp);
      for (i = 0; i < ssrcs_received->len; i++) {
        if (g_array_index (ssrcs_received, guint, i) == ssrc)
          break;
      }
      if (i == ssrcs_received->len) {
        g_array_append_val (ssrcs_received, ssrc);
      }

      gst_rtp_buffer_unmap (&rtp);

      gst_buffer_unref (buf);
    }
    g_mutex_lock (&t->lock);
  }
  g_mutex_unlock (&t->lock);

  test_webrtc_free (t);
  g_list_free (pad_added_data.sink_harnesses);

  g_array_unref (ssrcs_received);
}

GST_START_TEST (test_simulcast)
{
  do_test_simulcast (FALSE);
}

GST_END_TEST;

GST_START_TEST (test_simulcast_fec_rtx)
{
  do_test_simulcast (TRUE);
}

GST_END_TEST;

GST_START_TEST (test_bundle_multiple_media_rtx_payload_mapping)
{
  struct test_webrtc *t = test_webrtc_new ();
  guint offer_media_format_count[] = { 5, 5, };
  VAL_SDP_INIT (payloads0, on_sdp_media_payload_types, GUINT_TO_POINTER (0),
      NULL);
  VAL_SDP_INIT (payloads1, on_sdp_media_payload_types, GUINT_TO_POINTER (1),
      &payloads0);
  VAL_SDP_INIT (no_dup_payloads, on_sdp_media_no_duplicate_payloads, NULL,
      &payloads1);
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      offer_media_format_count, &no_dup_payloads);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", };
  VAL_SDP_INIT (setup, on_sdp_media_setup, expected_offer_setup,
      &media_formats);
  const gchar *expected_offer_direction[] = { "recvonly", "recvonly", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &setup);
  GstWebRTCRTPTransceiverDirection direction;
  GstWebRTCRTPTransceiver *trans;
  GstCaps *caps;

  /* add two identical transceivers that will only receive a vp8 stream and check that
   * the created offer has the same rtx/red mappings */
  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;

  gst_util_set_object_arg (G_OBJECT (t->webrtc1), "bundle-policy",
      "max-bundle");
  gst_util_set_object_arg (G_OBJECT (t->webrtc2), "bundle-policy",
      "max-bundle");

  /* setup recvonly transceiver */
  caps = gst_caps_from_string (VP8_RTP_CAPS (97));
  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  fail_unless (trans != NULL);
  g_object_set (GST_OBJECT (trans), "do-nack", TRUE, "fec-type",
      GST_WEBRTC_FEC_TYPE_ULP_RED, NULL);
  gst_object_unref (trans);

  direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
  g_signal_emit_by_name (t->webrtc1, "add-transceiver", direction, caps,
      &trans);
  fail_unless (trans != NULL);
  g_object_set (GST_OBJECT (trans), "do-nack", TRUE, "fec-type",
      GST_WEBRTC_FEC_TYPE_ULP_RED, NULL);
  gst_object_unref (trans);

  gst_caps_unref (caps);

  /* don't really care about the answer */
  test_validate_sdp (t, &offer, NULL);

  test_webrtc_free (t);
}

GST_END_TEST;

static void
add_media_line (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  GstSDPMedia *media = NULL;
  const GstSDPMedia *existing_media;
  GstSDPResult res;

  existing_media = gst_sdp_message_get_media (desc->sdp, 0);

  res = gst_sdp_media_copy (existing_media, &media);
  fail_unless (res == GST_SDP_OK);
  res = gst_sdp_message_add_media (desc->sdp, media);
  fail_unless (res == GST_SDP_OK);
  gst_sdp_media_free (media);
}

static void
on_answer_set_rejected (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  const GstStructure *s;
  GError *error = NULL;
  GError *compare_error = user_data;

  s = gst_promise_get_reply (promise);
  fail_unless (s != NULL);
  gst_structure_get (s, "error", G_TYPE_ERROR, &error, NULL);
  fail_unless (g_error_matches (error, compare_error->domain,
          compare_error->code));
  fail_unless_equals_string (compare_error->message, error->message);
  g_clear_error (&error);
}

GST_START_TEST (test_invalid_add_media_in_answer)
{
  struct test_webrtc *t = create_audio_test ();
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (1),
      &media_formats);
  const gchar *expected_offer_setup[] = { "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup, &count);
  const gchar *expected_offer_direction[] = { "sendrecv", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  VAL_SDP_INIT (answer, add_media_line, NULL, NULL);
  GError answer_set_error = { GST_WEBRTC_ERROR,
    GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
    (gchar *) "Answer doesn't have the same number of m-lines as the offer."
  };

  /* Ensure that if the answer has more m-lines than the offer, it gets
   * rejected.
   */

  t->on_answer_set = on_answer_set_rejected;
  t->answer_set_data = &answer_set_error;

  test_validate_sdp (t, &offer, &answer);
  test_webrtc_free (t);
}

GST_END_TEST;

#define VALID_TURN_SERVER_URL1   "turn://testuser:testpass@test.com:1234"
#define VALID_TURN_SERVER_URL2   "turns://1665056262%3Atestuser:T4VwcehYgPAa5bpFAO14gVE19so=@test.com:1234"
#define INVALID_TURN_SERVER_URL1 "testuser@testpass@test.com:1234"      /* protocol of uri is missing */
#define INVALID_TURN_SERVER_URL2 "turns://testuser:testpass/@test.com:1234"     /* unescaped character in password */
#define INVALID_TURN_SERVER_URL3 "turns://test.com:1234"        /* 'user:pass' is missing */

GST_START_TEST (test_add_turn_server)
{
  struct test_webrtc *t = test_webrtc_new ();
  gboolean ret = FALSE;

  g_signal_emit_by_name (t->webrtc1, "add-turn-server",
      VALID_TURN_SERVER_URL1, &ret);
  fail_unless (ret != FALSE);

  g_signal_emit_by_name (t->webrtc1, "add-turn-server",
      VALID_TURN_SERVER_URL2, &ret);
  fail_unless (ret != FALSE);

  g_signal_emit_by_name (t->webrtc1, "add-turn-server",
      INVALID_TURN_SERVER_URL1, &ret);
  fail_unless (ret != TRUE);

  g_signal_emit_by_name (t->webrtc1, "add-turn-server",
      INVALID_TURN_SERVER_URL2, &ret);
  fail_unless (ret != TRUE);

  g_signal_emit_by_name (t->webrtc1, "add-turn-server",
      INVALID_TURN_SERVER_URL3, &ret);
  fail_unless (ret != TRUE);

  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_data_channel_recreate_offer)
{
  GstHarness *h;
  GstWebRTCDataChannel *channel;
  GstPromise *promise;
  const GstStructure *s;
  GstPromiseResult res;
  GstPad *pad;

  h = gst_harness_new_with_padnames ("webrtcbin", "sink_0", NULL);
  add_audio_test_src_harness (h, 0xDEADBEEF);

  g_signal_emit_by_name (h->element, "create-data-channel", "label", NULL,
      &channel);
  fail_unless (GST_IS_WEBRTC_DATA_CHANNEL (channel));

  pad = gst_element_get_static_pad (h->element, "sink_0");
  fail_unless (pad != NULL);

  promise = gst_promise_new ();
  g_signal_emit_by_name (h->element, "create-offer", NULL, promise);
  res = gst_promise_wait (promise);
  fail_unless_equals_int (res, GST_PROMISE_RESULT_REPLIED);
  s = gst_promise_get_reply (promise);
  fail_unless (s != NULL);
  gst_promise_unref (promise);

  promise = gst_promise_new ();
  g_signal_emit_by_name (h->element, "create-offer", NULL, promise);
  res = gst_promise_wait (promise);
  fail_unless_equals_int (res, GST_PROMISE_RESULT_REPLIED);
  s = gst_promise_get_reply (promise);
  fail_unless (s != NULL);
  gst_promise_unref (promise);

  gst_object_unref (pad);
  gst_object_unref (channel);
  gst_harness_teardown (h);
}

GST_END_TEST;

static void
validate_msid (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  char **expected_msid = user_data;
  int i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);
    gboolean have_msid = FALSE;
    char *prev_msid = NULL;
    int j;

    for (j = 0; j < gst_sdp_media_attributes_len (media); j++) {
      const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, j);
      const char *start;

      if (!attr->value)
        continue;

      start = strstr (attr->value, "msid:");
      if (start) {
        const char *end;
        char *msid;

        start += strlen ("msid:");
        end = strstr (start, " ");
        msid = g_strndup (start, end - start);
        fail_unless (end, "Invalid msid attribute");
        fail_if (have_msid && g_strcmp0 (prev_msid, msid) != 0,
            "different values for multiple msid values at mline %u, "
            "prev msid %s, msid %s", i, prev_msid, msid);
        have_msid = TRUE;
        fail_unless_equals_string (msid, expected_msid[i]);
        g_clear_pointer (&prev_msid, g_free);
        prev_msid = msid;
      }
    }
    g_clear_pointer (&prev_msid, g_free);
    fail_unless (have_msid, "no msid attribute in media %u", i);
  }
}

static void
_pad_added_src_check_msid (struct test_webrtc *t, GstElement * element,
    GstPad * pad, gpointer user_data)
{
  const char *expected_msid = user_data;
  char *msid;

  if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC)
    return;

  g_object_get (pad, "msid", &msid, NULL);
  fail_unless_equals_string (msid, expected_msid);
  g_clear_pointer (&msid, g_free);

  test_webrtc_signal_state_unlocked (t, STATE_CUSTOM);
}

GST_START_TEST (test_msid)
{
  struct test_webrtc *t = create_audio_test ();
  VAL_SDP_INIT (no_duplicate_payloads, on_sdp_media_no_duplicate_payloads,
      NULL, NULL);
  guint media_format_count[] = { 1, 5 };
  VAL_SDP_INIT (media_formats, on_sdp_media_count_formats,
      media_format_count, &no_duplicate_payloads);
  VAL_SDP_INIT (count, _count_num_sdp_media, GUINT_TO_POINTER (2),
      &media_formats);
  const gchar *expected_offer_msid[] = { "a1", "a1", };
  VAL_SDP_INIT (offer_msid, validate_msid, expected_offer_msid, &count);
  const gchar *expected_offer_setup[] = { "actpass", "actpass", };
  VAL_SDP_INIT (offer_setup, on_sdp_media_setup, expected_offer_setup,
      &offer_msid);
  const gchar *expected_offer_direction[] = { "sendrecv", "sendrecv", };
  VAL_SDP_INIT (offer, on_sdp_media_direction, expected_offer_direction,
      &offer_setup);
  const gchar *expected_answer_setup[] = { "active", "active", };
  VAL_SDP_INIT (answer_setup, on_sdp_media_setup, expected_answer_setup,
      &count);
  const gchar *expected_answer_direction[] = { "recvonly", "recvonly", };
  VAL_SDP_INIT (answer, on_sdp_media_direction, expected_answer_direction,
      &answer_setup);
  GstPad *pad;
  GstHarness *src;
  GstElement *rtpbin2;

  t->on_pad_added = _pad_added_src_check_msid;
  t->pad_added_data = (gpointer) "a1";

  rtpbin2 = gst_bin_get_by_name (GST_BIN (t->webrtc2), "rtpbin");
  fail_unless (rtpbin2 != NULL);
  g_signal_connect (rtpbin2, "new-jitterbuffer",
      G_CALLBACK (new_jitterbuffer_set_fast_start), NULL);
  g_object_unref (rtpbin2);

  g_signal_connect (t->webrtc1, "on-new-transceiver",
      G_CALLBACK (on_new_transceiver_set_rtx_fec), NULL);
  g_signal_connect (t->webrtc2, "on-new-transceiver",
      G_CALLBACK (on_new_transceiver_set_rtx_fec), NULL);

  src = gst_harness_new_with_element (t->webrtc1, "sink_1", NULL);
  add_audio_test_src_harness (src, 0x12345678);
  t->harnesses = g_list_prepend (t->harnesses, src);

  pad = gst_element_get_static_pad (t->webrtc1, "sink_0");
  g_object_set (pad, "msid", "a1", NULL);
  gst_clear_object (&pad);

  pad = gst_element_get_static_pad (t->webrtc1, "sink_1");
  g_object_set (pad, "msid", "a1", NULL);
  gst_clear_object (&pad);

  test_validate_sdp (t, &offer, &answer);

  fail_if (gst_element_set_state (t->webrtc1,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE);

  while (TRUE) {
    gst_harness_push_from_src (src);

    if (test_webrtc_check_for_state_mask (t, 1 << STATE_CUSTOM))
      break;

    g_usleep (10 * 1000);
  }

  test_webrtc_free (t);
}

GST_END_TEST;

static void
_check_ice_end_of_candidates (struct test_webrtc *t, GstElement * element,
    guint mlineindex, gchar * candidate, GstElement * other, gpointer user_data)
{
  gint *end_count = user_data;

  if (!candidate || candidate[0] == '\0') {
    g_atomic_int_inc (end_count);
  }
}

static void
sdp_media_has_end_of_candidates (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  guint i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);

    fail_unless_equals_string (gst_sdp_media_get_attribute_val_n (media,
            "end-of-candidates", 0), "");

    fail_unless (gst_sdp_media_get_attribute_val_n (media, "end-of-candidates",
            1) == NULL);
  }
}

GST_START_TEST (test_ice_end_of_candidates)
{
  struct test_webrtc *t = create_audio_test ();
  GstWebRTCSessionDescription *local_desc;
  gint end_candidate_count = 0;

  VAL_SDP_INIT (offer, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);
  VAL_SDP_INIT (answer, _count_num_sdp_media, GUINT_TO_POINTER (1), NULL);


  t->on_ice_candidate = _check_ice_end_of_candidates;
  t->ice_candidate_data = &end_candidate_count;
  test_validate_sdp (t, &offer, &answer);

  test_webrtc_wait_for_ice_gathering_complete (t);

  fail_unless_equals_int (end_candidate_count, 2);

  g_object_get (t->webrtc1, "current-local-description", &local_desc, NULL);
  sdp_media_has_end_of_candidates (t, t->webrtc1, local_desc, NULL);
  gst_webrtc_session_description_free (local_desc);

  test_webrtc_free (t);
}

GST_END_TEST;

static void
_set_setup_session_attr_on_answer (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  GstSDPMessage *sdp;
  GstSDPMessage *modified_sdp = NULL;
  const GstSDPMedia *media;
  GstSDPMedia *modified_media;
  const gchar *attr;

  if (TEST_IS_OFFER_ELEMENT (t, element))
    return;

  sdp = t->answer_desc->sdp;
  media = gst_sdp_message_get_media (sdp, 0);
  attr = gst_sdp_media_get_attribute_val (media, "setup");

  /* Remove the setup attribute from first media */
  gst_sdp_media_copy (media, &modified_media);
  for (unsigned index = 0;
      index < gst_sdp_media_attributes_len (modified_media); index++) {
    const GstSDPAttribute *current =
        gst_sdp_media_get_attribute (modified_media, index);
    if (!g_str_equal (current->key, "setup"))
      continue;
    gst_sdp_media_remove_attribute (modified_media, index);
    break;
  }

  gst_sdp_message_copy (sdp, &modified_sdp);

  /* Add session-level setup attribute to modified answer */
  gst_sdp_message_add_attribute (modified_sdp, "setup", attr);

  /* Replace first media of answer with a media without session attribute */
  gst_sdp_message_remove_media (modified_sdp, 0);
  gst_sdp_message_add_media (modified_sdp, modified_media);
  gst_sdp_media_free (modified_media);

  gst_sdp_message_free (sdp);
  t->answer_desc->sdp = modified_sdp;
}

static void
_offer_created_do_nothing (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
}

GST_START_TEST (test_sdp_session_setup_attribute)
{
  struct test_webrtc *t = create_audio_test ();

  t->on_offer_created = _offer_created_do_nothing;
  t->on_answer_created = _set_setup_session_attr_on_answer;

  fail_if (gst_element_set_state (t->webrtc1, GST_STATE_READY) ==
      GST_STATE_CHANGE_FAILURE);
  fail_if (gst_element_set_state (t->webrtc2, GST_STATE_READY) ==
      GST_STATE_CHANGE_FAILURE);
  test_webrtc_create_offer (t);
  test_webrtc_wait_for_state_mask (t, 1 << STATE_ANSWER_SET);

  test_webrtc_wait_for_ice_gathering_complete (t);

  test_webrtc_free (t);
}

GST_END_TEST;

static Suite *
webrtcbin_suite (void)
{
  Suite *s = suite_create ("webrtcbin");
  TCase *tc = tcase_create ("general");
  GstPluginFeature *nicesrc, *nicesink, *dtlssrtpdec, *dtlssrtpenc;
  GstPluginFeature *sctpenc, *sctpdec;
  GstRegistry *registry;

  registry = gst_registry_get ();
  nicesrc = gst_registry_lookup_feature (registry, "nicesrc");
  nicesink = gst_registry_lookup_feature (registry, "nicesink");
  dtlssrtpenc = gst_registry_lookup_feature (registry, "dtlssrtpenc");
  dtlssrtpdec = gst_registry_lookup_feature (registry, "dtlssrtpdec");
  sctpenc = gst_registry_lookup_feature (registry, "sctpenc");
  sctpdec = gst_registry_lookup_feature (registry, "sctpdec");

  tcase_add_test (tc, test_no_nice_elements_request_pad);
  tcase_add_test (tc, test_no_nice_elements_state_change);
  if (nicesrc && nicesink && dtlssrtpenc && dtlssrtpdec) {
    tcase_add_test (tc, test_sdp_no_media);
    tcase_add_test (tc, test_session_stats);
    tcase_add_test (tc, test_stats_with_stream);
    tcase_add_test (tc, test_audio);
    tcase_add_test (tc, test_ice_port_restriction);
    tcase_add_test (tc, test_audio_video);
    tcase_add_test (tc, test_media_direction);
    tcase_add_test (tc, test_add_transceiver);
    tcase_add_test (tc, test_get_transceivers);
    tcase_add_test (tc, test_add_recvonly_transceiver);
    tcase_add_test (tc, test_recvonly_sendonly);
    tcase_add_test (tc, test_payload_types);
    tcase_add_test (tc, test_bundle_audio_video_max_bundle_max_bundle);
    tcase_add_test (tc, test_bundle_audio_video_max_bundle_none);
    tcase_add_test (tc, test_bundle_audio_video_max_compat_max_bundle);
    tcase_add_test (tc, test_dual_audio);
    tcase_add_test (tc, test_duplicate_nego);
    tcase_add_test (tc, test_renego_add_stream);
    tcase_add_test (tc, test_bundle_renego_add_stream);
    tcase_add_test (tc, test_bundle_max_compat_max_bundle_renego_add_stream);
    tcase_add_test (tc, test_renego_transceiver_set_direction);
    tcase_add_test (tc, test_renego_lose_media_fails);
    tcase_add_test (tc,
        test_bundle_codec_preferences_rtx_no_duplicate_payloads);
    tcase_add_test (tc, test_reject_request_pad);
    tcase_add_test (tc, test_reject_create_offer);
    tcase_add_test (tc, test_reject_create_offer_mline_locked_no_caps);
    tcase_add_test (tc, test_reject_set_description);
    tcase_add_test (tc, test_force_second_media);
    tcase_add_test (tc, test_codec_preferences_caps);
    tcase_add_test (tc, test_codec_preferences_negotiation_sinkpad);
    tcase_add_test (tc, test_codec_preferences_negotiation_srcpad);
    tcase_add_test (tc, test_codec_preferences_in_on_new_transceiver);
    tcase_add_test (tc, test_codec_preferences_no_duplicate_extmaps);
    tcase_add_test (tc, test_codec_preferences_incompatible_extmaps);
    tcase_add_test (tc, test_codec_preferences_invalid_extmap);
    tcase_add_test (tc, test_renego_rtx);
    tcase_add_test (tc, test_bundle_mid_header_extension);
    tcase_add_test (tc, test_max_bundle_fec);
    tcase_add_test (tc, test_simulcast);
    tcase_add_test (tc, test_simulcast_fec_rtx);
    tcase_add_test (tc, test_bundle_multiple_media_rtx_payload_mapping);
    tcase_add_test (tc, test_invalid_add_media_in_answer);
    tcase_add_test (tc, test_add_turn_server);
    tcase_add_test (tc, test_msid);
    tcase_add_test (tc, test_ice_end_of_candidates);
    tcase_add_test (tc, test_sdp_session_setup_attribute);
    if (sctpenc && sctpdec) {
      tcase_add_test (tc, test_data_channel_create);
      tcase_add_test (tc, test_data_channel_create_two_channels);
      tcase_add_test (tc, test_data_channel_remote_notify);
      tcase_add_test (tc, test_data_channel_transfer_string);
      tcase_add_test (tc, test_data_channel_transfer_data);
      tcase_add_test (tc, test_data_channel_create_after_negotiate);
      tcase_add_test (tc, test_data_channel_close);
      tcase_add_test (tc, test_data_channel_low_threshold);
      tcase_add_test (tc, test_data_channel_max_message_size);
      tcase_add_test (tc, test_data_channel_pre_negotiated);
      tcase_add_test (tc, test_bundle_audio_video_data);
      tcase_add_test (tc, test_renego_stream_add_data_channel);
      tcase_add_test (tc, test_renego_data_channel_add_stream);
      tcase_add_test (tc, test_renego_stream_data_channel_add_stream);
      tcase_add_test (tc, test_data_channel_recreate_offer);
    } else {
      GST_WARNING ("Some required elements were not found. "
          "All datachannel tests are disabled. sctpenc %p, sctpdec %p", sctpenc,
          sctpdec);
    }
  } else {
    GST_WARNING ("Some required elements were not found. "
        "All media tests are disabled. nicesrc %p, nicesink %p, "
        "dtlssrtpenc %p, dtlssrtpdec %p", nicesrc, nicesink, dtlssrtpenc,
        dtlssrtpdec);
  }

  if (nicesrc)
    gst_object_unref (nicesrc);
  if (nicesink)
    gst_object_unref (nicesink);
  if (dtlssrtpdec)
    gst_object_unref (dtlssrtpdec);
  if (dtlssrtpenc)
    gst_object_unref (dtlssrtpenc);
  if (sctpenc)
    gst_object_unref (sctpenc);
  if (sctpdec)
    gst_object_unref (sctpdec);

  suite_add_tcase (s, tc);

  return s;
}

GST_CHECK_MAIN (webrtcbin);
