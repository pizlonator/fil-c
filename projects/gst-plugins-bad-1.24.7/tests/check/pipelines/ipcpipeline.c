/* GStreamer
 *
 * tests for the ipcpipelinesrc/ipcpipelinesink elements
 *
 * Copyright (C) 2015-2017 YouView TV Ltd
 *   Author: Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
 *   Author: George Kiagiadakis <george.kiagiadakis@collabora.com>
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

#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <gst/check/gstcheck.h>
#include <gst/video/navigation.h>
#include <string.h>

#ifndef HAVE_PIPE2
static int
pipe2 (int pipedes[2], int flags)
{
  int ret = pipe (pipedes);
  if (ret < 0)
    return ret;
  if (flags != 0) {
    ret = fcntl (pipedes[0], F_SETFL, flags);
    if (ret < 0)
      return ret;
    ret = fcntl (pipedes[1], F_SETFL, flags);
    if (ret < 0)
      return ret;
  }
  return 0;
}
#endif

/* This enum contains flags that are used to configure the setup that
 * test_base() will do internally */
typedef enum
{
  /* Features related to the multi-process setup */
  TEST_FEATURE_SPLIT_SINKS = 0x1,       /* separate audio and video sink processes */
  TEST_FEATURE_RECOVERY_SLAVE_PROCESS = 0x2,
  TEST_FEATURE_RECOVERY_MASTER_PROCESS = 0x4,

  TEST_FEATURE_HAS_VIDEO = 0x10,
  TEST_FEATURE_LIVE = 0x20,     /* sets is-live=true in {audio,video}testsrc */
  TEST_FEATURE_ASYNC_SINK = 0x40,       /* sets sync=false in fakesink */
  TEST_FEATURE_ERROR_SINK = 0x80,       /* generates error message in the slave */
  TEST_FEATURE_LONG_DURATION = 0x100,   /* bigger num-buffers in {audio,video}testsrc */
  TEST_FEATURE_FILTER_SINK_CAPS = 0x200,        /* plugs capsfilter before fakesink */

  /* Source selection; Use only one of those, do not combine! */
  TEST_FEATURE_TEST_SOURCE = 0x400,
  TEST_FEATURE_WAV_SOURCE = 0x800,
  TEST_FEATURE_MPEGTS_SOURCE = 0x1000 | TEST_FEATURE_HAS_VIDEO,
  TEST_FEATURE_LIVE_A_SOURCE =
      TEST_FEATURE_TEST_SOURCE | TEST_FEATURE_LIVE | TEST_FEATURE_ASYNC_SINK,
  TEST_FEATURE_LIVE_AV_SOURCE =
      TEST_FEATURE_LIVE_A_SOURCE | TEST_FEATURE_HAS_VIDEO,
} TestFeatures;

/* This is the data structure that each function of the each test receives
 * in user_data. It contains pointers to stack-allocated, test-specific
 * structures that contain the test parameters (input data), the runtime
 * data of the master (source) process (master data) and the runtime data
 * of the slave (sink) process (slave data) */
typedef struct
{
  gpointer id;                  /* input data struct */
  gpointer md;                  /* master data struct */
  gpointer sd;                  /* slave data struct */

  TestFeatures features;        /* the features that this test is running with */

  /* whether there is both an audio and a video stream
   * in this process'es pipeline */
  gboolean two_streams;

  /* the pipeline of this process; could be either master or slave */
  GstElement *p;

  /* this callback will be called in the master process when
   * the master gets STATE_CHANGED with the new state being state_target */
  void (*state_changed_cb) (gpointer);
  GstState state_target;

  /* used by EXCLUSIVE_CALL() */
  gint exclusive_call_counter;
} test_data;

/* All pipelines do not start buffers at exactly zero, so we consider
   timestamps within a small tolerance to be zero */
#define CLOSE_ENOUGH_TO_ZERO (GST_SECOND / 5)

/* milliseconds */
#define STEP_AT  100
#define PAUSE_AT 500
#define SEEK_AT  700
#define QUERY_AT 600
#define MESSAGE_AT 600
#define CRASH_AT 600
#define STOP_AT  600

/* Rough duration of the sample files we use */
#define MPEGTS_SAMPLE_ROUGH_DURATION (GST_SECOND * 64 / 10)
#define WAV_SAMPLE_ROUGH_DURATION (GST_SECOND * 65 / 10)

enum
{
  MSG_ACK = 0,
  MSG_START = 1
};

static GMainLoop *loop;
static gboolean child_dead;
static int pipesfa[2], pipesba[2], pipesfv[2], pipesbv[2];
static int ctlsock[2];
static int recovery_pid = 0;
static int check_fd = -1;
static GList *weak_refs = NULL;

/* lock helpers */

#define FAIL_IF(x) do { lock_check (); fail_if(x); unlock_check (); } while(0)
#define FAIL_UNLESS(x) do { lock_check (); fail_unless(x); unlock_check (); } while(0)
#define FAIL_UNLESS_EQUALS_INT(x,y) do { lock_check (); fail_unless_equals_int(x,y); unlock_check (); } while(0)
#define FAIL() do { lock_check (); fail(); unlock_check (); } while(0)

static void
lock_check (void)
{
  flock (check_fd, LOCK_EX);
}

static void
unlock_check (void)
{
  flock (check_fd, LOCK_UN);
}

static void
setup_lock (void)
{
  gchar *name = NULL;
  check_fd = g_file_open_tmp (NULL, &name, NULL);
  unlink (name);
  g_free (name);
}

/* tracking for ipcpipeline elements; this is used mainly to detect leaks,
 * but also to provide a method for calling "disconnect" on all of them
 * in the tests that require it */

static void
remove_weak_ref (GstElement * element)
{
  weak_refs = g_list_remove (weak_refs, element);
}

static void
add_weak_ref (GstElement * element)
{
  weak_refs = g_list_append (weak_refs, element);
  g_object_weak_ref (G_OBJECT (element), (GWeakNotify) remove_weak_ref,
      element);
}

static void
disconnect_ipcpipeline_elements (void)
{
  GList *l;

  for (l = weak_refs; l; l = l->next) {
    g_signal_emit_by_name (G_OBJECT (l->data), "disconnect", NULL);
  }
}

/* helper functions */

#define EXCLUSIVE_CALL(td,func) \
  G_STMT_START { \
    if (!td->two_streams || \
        g_atomic_int_add (&td->exclusive_call_counter, 1) == 1) { \
      func; \
    } \
  } G_STMT_END

static void
cleanup_bus (GstElement * pipeline)
{
  gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));
  gst_bus_set_flushing (GST_ELEMENT_BUS (pipeline), TRUE);
}

static void
setup_log (const char *logfile, int append)
{
  FILE *f;

  f = fopen (logfile, append ? "a+" : "w");
  gst_debug_add_log_function (gst_debug_log_default, f, NULL);
}

static GstElement *
create_pipeline (const char *type)
{
  GstElement *pipeline;

  pipeline = gst_element_factory_make (type, NULL);
  FAIL_UNLESS (pipeline);

  return pipeline;
}

static GQuark
to_be_removed_quark (void)
{
  static GQuark q = 0;
  if (!q)
    q = g_quark_from_static_string ("to_be_removed");
  return q;
}

static gboolean
are_caps_audio (const GstCaps * caps)
{
  GstStructure *structure;
  const char *name;

  structure = gst_caps_get_structure (caps, 0);
  name = gst_structure_get_name (structure);
  return g_str_has_prefix (name, "audio/");
}

static gboolean
are_caps_video (const GstCaps * caps)
{
  GstStructure *structure;
  const char *name;

  structure = gst_caps_get_structure (caps, 0);
  name = gst_structure_get_name (structure);
  return (g_str_has_prefix (name, "video/")
      && strcmp (name, "video/x-dvd-subpicture"));
}

static int
caps2idx (GstCaps * caps, gboolean two_streams)
{
  int idx;

  if (!two_streams)
    return 0;

  if (are_caps_audio (caps)) {
    idx = 0;
  } else if (are_caps_video (caps)) {
    idx = 1;
  } else {
    FAIL_IF (1);
    idx = 0;
  }
  return idx;
}

static int
pad2idx (GstPad * pad, gboolean two_streams)
{
  GstCaps *caps;
  int idx;

  if (!two_streams)
    return 0;

  caps = gst_pad_get_current_caps (pad);
  if (!caps)
    caps = gst_pad_get_pad_template_caps (pad);
  FAIL_UNLESS (caps);

  idx = caps2idx (caps, two_streams);

  gst_caps_unref (caps);
  return idx;
}

static gboolean
stop_pipeline (gpointer user_data)
{
  GstElement *pipeline = user_data;
  GstStateChangeReturn ret;

  ret = gst_element_set_state (pipeline, GST_STATE_NULL);
  FAIL_IF (ret == GST_STATE_CHANGE_FAILURE);
  gst_object_unref (pipeline);
  g_main_loop_quit (loop);
  return FALSE;
}

static void
hook_peer_probe_types (const GValue * sinkv, GstPadProbeCallback probe,
    unsigned int types, gpointer user_data)
{
  GstElement *sink;
  GstPad *pad, *peer;

  sink = g_value_get_object (sinkv);
  FAIL_UNLESS (sink);
  pad = gst_element_get_static_pad (sink, "sink");
  FAIL_UNLESS (pad);
  peer = gst_pad_get_peer (pad);
  FAIL_UNLESS (peer);
  gst_pad_add_probe (peer, types, probe, user_data, NULL);
  gst_object_unref (peer);
  gst_object_unref (pad);
}

static void
hook_probe_types (const GValue * sinkv, GstPadProbeCallback probe,
    unsigned int types, gpointer user_data)
{
  GstElement *sink;
  GstPad *pad;

  sink = g_value_get_object (sinkv);
  FAIL_UNLESS (sink);
  pad = gst_element_get_static_pad (sink, "sink");
  FAIL_UNLESS (pad);
  gst_pad_add_probe (pad, types, probe, user_data, NULL);
  gst_object_unref (pad);
}

static void
hook_probe (const GValue * sinkv, GstPadProbeCallback probe, gpointer user_data)
{
  hook_probe_types (sinkv, probe,
      GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_FLUSH |
      GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, user_data);
}

/* the master process'es async GstBus callback */
static gboolean
master_bus_msg (GstBus * bus, GstMessage * message, gpointer user_data)
{
  test_data *td = user_data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      /* elements we are removing might error out as they are taken out
         of the pipeline, and fail to push. We don't care about those. */
      if (g_object_get_qdata (G_OBJECT (GST_MESSAGE_SRC (message)),
              to_be_removed_quark ()))
        break;

      gst_message_parse_error (message, &err, &dbg);
      g_printerr ("ERROR: %s\n", err->message);
      if (dbg != NULL)
        g_printerr ("ERROR debug information: %s\n", dbg);
      g_error_free (err);
      g_free (dbg);
      g_assert_not_reached ();
      break;
    }
    case GST_MESSAGE_WARNING:{
      GError *err;
      gchar *dbg;

      gst_message_parse_warning (message, &err, &dbg);
      g_printerr ("WARNING: %s\n", err->message);
      if (dbg != NULL)
        g_printerr ("WARNING debug information: %s\n", dbg);
      g_error_free (err);
      g_free (dbg);
      g_assert_not_reached ();
      break;
    }
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC (message) == GST_OBJECT_CAST (td->p)
          && td->state_changed_cb) {
        GstState state;
        gst_message_parse_state_changed (message, NULL, &state, NULL);
        if (state == td->state_target)
          td->state_changed_cb (td);
      }
      break;
    default:
      break;
  }
  return TRUE;
}

/* source construction functions */

static GstElement *
create_wavparse_source_loc (const char *loc, int fdina, int fdouta)
{
  GstElement *sbin, *pipeline, *filesrc, *ipcpipelinesink;
  GError *e = NULL;

  pipeline = create_pipeline ("pipeline");
  sbin =
      gst_parse_bin_from_description ("pushfilesrc name=filesrc ! wavparse",
      TRUE, &e);
  FAIL_IF (e || !sbin);
  gst_element_set_name (sbin, "source");
  filesrc = gst_bin_get_by_name (GST_BIN (sbin), "filesrc");
  FAIL_UNLESS (filesrc);
  g_object_set (filesrc, "location", loc, NULL);
  gst_object_unref (filesrc);
  ipcpipelinesink =
      gst_element_factory_make ("ipcpipelinesink", "ipcpipelinesink");
  add_weak_ref (ipcpipelinesink);
  g_object_set (ipcpipelinesink, "fdin", fdina, "fdout", fdouta, NULL);
  gst_bin_add_many (GST_BIN (pipeline), sbin, ipcpipelinesink, NULL);
  FAIL_UNLESS (gst_element_link_many (sbin, ipcpipelinesink, NULL));

  return pipeline;
}

static void
on_pad_added (GstElement * element, GstPad * pad, gpointer data)
{
  GstCaps *caps;
  GstElement *next;
  GstBin *pipeline = data;
  GstPad *sink_pad;

  caps = gst_pad_get_current_caps (pad);
  if (!caps)
    caps = gst_pad_get_pad_template_caps (pad);

  if (are_caps_video (caps)) {
    next = gst_bin_get_by_name (GST_BIN (pipeline), "vqueue");
  } else if (are_caps_audio (caps)) {
    next = gst_bin_get_by_name (GST_BIN (pipeline), "aqueue");
  } else {
    gst_caps_unref (caps);
    return;
  }
  gst_caps_unref (caps);

  FAIL_UNLESS (next);
  sink_pad = gst_element_get_static_pad (next, "sink");
  FAIL_UNLESS (sink_pad);
  FAIL_UNLESS (gst_pad_link (pad, sink_pad) == GST_PAD_LINK_OK);
  gst_object_unref (sink_pad);

  gst_object_unref (next);
}

static GstElement *
create_mpegts_source_loc (const char *loc, int fdina, int fdouta, int fdinv,
    int fdoutv)
{
  GstElement *pipeline, *filesrc, *tsdemux, *aqueue, *vqueue, *aipcpipelinesink,
      *vipcpipelinesink;

  pipeline = create_pipeline ("pipeline");
  filesrc = gst_element_factory_make ("filesrc", NULL);
  g_object_set (filesrc, "location", loc, NULL);
  tsdemux = gst_element_factory_make ("tsdemux", NULL);
  g_signal_connect (tsdemux, "pad-added", G_CALLBACK (on_pad_added), pipeline);
  aqueue = gst_element_factory_make ("queue", "aqueue");
  aipcpipelinesink = gst_element_factory_make ("ipcpipelinesink", NULL);
  add_weak_ref (aipcpipelinesink);
  g_object_set (aipcpipelinesink, "fdin", fdina, "fdout", fdouta, NULL);
  vqueue = gst_element_factory_make ("queue", "vqueue");
  vipcpipelinesink = gst_element_factory_make ("ipcpipelinesink", NULL);
  add_weak_ref (vipcpipelinesink);
  g_object_set (vipcpipelinesink, "fdin", fdinv, "fdout", fdoutv, NULL);
  gst_bin_add_many (GST_BIN (pipeline), filesrc, tsdemux, aqueue,
      aipcpipelinesink, vqueue, vipcpipelinesink, NULL);
  FAIL_UNLESS (gst_element_link_many (filesrc, tsdemux, NULL));
  FAIL_UNLESS (gst_element_link_many (aqueue, aipcpipelinesink, NULL));
  FAIL_UNLESS (gst_element_link_many (vqueue, vipcpipelinesink, NULL));

  return pipeline;
}

static GstElement *
create_test_source (gboolean live, int fdina, int fdouta, int fdinv, int fdoutv,
    gboolean audio, gboolean video, gboolean Long)
{
  GstElement *pipeline, *audiotestsrc, *aipcpipelinesink;
  GstElement *videotestsrc, *vipcpipelinesink;
  int L = Long ? 2 : 1;

  pipeline = create_pipeline ("pipeline");

  if (audio) {
    audiotestsrc = gst_element_factory_make ("audiotestsrc", "audiotestsrc");
    g_object_set (audiotestsrc, "is-live", live, "num-buffers",
        live ? 270 * L : 600, NULL);
    aipcpipelinesink = gst_element_factory_make ("ipcpipelinesink",
        "aipcpipelinesink");
    add_weak_ref (aipcpipelinesink);
    g_object_set (aipcpipelinesink, "fdin", fdina, "fdout", fdouta, NULL);
    gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, aipcpipelinesink, NULL);
    FAIL_UNLESS (gst_element_link_many (audiotestsrc, aipcpipelinesink, NULL));
  }

  if (video) {
    videotestsrc = gst_element_factory_make ("videotestsrc", "videotestsrc");
    g_object_set (videotestsrc, "is-live", live, "num-buffers",
        live ? 190 * L : 600, NULL);
    vipcpipelinesink =
        gst_element_factory_make ("ipcpipelinesink", "vipcpipelinesink");
    add_weak_ref (vipcpipelinesink);
    g_object_set (vipcpipelinesink, "fdin", fdinv, "fdout", fdoutv, NULL);
    gst_bin_add_many (GST_BIN (pipeline), videotestsrc, vipcpipelinesink, NULL);
    FAIL_UNLESS (gst_element_link_many (videotestsrc, vipcpipelinesink, NULL));
  }

  return pipeline;
}

static GstElement *
create_source (TestFeatures features, int fdina, int fdouta, int fdinv,
    int fdoutv, test_data * td)
{
  GstElement *pipeline = NULL;
  gboolean live = !!(features & TEST_FEATURE_LIVE);
  gboolean longdur = !!(features & TEST_FEATURE_LONG_DURATION);
  gboolean has_video = !!(features & TEST_FEATURE_HAS_VIDEO);

  if (features & TEST_FEATURE_TEST_SOURCE) {

    pipeline = create_test_source (live, fdina, fdouta, fdinv, fdoutv, TRUE,
        has_video, longdur);
  } else if (features & TEST_FEATURE_WAV_SOURCE) {
    pipeline = create_wavparse_source_loc ("../../tests/files/sine.wav", fdina,
        fdouta);
  } else if (features & TEST_FEATURE_MPEGTS_SOURCE) {
    pipeline = create_mpegts_source_loc ("../../tests/files/test.ts", fdina,
        fdouta, fdinv, fdoutv);
  } else {
    g_assert_not_reached ();
  }

  td->two_streams = has_video;
  td->p = pipeline;

  if (pipeline)
    gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), master_bus_msg, td);

  return pipeline;
}

/* sink construction */

static GstElement *
create_sink (TestFeatures features, GstElement ** slave_pipeline,
    int fdin, int fdout, const char *filter_caps)
{
  GstElement *ipcpipelinesrc, *fakesink, *identity, *capsfilter, *endpoint;
  GstCaps *caps;

  if (!*slave_pipeline)
    *slave_pipeline = create_pipeline ("ipcslavepipeline");
  else
    gst_object_ref (*slave_pipeline);
  ipcpipelinesrc = gst_element_factory_make ("ipcpipelinesrc", NULL);
  add_weak_ref (ipcpipelinesrc);
  g_object_set (ipcpipelinesrc, "fdin", fdin, "fdout", fdout, NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (fakesink, "sync", !(features & TEST_FEATURE_ASYNC_SINK), NULL);
  gst_bin_add_many (GST_BIN (*slave_pipeline), ipcpipelinesrc, fakesink, NULL);
  endpoint = ipcpipelinesrc;

  if (features & TEST_FEATURE_ERROR_SINK &&
      !g_strcmp0 (filter_caps, "audio/x-raw")) {
    identity = gst_element_factory_make ("identity", "error-element");
    g_object_set (identity, "error-after", 5, NULL);
    gst_bin_add (GST_BIN (*slave_pipeline), identity);
    FAIL_UNLESS (gst_element_link_many (endpoint, identity, NULL));
    endpoint = identity;
  }

  if ((features & TEST_FEATURE_FILTER_SINK_CAPS) && filter_caps) {
    capsfilter = gst_element_factory_make ("capsfilter", NULL);
    caps = gst_caps_from_string (filter_caps);
    FAIL_UNLESS (caps);
    g_object_set (capsfilter, "caps", caps, NULL);
    gst_caps_unref (caps);
    gst_bin_add (GST_BIN (*slave_pipeline), capsfilter);
    FAIL_UNLESS (gst_element_link_many (endpoint, capsfilter, NULL));
    endpoint = capsfilter;
  }
  FAIL_UNLESS (gst_element_link_many (endpoint, fakesink, NULL));

  return *slave_pipeline;
}

static void
ensure_sink_setup (GstElement * sink, void (*setup_sink) (GstElement *, void *),
    gpointer user_data)
{
  static GQuark setup_done = 0;
  test_data *td = user_data;

  if (!setup_done)
    setup_done = g_quark_from_static_string ("setup_done");

  if (sink)
    td->p = sink;

  if (sink && setup_sink && !g_object_get_qdata (G_OBJECT (sink), setup_done)) {
    g_object_set_qdata (G_OBJECT (sink), setup_done, GINT_TO_POINTER (1));
    setup_sink (sink, user_data);
  }
}

/* GstCheck multi-process setup helpers */

static void
on_child_exit (int signal)
{
  int status = 0;
  if (waitpid (-1, &status, 0) > 0 && status) {
    FAIL ();
    exit (status);
  } else {
    child_dead = TRUE;
  }
}

static void
die_on_child_death (void)
{
  struct sigaction sa;

  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = on_child_exit;
  sigaction (SIGCHLD, &sa, NULL);
}

static void
wait_for_recovery (void)
{
  int value;

  FAIL_UNLESS (ctlsock[1]);
  FAIL_UNLESS (read (ctlsock[1], &value, sizeof (int)) == sizeof (int));
  FAIL_UNLESS (value == MSG_START);
}

static void
ack_recovery (void)
{
  int value = MSG_ACK;
  FAIL_UNLESS (ctlsock[1]);
  FAIL_UNLESS (write (ctlsock[1], &value, sizeof (int)) == sizeof (int));
}

static void
recreate_crashed_slave_process (void)
{
  int value = MSG_START;
  /* We don't recreate, because there seems to be some subtle issues
     with forking after gst has started running. So we create a new
     recovery process at start, and wake it up after the current
     slave dies, so it can take its place. It's a bit hacky, but it
     works. The spare process waits for SIGUSR2 to setup a replacement
     pipeline and connect to the master. */
  FAIL_UNLESS (recovery_pid);
  FAIL_UNLESS (ctlsock[0]);
  FAIL_UNLESS (write (ctlsock[0], &value, sizeof (int)) == sizeof (int));
  FAIL_UNLESS (read (ctlsock[0], &value, sizeof (int)) == sizeof (int));
  FAIL_UNLESS (value == MSG_ACK);
}

static gboolean
crash (gpointer user_data)
{
  _exit (0);
}

static gboolean
unwind (gpointer user_data)
{
  g_main_loop_quit (loop);
  return FALSE;
}

static void
on_unwind (int signal)
{
  g_idle_add (unwind, NULL);
}

static void
listen_for_unwind (void)
{
  struct sigaction sa;

  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = on_unwind;
  sigaction (SIGUSR1, &sa, NULL);
}

static void
stop_listening_for_unwind (void)
{
  struct sigaction sa;

  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = SIG_DFL;
  sigaction (SIGUSR1, &sa, NULL);
}

#define TEST_BASE(...) test_base(__FUNCTION__,##__VA_ARGS__)

/*
 * This is the main function driving the tests. All tests configure it
 * by way of all the function pointers it takes as arguments, which have
 * self-explanatory names.
 * Most tests are run over a number of different pipelines with the same
 * configuration (eg, a wavparse based pipeline, a live pipeline with
 * test audio/video, etc). Those pipelines that have more than one sink
 * (eg, MPEG-TS source demuxing audio and video) have a version with a
 * single slave pipeline and process, and a version with the audio and
 * video sinks in two different processes, each with its slave pipeline.
 * The master and slave crash tests are also run via this function, and
 * have specific code (grep for recovery).
 * There is a fair amount of hairy stuff to do with letting the main
 * check process when a subprocess has failed. Best not to look at it
 * and let it do its thing.
 * To add new tests, duplicate a set of tests, eg the *_end_of_stream
 * ones, and s/_end_of_stream/new_test_name/g. Then do the same for
 * the functions they pass as parameters to test_base. Typically, the
 * source creation sets a message hook to catch things like async-done
 * messages. Sink creation typically adds a probe to check that events,
 * buffers, etc, come through as expected. The two success functions
 * check all went well for the source and sink. Note that since all of
 * these functions take the same user data structure, and the process
 * will fork, writing something from one process will not be reflected
 * in the other, so there is usually a subset of data relevant to the
 * source, and another to the sink. But some have data relevant to both,
 * it depends on the test and what you are doing.
 * New tests do not have to use this framework, it just avoids spending
 * more time and effort on multi process handling.
 */
static void
test_base (const char *name, TestFeatures features,
    void (*run_source) (GstElement *, void *),
    void (*setup_sink) (GstElement *, void *),
    void (*check_success_source) (void *),
    void (*check_success_sink) (void *),
    gpointer input_data, gpointer master_data, gpointer slave_data)
{
  GstElement *source = NULL, *asink = NULL, *vsink = NULL;
  GstElement *slave_pipeline = NULL;
  GstStateChangeReturn ret;
  gboolean c_src, c_sink;
  pid_t pid = 0;
  unsigned char x;
  int master_recovery_pid_comm[2] = { -1, -1 };
  test_data td = { input_data, master_data, slave_data, features, FALSE, NULL,
    NULL, GST_STATE_NULL, 0
  };

  g_print ("Testing: %s\n", name);

  weak_refs = NULL;

  FAIL_IF (pipe2 (pipesfa, O_NONBLOCK) < 0);
  FAIL_IF (pipe2 (pipesba, O_NONBLOCK) < 0);
  FAIL_IF (pipe2 (pipesfv, O_NONBLOCK) < 0);
  FAIL_IF (pipe2 (pipesbv, O_NONBLOCK) < 0);
  FAIL_IF (socketpair (PF_UNIX, SOCK_STREAM, 0, ctlsock) < 0);

  FAIL_IF (pipesfa[0] < 0);
  FAIL_IF (pipesfa[1] < 0);
  FAIL_IF (pipesba[0] < 0);
  FAIL_IF (pipesba[1] < 0);
  FAIL_IF (pipesfv[0] < 0);
  FAIL_IF (pipesfv[1] < 0);
  FAIL_IF (pipesbv[0] < 0);
  FAIL_IF (pipesbv[1] < 0);

  gst_debug_remove_log_function (gst_debug_log_default);

  listen_for_unwind ();
  child_dead = FALSE;

  if (features & TEST_FEATURE_RECOVERY_MASTER_PROCESS) {
    /* the other master will let us know its child's PID so we can unwind
       it when we're finished */
    FAIL_IF (pipe2 (master_recovery_pid_comm, O_NONBLOCK) < 0);

    recovery_pid = fork ();
    if (recovery_pid > 0) {
      /* we're the main process that libcheck waits for */
      die_on_child_death ();
      while (!child_dead)
        g_usleep (1000);
      /* leave some time for the slave to timeout (1 second), record error, etc */
      g_usleep (1500 * 1000);

      /* Discard anything that was sent to the previous process when it died */
      while (read (pipesba[0], &x, 1) == 1);

      FAIL_UNLESS (read (master_recovery_pid_comm[0], &pid,
              sizeof (pid)) == sizeof (pid));

      setup_log ("gstsrc.log", TRUE);
      source = create_source (features, pipesba[0], pipesfa[1], pipesbv[0],
          pipesfv[1], &td);
      FAIL_UNLESS (source);
      if (run_source)
        run_source (source, &td);
      goto setup_done;
    }
  }

  if (features & TEST_FEATURE_RECOVERY_SLAVE_PROCESS) {
    recovery_pid = fork ();
    if (!recovery_pid) {
      wait_for_recovery ();

      /* Discard anything that was sent to the previous process when it died */
      while (read (pipesfa[0], &x, 1) == 1);

      setup_log ("gstasink.log", TRUE);
      asink = create_sink (features, &slave_pipeline, pipesfa[0], pipesba[1],
          "audio/x-raw");
      FAIL_UNLESS (asink);
      ensure_sink_setup (asink, setup_sink, &td);
      ack_recovery ();
      goto setup_done;
    }
  }

  pid = fork ();
  FAIL_IF (pid < 0);
  if (pid) {
    if (features & TEST_FEATURE_RECOVERY_MASTER_PROCESS) {
      FAIL_UNLESS (write (master_recovery_pid_comm[1], &pid,
              sizeof (pid)) == sizeof (pid));
    }
    die_on_child_death ();
    if (features & TEST_FEATURE_SPLIT_SINKS) {
      pid = fork ();
      FAIL_IF (pid < 0);
      if (pid) {
        die_on_child_death ();
      }
      c_src = !!pid;
      c_sink = !pid;
    } else {
      c_src = TRUE;
      c_sink = FALSE;
    }
    if (c_src) {
      setup_log ("gstsrc.log", FALSE);
      source = create_source (features, pipesba[0], pipesfa[1], pipesbv[0],
          pipesfv[1], &td);
      FAIL_UNLESS (source);
      run_source (source, &td);
    }
    if (c_sink) {
      setup_log ("gstasink.log", FALSE);
      asink = create_sink (features, &slave_pipeline, pipesfa[0], pipesba[1],
          "audio/x-raw");
      FAIL_UNLESS (asink);
    }
  } else {
    td.two_streams = (features & TEST_FEATURE_HAS_VIDEO) &&
        !(features & TEST_FEATURE_SPLIT_SINKS);

    if (features & TEST_FEATURE_HAS_VIDEO) {
      setup_log ("gstvsink.log", FALSE);
      vsink = create_sink (features, &slave_pipeline, pipesfv[0], pipesbv[1],
          "video/x-raw");
      FAIL_UNLESS (vsink);
    }
    if (!(features & TEST_FEATURE_SPLIT_SINKS)) {
      setup_log ("gstasink.log", FALSE);
      asink = create_sink (features, &slave_pipeline, pipesfa[0], pipesba[1],
          "audio/x-raw");
      FAIL_UNLESS (asink);
    }
  }

setup_done:
  ensure_sink_setup (asink, setup_sink, &td);
  ensure_sink_setup (vsink, setup_sink, &td);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  /* tell the child process to unwind too */
  stop_listening_for_unwind ();

  if (source) {
    ret = gst_element_set_state (source, GST_STATE_NULL);
    FAIL_UNLESS (ret == GST_STATE_CHANGE_SUCCESS
        || ret == GST_STATE_CHANGE_ASYNC);
  }

  if (pid)
    kill (pid, SIGUSR1);

  g_main_loop_unref (loop);

  if (source) {
    cleanup_bus (source);
    if (check_success_source)
      check_success_source (&td);
  } else {
    if (asink)
      cleanup_bus (asink);
    if (vsink)
      cleanup_bus (vsink);
    if (check_success_sink)
      check_success_sink (&td);
  }

  disconnect_ipcpipeline_elements ();

  close (pipesfa[0]);
  close (pipesfa[1]);
  close (pipesba[0]);
  close (pipesba[1]);
  close (pipesfv[0]);
  close (pipesfv[1]);
  close (pipesbv[0]);
  close (pipesbv[1]);

  /* If we have a child, we must now wait for it to be finished.
     We can't just waitpid, because this child might be still doing
     its shutdown, and might assert, and the die_on_child_death
     function will exit with the right exit code if so. So we wait
     for the child_dead boolean to be set, which die_on_child_death
     sets if the child dies normally. */
  if (pid) {
    while (!child_dead)
      g_usleep (1000);
  }

  if (source) {
    FAIL_UNLESS_EQUALS_INT (GST_OBJECT_REFCOUNT_VALUE (source), 1);
    gst_object_unref (source);
  }
  /* asink and vsink may be the same object, so refcount is not sure to be 1 */
  if (asink)
    gst_object_unref (asink);
  if (vsink)
    gst_object_unref (vsink);

  /* cleanup tasks a bit earlier to make sure all weak refs are gone */
  gst_task_cleanup_all ();

  /* all ipcpipeline elements we created should now be destroyed */
  if (weak_refs) {
#if 1
    /* to make it easier to see what leaks */
    GList *l;
    for (l = weak_refs; l; l = l->next) {
      g_print ("%s has %u refs\n", GST_ELEMENT_NAME (l->data),
          GST_OBJECT_REFCOUNT_VALUE (l->data));
    }
#endif
    FAIL_UNLESS (0);
  }
}

/**** play-pause test ****/

typedef struct
{
  gboolean got_state_changed_to_playing[2];
  gboolean got_state_changed_to_paused;
} play_pause_master_data;

#define PLAY_PAUSE_MASTER_DATA_INIT { { FALSE, FALSE }, FALSE }

typedef struct
{
  gboolean got_caps[2];
  gboolean got_segment[2];
  gboolean got_buffer[2];
} play_pause_slave_data;

#define PLAY_PAUSE_SLAVE_DATA_INIT \
  { { FALSE, FALSE }, { FALSE, FALSE }, { FALSE, FALSE } }

static gboolean
idlenull (gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  ret = gst_element_set_state (td->p, GST_STATE_NULL);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_SUCCESS);
  gst_object_unref (td->p);
  g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static gboolean idleplay (gpointer user_data);
static gboolean
idlepause (gpointer user_data)
{
  test_data *td = user_data;
  play_pause_master_data *d = td->md;
  GstStateChangeReturn ret;

  ret = gst_element_set_state (td->p, GST_STATE_PAUSED);
  FAIL_IF (ret == GST_STATE_CHANGE_FAILURE);
  if (ret == GST_STATE_CHANGE_SUCCESS || ret == GST_STATE_CHANGE_NO_PREROLL) {
    /* if the state change is not async, we won't get an aync-done, but
       this is expected, so set the flag here */
    d->got_state_changed_to_paused = TRUE;
    td->state_target = GST_STATE_PLAYING;
    g_timeout_add (STEP_AT, idleplay, user_data);
    return G_SOURCE_REMOVE;
  }
  gst_object_unref (td->p);
  return G_SOURCE_REMOVE;
}

static gboolean
idleplay (gpointer user_data)
{
  test_data *td = user_data;
  play_pause_master_data *d = td->md;
  GstStateChangeReturn ret;

  ret = gst_element_set_state (td->p, GST_STATE_PLAYING);
  FAIL_IF (ret == GST_STATE_CHANGE_FAILURE);
  if (ret == GST_STATE_CHANGE_SUCCESS || ret == GST_STATE_CHANGE_NO_PREROLL) {
    /* if the state change is not async, we won't get an aync-done, but
       this is expected, so set the flag here */
    d->got_state_changed_to_playing[1] = TRUE;
    td->state_target = GST_STATE_NULL;
    g_timeout_add (STEP_AT, idlenull, user_data);
    return G_SOURCE_REMOVE;
  }
  gst_object_unref (td->p);
  return G_SOURCE_REMOVE;
}

static void
play_pause_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  play_pause_master_data *d = td->md;
  GstStateChangeReturn ret;

  if (d->got_state_changed_to_paused) {
    d->got_state_changed_to_playing[1] = TRUE;
    td->state_target = GST_STATE_NULL;
    ret = gst_element_set_state (td->p, GST_STATE_NULL);
    FAIL_UNLESS (ret == GST_STATE_CHANGE_SUCCESS);
    g_main_loop_quit (loop);
  } else if (d->got_state_changed_to_playing[0]) {
    d->got_state_changed_to_paused = TRUE;
    td->state_target = GST_STATE_PLAYING;
    gst_object_ref (td->p);
    g_timeout_add (STEP_AT, (GSourceFunc) idleplay, td);
  } else {
    d->got_state_changed_to_playing[0] = TRUE;
    td->state_target = GST_STATE_PAUSED;
    gst_object_ref (td->p);
    g_timeout_add (STEP_AT, (GSourceFunc) idlepause, td);
  }
}

static void
play_pause_source (GstElement * source, void *user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = play_pause_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static GstPadProbeReturn
play_pause_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  play_pause_slave_data *d = td->sd;
  GstCaps *caps;

  if (GST_IS_BUFFER (info->data)) {
    d->got_buffer[pad2idx (pad, td->two_streams)] = TRUE;
  } else if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_CAPS) {
      gst_event_parse_caps (info->data, &caps);
      d->got_caps[caps2idx (caps, td->two_streams)] = TRUE;
    } else if (GST_EVENT_TYPE (info->data) == GST_EVENT_SEGMENT) {
      d->got_segment[pad2idx (pad, td->two_streams)] = TRUE;
    }
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_play_pause_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, play_pause_probe, user_data);
}

static void
setup_sink_play_pause (GstElement * sink, void *user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_play_pause_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_play_pause (void *user_data)
{
  test_data *td = user_data;
  play_pause_master_data *d = td->md;

  FAIL_UNLESS (d->got_state_changed_to_playing[0]);
  FAIL_UNLESS (d->got_state_changed_to_playing[1]);
  FAIL_UNLESS (d->got_state_changed_to_paused);
}

static void
check_success_sink_play_pause (void *user_data)
{
  test_data *td = user_data;
  play_pause_slave_data *d = td->sd;
  int idx;

  for (idx = 0; idx < (td->two_streams ? 2 : 1); idx++) {
    FAIL_UNLESS (d->got_caps[idx]);
    FAIL_UNLESS (d->got_segment[idx]);
    FAIL_UNLESS (d->got_buffer[idx]);
  }
}

GST_START_TEST (test_empty_play_pause)
{
  play_pause_master_data md = PLAY_PAUSE_MASTER_DATA_INIT;
  play_pause_slave_data sd = PLAY_PAUSE_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, play_pause_source, setup_sink_play_pause,
      check_success_source_play_pause, check_success_sink_play_pause, NULL, &md,
      &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_play_pause)
{
  play_pause_master_data md = PLAY_PAUSE_MASTER_DATA_INIT;
  play_pause_slave_data sd = PLAY_PAUSE_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, play_pause_source, setup_sink_play_pause,
      check_success_source_play_pause, check_success_sink_play_pause, NULL, &md,
      &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_play_pause)
{
  play_pause_master_data md = PLAY_PAUSE_MASTER_DATA_INIT;
  play_pause_slave_data sd = PLAY_PAUSE_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, play_pause_source,
      setup_sink_play_pause, check_success_source_play_pause,
      check_success_sink_play_pause, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_play_pause)
{
  play_pause_master_data md = PLAY_PAUSE_MASTER_DATA_INIT;
  play_pause_slave_data sd = PLAY_PAUSE_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      play_pause_source, setup_sink_play_pause, check_success_source_play_pause,
      check_success_sink_play_pause, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_a_play_pause)
{
  play_pause_master_data md = PLAY_PAUSE_MASTER_DATA_INIT;
  play_pause_slave_data sd = PLAY_PAUSE_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE, play_pause_source,
      setup_sink_play_pause, check_success_source_play_pause,
      check_success_sink_play_pause, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_play_pause)
{
  play_pause_master_data md = PLAY_PAUSE_MASTER_DATA_INIT;
  play_pause_slave_data sd = PLAY_PAUSE_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, play_pause_source,
      setup_sink_play_pause, check_success_source_play_pause,
      check_success_sink_play_pause, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_play_pause)
{
  play_pause_master_data md = PLAY_PAUSE_MASTER_DATA_INIT;
  play_pause_slave_data sd = PLAY_PAUSE_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      play_pause_source, setup_sink_play_pause, check_success_source_play_pause,
      check_success_sink_play_pause, NULL, &md, &sd);
}

GST_END_TEST;

/**** flushing seek test ****/

typedef struct
{
  gboolean segment_seek;
  gboolean pause;
} flushing_seek_input_data;

#define FLUSHING_SEEK_INPUT_DATA_INIT { FALSE, FALSE }
#define FLUSHING_SEEK_INPUT_DATA_INIT_PAUSED { FALSE, TRUE }
#define FLUSHING_SEEK_INPUT_DATA_INIT_SEGMENT_SEEK { TRUE, FALSE }

typedef struct
{
  gboolean got_state_changed_to_playing;
  gboolean got_segment_done;
  gboolean seek_sent;
} flushing_seek_master_data;

#define FLUSHING_SEEK_MASTER_DATA_INIT { FALSE, FALSE, FALSE }

typedef struct
{
  GstClockTime first_ts[2];
  gboolean got_caps[2];
  gboolean got_buffer_before_seek[2];
  gboolean got_buffer_after_seek[2];
  gboolean first_buffer_after_seek_has_timestamp_0[2];
  gboolean got_segment_after_seek[2];
  gboolean got_flush_start[2];
  gboolean got_flush_stop[2];
} flushing_seek_slave_data;

#define FLUSHING_SEEK_SLAVE_DATA_INIT { { 0, 0 }, }

static gboolean
send_flushing_seek (gpointer user_data)
{
  test_data *td = user_data;
  const flushing_seek_input_data *i = td->id;
  flushing_seek_master_data *d = td->md;
  GstEvent *seek_event;

  if (i->segment_seek) {
    GST_INFO_OBJECT (td->p, "Sending segment seek");
    seek_event =
        gst_event_new_seek (1.0, GST_FORMAT_TIME,
        GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 0,
        GST_SEEK_TYPE_SET, 1 * GST_SECOND);
    FAIL_UNLESS (gst_element_send_event (td->p, seek_event));
  } else {
    GST_INFO_OBJECT (td->p, "Sending flushing seek");
    gst_element_seek_simple (td->p, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, 0);
    g_timeout_add (STEP_AT, (GSourceFunc) stop_pipeline,
        gst_object_ref (td->p));
  }
  d->seek_sent = TRUE;
  return G_SOURCE_REMOVE;
}

static gboolean
pause_before_seek (gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  ret = gst_element_set_state (td->p, GST_STATE_PAUSED);
  FAIL_IF (ret == GST_STATE_CHANGE_FAILURE);

  return G_SOURCE_REMOVE;
}

static gboolean
flushing_seek_bus_msg (GstBus * bus, GstMessage * message, gpointer user_data)
{
  test_data *td = user_data;
  flushing_seek_master_data *d = td->md;

  if (GST_IS_PIPELINE (GST_MESSAGE_SRC (message))) {
    if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_SEGMENT_DONE) {
      d->got_segment_done = TRUE;
      g_timeout_add (STEP_AT, (GSourceFunc) stop_pipeline,
          gst_object_ref (td->p));
    }
  }
  return master_bus_msg (bus, message, user_data);
}

static void
flushing_seek_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  const flushing_seek_input_data *i = td->id;
  flushing_seek_master_data *d = td->md;

  if (!d->got_state_changed_to_playing) {
    d->got_state_changed_to_playing = TRUE;
    if (i->pause)
      g_timeout_add (PAUSE_AT, (GSourceFunc) pause_before_seek, td);
    g_timeout_add (SEEK_AT, (GSourceFunc) send_flushing_seek, td);
  }
}

static void
flushing_seek_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  /* we're on the source, there's already the basic master_bus_msg watch,
     and gst doesn't want more than one watch, so we remove the watch and
     call it directly when done in the new watch */
  gst_bus_remove_watch (GST_ELEMENT_BUS (source));
  gst_bus_add_watch (GST_ELEMENT_BUS (source), flushing_seek_bus_msg,
      user_data);
  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = flushing_seek_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static GstPadProbeReturn
flushing_seek_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  flushing_seek_slave_data *d = td->sd;
  GstClockTime ts;
  int idx;
  GstCaps *caps;

  if (GST_IS_BUFFER (info->data)) {
    idx = pad2idx (pad, td->two_streams);
    if (d->got_flush_stop[idx]) {
      if (!d->got_buffer_after_seek[idx]) {
        ts = GST_BUFFER_TIMESTAMP (info->data);
        d->first_buffer_after_seek_has_timestamp_0[idx] =
            (ts < d->first_ts[idx] + 10 * GST_MSECOND);
        d->got_buffer_after_seek[idx] = TRUE;
      }
    } else if (!d->got_buffer_before_seek[idx]) {
      d->got_buffer_before_seek[idx] = TRUE;
      d->first_ts[idx] = GST_BUFFER_TIMESTAMP (info->data);
    }
  } else if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_CAPS) {
      gst_event_parse_caps (info->data, &caps);
      if (are_caps_audio (caps) || are_caps_video (caps)) {
        idx = caps2idx (caps, td->two_streams);
        d->got_caps[idx] = TRUE;
      }
    } else if (GST_EVENT_TYPE (info->data) == GST_EVENT_SEGMENT) {
      /* from the sink pipeline, we don't know whether the master issued a seek,
         as the seek_sent memory location isn't directly accessible to us, so we
         look for a segment after a buffer to mean a seek was sent */
      idx = pad2idx (pad, td->two_streams);
      if (d->got_buffer_before_seek[idx])
        d->got_segment_after_seek[idx] = TRUE;
    } else if (GST_EVENT_TYPE (info->data) == GST_EVENT_FLUSH_START) {
      idx = pad2idx (pad, td->two_streams);
      d->got_flush_start[idx] = TRUE;
    } else if (GST_EVENT_TYPE (info->data) == GST_EVENT_FLUSH_STOP) {
      idx = pad2idx (pad, td->two_streams);
      if (d->got_buffer_before_seek[idx])
        d->got_flush_stop[idx] = TRUE;
    }
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_flushing_seek_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, flushing_seek_probe, user_data);
}

static void
setup_sink_flushing_seek (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_flushing_seek_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_flushing_seek (gpointer user_data)
{
  test_data *td = user_data;
  const flushing_seek_input_data *i = td->id;
  flushing_seek_master_data *d = td->md;

  FAIL_UNLESS (d->got_state_changed_to_playing);
  FAIL_UNLESS (d->seek_sent);
  FAIL_UNLESS (d->got_segment_done == i->segment_seek);
}

static void
check_success_sink_flushing_seek (gpointer user_data)
{
  test_data *td = user_data;
  flushing_seek_slave_data *d = td->sd;
  gint idx;

  for (idx = 0; idx < (td->two_streams ? 2 : 1); idx++) {
    FAIL_UNLESS (d->got_caps[idx]);
    FAIL_UNLESS (d->got_buffer_before_seek[idx]);
    FAIL_UNLESS (d->got_buffer_after_seek[idx]);
    FAIL_UNLESS (d->got_segment_after_seek[idx]);
    FAIL_UNLESS (d->got_flush_start[idx]);
    FAIL_UNLESS (d->got_flush_stop[idx]);
    FAIL_UNLESS (d->first_buffer_after_seek_has_timestamp_0[idx]);
  }
}

GST_START_TEST (test_empty_flushing_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_flushing_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_flushing_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_flushing_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      flushing_seek_source, setup_sink_flushing_seek,
      check_success_source_flushing_seek, check_success_sink_flushing_seek, &id,
      &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_a_flushing_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_flushing_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_flushing_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      flushing_seek_source, setup_sink_flushing_seek,
      check_success_source_flushing_seek, check_success_sink_flushing_seek, &id,
      &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_empty_flushing_seek_in_pause)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT_PAUSED;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_flushing_seek_in_pause)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT_PAUSED;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_flushing_seek_in_pause)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT_PAUSED;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_flushing_seek_in_pause)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT_PAUSED;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      flushing_seek_source, setup_sink_flushing_seek,
      check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_empty_segment_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT_SEGMENT_SEEK;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_segment_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT_SEGMENT_SEEK;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, flushing_seek_source,
      setup_sink_flushing_seek, check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_a_segment_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT_SEGMENT_SEEK;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE,
      flushing_seek_source, setup_sink_flushing_seek,
      check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_segment_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT_SEGMENT_SEEK;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE,
      flushing_seek_source, setup_sink_flushing_seek,
      check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_segment_seek)
{
  flushing_seek_input_data id = FLUSHING_SEEK_INPUT_DATA_INIT_SEGMENT_SEEK;
  flushing_seek_master_data md = FLUSHING_SEEK_MASTER_DATA_INIT;
  flushing_seek_slave_data sd = FLUSHING_SEEK_SLAVE_DATA_INIT;

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      flushing_seek_source, setup_sink_flushing_seek,
      check_success_source_flushing_seek,
      check_success_sink_flushing_seek, &id, &md, &sd);
}

GST_END_TEST;

/**** seek stress test ****/

typedef struct
{
  gint n_flushing_seeks;
  gint n_paused_seeks;
  gint n_segment_seeks;
} seek_stress_input_data;

typedef struct
{
  gboolean got_state_changed_to_playing;
  gboolean got_eos;
  gboolean seek_sent;
  guint64 t0;
} seek_stress_master_data;

static gboolean
send_seek_stress (gpointer user_data)
{
  test_data *td = user_data;
  seek_stress_input_data *i = td->id;
  seek_stress_master_data *d = td->md;
  GstEvent *seek_event;
  unsigned int available, seekidx;
  GstClockTime t, base;

  /* Live streams don't like to be seeked too far away from the
     "current" time, since they're live, so always seek near the
     "real" time, so we still exercise seeking to another position
     but still land somewhere close enough to "live" position. */
  t = (g_get_monotonic_time () - d->t0) * 1000;
  base = t > GST_SECOND / 2 ? t - GST_SECOND / 2 : 0;
  t = base + g_random_int_range (0, GST_SECOND);

  /* pick a random seek type among the ones we have left */
  available = i->n_flushing_seeks + i->n_paused_seeks + i->n_segment_seeks;
  if (available == 0) {
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (td->p),
        GST_DEBUG_GRAPH_SHOW_ALL, "inter.test.toplaying");
    FAIL_UNLESS (gst_element_set_state (td->p,
            GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    g_timeout_add (STEP_AT, (GSourceFunc) stop_pipeline,
        gst_object_ref (td->p));
    gst_object_unref (td->p);
    return G_SOURCE_REMOVE;
  }

  seekidx = rand () % available;
  if (seekidx < i->n_flushing_seeks) {
    GST_INFO_OBJECT (td->p, "Sending flushing seek to %" GST_TIME_FORMAT,
        GST_TIME_ARGS (t));
    FAIL_UNLESS (gst_element_set_state (td->p,
            GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    FAIL_UNLESS (gst_element_seek_simple (td->p, GST_FORMAT_TIME,
            GST_SEEK_FLAG_FLUSH, t));
    --i->n_flushing_seeks;
    return G_SOURCE_CONTINUE;
  }
  seekidx -= i->n_flushing_seeks;

  if (seekidx < i->n_paused_seeks) {
    GST_INFO_OBJECT (td->p,
        "Sending flushing seek in paused to %" GST_TIME_FORMAT,
        GST_TIME_ARGS (t));
    FAIL_UNLESS (gst_element_set_state (td->p,
            GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE);
    FAIL_UNLESS (gst_element_seek_simple (td->p, GST_FORMAT_TIME,
            GST_SEEK_FLAG_FLUSH, t));
    --i->n_paused_seeks;
    return G_SOURCE_CONTINUE;
  }
  seekidx -= i->n_paused_seeks;

  GST_INFO_OBJECT (td->p, "Sending segment seek to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (t));
  seek_event =
      gst_event_new_seek (1.0, GST_FORMAT_TIME,
      GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, t,
      GST_SEEK_TYPE_SET, t + 5 * GST_SECOND);
  FAIL_UNLESS (gst_element_send_event (td->p, seek_event));
  --i->n_segment_seeks;
  return G_SOURCE_CONTINUE;
}

static gboolean
seek_stress_bus_msg (GstBus * bus, GstMessage * message, gpointer user_data)
{
  test_data *td = user_data;
  seek_stress_master_data *d = td->md;

  if (GST_IS_PIPELINE (GST_MESSAGE_SRC (message))) {
    if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_EOS ||
        GST_MESSAGE_TYPE (message) == GST_MESSAGE_SEGMENT_DONE) {
      d->got_eos = TRUE;
    }
  }
  return master_bus_msg (bus, message, user_data);
}

static void
seek_stress_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  seek_stress_master_data *d = td->md;

  if (!d->got_state_changed_to_playing) {
    d->got_state_changed_to_playing = TRUE;
    d->t0 = g_get_monotonic_time ();
    gst_object_ref (td->p);
    g_timeout_add (10, (GSourceFunc) send_seek_stress, td);
  }
}

static void
seek_stress_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  /* we're on the source, there's already the basic master_bus_msg watch,
     and gst doesn't want more than one watch, so we remove the watch and
     call it directly when done in the new watch */
  gst_bus_remove_watch (GST_ELEMENT_BUS (source));
  gst_bus_add_watch (GST_ELEMENT_BUS (source), seek_stress_bus_msg, user_data);
  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = seek_stress_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static void
check_success_source_seek_stress (gpointer user_data)
{
  test_data *td = user_data;
  seek_stress_input_data *i = td->id;
  seek_stress_master_data *d = td->md;

  FAIL_UNLESS (d->got_state_changed_to_playing);
  FAIL_UNLESS_EQUALS_INT (i->n_flushing_seeks, 0);
  FAIL_UNLESS_EQUALS_INT (i->n_paused_seeks, 0);
  FAIL_UNLESS_EQUALS_INT (i->n_segment_seeks, 0);
  FAIL_IF (d->got_eos);
}

GST_START_TEST (test_empty_seek_stress)
{
  seek_stress_input_data id = { 100, 100, 100 };
  seek_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, seek_stress_source, NULL,
      check_success_source_seek_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_seek_stress)
{
  seek_stress_input_data id = { 100, 100, 100 };
  seek_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, seek_stress_source, NULL,
      check_success_source_seek_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_seek_stress)
{
  seek_stress_input_data id = { 100, 100, 0 };
  seek_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, seek_stress_source, NULL,
      check_success_source_seek_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_seek_stress)
{
  seek_stress_input_data id = { 100, 100, 0 };
  seek_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      seek_stress_source, NULL, check_success_source_seek_stress, NULL, &id,
      &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_a_seek_stress)
{
  seek_stress_input_data id = { 100, 0, 100 };
  seek_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE | TEST_FEATURE_LONG_DURATION,
      seek_stress_source, NULL, check_success_source_seek_stress, NULL, &id,
      &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_seek_stress)
{
  seek_stress_input_data id = { 100, 0, 100 };
  seek_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_LONG_DURATION,
      seek_stress_source, NULL, check_success_source_seek_stress, NULL, &id,
      &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_seek_stress)
{
  seek_stress_input_data id = { 100, 0, 100 };
  seek_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_LONG_DURATION |
      TEST_FEATURE_SPLIT_SINKS,
      seek_stress_source, NULL, check_success_source_seek_stress, NULL, &id,
      &md, NULL);
}

GST_END_TEST;

/**** upstream query test ****/

typedef struct
{
  GstClockTime expected_duration;

  /* In this test, the source does a position query (in the source pipeline
     process), and must check its return against the last buffer timestamp
     in the sink pipeline process. We open a pipe to let the sink send us
     the timestamps it receives so the source can make the comparison. */
  gint ts_pipes[2];
} upstream_query_input_data;

typedef struct
{
  gboolean got_state_changed_to_playing;
  gboolean got_correct_position;
  gboolean got_correct_duration;
  GstClockTime last_buffer_ts;
} upstream_query_master_data;

typedef struct
{
  gboolean got_caps[2];
  gboolean got_buffer[2];
  GstClockTime last_buffer_ts;
} upstream_query_slave_data;

static gboolean
send_upstream_queries (gpointer user_data)
{
  test_data *td = user_data;
  upstream_query_input_data *i = td->id;
  upstream_query_master_data *d = td->md;
  gint64 pos, dur, last;

  FAIL_UNLESS (gst_element_query_position (td->p, GST_FORMAT_TIME, &pos));

  /* read up the buffer ts sent by the sink process till the last one */
  while (read (i->ts_pipes[0], &last, sizeof (last)) == sizeof (last)) {
    /* timestamps may not be increasing because we are getting ts from
     * both the audio and video streams; the position query will report
     * the higher */
    if (last > d->last_buffer_ts)
      d->last_buffer_ts = last;
  }
  if (ABS ((gint64) (pos - d->last_buffer_ts)) <= CLOSE_ENOUGH_TO_ZERO)
    d->got_correct_position = TRUE;

  FAIL_UNLESS (gst_element_query_duration (td->p, GST_FORMAT_TIME, &dur));
  if (GST_CLOCK_TIME_IS_VALID (i->expected_duration)) {
    GstClockTimeDiff diff = GST_CLOCK_DIFF (dur, i->expected_duration);
    if (diff >= -CLOSE_ENOUGH_TO_ZERO && diff <= CLOSE_ENOUGH_TO_ZERO)
      d->got_correct_duration = TRUE;
  } else {
    if (!GST_CLOCK_TIME_IS_VALID (dur))
      d->got_correct_duration = TRUE;
  }

  g_timeout_add (STEP_AT, (GSourceFunc) stop_pipeline, td->p);
  return FALSE;
}

static void
upstream_query_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  upstream_query_master_data *d = td->md;

  if (!d->got_state_changed_to_playing) {
    d->got_state_changed_to_playing = TRUE;
    gst_object_ref (td->p);
    g_timeout_add (QUERY_AT, (GSourceFunc) send_upstream_queries, td);
  }
}

static void
upstream_query_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  td->state_changed_cb = upstream_query_on_state_changed;
  td->state_target = GST_STATE_PLAYING;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static GstPadProbeReturn
upstream_query_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  upstream_query_input_data *i = td->id;
  upstream_query_slave_data *d = td->sd;
  GstCaps *caps;

  if (GST_IS_BUFFER (info->data)) {
    d->got_buffer[pad2idx (pad, td->two_streams)] = TRUE;
    if (GST_BUFFER_TIMESTAMP_IS_VALID (info->data)) {
      d->last_buffer_ts = GST_BUFFER_TIMESTAMP (info->data);
      FAIL_UNLESS (write (i->ts_pipes[1], &d->last_buffer_ts,
              sizeof (d->last_buffer_ts)) == sizeof (d->last_buffer_ts));
    }
  } else if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_CAPS) {
      gst_event_parse_caps (info->data, &caps);
      d->got_caps[caps2idx (caps, td->two_streams)] = TRUE;
    }
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_upstream_query_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, upstream_query_probe, user_data);
}

static void
setup_sink_upstream_query (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_upstream_query_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_upstream_query (gpointer user_data)
{
  test_data *td = user_data;
  upstream_query_master_data *d = td->md;

  FAIL_UNLESS (d->got_state_changed_to_playing);
  FAIL_UNLESS (d->got_correct_position);
  FAIL_UNLESS (d->got_correct_duration);
}

static void
check_success_sink_upstream_query (gpointer user_data)
{
  test_data *td = user_data;
  upstream_query_slave_data *d = td->sd;
  int idx;

  for (idx = 0; idx < (td->two_streams ? 2 : 1); ++idx) {
    FAIL_UNLESS (d->got_caps[idx]);
    FAIL_UNLESS (d->got_buffer[idx]);
  }
}

GST_START_TEST (test_empty_upstream_query)
{
  upstream_query_input_data id = { GST_CLOCK_TIME_NONE, };
  upstream_query_master_data md = { 0 };
  upstream_query_slave_data sd = { {0}
  };

  FAIL_UNLESS (pipe2 (id.ts_pipes, O_NONBLOCK) == 0);
  TEST_BASE (TEST_FEATURE_TEST_SOURCE, upstream_query_source,
      setup_sink_upstream_query, check_success_source_upstream_query,
      check_success_sink_upstream_query, &id, &md, &sd);
  close (id.ts_pipes[0]);
  close (id.ts_pipes[1]);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_upstream_query)
{
  upstream_query_input_data id = { WAV_SAMPLE_ROUGH_DURATION, };
  upstream_query_master_data md = { 0 };
  upstream_query_slave_data sd = { {0}
  };

  FAIL_UNLESS (pipe2 (id.ts_pipes, O_NONBLOCK) == 0);
  TEST_BASE (TEST_FEATURE_WAV_SOURCE, upstream_query_source,
      setup_sink_upstream_query, check_success_source_upstream_query,
      check_success_sink_upstream_query, &id, &md, &sd);
  close (id.ts_pipes[0]);
  close (id.ts_pipes[1]);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_upstream_query)
{
  upstream_query_input_data id = { MPEGTS_SAMPLE_ROUGH_DURATION, };
  upstream_query_master_data md = { 0 };
  upstream_query_slave_data sd = { {0}
  };

  FAIL_UNLESS (pipe2 (id.ts_pipes, O_NONBLOCK) == 0);
  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, upstream_query_source,
      setup_sink_upstream_query, check_success_source_upstream_query,
      check_success_sink_upstream_query, &id, &md, &sd);
  close (id.ts_pipes[0]);
  close (id.ts_pipes[1]);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_upstream_query)
{
  upstream_query_input_data id = { MPEGTS_SAMPLE_ROUGH_DURATION, };
  upstream_query_master_data md = { 0 };
  upstream_query_slave_data sd = { {0}
  };

  FAIL_UNLESS (pipe2 (id.ts_pipes, O_NONBLOCK) == 0);
  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      upstream_query_source, setup_sink_upstream_query,
      check_success_source_upstream_query, check_success_sink_upstream_query,
      &id, &md, &sd);
  close (id.ts_pipes[0]);
  close (id.ts_pipes[1]);
}

GST_END_TEST;

GST_START_TEST (test_live_a_upstream_query)
{
  upstream_query_input_data id = { GST_CLOCK_TIME_NONE, };
  upstream_query_master_data md = { 0 };
  upstream_query_slave_data sd = { {0}
  };

  FAIL_UNLESS (pipe2 (id.ts_pipes, O_NONBLOCK) == 0);
  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE,
      upstream_query_source, setup_sink_upstream_query,
      check_success_source_upstream_query, check_success_sink_upstream_query,
      &id, &md, &sd);
  close (id.ts_pipes[0]);
  close (id.ts_pipes[1]);
}

GST_END_TEST;

GST_START_TEST (test_live_av_upstream_query)
{
  upstream_query_input_data id = { GST_CLOCK_TIME_NONE, };
  upstream_query_master_data md = { 0 };
  upstream_query_slave_data sd = { {0}
  };

  FAIL_UNLESS (pipe2 (id.ts_pipes, O_NONBLOCK) == 0);
  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE,
      upstream_query_source, setup_sink_upstream_query,
      check_success_source_upstream_query, check_success_sink_upstream_query,
      &id, &md, &sd);
  close (id.ts_pipes[0]);
  close (id.ts_pipes[1]);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_upstream_query)
{
  upstream_query_input_data id = { GST_CLOCK_TIME_NONE, };
  upstream_query_master_data md = { 0 };
  upstream_query_slave_data sd = { {0}
  };

  FAIL_UNLESS (pipe2 (id.ts_pipes, O_NONBLOCK) == 0);
  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      upstream_query_source, setup_sink_upstream_query,
      check_success_source_upstream_query, check_success_sink_upstream_query,
      &id, &md, &sd);
  close (id.ts_pipes[0]);
  close (id.ts_pipes[1]);
}

GST_END_TEST;

/**** message test ****/

typedef struct
{
  gboolean got_state_changed_to_playing;
  guint8 num_got_message;
  guint8 num_sent_message;
} message_master_data;

static void
send_ipcpipeline_test_message_event (const GValue * v, gpointer user_data)
{
  test_data *td = user_data;
  message_master_data *d = td->md;
  GstElement *element = g_value_get_object (v);
  GstMessage *msg;
  gboolean ret;

  d->num_sent_message++;

  msg = gst_message_new_element (GST_OBJECT (element),
      gst_structure_new_empty ("ipcpipeline-test"));
  ret = gst_element_send_event (element,
      gst_event_new_sink_message ("ipcpipeline-test", msg));
  FAIL_UNLESS (ret);
  gst_message_unref (msg);
}

static gboolean
send_sink_message (gpointer user_data)
{
  test_data *td = user_data;
  GstIterator *it;

  it = gst_bin_iterate_sources (GST_BIN (td->p));
  while (gst_iterator_foreach (it, send_ipcpipeline_test_message_event, td))
    gst_iterator_resync (it);
  gst_iterator_free (it);

  gst_object_unref (td->p);
  return G_SOURCE_REMOVE;
}

static gboolean
message_bus_msg (GstBus * bus, GstMessage * message, gpointer user_data)
{
  test_data *td = user_data;
  message_master_data *d = td->md;
  const GstStructure *structure;

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT) {
    structure = gst_message_get_structure (message);
    FAIL_UNLESS (structure);
    if (gst_structure_has_name (structure, "ipcpipeline-test")) {
      d->num_got_message++;
      if (d->num_got_message == d->num_sent_message)
        g_main_loop_quit (loop);
    }
  }
  return master_bus_msg (bus, message, user_data);
}

static void
message_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  message_master_data *d = td->md;

  if (!d->got_state_changed_to_playing) {
    d->got_state_changed_to_playing = TRUE;
    gst_object_ref (td->p);
    g_timeout_add (MESSAGE_AT, (GSourceFunc) send_sink_message, td);
  }
}

static void
message_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  /* we're on the source, there's already the basic master_bus_msg watch,
     and gst doesn't want more than one watch, so we remove the watch and
     call it directly when done in the new watch */
  gst_bus_remove_watch (GST_ELEMENT_BUS (source));
  gst_bus_add_watch (GST_ELEMENT_BUS (source), message_bus_msg, user_data);
  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = message_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static void
check_success_source_message (gpointer user_data)
{
  test_data *td = user_data;
  message_master_data *d = td->md;

  FAIL_UNLESS (d->got_state_changed_to_playing);
  FAIL_UNLESS_EQUALS_INT (d->num_got_message, d->num_sent_message);
}

GST_START_TEST (test_empty_message)
{
  message_master_data md = { 0 };
  TEST_BASE (TEST_FEATURE_TEST_SOURCE, message_source, NULL,
      check_success_source_message, NULL, NULL, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_message)
{
  message_master_data md = { 0 };
  TEST_BASE (TEST_FEATURE_WAV_SOURCE, message_source, NULL,
      check_success_source_message, NULL, NULL, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_a_message)
{
  message_master_data md = { 0 };
  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE, message_source, NULL,
      check_success_source_message, NULL, NULL, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_message)
{
  message_master_data md = { 0 };
  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, message_source, NULL,
      check_success_source_message, NULL, NULL, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_message)
{
  message_master_data md = { 0 };
  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      message_source, NULL, check_success_source_message, NULL, NULL, &md,
      NULL);
}

GST_END_TEST;

/**** end of stream test ****/

typedef struct
{
  gboolean got_state_changed_to_playing;
} end_of_stream_master_data;

typedef struct
{
  gboolean got_buffer[2];
  gboolean got_eos[2];
} end_of_stream_slave_data;

static void
end_of_stream_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  end_of_stream_master_data *d = td->md;

  if (!d->got_state_changed_to_playing)
    d->got_state_changed_to_playing = TRUE;
}

static void
end_of_stream_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  td->state_changed_cb = end_of_stream_on_state_changed;
  td->state_target = GST_STATE_PLAYING;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static GstPadProbeReturn
end_of_stream_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  end_of_stream_slave_data *d = td->sd;

  if (GST_IS_BUFFER (info->data)) {
    d->got_buffer[pad2idx (pad, td->two_streams)] = TRUE;
  } else if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_EOS) {
      d->got_eos[pad2idx (pad, td->two_streams)] = TRUE;
    }
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_end_of_stream_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, end_of_stream_probe, user_data);
}

static void
setup_sink_end_of_stream (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_end_of_stream_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_end_of_stream (gpointer user_data)
{
  test_data *td = user_data;
  end_of_stream_master_data *d = td->md;

  FAIL_UNLESS (d->got_state_changed_to_playing);
}

static void
check_success_sink_end_of_stream (gpointer user_data)
{
  test_data *td = user_data;
  end_of_stream_slave_data *d = td->sd;
  int idx;

  for (idx = 0; idx < (td->two_streams ? 2 : 1); idx++) {
    FAIL_UNLESS (d->got_buffer[idx]);
    FAIL_UNLESS (d->got_eos[idx]);
  }
}

GST_START_TEST (test_empty_end_of_stream)
{
  end_of_stream_master_data md = { 0 };
  end_of_stream_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE | TEST_FEATURE_ASYNC_SINK,
      end_of_stream_source, setup_sink_end_of_stream,
      check_success_source_end_of_stream, check_success_sink_end_of_stream,
      NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_end_of_stream)
{
  end_of_stream_master_data md = { 0 };
  end_of_stream_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE | TEST_FEATURE_ASYNC_SINK,
      end_of_stream_source, setup_sink_end_of_stream,
      check_success_source_end_of_stream, check_success_sink_end_of_stream,
      NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_end_of_stream)
{
  end_of_stream_master_data md = { 0 };
  end_of_stream_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_ASYNC_SINK,
      end_of_stream_source, setup_sink_end_of_stream,
      check_success_source_end_of_stream, check_success_sink_end_of_stream,
      NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_end_of_stream)
{
  end_of_stream_master_data md = { 0 };
  end_of_stream_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS |
      TEST_FEATURE_ASYNC_SINK,
      end_of_stream_source, setup_sink_end_of_stream,
      check_success_source_end_of_stream, check_success_sink_end_of_stream,
      NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_a_end_of_stream)
{
  end_of_stream_master_data md = { 0 };
  end_of_stream_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE,
      end_of_stream_source, setup_sink_end_of_stream,
      check_success_source_end_of_stream, check_success_sink_end_of_stream,
      NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_end_of_stream)
{
  end_of_stream_master_data md = { 0 };
  end_of_stream_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE,
      end_of_stream_source, setup_sink_end_of_stream,
      check_success_source_end_of_stream, check_success_sink_end_of_stream,
      NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_end_of_stream)
{
  end_of_stream_master_data md = { 0 };
  end_of_stream_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      end_of_stream_source, setup_sink_end_of_stream,
      check_success_source_end_of_stream, check_success_sink_end_of_stream,
      NULL, &md, &sd);
}

GST_END_TEST;

/**** reverse playback test ****/

typedef struct
{
  gboolean got_state_changed_to_playing;
  gboolean seek_sent;
} reverse_playback_master_data;

typedef struct
{
  gboolean got_segment_with_negative_rate;
  gboolean got_buffer_after_segment_with_negative_rate;
  GstClockTime first_backward_buffer_timestamp;
  gboolean got_buffer_one_second_early;
} reverse_playback_slave_data;

static gboolean
play_backwards (gpointer user_data)
{
  test_data *td = user_data;
  reverse_playback_master_data *d = td->md;
  gint64 pos;
  gboolean ret;

  FAIL_UNLESS (gst_element_query_position (td->p, GST_FORMAT_TIME, &pos));

  ret =
      gst_element_seek (td->p, -0.5, GST_FORMAT_TIME, 0, GST_SEEK_TYPE_SET, 0,
      GST_SEEK_TYPE_SET, pos);
  FAIL_UNLESS (ret);
  d->seek_sent = TRUE;

  gst_object_unref (td->p);
  return G_SOURCE_REMOVE;
}

static void
reverse_playback_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  reverse_playback_master_data *d = td->md;

  if (!d->got_state_changed_to_playing) {
    d->got_state_changed_to_playing = TRUE;
    gst_object_ref (td->p);
    g_timeout_add (2000, (GSourceFunc) play_backwards, td);
  }
}

static void
reverse_playback_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = reverse_playback_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static GstPadProbeReturn
reverse_playback_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  test_data *td = user_data;
  reverse_playback_slave_data *d = td->sd;

  if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_SEGMENT) {
      const GstSegment *s;
      gst_event_parse_segment (GST_EVENT (info->data), &s);
      if (s->rate < 0)
        d->got_segment_with_negative_rate = TRUE;
    }
  } else if (GST_IS_BUFFER (info->data)) {
    GstClockTime ts = GST_BUFFER_TIMESTAMP (info->data);
    if (GST_CLOCK_TIME_IS_VALID (ts)) {
      if (d->got_segment_with_negative_rate) {
        if (d->got_buffer_after_segment_with_negative_rate) {
          /* We test for 1 second, not just earlier, to make sure we don't
             just see B frames, or whatever else */
          if (ts < d->first_backward_buffer_timestamp - GST_SECOND) {
            d->got_buffer_one_second_early = TRUE;
          }
        } else {
          d->got_buffer_after_segment_with_negative_rate = TRUE;
          d->first_backward_buffer_timestamp = ts;
        }
      }
    }
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_reverse_playback_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, reverse_playback_probe, user_data);
}

static void
setup_sink_reverse_playback (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_reverse_playback_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_reverse_playback (gpointer user_data)
{
  test_data *td = user_data;
  reverse_playback_master_data *d = td->md;

  FAIL_UNLESS (d->got_state_changed_to_playing);
  FAIL_UNLESS (d->seek_sent);
}

static void
check_success_sink_reverse_playback (gpointer user_data)
{
  test_data *td = user_data;
  reverse_playback_slave_data *d = td->sd;

  FAIL_UNLESS (d->got_segment_with_negative_rate);
  FAIL_UNLESS (d->got_buffer_after_segment_with_negative_rate);
  FAIL_UNLESS (GST_CLOCK_TIME_IS_VALID (d->first_backward_buffer_timestamp));
  FAIL_UNLESS (d->first_backward_buffer_timestamp >= GST_SECOND);
  FAIL_UNLESS (d->got_buffer_one_second_early);
}

GST_START_TEST (test_a_reverse_playback)
{
  reverse_playback_master_data md = { 0 };
  reverse_playback_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE,
      reverse_playback_source, setup_sink_reverse_playback,
      check_success_source_reverse_playback,
      check_success_sink_reverse_playback, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_av_reverse_playback)
{
  reverse_playback_master_data md = { 0 };
  reverse_playback_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE | TEST_FEATURE_HAS_VIDEO,
      reverse_playback_source, setup_sink_reverse_playback,
      check_success_source_reverse_playback,
      check_success_sink_reverse_playback, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_av_2_reverse_playback)
{
  reverse_playback_master_data md = { 0 };
  reverse_playback_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE | TEST_FEATURE_HAS_VIDEO |
      TEST_FEATURE_SPLIT_SINKS,
      reverse_playback_source, setup_sink_reverse_playback,
      check_success_source_reverse_playback,
      check_success_sink_reverse_playback, NULL, &md, &sd);
}

GST_END_TEST;

/**** tags test ****/

enum
{
  TEST_TAG_EMPTY,
  TEST_TAG_TWO_TAGS,
  N_TEST_TAGS
};

typedef struct
{
  gboolean got_state_changed_to_playing;
  gboolean tags_sent[2][N_TEST_TAGS];
} tags_master_data;

typedef struct
{
  gboolean tags_received[N_TEST_TAGS];
} tags_slave_data;

static void
send_tags_on_pad (GstPad * pad, gpointer user_data)
{
  test_data *td = user_data;
  tags_master_data *d = td->md;
  GstEvent *e;
  gint idx;

  idx = pad2idx (pad, td->two_streams);

  e = gst_event_new_tag (gst_tag_list_new_empty ());
  FAIL_UNLESS (gst_pad_send_event (pad, e));
  d->tags_sent[idx][TEST_TAG_EMPTY] = TRUE;

  e = gst_event_new_tag (gst_tag_list_new (GST_TAG_TITLE, "title",
          GST_TAG_BITRATE, 56000, NULL));
  FAIL_UNLESS (gst_pad_send_event (pad, e));
  d->tags_sent[idx][TEST_TAG_TWO_TAGS] = TRUE;
}

static GstPadProbeReturn
tags_probe_source (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  tags_master_data *d = td->md;
  GstClockTime ts;

  if (GST_IS_BUFFER (info->data)) {
    ts = GST_BUFFER_TIMESTAMP (info->data);
    if (GST_CLOCK_TIME_IS_VALID (ts) && ts > STEP_AT * GST_MSECOND) {
      gint idx = pad2idx (pad, td->two_streams);
      if (!d->tags_sent[idx][0]) {
        GstPad *peer = gst_pad_get_peer (pad);
        FAIL_UNLESS (peer);
        send_tags_on_pad (peer, td);
        gst_object_unref (peer);
        EXCLUSIVE_CALL (td, g_timeout_add (STEP_AT, (GSourceFunc) stop_pipeline,
                gst_object_ref (td->p)));
      }
    }
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_tags_probe_source (const GValue * v, gpointer user_data)
{
  hook_peer_probe_types (v, tags_probe_source,
      GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, user_data);
}

static void
tags_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  tags_master_data *d = td->md;
  GstIterator *it;

  if (!d->got_state_changed_to_playing) {
    d->got_state_changed_to_playing = TRUE;

    it = gst_bin_iterate_sinks (GST_BIN (td->p));
    while (gst_iterator_foreach (it, hook_tags_probe_source, user_data))
      gst_iterator_resync (it);
    gst_iterator_free (it);
  }
}

static void
tags_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = tags_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static GstPadProbeReturn
tags_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  tags_slave_data *d = td->sd;
  guint funsigned;
  gchar *fstring = NULL;

  if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_TAG) {
      GstTagList *taglist = NULL;
      gst_event_parse_tag (GST_EVENT (info->data), &taglist);
      FAIL_UNLESS (taglist);
      if (gst_tag_list_is_empty (taglist)) {
        d->tags_received[TEST_TAG_EMPTY] = TRUE;
      } else if (gst_tag_list_get_string (taglist, GST_TAG_TITLE, &fstring)
          && !strcmp (fstring, "title")
          && gst_tag_list_get_uint (taglist, GST_TAG_BITRATE, &funsigned)
          && funsigned == 56000) {
        d->tags_received[TEST_TAG_TWO_TAGS] = TRUE;
      }
    }
  }
  g_free (fstring);

  return GST_PAD_PROBE_OK;
}

static void
hook_tags_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, tags_probe, user_data);
}

static void
setup_sink_tags (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_tags_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_tags (gpointer user_data)
{
  test_data *td = user_data;
  tags_master_data *d = td->md;
  gint n;

  FAIL_UNLESS (d->got_state_changed_to_playing);
  for (n = 0; n < N_TEST_TAGS; ++n) {
    FAIL_UNLESS (d->tags_sent[n]);
  }
}

static void
check_success_sink_tags (gpointer user_data)
{
  test_data *td = user_data;
  tags_slave_data *d = td->sd;
  gint n;

  for (n = 0; n < N_TEST_TAGS; ++n) {
    FAIL_UNLESS (d->tags_received[n]);
  }
}

GST_START_TEST (test_empty_tags)
{
  tags_master_data md = { 0 };
  tags_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, tags_source, setup_sink_tags,
      check_success_source_tags, check_success_sink_tags, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_tags)
{
  tags_master_data md = { 0 };
  tags_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, tags_source, setup_sink_tags,
      check_success_source_tags, check_success_sink_tags, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_tags)
{
  tags_master_data md = { 0 };
  tags_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, tags_source, setup_sink_tags,
      check_success_source_tags, check_success_sink_tags, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_tags)
{
  tags_master_data md = { 0 };
  tags_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS, tags_source,
      setup_sink_tags, check_success_source_tags, check_success_sink_tags, NULL,
      &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_a_tags)
{
  tags_master_data md = { 0 };
  tags_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE, tags_source, setup_sink_tags,
      check_success_source_tags, check_success_sink_tags, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_tags)
{
  tags_master_data md = { 0 };
  tags_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, tags_source, setup_sink_tags,
      check_success_source_tags, check_success_sink_tags, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_tags)
{
  tags_master_data md = { 0 };
  tags_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      tags_source, setup_sink_tags, check_success_source_tags,
      check_success_sink_tags, NULL, &md, &sd);
}

GST_END_TEST;

/**** nagivation test ****/

enum
{
  TEST_NAV_MOUSE_MOVE,
  TEST_NAV_KEY_PRESS,
  N_NAVIGATION_EVENTS
};

typedef struct
{
  gboolean got_state_changed_to_playing;
  gboolean navigation_received[N_NAVIGATION_EVENTS];
} navigation_master_data;

typedef struct
{
  gboolean started;
  gboolean navigation_sent[N_NAVIGATION_EVENTS];
  gint step;
} navigation_slave_data;

static GstPadProbeReturn
navigation_probe_source (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  test_data *td = user_data;
  navigation_master_data *d = td->md;
  GstEvent *e;
  const gchar *key;
  double x, y;

  if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_NAVIGATION) {
      e = GST_EVENT (info->data);

      switch (gst_navigation_event_get_type (e)) {
        case GST_NAVIGATION_EVENT_MOUSE_MOVE:
          gst_navigation_event_parse_mouse_move_event (e, &x, &y);
          if (x == 4.7 && y == 0.1)
            d->navigation_received[TEST_NAV_MOUSE_MOVE] = TRUE;
          break;

        case GST_NAVIGATION_EVENT_KEY_PRESS:
          gst_navigation_event_parse_key_event (e, &key);
          if (!strcmp (key, "Left"))
            d->navigation_received[TEST_NAV_KEY_PRESS] = TRUE;
          break;

        default:
          break;
      }

      /* drop at this point to imply successful handling; the upstream filesrc
       * does not know how to handle navigation events and returns FALSE,
       * which makes the test fail */
      return GST_PAD_PROBE_DROP;
    }
  }
  return GST_PAD_PROBE_OK;
}

static void
hook_navigation_probe_source (const GValue * v, gpointer user_data)
{
  hook_probe_types (v, navigation_probe_source,
      GST_PAD_PROBE_TYPE_EVENT_UPSTREAM, user_data);
}

static void
navigation_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  navigation_master_data *d = td->md;

  if (!d->got_state_changed_to_playing)
    d->got_state_changed_to_playing = TRUE;
}

static void
navigation_source (GstElement * source, void *user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (source));
  while (gst_iterator_foreach (it, hook_navigation_probe_source, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);

  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = navigation_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static void
send_navigation_event (const GValue * v, gpointer user_data)
{
  test_data *td = user_data;
  navigation_slave_data *d = td->sd;
  GstElement *sink;
  GstPad *pad, *peer;
  GstEvent *e = NULL;

  sink = g_value_get_object (v);
  FAIL_UNLESS (sink);
  pad = gst_element_get_static_pad (sink, "sink");
  FAIL_UNLESS (pad);
  peer = gst_pad_get_peer (pad);
  FAIL_UNLESS (peer);
  gst_object_unref (pad);

  switch (d->step) {
    case TEST_NAV_MOUSE_MOVE:
      e = gst_navigation_event_new_mouse_move (4.7, 0.1,
          GST_NAVIGATION_MODIFIER_NONE);
      break;
    case TEST_NAV_KEY_PRESS:
      e = gst_navigation_event_new_key_press ("Left",
          GST_NAVIGATION_MODIFIER_NONE);
      break;
  }

  FAIL_UNLESS (e);
  FAIL_UNLESS (gst_pad_send_event (peer, e));
  d->navigation_sent[d->step] = TRUE;

  gst_object_unref (peer);
}

static gboolean
step_navigation (gpointer user_data)
{
  test_data *td = user_data;
  navigation_slave_data *d = td->sd;
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (td->p));
  while (gst_iterator_foreach (it, send_navigation_event, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);

  if (++d->step < N_NAVIGATION_EVENTS)
    return G_SOURCE_CONTINUE;

  /* we are in the slave; send EOS to force the master to stop the pipeline */
  gst_element_post_message (GST_ELEMENT (td->p),
      gst_message_new_eos (GST_OBJECT (td->p)));

  gst_object_unref (td->p);
  return G_SOURCE_REMOVE;
}

static GstPadProbeReturn
navigation_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  navigation_slave_data *d = td->sd;
  GstClockTime ts;

  if (GST_IS_BUFFER (info->data)) {
    ts = GST_BUFFER_TIMESTAMP (info->data);
    if (GST_CLOCK_TIME_IS_VALID (ts) && ts > STEP_AT * GST_MSECOND) {
      if (!d->started) {
        d->started = TRUE;
        gst_object_ref (td->p);
        g_timeout_add (50, step_navigation, td);
      }
    }
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_navigation_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, navigation_probe, user_data);
}

static void
setup_sink_navigation (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_navigation_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_navigation (gpointer user_data)
{
  test_data *td = user_data;
  navigation_master_data *d = td->md;
  gint n;

  FAIL_UNLESS (d->got_state_changed_to_playing);
  for (n = 0; n < N_NAVIGATION_EVENTS; ++n) {
    FAIL_UNLESS (d->navigation_received[n]);
  }
}

static void
check_success_sink_navigation (gpointer user_data)
{
  test_data *td = user_data;
  navigation_slave_data *d = td->sd;
  gint n;

  FAIL_UNLESS (d->started);
  for (n = 0; n < N_NAVIGATION_EVENTS; ++n) {
    FAIL_UNLESS (d->navigation_sent[n]);
  }
}

GST_START_TEST (test_non_live_av_navigation)
{
  navigation_master_data md = { 0 };
  navigation_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, navigation_source,
      setup_sink_navigation, check_success_source_navigation,
      check_success_sink_navigation, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_non_live_av_2_navigation)
{
  navigation_master_data md = { 0 };
  navigation_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      navigation_source, setup_sink_navigation, check_success_source_navigation,
      check_success_sink_navigation, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_navigation)
{
  navigation_master_data md = { 0 };
  navigation_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, navigation_source,
      setup_sink_navigation, check_success_source_navigation,
      check_success_sink_navigation, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_navigation)
{
  navigation_master_data md = { 0 };
  navigation_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      navigation_source, setup_sink_navigation, check_success_source_navigation,
      check_success_sink_navigation, NULL, &md, &sd);
}

GST_END_TEST;

/**** reconfigure test ****/

typedef struct
{
  gboolean got_state_changed_to_playing;
  gboolean reconfigure_sent[2];
} reconfigure_master_data;

typedef struct
{
  gboolean reconfigure_scheduled;
  gboolean reconfigure_sent[2];
  gboolean got_caps[2][2];
} reconfigure_slave_data;

static GstPadProbeReturn
reconfigure_source_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  test_data *td = user_data;
  reconfigure_master_data *d = td->md;

  if (GST_EVENT_TYPE (info->data) == GST_EVENT_RECONFIGURE) {
    gint idx = pad2idx (pad, td->two_streams);
    d->reconfigure_sent[idx] = TRUE;
    EXCLUSIVE_CALL (td, g_timeout_add (STEP_AT, (GSourceFunc) stop_pipeline,
            gst_object_ref (td->p)));
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_reconfigure_source_probe (const GValue * v, gpointer user_data)
{
  hook_probe_types (v, reconfigure_source_probe,
      GST_PAD_PROBE_TYPE_EVENT_UPSTREAM, user_data);
}

static void
reconfigure_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  reconfigure_master_data *d = td->md;

  if (!d->got_state_changed_to_playing)
    d->got_state_changed_to_playing = TRUE;
}

static void
reconfigure_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (source));
  while (gst_iterator_foreach (it, hook_reconfigure_source_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);

  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = reconfigure_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static void
send_reconfigure_on_element (const GValue * v, gpointer user_data)
{
  test_data *td = user_data;
  reconfigure_slave_data *d = td->sd;
  GstElement *sink, *capsfilter;
  GstPad *pad, *peer;
  GstCaps *caps = NULL;

  sink = g_value_get_object (v);
  FAIL_UNLESS (sink);
  pad = gst_element_get_static_pad (sink, "sink");
  FAIL_UNLESS (pad);

  // look for the previous element, change caps if a capsfilter
  peer = gst_pad_get_peer (pad);
  FAIL_UNLESS (peer);
  capsfilter = GST_ELEMENT (gst_pad_get_parent (peer));
  g_object_get (capsfilter, "caps", &caps, NULL);
  FAIL_UNLESS (caps);
  caps = gst_caps_make_writable (caps);
  if (!strcmp (gst_structure_get_name (gst_caps_get_structure (caps, 0)),
          "audio/x-raw")) {
    gst_caps_set_simple (caps, "rate", G_TYPE_INT, 48000, NULL);
  } else {
    gst_caps_set_simple (caps, "width", G_TYPE_INT, 320, "height", G_TYPE_INT,
        200, NULL);
  }
  g_object_set (capsfilter, "caps", caps, NULL);
  FAIL_UNLESS (capsfilter);

  gst_object_unref (capsfilter);
  gst_object_unref (peer);

  d->reconfigure_sent[caps2idx (caps, td->two_streams)] = TRUE;

  gst_caps_unref (caps);
  gst_object_unref (pad);
}

static gboolean
send_reconfigure (gpointer user_data)
{
  test_data *td = user_data;
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (td->p));
  while (gst_iterator_foreach (it, send_reconfigure_on_element, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);

  gst_object_unref (td->p);
  return G_SOURCE_REMOVE;
}

static GstPadProbeReturn
reconfigure_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  reconfigure_slave_data *d = td->sd;
  GstClockTime ts;
  GstCaps *caps;
  int idx;

  if (GST_IS_BUFFER (info->data)) {
    ts = GST_BUFFER_TIMESTAMP (info->data);
    if (GST_CLOCK_TIME_IS_VALID (ts) && ts >= STEP_AT * GST_MSECOND) {
      if (!d->reconfigure_scheduled) {
        d->reconfigure_scheduled = TRUE;
        gst_object_ref (td->p);
        g_idle_add ((GSourceFunc) send_reconfigure, td);
      }
    }
  } else if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_CAPS) {
      gst_event_parse_caps (GST_EVENT (info->data), &caps);
      idx = caps2idx (caps, td->two_streams);
      if (d->reconfigure_sent[idx]) {
        d->got_caps[idx][1] = TRUE;
      } else {
        d->got_caps[idx][0] = TRUE;
      }
    }
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_reconfigure_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, reconfigure_probe, user_data);
}

static void
setup_sink_reconfigure (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_reconfigure_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_reconfigure (gpointer user_data)
{
  test_data *td = user_data;
  reconfigure_master_data *d = td->md;
  gint idx;

  FAIL_UNLESS (d->got_state_changed_to_playing);
  for (idx = 0; idx < (td->two_streams ? 2 : 1); idx++) {
    FAIL_UNLESS (d->reconfigure_sent[idx]);
  }
}

static void
check_success_sink_reconfigure (gpointer user_data)
{
  test_data *td = user_data;
  reconfigure_slave_data *d = td->sd;
  gint idx;

  FAIL_UNLESS (d->reconfigure_scheduled);
  for (idx = 0; idx < (td->two_streams ? 2 : 1); idx++) {
    FAIL_UNLESS (d->reconfigure_sent[idx]);
    FAIL_UNLESS (d->got_caps[idx][0]);
    FAIL_UNLESS (d->got_caps[idx][1]);
  }
}

GST_START_TEST (test_non_live_a_reconfigure)
{
  reconfigure_master_data md = { 0 };
  reconfigure_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE | TEST_FEATURE_FILTER_SINK_CAPS,
      reconfigure_source, setup_sink_reconfigure,
      check_success_source_reconfigure, check_success_sink_reconfigure, NULL,
      &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_non_live_av_reconfigure)
{
  reconfigure_master_data md = { 0 };
  reconfigure_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE | TEST_FEATURE_HAS_VIDEO |
      TEST_FEATURE_FILTER_SINK_CAPS,
      reconfigure_source, setup_sink_reconfigure,
      check_success_source_reconfigure, check_success_sink_reconfigure, NULL,
      &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_a_reconfigure)
{
  reconfigure_master_data md = { 0 };
  reconfigure_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE | TEST_FEATURE_FILTER_SINK_CAPS,
      reconfigure_source, setup_sink_reconfigure,
      check_success_source_reconfigure, check_success_sink_reconfigure, NULL,
      &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_reconfigure)
{
  reconfigure_master_data md = { 0 };
  reconfigure_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_FILTER_SINK_CAPS,
      reconfigure_source, setup_sink_reconfigure,
      check_success_source_reconfigure, check_success_sink_reconfigure, NULL,
      &md, &sd);
}

GST_END_TEST;

/**** state changes test ****/

typedef struct
{
  gint step;
  GHashTable *fdin, *fdout;
  gboolean waiting_state_change;
} state_changes_master_data;

typedef struct
{
  gint n_null;
  gint n_ready;
  gint n_paused;
  gint n_playing;
  gboolean got_eos;
} state_changes_slave_data;

static void
set_fdin (gpointer key, gpointer value, gpointer user_data)
{
  g_object_set (key, "fdin", GPOINTER_TO_INT (value), NULL);
}

static void
set_fdout (gpointer key, gpointer value, gpointer user_data)
{
  g_object_set (key, "fdout", GPOINTER_TO_INT (value), NULL);
}

/*
 * NULL
 * 0: READY NULL READY PAUSED READY PAUSED READY NULL
 * 8: READY PAUSED PLAYING PAUSED PLAYING PAUSED READY PAUSED READY NULL
 * 18: disconnect
 * 19: READY NULL READY PAUSED READY PAUSED READY NULL
 * 27: READY PAUSED PLAYING PAUSED PLAYING PAUSED READY PAUSED READY NULL
 * 37: reconnect
 * 38: READY NULL READY PAUSED READY PAUSED READY NULL
 * 46: READY PAUSED PLAYING PAUSED PLAYING
 * 51: EOS
 */
static gboolean
step_state_changes (gpointer user_data)
{
  test_data *td = user_data;
  state_changes_master_data *d = td->md;
  gboolean ret = G_SOURCE_CONTINUE;
  GstStateChangeReturn scret = GST_STATE_CHANGE_FAILURE;
  GList *l;
  int fdin, fdout;

  if (d->waiting_state_change)
    goto done;

  switch (d->step++) {
    case 1:
    case 7:
    case 17:
    case 20:
    case 26:
    case 36:
    case 39:
    case 45:
      scret = gst_element_set_state (td->p, GST_STATE_NULL);
      FAIL_UNLESS_EQUALS_INT (scret, GST_STATE_CHANGE_SUCCESS);
      break;
    case 0:
    case 2:
    case 4:
    case 6:
    case 8:
    case 14:
    case 16:
    case 38:
    case 40:
    case 42:
    case 44:
    case 46:
      scret = gst_element_set_state (td->p, GST_STATE_READY);
      FAIL_UNLESS_EQUALS_INT (scret, GST_STATE_CHANGE_SUCCESS);
      break;
    case 19:
    case 21:
    case 23:
    case 25:
    case 27:
    case 33:
    case 35:
      /* while we are disconnected, we can't do NULL -> READY */
      scret = gst_element_set_state (td->p, GST_STATE_READY);
      FAIL_UNLESS_EQUALS_INT (scret, GST_STATE_CHANGE_FAILURE);
      break;
    case 3:
    case 5:
    case 9:
    case 11:
    case 13:
    case 15:
    case 41:
    case 43:
    case 47:
    case 49:
      td->state_target = GST_STATE_PAUSED;
      scret = gst_element_set_state (td->p, GST_STATE_PAUSED);
      FAIL_IF (scret == GST_STATE_CHANGE_FAILURE);
      break;
    case 22:
    case 24:
    case 28:
    case 30:
    case 32:
    case 34:
      /* while we are disconnected, we can't do NULL -> READY */
      scret = gst_element_set_state (td->p, GST_STATE_PAUSED);
      FAIL_UNLESS_EQUALS_INT (scret, GST_STATE_CHANGE_FAILURE);
      break;
    case 10:
    case 12:
    case 48:
    case 50:
      td->state_target = GST_STATE_PLAYING;
      scret = gst_element_set_state (td->p, GST_STATE_PLAYING);
      FAIL_IF (scret == GST_STATE_CHANGE_FAILURE);
      break;
    case 29:
    case 31:
      /* while we are disconnected, we can't do NULL -> READY */
      scret = gst_element_set_state (td->p, GST_STATE_PLAYING);
      FAIL_UNLESS_EQUALS_INT (scret, GST_STATE_CHANGE_FAILURE);
      break;
    case 18:
      d->fdin = g_hash_table_new (g_direct_hash, g_direct_equal);
      d->fdout = g_hash_table_new (g_direct_hash, g_direct_equal);
      for (l = weak_refs; l; l = l->next) {
        g_object_get (l->data, "fdin", &fdin, "fdout", &fdout, NULL);
        g_hash_table_insert (d->fdin, (gpointer) l->data,
            GINT_TO_POINTER (fdin));
        g_hash_table_insert (d->fdout, (gpointer) l->data,
            GINT_TO_POINTER (fdout));
        g_signal_emit_by_name (G_OBJECT (l->data), "disconnect", NULL);
      }
      break;
    case 37:
      g_hash_table_foreach (d->fdin, set_fdin, NULL);
      g_hash_table_foreach (d->fdout, set_fdout, NULL);
      g_hash_table_destroy (d->fdin);
      g_hash_table_destroy (d->fdout);
      break;
    case 51:
      /* send EOS early to avoid waiting for the actual end of the file */
      gst_element_send_event (td->p, gst_event_new_eos ());
      gst_object_unref (td->p);
      ret = G_SOURCE_REMOVE;
      break;
  }

  if (scret == GST_STATE_CHANGE_ASYNC)
    d->waiting_state_change = TRUE;

done:
  return ret;
}

static void
state_changes_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  state_changes_master_data *d = td->md;

  d->waiting_state_change = FALSE;
}

static void
state_changes_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  state_changes_master_data *d = td->md;

  gst_object_ref (source);
  g_timeout_add (STEP_AT, (GSourceFunc) step_state_changes, td);

  d->waiting_state_change = FALSE;
  td->state_changed_cb = state_changes_state_changed;
}

static GstBusSyncReply
state_changes_sink_bus_msg (GstBus * bus, GstMessage * message,
    gpointer user_data)
{
  test_data *td = user_data;
  state_changes_slave_data *d = td->sd;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      d->got_eos = TRUE;
      break;
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC (message) == GST_OBJECT_CAST (td->p)) {
        GstState state;
        gst_message_parse_state_changed (message, NULL, &state, NULL);
        switch (state) {
          case GST_STATE_NULL:
            d->n_null++;
            break;
          case GST_STATE_READY:
            d->n_ready++;
            break;
          case GST_STATE_PAUSED:
            d->n_paused++;
            break;
          case GST_STATE_PLAYING:
            d->n_playing++;
            break;
          default:
            fail_if (1);
        }
      }
      break;
    default:
      break;
  }
  return GST_BUS_PASS;
}

static void
setup_sink_state_changes (GstElement * sink, gpointer user_data)
{
  g_object_set (sink, "auto-flush-bus", FALSE, NULL);
  gst_bus_set_sync_handler (GST_ELEMENT_BUS (sink), state_changes_sink_bus_msg,
      user_data, NULL);
}

static void
check_success_source_state_changes (gpointer user_data)
{
  test_data *td = user_data;
  state_changes_master_data *d = td->md;

  FAIL_UNLESS_EQUALS_INT (d->step, 52);
}

static void
check_success_sink_state_changes (gpointer user_data)
{
  test_data *td = user_data;
  state_changes_slave_data *d = td->sd;
  GstBus *bus;

  bus = gst_pipeline_get_bus (GST_PIPELINE (td->p));
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);

  FAIL_UNLESS (d->got_eos);
  FAIL_UNLESS_EQUALS_INT (d->n_null, 6);
  FAIL_UNLESS_EQUALS_INT (d->n_ready, 13);
  FAIL_UNLESS_EQUALS_INT (d->n_paused, 11);
  FAIL_UNLESS_EQUALS_INT (d->n_playing, 4);
}

GST_START_TEST (test_empty_state_changes)
{
  state_changes_master_data md = { 0 };
  state_changes_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, state_changes_source,
      setup_sink_state_changes, check_success_source_state_changes,
      check_success_sink_state_changes, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_state_changes)
{
  state_changes_master_data md = { 0 };
  state_changes_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, state_changes_source,
      setup_sink_state_changes, check_success_source_state_changes,
      check_success_sink_state_changes, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_state_changes)
{
  state_changes_master_data md = { 0 };
  state_changes_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, state_changes_source,
      setup_sink_state_changes, check_success_source_state_changes,
      check_success_sink_state_changes, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_state_changes)
{
  state_changes_master_data md = { 0 };
  state_changes_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      state_changes_source, setup_sink_state_changes,
      check_success_source_state_changes, check_success_sink_state_changes,
      NULL, &md, &sd);
}

GST_END_TEST;

/**** state changes stress test ****/

typedef struct
{
  gint n_state_changes;
} state_changes_stress_input_data;

typedef struct
{
  gboolean got_state_changed_to_playing;
  gboolean async_state_change_completed;
} state_changes_stress_master_data;

static gboolean
step_state_changes_stress (gpointer user_data)
{
  test_data *td = user_data;
  state_changes_stress_input_data *i = td->id;
  state_changes_stress_master_data *d = td->md;
  static const GstState states[] =
      { GST_STATE_NULL, GST_STATE_READY, GST_STATE_PAUSED, GST_STATE_PLAYING };
  GstState state;
  GstStateChangeReturn ret;

  /* wait for async state change to complete before continuing */
  if (!d->async_state_change_completed)
    return G_SOURCE_CONTINUE;

  if (i->n_state_changes == 0) {
    ret = gst_element_set_state (td->p, GST_STATE_PLAYING);
    FAIL_IF (ret == GST_STATE_CHANGE_FAILURE);
    g_timeout_add (STEP_AT, (GSourceFunc) stop_pipeline, td->p);
    return G_SOURCE_REMOVE;
  }
  --i->n_state_changes;

  state = states[rand () % 4];
  ret = gst_element_set_state (td->p, state);
  FAIL_IF (ret == GST_STATE_CHANGE_FAILURE);

  if (ret == GST_STATE_CHANGE_ASYNC) {
    td->state_target = state;
    d->async_state_change_completed = FALSE;
  }

  return G_SOURCE_CONTINUE;
}

static void
state_changes_stress_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  state_changes_stress_master_data *d = td->md;

  if (!d->got_state_changed_to_playing) {
    d->got_state_changed_to_playing = TRUE;
    gst_object_ref (td->p);
    g_timeout_add (50, (GSourceFunc) step_state_changes_stress, td);
  }
  d->async_state_change_completed = TRUE;
}

static void
state_changes_stress_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = state_changes_stress_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static void
check_success_source_state_changes_stress (gpointer user_data)
{
  test_data *td = user_data;
  state_changes_stress_input_data *i = td->id;
  state_changes_stress_master_data *d = td->md;

  FAIL_UNLESS (d->got_state_changed_to_playing);
  FAIL_UNLESS_EQUALS_INT (i->n_state_changes, 0);
}

GST_START_TEST (test_empty_state_changes_stress)
{
  state_changes_stress_input_data id = { 500 };
  state_changes_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, state_changes_stress_source, NULL,
      check_success_source_state_changes_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_state_changes_stress)
{
  state_changes_stress_input_data id = { 500 };
  state_changes_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, state_changes_stress_source, NULL,
      check_success_source_state_changes_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_state_changes_stress)
{
  state_changes_stress_input_data id = { 500 };
  state_changes_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, state_changes_stress_source, NULL,
      check_success_source_state_changes_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_state_changes_stress)
{
  state_changes_stress_input_data id = { 500 };
  state_changes_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      state_changes_stress_source, NULL,
      check_success_source_state_changes_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_a_state_changes_stress)
{
  state_changes_stress_input_data id = { 500 };
  state_changes_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE, state_changes_stress_source, NULL,
      check_success_source_state_changes_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_state_changes_stress)
{
  state_changes_stress_input_data id = { 500 };
  state_changes_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, state_changes_stress_source, NULL,
      check_success_source_state_changes_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_state_changes_stress)
{
  state_changes_stress_input_data id = { 500 };
  state_changes_stress_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      state_changes_stress_source, NULL,
      check_success_source_state_changes_stress, NULL, &id, &md, NULL);
}

GST_END_TEST;

/**** serialized query test ****/

typedef struct
{
  gboolean sent_query[2];
  gboolean got_query_reply[2];
  GstPad *pad[2];
} serialized_query_master_data;

typedef struct
{
  gboolean got_query;
} serialized_query_slave_data;

static gboolean
send_drain (gpointer user_data)
{
  test_data *td = user_data;
  serialized_query_master_data *d = td->md;
  GstQuery *q;
  gint idx;

  for (idx = 0; idx < (td->two_streams ? 2 : 1); idx++) {
    q = gst_query_new_drain ();
    FAIL_UNLESS (gst_pad_query (d->pad[idx], q));
    d->got_query_reply[idx] = TRUE;
    gst_query_unref (q);
    gst_object_unref (d->pad[idx]);
  }

  g_timeout_add (STEP_AT, (GSourceFunc) stop_pipeline, gst_object_ref (td->p));
  return G_SOURCE_REMOVE;
}

static GstPadProbeReturn
serialized_query_probe_source (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  test_data *td = user_data;
  serialized_query_master_data *d = td->md;
  GstClockTime ts;
  gint idx;

  if (GST_IS_BUFFER (info->data)) {
    ts = GST_BUFFER_TIMESTAMP (info->data);
    idx = pad2idx (pad, td->two_streams);
    if (!d->sent_query[idx] && GST_CLOCK_TIME_IS_VALID (ts)
        && ts > STEP_AT * GST_MSECOND) {
      d->sent_query[idx] = TRUE;
      d->pad[idx] = gst_object_ref (pad);
      EXCLUSIVE_CALL (td, g_idle_add (send_drain, td));
    }
  }
  return GST_PAD_PROBE_OK;
}

static void
hook_serialized_query_probe_source (const GValue * v, gpointer user_data)
{
  hook_probe (v, serialized_query_probe_source, user_data);
}

static void
serialized_query_source (GstElement * source, gpointer user_data)
{
  GstIterator *it;
  GstStateChangeReturn ret;

  it = gst_bin_iterate_sinks (GST_BIN (source));
  while (gst_iterator_foreach (it, hook_serialized_query_probe_source,
          user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);

  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC
      || ret == GST_STATE_CHANGE_SUCCESS);
}

static GstPadProbeReturn
serialized_query_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  test_data *td = user_data;
  serialized_query_slave_data *d = td->sd;

  if (GST_IS_QUERY (info->data)) {
    if (GST_QUERY_TYPE (info->data) == GST_QUERY_DRAIN) {
      d->got_query = TRUE;
    }
  }
  return GST_PAD_PROBE_OK;
}

static void
hook_serialized_query_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, serialized_query_probe, user_data);
}

static void
setup_sink_serialized_query (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_serialized_query_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_serialized_query (gpointer user_data)
{
  test_data *td = user_data;
  serialized_query_master_data *d = td->md;
  gint idx;

  for (idx = 0; idx < (td->two_streams ? 2 : 1); idx++) {
    FAIL_UNLESS (d->sent_query[idx]);
    FAIL_UNLESS (d->got_query_reply[idx]);
  }
}

static void
check_success_sink_serialized_query (gpointer user_data)
{
  test_data *td = user_data;
  serialized_query_slave_data *d = td->sd;

  FAIL_UNLESS (d->got_query);
}

GST_START_TEST (test_empty_serialized_query)
{
  serialized_query_master_data md = { {0} };
  serialized_query_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, serialized_query_source,
      setup_sink_serialized_query, check_success_source_serialized_query,
      check_success_sink_serialized_query, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_serialized_query)
{
  serialized_query_master_data md = { {0} };
  serialized_query_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, serialized_query_source,
      setup_sink_serialized_query, check_success_source_serialized_query,
      check_success_sink_serialized_query, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_serialized_query)
{
  serialized_query_master_data md = { {0} };
  serialized_query_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, serialized_query_source,
      setup_sink_serialized_query, check_success_source_serialized_query,
      check_success_sink_serialized_query, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_serialized_query)
{
  serialized_query_master_data md = { {0} };
  serialized_query_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      serialized_query_source, setup_sink_serialized_query,
      check_success_source_serialized_query,
      check_success_sink_serialized_query, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_a_serialized_query)
{
  serialized_query_master_data md = { {0} };
  serialized_query_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE, serialized_query_source,
      setup_sink_serialized_query, check_success_source_serialized_query,
      check_success_sink_serialized_query, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_serialized_query)
{
  serialized_query_master_data md = { {0} };
  serialized_query_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, serialized_query_source,
      setup_sink_serialized_query, check_success_source_serialized_query,
      check_success_sink_serialized_query, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_serialized_query)
{
  serialized_query_master_data md = { {0} };
  serialized_query_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      serialized_query_source, setup_sink_serialized_query,
      check_success_source_serialized_query,
      check_success_sink_serialized_query, NULL, &md, &sd);
}

GST_END_TEST;

/**** non serialized event test ****/

typedef struct
{
  gboolean sent_event[2];
} non_serialized_event_master_data;

typedef struct
{
  gboolean got_event;
} non_serialized_event_slave_data;

static GstPadProbeReturn
non_serialized_event_probe_source (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  test_data *td = user_data;
  non_serialized_event_master_data *d = td->md;
  GstClockTime ts;
  GstEvent *e;
  gint idx;

  if (GST_IS_BUFFER (info->data)) {
    ts = GST_BUFFER_TIMESTAMP (info->data);
    idx = pad2idx (pad, td->two_streams);
    if (!d->sent_event[idx]
        && GST_CLOCK_TIME_IS_VALID (ts) && ts > STEP_AT * GST_MSECOND) {
      e = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB,
          gst_structure_new ("name", "field", G_TYPE_INT, 42, NULL));
      FAIL_UNLESS (e);
      FAIL_UNLESS (gst_pad_send_event (pad, e));
      d->sent_event[idx] = TRUE;
      EXCLUSIVE_CALL (td, g_timeout_add (STEP_AT, (GSourceFunc) stop_pipeline,
              gst_object_ref (td->p)));
    }
  }
  return GST_PAD_PROBE_OK;
}

static void
hook_non_serialized_event_probe_source (const GValue * v, gpointer user_data)
{
  hook_probe (v, non_serialized_event_probe_source, user_data);
}

static void
non_serialized_event_source (GstElement * source, gpointer user_data)
{
  GstIterator *it;
  GstStateChangeReturn ret;

  it = gst_bin_iterate_sinks (GST_BIN (source));
  while (gst_iterator_foreach (it, hook_non_serialized_event_probe_source,
          user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);

  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC
      || ret == GST_STATE_CHANGE_SUCCESS);
}

static GstPadProbeReturn
non_serialized_event_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  test_data *td = user_data;
  non_serialized_event_slave_data *d = td->sd;
  const GstStructure *s;
  gint val;

  if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_CUSTOM_DOWNSTREAM_OOB) {
      s = gst_event_get_structure (info->data);
      FAIL_UNLESS (!strcmp (gst_structure_get_name (s), "name"));
      FAIL_UNLESS (gst_structure_get_int (s, "field", &val));
      FAIL_UNLESS (val == 42);
      d->got_event = TRUE;
    }
  }
  return GST_PAD_PROBE_OK;
}

static void
hook_non_serialized_event_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, non_serialized_event_probe, user_data);
}

static void
setup_sink_non_serialized_event (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_non_serialized_event_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_non_serialized_event (gpointer user_data)
{
  test_data *td = user_data;
  non_serialized_event_master_data *d = td->md;
  gint idx;

  for (idx = 0; idx < (td->two_streams ? 2 : 1); idx++) {
    FAIL_UNLESS (d->sent_event[idx]);
  }
}

static void
check_success_sink_non_serialized_event (gpointer user_data)
{
  test_data *td = user_data;
  non_serialized_event_slave_data *d = td->sd;

  FAIL_UNLESS (d->got_event);
}

GST_START_TEST (test_empty_non_serialized_event)
{
  non_serialized_event_master_data md = { {0} };
  non_serialized_event_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, non_serialized_event_source,
      setup_sink_non_serialized_event,
      check_success_source_non_serialized_event,
      check_success_sink_non_serialized_event, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_non_serialized_event)
{
  non_serialized_event_master_data md = { {0} };
  non_serialized_event_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, non_serialized_event_source,
      setup_sink_non_serialized_event,
      check_success_source_non_serialized_event,
      check_success_sink_non_serialized_event, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_non_serialized_event)
{
  non_serialized_event_master_data md = { {0} };
  non_serialized_event_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, non_serialized_event_source,
      setup_sink_non_serialized_event,
      check_success_source_non_serialized_event,
      check_success_sink_non_serialized_event, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_non_serialized_event)
{
  non_serialized_event_master_data md = { {0} };
  non_serialized_event_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      non_serialized_event_source, setup_sink_non_serialized_event,
      check_success_source_non_serialized_event,
      check_success_sink_non_serialized_event, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_a_non_serialized_event)
{
  non_serialized_event_master_data md = { {0} };
  non_serialized_event_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE, non_serialized_event_source,
      setup_sink_non_serialized_event,
      check_success_source_non_serialized_event,
      check_success_sink_non_serialized_event, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_non_serialized_event)
{
  non_serialized_event_master_data md = { {0} };
  non_serialized_event_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, non_serialized_event_source,
      setup_sink_non_serialized_event,
      check_success_source_non_serialized_event,
      check_success_sink_non_serialized_event, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_non_serialized_event)
{
  non_serialized_event_master_data md = { {0} };
  non_serialized_event_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      non_serialized_event_source, setup_sink_non_serialized_event,
      check_success_source_non_serialized_event,
      check_success_sink_non_serialized_event, NULL, &md, &sd);
}

GST_END_TEST;

/**** meta test ****/

enum
{
  TEST_META_PROTECTION = 0,
  N_TEST_META
};

typedef struct
{
  gboolean meta_sent[N_TEST_META];
} meta_master_data;

typedef struct
{
  gboolean meta_received[N_TEST_META];
} meta_slave_data;

static GstPadProbeReturn
meta_probe_source (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  meta_master_data *d = td->md;
  GstBuffer *buffer;
  GstProtectionMeta *meta;

  if (GST_IS_BUFFER (info->data)) {
    buffer = GST_BUFFER (info->data);
    meta =
        gst_buffer_add_protection_meta (buffer, gst_structure_new ("name",
            "somefield", G_TYPE_INT, 42, NULL));
    FAIL_UNLESS (meta);
    d->meta_sent[TEST_META_PROTECTION] = TRUE;
  }
  return GST_PAD_PROBE_OK;
}

static void
hook_meta_probe_source (const GValue * v, gpointer user_data)
{
  hook_probe (v, meta_probe_source, user_data);
}

static void
meta_source (GstElement * source, gpointer user_data)
{
  GstIterator *it;
  GstStateChangeReturn ret;

  it = gst_bin_iterate_sinks (GST_BIN (source));
  while (gst_iterator_foreach (it, hook_meta_probe_source, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);

  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC
      || ret == GST_STATE_CHANGE_SUCCESS);

  g_timeout_add (STOP_AT, (GSourceFunc) stop_pipeline, gst_object_ref (source));
}

static gboolean
scan_meta (GstBuffer * buffer, GstMeta ** meta, gpointer user_data)
{
  test_data *td = user_data;
  meta_slave_data *d = td->sd;
  int val;
  GstStructure *s;
  GstProtectionMeta *pmeta;

  if ((*meta)->info->api == GST_PROTECTION_META_API_TYPE) {
    pmeta = (GstProtectionMeta *) * meta;
    FAIL_UNLESS (GST_IS_STRUCTURE (pmeta->info));
    s = GST_STRUCTURE (pmeta->info);
    FAIL_UNLESS (!strcmp (gst_structure_get_name (s), "name"));
    FAIL_UNLESS (gst_structure_get_int (s, "somefield", &val));
    FAIL_UNLESS (val == 42);
    d->meta_received[TEST_META_PROTECTION] = TRUE;
  }
  return TRUE;
}

static GstPadProbeReturn
meta_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  if (GST_IS_BUFFER (info->data)) {
    gst_buffer_foreach_meta (info->data, scan_meta, user_data);
  }
  return GST_PAD_PROBE_OK;
}

static void
hook_meta_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, meta_probe, user_data);
}

static void
setup_sink_meta (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_meta_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_meta (gpointer user_data)
{
  test_data *td = user_data;
  meta_master_data *d = td->md;
  size_t n;

  for (n = 0; n < N_TEST_META; ++n)
    FAIL_UNLESS (d->meta_sent[n]);
}

static void
check_success_sink_meta (gpointer user_data)
{
  test_data *td = user_data;
  meta_slave_data *d = td->sd;
  size_t n;

  for (n = 0; n < N_TEST_META; ++n)
    FAIL_UNLESS (d->meta_received[n]);
}

GST_START_TEST (test_empty_meta)
{
  meta_master_data md = { {0}
  };
  meta_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE, meta_source, setup_sink_meta,
      check_success_source_meta, check_success_sink_meta, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_meta)
{
  meta_master_data md = { {0}
  };
  meta_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, meta_source, setup_sink_meta,
      check_success_source_meta, check_success_sink_meta, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_meta)
{
  meta_master_data md = { {0}
  };
  meta_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE, meta_source, setup_sink_meta,
      check_success_source_meta, check_success_sink_meta, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_meta)
{
  meta_master_data md = { {0}
  };
  meta_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_SPLIT_SINKS, meta_source,
      setup_sink_meta, check_success_source_meta, check_success_sink_meta,
      NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_a_meta)
{
  meta_master_data md = { {0}
  };
  meta_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE, meta_source, setup_sink_meta,
      check_success_source_meta, check_success_sink_meta, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_meta)
{
  meta_master_data md = { {0}
  };
  meta_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, meta_source, setup_sink_meta,
      check_success_source_meta, check_success_sink_meta, NULL, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_meta)
{
  meta_master_data md = { {0}
  };
  meta_slave_data sd = { {0}
  };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      meta_source, setup_sink_meta, check_success_source_meta,
      check_success_sink_meta, NULL, &md, &sd);
}

GST_END_TEST;

/**** source change test ****/

typedef struct
{
  void (*switcher) (GstElement *, char *name);
} source_change_input_data;

typedef struct
{
  gboolean source_change_scheduled;
  gboolean source_changed;
} source_change_master_data;

typedef struct
{
  gboolean got_caps[2][2];
  gboolean got_buffer[2][2];
  GstCaps *caps[2];
} source_change_slave_data;

static gboolean
stop_source (gpointer user_data)
{
  GstElement *source = user_data;

  FAIL_UNLESS (gst_element_set_state (source,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);
  gst_object_unref (source);
  return FALSE;
}

static gboolean
remove_source (gpointer user_data)
{
  GstElement *source = user_data;

  FAIL_UNLESS (gst_element_set_state (source,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);
  gst_bin_remove (GST_BIN (GST_ELEMENT_PARENT (source)), source);
  return FALSE;
}

static void
switch_to_aiff (GstElement * pipeline, char *name)
{
  GstElement *sbin, *filesrc, *ipcpipelinesink;
  GError *e = NULL;

  sbin =
      gst_parse_bin_from_description ("pushfilesrc name=filesrc ! aiffparse",
      TRUE, &e);
  FAIL_IF (e || !sbin);
  gst_element_set_name (sbin, name);
  filesrc = gst_bin_get_by_name (GST_BIN (sbin), "filesrc");
  FAIL_UNLESS (filesrc);
  g_object_set (filesrc, "location", "../../tests/files/s16be-id3v2.aiff",
      NULL);
  gst_object_unref (filesrc);
  gst_bin_add (GST_BIN (pipeline), sbin);
  ipcpipelinesink = gst_bin_get_by_name (GST_BIN (pipeline), "ipcpipelinesink");
  FAIL_UNLESS (ipcpipelinesink);
  FAIL_UNLESS (gst_element_link (sbin, ipcpipelinesink));
  gst_object_unref (ipcpipelinesink);
  gst_element_sync_state_with_parent (sbin);
  g_free (name);
}

static void
switch_av (GstElement * pipeline, char *name, gboolean live, gboolean Long)
{
  GstElement *src, *ipcpipelinesink;
  gint L = Long ? 10 : 1;

  if (g_str_has_prefix (name, "videotestsrc")) {
    /* replace video source with audio source */
    src = gst_element_factory_make ("audiotestsrc", NULL);
    FAIL_UNLESS (src);
    g_object_set (src, "is-live", live, "num-buffers", live ? 27 * L : -1,
        NULL);
    gst_bin_add (GST_BIN (pipeline), src);
    ipcpipelinesink =
        gst_bin_get_by_name (GST_BIN (pipeline), "vipcpipelinesink");
    FAIL_UNLESS (ipcpipelinesink);
    FAIL_UNLESS (gst_element_link (src, ipcpipelinesink));
    gst_object_unref (ipcpipelinesink);
    gst_element_sync_state_with_parent (src);
  }

  if (g_str_has_prefix (name, "audiotestsrc")) {
    /* replace audio source with video source */
    src = gst_element_factory_make ("videotestsrc", NULL);
    FAIL_UNLESS (src);
    g_object_set (src, "is-live", live, "num-buffers", live ? 19 * L : -1,
        NULL);
    gst_bin_add (GST_BIN (pipeline), src);
    ipcpipelinesink =
        gst_bin_get_by_name (GST_BIN (pipeline), "aipcpipelinesink");
    FAIL_UNLESS (ipcpipelinesink);
    FAIL_UNLESS (gst_element_link (src, ipcpipelinesink));
    gst_object_unref (ipcpipelinesink);
    gst_element_sync_state_with_parent (src);
  }

  g_free (name);
}

static void
switch_live_av (GstElement * pipeline, char *name)
{
  switch_av (pipeline, name, TRUE, FALSE);
}

static GstPadProbeReturn
change_source_blocked (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  const source_change_input_data *i = td->id;
  source_change_master_data *d = td->md;
  GstElement *source;
  GstPad *peer;

  peer = gst_pad_get_peer (pad);
  FAIL_UNLESS (peer);
  FAIL_UNLESS (gst_pad_unlink (pad, peer));
  gst_object_unref (peer);

  source = GST_ELEMENT (gst_element_get_parent (pad));
  FAIL_UNLESS (source);
  g_object_set_qdata (G_OBJECT (source), to_be_removed_quark (),
      GINT_TO_POINTER (1));

  gst_bin_remove (GST_BIN (GST_ELEMENT_PARENT (source)), source);
  (*i->switcher) (td->p, gst_element_get_name (source));

  g_idle_add (stop_source, source);

  d->source_changed = TRUE;

  gst_object_unref (td->p);
  return GST_PAD_PROBE_REMOVE;
}

static gboolean
change_source (gpointer user_data)
{
  test_data *td = user_data;
  GstElement *source;
  GstPad *pad;
  static const char *const names[] =
      { "source", "audiotestsrc", "videotestsrc" };
  gboolean found = FALSE;
  size_t n;

  for (n = 0; n < G_N_ELEMENTS (names); ++n) {
    source = gst_bin_get_by_name (GST_BIN (td->p), names[n]);
    if (source) {
      found = TRUE;
      pad = gst_element_get_static_pad (source, "src");
      FAIL_UNLESS (pad);
      gst_object_ref (td->p);
      gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_IDLE, change_source_blocked,
          user_data, NULL);
      gst_object_unref (pad);
      gst_object_unref (source);
    }
  }
  FAIL_UNLESS (found);

  gst_object_unref (td->p);
  return G_SOURCE_REMOVE;
}

static void
source_change_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  source_change_master_data *d = td->md;

  if (!d->source_change_scheduled) {
    d->source_change_scheduled = TRUE;
    gst_object_ref (td->p);
    g_timeout_add (STEP_AT, change_source, td);
  }
}

static void
source_change_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = source_change_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC
      || ret == GST_STATE_CHANGE_SUCCESS);
}

static int
scppad2idx (GstPad * pad, gboolean two_streams, GstCaps * newcaps)
{
  static GQuark scpidx = 0;
  gpointer p;
  int idx;
  GstCaps *caps;

  if (!scpidx)
    scpidx = g_quark_from_static_string ("scpidx");

  if (!two_streams)
    return 0;

  p = g_object_get_qdata (G_OBJECT (pad), scpidx);
  if (p)
    return GPOINTER_TO_INT (p) - 1;

  caps = gst_pad_get_current_caps (pad);
  if (!caps)
    caps = gst_pad_get_pad_template_caps (pad);
  if ((!caps || gst_caps_is_any (caps)) && newcaps)
    caps = gst_caps_ref (newcaps);
  FAIL_UNLESS (caps);
  idx = caps2idx (caps, two_streams);
  gst_caps_unref (caps);
  g_object_set_qdata (G_OBJECT (pad), scpidx, GINT_TO_POINTER (idx + 1));
  return idx;
}

static GstPadProbeReturn
source_change_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  source_change_slave_data *d = td->sd;
  GstCaps *caps;
  int idx;

  if (GST_IS_BUFFER (info->data)) {
    idx = scppad2idx (pad, td->two_streams, NULL);
    if (d->got_caps[idx][1])
      d->got_buffer[idx][1] = TRUE;
    else if (d->got_caps[idx][0])
      d->got_buffer[idx][0] = TRUE;
  } else if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_CAPS) {
      gst_event_parse_caps (info->data, &caps);
      idx = scppad2idx (pad, td->two_streams, caps);
      if (!d->got_caps[idx][0]) {
        FAIL_IF (d->caps[idx]);
        d->got_caps[idx][0] = TRUE;
        d->caps[idx] = gst_caps_ref (caps);
      } else {
        FAIL_UNLESS (d->caps);
        if (gst_caps_is_equal (caps, d->caps[idx])) {
          FAIL ();
        } else {
          gst_caps_replace (&d->caps[idx], NULL);
          d->got_caps[idx][1] = TRUE;
        }
      }
    }
  }
  return GST_PAD_PROBE_OK;
}

static void
hook_source_change_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, source_change_probe, user_data);
}

static void
setup_sink_source_change (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_source_change_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);
}

static void
check_success_source_source_change (gpointer user_data)
{
  test_data *td = user_data;
  source_change_master_data *d = td->md;

  FAIL_UNLESS (d->source_change_scheduled);
  FAIL_UNLESS (d->source_changed);
}

static void
check_success_sink_source_change (gpointer user_data)
{
  test_data *td = user_data;
  source_change_slave_data *d = td->sd;
  int idx;

  for (idx = 0; idx < (td->two_streams ? 2 : 1); idx++) {
    FAIL_UNLESS (d->got_caps[idx][0]);
    FAIL_UNLESS (d->got_buffer[idx][0]);
    FAIL_UNLESS (d->got_caps[idx][1]);
    FAIL_UNLESS (d->got_buffer[idx][1]);
  }
}

GST_START_TEST (test_non_live_source_change)
{
  source_change_input_data id = { switch_to_aiff };
  source_change_master_data md = { 0 };
  source_change_slave_data sd = { {{0}
      }
  };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE, source_change_source,
      setup_sink_source_change, check_success_source_source_change,
      check_success_sink_source_change, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_source_change)
{
  source_change_input_data id = { switch_live_av };
  source_change_master_data md = { 0 };
  source_change_slave_data sd = { {{0}
      }
  };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, source_change_source,
      setup_sink_source_change, check_success_source_source_change,
      check_success_sink_source_change, &id, &md, &sd);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_source_change)
{
  source_change_input_data id = { switch_live_av };
  source_change_master_data md = { 0 };
  source_change_slave_data sd = { {{0}
      }
  };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      source_change_source, setup_sink_source_change,
      check_success_source_source_change, check_success_sink_source_change,
      &id, &md, &sd);
}

GST_END_TEST;

/**** dynamic pipeline change stress test ****/

typedef struct
{
  guint n_switches_0;
  void (*switcher0) (test_data *);
  guint n_switches_1;
  void (*switcher1) (test_data *);
} dynamic_pipeline_change_stress_input_data;

typedef struct
{
  GMutex mutex;
  GCond cond;
  guint n_blocks_left;
  guint n_blocks_done;
  gboolean adding_probes;
  gboolean dynamic_pipeline_change_stress_scheduled;
} dynamic_pipeline_change_stress_master_data;

static gboolean dynamic_pipeline_change_stress_step (gpointer user_data);

static GstPadProbeReturn
dynamic_pipeline_change_stress_source_blocked_switch_av (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  dynamic_pipeline_change_stress_master_data *d = td->md;
  GstElement *source;
  GstPad *peer;

  /* An idle pad probe could be called directly from the gst_pad_add_probe call
     if the pad happens to be idle right now. This would deadlock us though, as
     we need all pads to be blocked at the same time, so we need the iteration
     over all pads to be done before the pad probes execute. So we keep track of
     whether we're iterating to add the probes, and pass if so. */
  if (d->adding_probes) {
    return GST_PAD_PROBE_PASS;
  }

  peer = gst_pad_get_peer (pad);
  FAIL_UNLESS (peer);
  FAIL_UNLESS (gst_pad_unlink (pad, peer));
  gst_object_unref (peer);

  source = GST_ELEMENT (gst_element_get_parent (pad));
  FAIL_UNLESS (source);
  g_object_set_qdata (G_OBJECT (source), to_be_removed_quark (),
      GINT_TO_POINTER (1));

  /* we want all pads to be blocked before we proceed */
  g_mutex_lock (&d->mutex);
  d->n_blocks_left--;
  while (d->n_blocks_left > 0)
    g_cond_wait (&d->cond, &d->mutex);
  g_mutex_unlock (&d->mutex);
  g_cond_broadcast (&d->cond);

  g_mutex_lock (&d->mutex);
  switch_av (td->p, gst_element_get_name (source),
      !!(td->features & TEST_FEATURE_LIVE), TRUE);
  g_mutex_unlock (&d->mutex);

  g_idle_add_full (G_PRIORITY_HIGH, remove_source, source, g_object_unref);

  if (g_atomic_int_dec_and_test (&d->n_blocks_done))
    g_timeout_add (STEP_AT, dynamic_pipeline_change_stress_step, td);

  return GST_PAD_PROBE_REMOVE;
}

static void
change_audio_channel (GstElement * pipeline, char *name,
    const char *ipcpipelinesink_name, gboolean live)
{
  GstElement *src, *ipcpipelinesink;

  /* replace audio source with video source */
  src = gst_element_factory_make ("audiotestsrc", NULL);
  FAIL_UNLESS (src);
  g_object_set (src, "is-live", live, "num-buffers", live ? 190 : -1, NULL);

  gst_bin_add (GST_BIN (pipeline), src);
  ipcpipelinesink =
      gst_bin_get_by_name (GST_BIN (pipeline), ipcpipelinesink_name);
  FAIL_UNLESS (ipcpipelinesink);
  FAIL_UNLESS (gst_element_link (src, ipcpipelinesink));
  gst_object_unref (ipcpipelinesink);
  gst_element_sync_state_with_parent (src);

  g_free (name);
}

static GstPadProbeReturn
dynamic_pipeline_change_stress_source_blocked_change_audio_channel (GstPad *
    pad, GstPadProbeInfo * info, gpointer user_data)
{
  test_data *td = user_data;
  dynamic_pipeline_change_stress_master_data *d = td->md;
  GstElement *source;
  GstPad *peer;
  const char *ipcpipelinesink_name;

  /* An idle pad probe could be called directly from the gst_pad_add_probe call
     if the pad happens to be idle right now. This would deadlock us though, as
     we need all pads to be blocked at the same time, so we need the iteration
     over all pads to be done before the pad probes execute. So we keep track of
     whether we're iterating to add the probes, and pass if so. */
  if (d->adding_probes) {
    return GST_PAD_PROBE_PASS;
  }

  peer = gst_pad_get_peer (pad);
  FAIL_UNLESS (peer);
  ipcpipelinesink_name = GST_ELEMENT_NAME (GST_PAD_PARENT (peer));
  FAIL_UNLESS (gst_pad_unlink (pad, peer));
  gst_object_unref (peer);

  source = GST_ELEMENT (gst_element_get_parent (pad));
  FAIL_UNLESS (source);
  g_object_set_qdata (G_OBJECT (source), to_be_removed_quark (),
      GINT_TO_POINTER (1));

  /* we want all pads to be blocked before we proceed */
  g_mutex_lock (&d->mutex);
  d->n_blocks_left--;
  while (d->n_blocks_left > 0)
    g_cond_wait (&d->cond, &d->mutex);
  g_cond_broadcast (&d->cond);
  g_mutex_unlock (&d->mutex);

  g_mutex_lock (&d->mutex);
  change_audio_channel (td->p, gst_element_get_name (source),
      ipcpipelinesink_name, !!(td->features & TEST_FEATURE_LIVE));
  g_mutex_unlock (&d->mutex);

  g_idle_add_full (G_PRIORITY_HIGH, remove_source, source, g_object_unref);

  if (g_atomic_int_dec_and_test (&d->n_blocks_done))
    g_timeout_add (STEP_AT, dynamic_pipeline_change_stress_step, td);

  return GST_PAD_PROBE_REMOVE;
}

typedef struct
{
  const char *const *names;
  size_t n_names;
    GstPadProbeReturn (*f) (GstPad * pad, GstPadProbeInfo * info,
      gpointer user_data);
  test_data *td;
} block_if_named_data;

static void
block_if_named (const GValue * v, gpointer user_data)
{
  block_if_named_data *bind = user_data;
  GstElement *e;
  GstPad *pad;
  size_t n;

  e = g_value_get_object (v);
  FAIL_UNLESS (e);
  for (n = 0; n < bind->n_names; ++n) {
    if (g_str_has_prefix (GST_ELEMENT_NAME (e), bind->names[n])) {
      pad = gst_element_get_static_pad (e, "src");
      FAIL_UNLESS (pad);

      if (!g_object_get_qdata (G_OBJECT (e), to_be_removed_quark ()))
        gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_IDLE, bind->f, bind->td,
            NULL);
      gst_object_unref (pad);
    }
  }
}

static void
count_audio_sources (const GValue * v, gpointer user_data)
{
  GstElement *e;

  e = g_value_get_object (v);
  FAIL_UNLESS (e);

  // we don't want to count the sources that are in the process
  // of being removed asynchronously
  if (g_object_get_qdata (G_OBJECT (e), to_be_removed_quark ()))
    return;

  if (g_str_has_prefix (GST_ELEMENT_NAME (e), "audiotestsrc"))
    ++ * (guint *) user_data;
}

static void
dynamic_pipeline_change_stress_swap_source (test_data * td)
{
  dynamic_pipeline_change_stress_master_data *d = td->md;
  static const char *const names[] =
      { "source", "audiotestsrc", "videotestsrc" };
  block_if_named_data bind = { names, sizeof (names) / sizeof (names[0]),
    dynamic_pipeline_change_stress_source_blocked_switch_av, td
  };
  GstIterator *it;

  /* we have two sources, we need to wait for both */
  d->n_blocks_left = d->n_blocks_done = 2;

  it = gst_bin_iterate_sources (GST_BIN (td->p));
  d->adding_probes = TRUE;
  while (gst_iterator_foreach (it, block_if_named, &bind)) {
    GST_INFO_OBJECT (td->p, "Resync");
    gst_iterator_resync (it);
  }
  d->adding_probes = FALSE;
  gst_iterator_free (it);
}

static void
dynamic_pipeline_change_stress_change_audio_channel (test_data * td)
{
  dynamic_pipeline_change_stress_master_data *d = td->md;
  static const char *const names[] = { "audiotestsrc" };
  block_if_named_data bind = { names, sizeof (names) / sizeof (names[0]),
    dynamic_pipeline_change_stress_source_blocked_change_audio_channel, td
  };
  GstIterator *it;
  guint audio_sources;

  /* we have either zero or one audio source */
  it = gst_bin_iterate_sources (GST_BIN (td->p));
  audio_sources = 0;
  while (gst_iterator_foreach (it, count_audio_sources, &audio_sources)) {
    GST_INFO_OBJECT (td->p, "Resync");
    gst_iterator_resync (it);
  }
  gst_iterator_free (it);
  d->n_blocks_left = d->n_blocks_done = audio_sources;

  it = gst_bin_iterate_sources (GST_BIN (td->p));
  d->adding_probes = TRUE;
  while (gst_iterator_foreach (it, block_if_named, &bind)) {
    GST_INFO_OBJECT (td->p, "Resync");
    gst_iterator_resync (it);
  }
  d->adding_probes = FALSE;
  gst_iterator_free (it);
}

static gboolean
dynamic_pipeline_change_stress_step (gpointer user_data)
{
  test_data *td = user_data;
  dynamic_pipeline_change_stress_input_data *i = td->id;
  guint available, idx;

  /* pick a random action among the ones we have left */
  available = i->n_switches_0 + i->n_switches_1;
  if (available == 0) {
    GST_INFO_OBJECT (td->p, "Destroying pipeline");
    FAIL_UNLESS (gst_element_set_state (td->p,
            GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    g_timeout_add (STEP_AT, stop_pipeline, td->p);
    return G_SOURCE_REMOVE;
  }

  idx = rand () % available;
  if (idx < i->n_switches_0) {
    (*i->switcher0) (td);
    --i->n_switches_0;
    return G_SOURCE_REMOVE;
  }
  idx -= i->n_switches_0;

  if (idx < i->n_switches_1) {
    (*i->switcher1) (td);
    --i->n_switches_1;
    return G_SOURCE_REMOVE;
  }
  idx -= i->n_switches_1;

  return G_SOURCE_REMOVE;
}

static void
dynamic_pipeline_change_stress_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  dynamic_pipeline_change_stress_master_data *d = td->md;

  if (!d->dynamic_pipeline_change_stress_scheduled) {
    d->dynamic_pipeline_change_stress_scheduled = TRUE;
    gst_object_ref (td->p);
    g_timeout_add (STEP_AT, dynamic_pipeline_change_stress_step, td);
  }
}

static void
dynamic_pipeline_change_stress (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  dynamic_pipeline_change_stress_master_data *d = td->md;
  GstStateChangeReturn ret;

  g_mutex_init (&d->mutex);
  g_cond_init (&d->cond);

  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = dynamic_pipeline_change_stress_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC
      || ret == GST_STATE_CHANGE_SUCCESS);
}

static void
check_success_source_dynamic_pipeline_change_stress (gpointer user_data)
{
  test_data *td = user_data;
  dynamic_pipeline_change_stress_input_data *i = td->id;
  dynamic_pipeline_change_stress_master_data *d = td->md;

  FAIL_UNLESS (d->dynamic_pipeline_change_stress_scheduled);
  FAIL_UNLESS_EQUALS_INT (i->n_switches_0, 0);
  FAIL_UNLESS_EQUALS_INT (i->n_switches_1, 0);

  g_cond_clear (&d->cond);
  g_mutex_clear (&d->mutex);
}

GST_START_TEST (test_non_live_av_dynamic_pipeline_change_stress)
{
  dynamic_pipeline_change_stress_input_data id = { 100,
    dynamic_pipeline_change_stress_swap_source, 100,
    dynamic_pipeline_change_stress_change_audio_channel
  };
  dynamic_pipeline_change_stress_master_data md = { {0} };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE | TEST_FEATURE_HAS_VIDEO,
      dynamic_pipeline_change_stress, NULL,
      check_success_source_dynamic_pipeline_change_stress, NULL, &id, &md,
      NULL);
}

GST_END_TEST;

GST_START_TEST (test_non_live_av_2_dynamic_pipeline_change_stress)
{
  dynamic_pipeline_change_stress_input_data id = { 100,
    dynamic_pipeline_change_stress_swap_source, 100,
    dynamic_pipeline_change_stress_change_audio_channel
  };
  dynamic_pipeline_change_stress_master_data md = { {0} };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE | TEST_FEATURE_HAS_VIDEO |
      TEST_FEATURE_SPLIT_SINKS, dynamic_pipeline_change_stress, NULL,
      check_success_source_dynamic_pipeline_change_stress, NULL, &id, &md,
      NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_dynamic_pipeline_change_stress)
{
  dynamic_pipeline_change_stress_input_data id = { 100,
    dynamic_pipeline_change_stress_swap_source, 100,
    dynamic_pipeline_change_stress_change_audio_channel
  };
  dynamic_pipeline_change_stress_master_data md = { {0} };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE, dynamic_pipeline_change_stress, NULL,
      check_success_source_dynamic_pipeline_change_stress, NULL, &id, &md,
      NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_dynamic_pipeline_change_stress)
{
  dynamic_pipeline_change_stress_input_data id = { 100,
    dynamic_pipeline_change_stress_swap_source, 100,
    dynamic_pipeline_change_stress_change_audio_channel
  };
  dynamic_pipeline_change_stress_master_data md = { {0} };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_SPLIT_SINKS,
      dynamic_pipeline_change_stress, NULL,
      check_success_source_dynamic_pipeline_change_stress, NULL, &id, &md,
      NULL);
}

GST_END_TEST;

/**** error from slave test ****/

typedef struct
{
  gboolean crash;
} error_from_slave_input_data;

typedef struct
{
  gboolean second_pass;
  gboolean got_state_changed_to_playing_on_first_pass;
  gboolean got_error_on_first_pass;
  gboolean got_state_changed_to_playing_on_second_pass;
  gboolean got_error_on_second_pass;
} error_from_slave_master_data;

static gboolean
bump_through_NULL (gpointer user_data)
{
  test_data *td = user_data;
  error_from_slave_input_data *i = td->id;
  error_from_slave_master_data *d = td->md;
  GstStateChangeReturn ret;
  GstElement *sink;

  ret = gst_element_set_state (td->p, GST_STATE_NULL);
  if (!i->crash) {
    FAIL_UNLESS (ret == GST_STATE_CHANGE_SUCCESS);
  }
  FAIL_UNLESS (gst_element_get_state (td->p, NULL, NULL,
          GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_SUCCESS);

  d->second_pass = TRUE;

  if (i->crash) {
    recreate_crashed_slave_process ();
    /* give the process time to be created in the other process */
    g_usleep (500 * 1000);

    /* reconnect to to slave process */
    sink = gst_bin_get_by_name (GST_BIN (td->p), "ipcpipelinesink");
    FAIL_UNLESS (sink);
    g_object_set (sink, "fdin", pipesba[0], "fdout", pipesfa[1], NULL);
    gst_object_unref (sink);
  }

  ret = gst_element_set_state (td->p, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_SUCCESS
      || ret == GST_STATE_CHANGE_ASYNC);

  g_timeout_add (STOP_AT, (GSourceFunc) stop_pipeline, td->p);
  return G_SOURCE_REMOVE;
}

static void
disconnect (const GValue * v, gpointer user_data)
{
  GstElement *e;

  e = g_value_get_object (v);
  FAIL_UNLESS (e);
  g_signal_emit_by_name (G_OBJECT (e), "disconnect", NULL);
}

static gboolean
error_from_slave_source_bus_msg (GstBus * bus, GstMessage * message,
    gpointer user_data)
{
  test_data *td = user_data;
  error_from_slave_input_data *i = td->id;
  error_from_slave_master_data *d = td->md;

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
    if (!d->second_pass) {
      if (!d->got_error_on_first_pass) {
        GstIterator *it;

        d->got_error_on_first_pass = TRUE;

        if (i->crash) {
          it = gst_bin_iterate_sinks (GST_BIN (td->p));
          while (gst_iterator_foreach (it, disconnect, NULL))
            gst_iterator_resync (it);
          gst_iterator_free (it);
        }

        gst_object_ref (td->p);
        g_timeout_add (STEP_AT, bump_through_NULL, td);
      }

      /* don't pass the expected error */
      return TRUE;
    }
  } else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_EOS) {
    if (!d->second_pass) {
      /* We'll get an expected EOS as the source reacts to the error */
      return TRUE;
    }
  }
  return master_bus_msg (bus, message, user_data);
}

static void
error_from_slave_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  error_from_slave_master_data *d = td->md;

  if (d->second_pass)
    d->got_state_changed_to_playing_on_second_pass = TRUE;
  else
    d->got_state_changed_to_playing_on_first_pass = TRUE;
}

static gboolean
error_from_slave_position_getter (GstElement * element)
{
  gint64 pos;

  /* we do not care about the result */
  gst_element_query_position (element, GST_FORMAT_TIME, &pos);

  return TRUE;
}

static void
error_from_slave_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  /* we're on the source, there's already the basic master_bus_msg watch,
     and gst doesn't want more than one watch, so we remove the watch and
     call it directly when done in the new watch */
  gst_bus_remove_watch (GST_ELEMENT_BUS (source));
  gst_bus_add_watch (GST_ELEMENT_BUS (source), error_from_slave_source_bus_msg,
      user_data);
  g_timeout_add (STEP_AT, (GSourceFunc) error_from_slave_position_getter,
      source);

  td->state_changed_cb = error_from_slave_on_state_changed;
  td->state_target = GST_STATE_PLAYING;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC);
}

static gboolean
error_from_slave_sink_bus_msg (GstBus * bus, GstMessage * message,
    gpointer user_data)
{
  test_data *td = user_data;
  error_from_slave_input_data *i = td->id;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      if (!strcmp (GST_ELEMENT_NAME (GST_MESSAGE_SRC (message)),
              "error-element"))
        g_object_set (GST_MESSAGE_SRC (message), "error-after", -1, NULL);
      break;
    case GST_MESSAGE_ASYNC_DONE:
      if (GST_IS_PIPELINE (GST_MESSAGE_SRC (message))) {
        /* We have two identical processes, and only one must crash. They can
           be distinguished by recovery_pid, however. */
        if (i->crash && recovery_pid)
          g_timeout_add (CRASH_AT, (GSourceFunc) crash, NULL);
      }
      break;
    default:
      break;
  }
  return TRUE;
}

static void
setup_sink_error_from_slave (GstElement * sink, gpointer user_data)
{
  gst_bus_add_watch (GST_ELEMENT_BUS (sink), error_from_slave_sink_bus_msg,
      user_data);
}

static void
check_success_source_error_from_slave (gpointer user_data)
{
  test_data *td = user_data;
  error_from_slave_master_data *d = td->md;

  FAIL_UNLESS (d->second_pass);
  FAIL_UNLESS (d->got_state_changed_to_playing_on_first_pass);
  FAIL_UNLESS (d->got_state_changed_to_playing_on_second_pass);
  FAIL_UNLESS (d->got_error_on_first_pass);
  FAIL_IF (d->got_error_on_second_pass);
}

GST_START_TEST (test_empty_error_from_slave)
{
  error_from_slave_input_data id = { FALSE };
  error_from_slave_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_TEST_SOURCE | TEST_FEATURE_ERROR_SINK,
      error_from_slave_source, setup_sink_error_from_slave,
      check_success_source_error_from_slave, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_error_from_slave)
{
  error_from_slave_input_data id = { FALSE };
  error_from_slave_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE | TEST_FEATURE_ERROR_SINK,
      error_from_slave_source, setup_sink_error_from_slave,
      check_success_source_error_from_slave, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_error_from_slave)
{
  error_from_slave_input_data id = { FALSE };
  error_from_slave_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_ERROR_SINK,
      error_from_slave_source, setup_sink_error_from_slave,
      check_success_source_error_from_slave, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_2_error_from_slave)
{
  error_from_slave_input_data id = { FALSE };
  error_from_slave_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_MPEGTS_SOURCE | TEST_FEATURE_ERROR_SINK |
      TEST_FEATURE_SPLIT_SINKS,
      error_from_slave_source, setup_sink_error_from_slave,
      check_success_source_error_from_slave, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_a_error_from_slave)
{
  error_from_slave_input_data id = { FALSE };
  error_from_slave_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_A_SOURCE | TEST_FEATURE_ERROR_SINK,
      error_from_slave_source, setup_sink_error_from_slave,
      check_success_source_error_from_slave, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_error_from_slave)
{
  error_from_slave_input_data id = { FALSE };
  error_from_slave_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_ERROR_SINK,
      error_from_slave_source, setup_sink_error_from_slave,
      check_success_source_error_from_slave, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_live_av_2_error_from_slave)
{
  error_from_slave_input_data id = { FALSE };
  error_from_slave_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_LIVE_AV_SOURCE | TEST_FEATURE_ERROR_SINK |
      TEST_FEATURE_SPLIT_SINKS,
      error_from_slave_source, setup_sink_error_from_slave,
      check_success_source_error_from_slave, NULL, &id, &md, NULL);
}

GST_END_TEST;

GST_START_TEST (test_wavparse_slave_process_crash)
{
  error_from_slave_input_data id = { TRUE };
  error_from_slave_master_data md = { 0 };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE | TEST_FEATURE_RECOVERY_SLAVE_PROCESS,
      error_from_slave_source, setup_sink_error_from_slave,
      check_success_source_error_from_slave, NULL, &id, &md, NULL);
}

GST_END_TEST;

/**** master process crash test ****/

typedef struct
{
  gboolean got_state_changed_to_playing;
} master_process_crash_master_data;

typedef struct
{
  gboolean got_error;
  gboolean got_eos;
} master_process_crash_slave_data;

static void
master_process_crash_on_state_changed (gpointer user_data)
{
  test_data *td = user_data;
  master_process_crash_master_data *d = td->md;

  if (!d->got_state_changed_to_playing) {
    d->got_state_changed_to_playing = TRUE;

    /* We have two identical processes, and only one must crash. They can
       be distinguished by recovery_pid, however. */
    if (!recovery_pid)
      g_timeout_add (CRASH_AT, (GSourceFunc) crash, NULL);
  }
}

static void
master_process_crash_source (GstElement * source, gpointer user_data)
{
  test_data *td = user_data;
  GstStateChangeReturn ret;

  td->state_target = GST_STATE_PLAYING;
  td->state_changed_cb = master_process_crash_on_state_changed;
  ret = gst_element_set_state (source, GST_STATE_PLAYING);
  FAIL_UNLESS (ret == GST_STATE_CHANGE_ASYNC
      || ret == GST_STATE_CHANGE_SUCCESS);
}

static GstPadProbeReturn
master_process_crash_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  test_data *td = user_data;
  master_process_crash_slave_data *d = td->sd;

  if (GST_IS_EVENT (info->data)) {
    if (GST_EVENT_TYPE (info->data) == GST_EVENT_EOS) {
      d->got_eos = TRUE;
    }
  }

  return GST_PAD_PROBE_OK;
}

static void
hook_master_process_crash_probe (const GValue * v, gpointer user_data)
{
  hook_probe (v, master_process_crash_probe, user_data);
}

static gboolean
go_to_NULL_and_reconnect (gpointer user_data)
{
  GstElement *pipeline = user_data;
  GstStateChangeReturn ret;
  GstElement *src;

  ret = gst_element_set_state (pipeline, GST_STATE_NULL);
  FAIL_IF (ret == GST_STATE_CHANGE_FAILURE);

  /* reconnect to to master process */
  src = gst_bin_get_by_name (GST_BIN (pipeline), "ipcpipelinesrc0");
  FAIL_UNLESS (src);
  g_object_set (src, "fdin", pipesfa[0], "fdout", pipesba[1], NULL);
  gst_object_unref (src);

  gst_object_unref (pipeline);
  return G_SOURCE_REMOVE;
}

static gboolean
master_process_crash_bus_msg (GstBus * bus, GstMessage * message,
    gpointer user_data)
{
  test_data *td = user_data;
  master_process_crash_slave_data *d = td->sd;
  GstIterator *it;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      if (!d->got_error) {
        it = gst_bin_iterate_sources (GST_BIN (td->p));
        while (gst_iterator_foreach (it, disconnect, NULL))
          gst_iterator_resync (it);
        gst_iterator_free (it);
        g_timeout_add (10, (GSourceFunc) go_to_NULL_and_reconnect,
            gst_object_ref (td->p));
        d->got_error = TRUE;
      }
      break;
    default:
      break;
  }
  return TRUE;
}

static void
setup_sink_master_process_crash (GstElement * sink, gpointer user_data)
{
  GstIterator *it;

  it = gst_bin_iterate_sinks (GST_BIN (sink));
  while (gst_iterator_foreach (it, hook_master_process_crash_probe, user_data))
    gst_iterator_resync (it);
  gst_iterator_free (it);

  gst_bus_add_watch (GST_ELEMENT_BUS (sink), master_process_crash_bus_msg,
      user_data);
}

static void
check_success_source_master_process_crash (gpointer user_data)
{
  test_data *td = user_data;
  master_process_crash_master_data *d = td->md;

  FAIL_UNLESS (d->got_state_changed_to_playing);
}

static void
check_success_sink_master_process_crash (gpointer user_data)
{
  test_data *td = user_data;
  master_process_crash_slave_data *d = td->sd;

  FAIL_UNLESS (d->got_error);
  FAIL_UNLESS (d->got_eos);
}

GST_START_TEST (test_wavparse_master_process_crash)
{
  master_process_crash_master_data md = { 0 };
  master_process_crash_slave_data sd = { 0 };

  TEST_BASE (TEST_FEATURE_WAV_SOURCE | TEST_FEATURE_RECOVERY_MASTER_PROCESS,
      master_process_crash_source, setup_sink_master_process_crash,
      check_success_source_master_process_crash,
      check_success_sink_master_process_crash, NULL, &md, &sd);
}

GST_END_TEST;

static Suite *
ipcpipeline_suite (void)
{
  Suite *s = suite_create ("ipcpipeline");
  TCase *tc_chain = tcase_create ("general");

  setup_lock ();

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 180);

  /* play_pause tests put the pipeline in PLAYING state, then in
     PAUSED state, then in PLAYING state again. The sink expects
     async-done messages or state change successes. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_play_pause);
    tcase_add_test (tc_chain, test_wavparse_play_pause);
    tcase_add_test (tc_chain, test_mpegts_play_pause);
    tcase_add_test (tc_chain, test_mpegts_2_play_pause);
    tcase_add_test (tc_chain, test_live_a_play_pause);
    tcase_add_test (tc_chain, test_live_av_play_pause);
    tcase_add_test (tc_chain, test_live_av_2_play_pause);
  }

  /* flushing_seek tests perform a flushing seek in PLAYING
     state. The sinks check a buffer with the target timestamp
     is received after the seek. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_flushing_seek);
    tcase_add_test (tc_chain, test_wavparse_flushing_seek);
    tcase_add_test (tc_chain, test_mpegts_flushing_seek);
    tcase_add_test (tc_chain, test_mpegts_2_flushing_seek);
    tcase_add_test (tc_chain, test_live_a_flushing_seek);
    tcase_add_test (tc_chain, test_live_av_flushing_seek);
    tcase_add_test (tc_chain, test_live_av_2_flushing_seek);
  }

  /* flushing_seek_in_pause tests perform a flushing seek in
     PAUSED state. These are disabled for live pipelines since
     those will not generate data in PAUSED, so we won't get
     a buffer. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_flushing_seek_in_pause);
    tcase_add_test (tc_chain, test_wavparse_flushing_seek_in_pause);
    tcase_add_test (tc_chain, test_mpegts_flushing_seek_in_pause);
    tcase_add_test (tc_chain, test_mpegts_2_flushing_seek_in_pause);
    /* live scenarios skipped: live sources do not generate buffers
     * when paused */
  }

  /* segment_seek tests perform a segment seek in PLAYING
     state. The sinks check a buffer with the target timestamp
     is received after the seek, and that a SEGMENT_DONE is
     received at the end of the segment. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_segment_seek);
    tcase_add_test (tc_chain, test_wavparse_segment_seek);
    /* mpegts skipped: tsdemux does not support segment seeks */
    tcase_add_test (tc_chain, test_live_a_segment_seek);
    tcase_add_test (tc_chain, test_live_av_segment_seek);
    tcase_add_test (tc_chain, test_live_av_2_segment_seek);
  }

  /* seek_stress tests perform stress testing on seeks, then waits
     in PLAYING for EOS or segment-done. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_seek_stress);
    tcase_add_test (tc_chain, test_wavparse_seek_stress);
    tcase_add_test (tc_chain, test_mpegts_seek_stress);
    tcase_add_test (tc_chain, test_mpegts_2_seek_stress);
    tcase_add_test (tc_chain, test_live_a_seek_stress);
    tcase_add_test (tc_chain, test_live_av_seek_stress);
    tcase_add_test (tc_chain, test_live_av_2_seek_stress);
  }

  /* upstream_query tests send position and duration queries, and
     checks the results are as expected. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_upstream_query);
    tcase_add_test (tc_chain, test_wavparse_upstream_query);
    tcase_add_test (tc_chain, test_mpegts_upstream_query);
    tcase_add_test (tc_chain, test_mpegts_2_upstream_query);
    tcase_add_test (tc_chain, test_live_a_upstream_query);
    tcase_add_test (tc_chain, test_live_av_upstream_query);
    tcase_add_test (tc_chain, test_live_av_2_upstream_query);
  }

  /* message tests send a sink message downstream, which causes
     the sinks to reply with the embedded event, which is checked.
     This is not possible when elements go into pull mode. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_message);
    tcase_add_test (tc_chain, test_wavparse_message);
    /* mpegts skipped because it goes into pull mode:
       https://bugzilla.gnome.org/show_bug.cgi?id=751637 */
    tcase_add_test (tc_chain, test_live_a_message);
    tcase_add_test (tc_chain, test_live_av_message);
    tcase_add_test (tc_chain, test_live_av_2_message);
  }

  /* end_of_stream tests check the EOS event and message are
     properly received when the stream reaches its end. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_end_of_stream);
    tcase_add_test (tc_chain, test_wavparse_end_of_stream);
    tcase_add_test (tc_chain, test_mpegts_end_of_stream);
    tcase_add_test (tc_chain, test_mpegts_2_end_of_stream);
    tcase_add_test (tc_chain, test_live_a_end_of_stream);
    tcase_add_test (tc_chain, test_live_av_end_of_stream);
    tcase_add_test (tc_chain, test_live_av_2_end_of_stream);
  }

  /* reverse_playback tests issue a seek with negative rate,
     and check buffers timestamp are in decreasing order.
     This does not work with sources which do not support
     negative playback rate (live ones, and some demuxers). */
  if (1) {
    /* wavparse and tsdemux does not support backward playback */
    tcase_add_test (tc_chain, test_a_reverse_playback);
    tcase_add_test (tc_chain, test_av_reverse_playback);
    tcase_add_test (tc_chain, test_av_2_reverse_playback);
  }

  /* tags tests check tags are carried to the slave. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_tags);
    tcase_add_test (tc_chain, test_wavparse_tags);
    tcase_add_test (tc_chain, test_mpegts_tags);
    tcase_add_test (tc_chain, test_mpegts_2_tags);
    tcase_add_test (tc_chain, test_live_a_tags);
    tcase_add_test (tc_chain, test_live_av_tags);
    tcase_add_test (tc_chain, test_live_av_2_tags);
  }

  /* reconfigure tests that pipeline reconfiguration via
     the reconfigure event works */
  if (1) {
    tcase_add_test (tc_chain, test_non_live_a_reconfigure);
    tcase_add_test (tc_chain, test_non_live_av_reconfigure);
    tcase_add_test (tc_chain, test_live_a_reconfigure);
    tcase_add_test (tc_chain, test_live_av_reconfigure);
  }

  /* state_change tests issue a number of state changes in
     (hopefully) all interesting configurations, and checks
     the state changes occurred on the slave pipeline. The links
     are disconnected and reconnected to check it all still
     works after this. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_state_changes);
    tcase_add_test (tc_chain, test_wavparse_state_changes);
    tcase_add_test (tc_chain, test_mpegts_state_changes);
    tcase_add_test (tc_chain, test_mpegts_2_state_changes);
    /* live scenarios skipped: live sources will cause no buffer
     * to flow in PAUSED, so the pipeline will only finish READY->PAUSED
     * once switching to PLAYING */
  }

  /* state_changes_stress tests change state randomly and rapidly. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_state_changes_stress);
    tcase_add_test (tc_chain, test_wavparse_state_changes_stress);
    tcase_add_test (tc_chain, test_mpegts_state_changes_stress);
    tcase_add_test (tc_chain, test_mpegts_2_state_changes_stress);
    tcase_add_test (tc_chain, test_live_a_state_changes_stress);
    tcase_add_test (tc_chain, test_live_av_state_changes_stress);
    tcase_add_test (tc_chain, test_live_av_2_state_changes_stress);
  }

  /* serialized_query tests checks that a serialized query is
     handled by the slave pipeline. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_serialized_query);
    tcase_add_test (tc_chain, test_wavparse_serialized_query);
    tcase_add_test (tc_chain, test_mpegts_serialized_query);
    tcase_add_test (tc_chain, test_mpegts_2_serialized_query);
    tcase_add_test (tc_chain, test_live_a_serialized_query);
    tcase_add_test (tc_chain, test_live_av_serialized_query);
    tcase_add_test (tc_chain, test_live_av_2_serialized_query);
  }

  /* non_serialized_event tests checks that a non serialized event
     is handled by the slave pipeline. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_non_serialized_event);
    tcase_add_test (tc_chain, test_wavparse_non_serialized_event);
    tcase_add_test (tc_chain, test_mpegts_non_serialized_event);
    tcase_add_test (tc_chain, test_mpegts_2_non_serialized_event);
    tcase_add_test (tc_chain, test_live_a_non_serialized_event);
    tcase_add_test (tc_chain, test_live_av_non_serialized_event);
    tcase_add_test (tc_chain, test_live_av_2_non_serialized_event);
  }

  /* meta tests checks that GstMeta on buffers are correctly
     received by the slave pipeline. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_meta);
    tcase_add_test (tc_chain, test_wavparse_meta);
    tcase_add_test (tc_chain, test_mpegts_meta);
    tcase_add_test (tc_chain, test_mpegts_2_meta);
    tcase_add_test (tc_chain, test_live_a_meta);
    tcase_add_test (tc_chain, test_live_av_meta);
    tcase_add_test (tc_chain, test_live_av_2_meta);
  }

  /* source_change tests checks that the pipelines can handle a
     change of source/caps. */
  if (1) {
    tcase_add_test (tc_chain, test_non_live_source_change);
    tcase_add_test (tc_chain, test_live_av_source_change);
    tcase_add_test (tc_chain, test_live_av_2_source_change);
  }

  /* navigation tests checks that navigation events from the slave
     are received by the master. */
  if (1) {
    tcase_add_test (tc_chain, test_non_live_av_navigation);
    tcase_add_test (tc_chain, test_non_live_av_2_navigation);
    tcase_add_test (tc_chain, test_live_av_navigation);
    tcase_add_test (tc_chain, test_live_av_2_navigation);
  }

  /* dynamic_pipeline_change_stress tests stress tests dynamic
     pipeline changes. */
  if (1) {
    tcase_add_test (tc_chain, test_non_live_av_dynamic_pipeline_change_stress);
    tcase_add_test (tc_chain,
        test_non_live_av_2_dynamic_pipeline_change_stress);
    tcase_add_test (tc_chain, test_live_av_dynamic_pipeline_change_stress);
    tcase_add_test (tc_chain, test_live_av_2_dynamic_pipeline_change_stress);
  }

  /* error_from_slave tests checks an error message issued
     by the slave pipeline is received by the master pipeline. */
  if (1) {
    tcase_add_test (tc_chain, test_empty_error_from_slave);
    tcase_add_test (tc_chain, test_wavparse_error_from_slave);
    tcase_add_test (tc_chain, test_mpegts_error_from_slave);
    tcase_add_test (tc_chain, test_mpegts_2_error_from_slave);
    tcase_add_test (tc_chain, test_live_a_error_from_slave);
    tcase_add_test (tc_chain, test_live_av_error_from_slave);
    tcase_add_test (tc_chain, test_live_av_2_error_from_slave);
  }

  /* slave_process_crash tests test that a crash of the slave
     process can be recovered from by the master, which can
     replace the slave process and continue. */
  tcase_add_test (tc_chain, test_wavparse_slave_process_crash);

  /* master_process_crash tests test that a crash of the master
     process can be recovered from by the slave. I don't recall
     how the recovery from that works, but it does! A watchdog
     process replaces the master process, and the slave will
     go to NULL and reconnect after it gets a timeout talking
     with the master pipeline. */
  tcase_add_test (tc_chain, test_wavparse_master_process_crash);

  return s;
}

GST_CHECK_MAIN (ipcpipeline);
