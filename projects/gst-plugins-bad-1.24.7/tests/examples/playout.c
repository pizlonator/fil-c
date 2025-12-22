/*  Copyright (C) 2015 Centricular Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/* Change this to set the output resolution */
#define OUTPUT_VIDEO_WIDTH  1280
#define OUTPUT_VIDEO_HEIGHT 720

/* Video and audio caps outputted by the mixers */
#define RAW_AUDIO_CAPS_STR "audio/x-raw, format=(string)S16LE, " \
"layout=(string)interleaved, rate=(int)44100, channels=(int)2, " \
"channel-mask=(bitmask)0x03"

#define RAW_VIDEO_CAPS_STR "video/x-raw, width=(int)" STR(OUTPUT_VIDEO_WIDTH) \
", height=(int)" STR(OUTPUT_VIDEO_HEIGHT) ", framerate=(fraction)25/1, " \
"format=I420, pixel-aspect-ratio=(fraction)1/1, " \
"interlace-mode=(string)progressive"

GST_DEBUG_CATEGORY_STATIC (playout);
#define GST_CAT_DEFAULT playout

typedef enum
{
  PLAYOUT_APP_STATE_READY,      /* Newly created */
  PLAYOUT_APP_STATE_PLAYING,    /* Playing an item */
  PLAYOUT_APP_STATE_EOS         /* Finished playing, all items EOS */
} PlayoutAppState;

typedef struct
{
  /* Application state */
  PlayoutAppState state;

  /* An array of PlayoutItems that will be played in sequence */
  GPtrArray *play_queue;
  /* Index of the currently-playing item */
  gint play_queue_current;
  /* Lock access to the play queue */
  GMutex play_queue_lock;

  GMainLoop *main_loop;

  /* Pipeline */
  GstElement *pipeline;

  /* Output */
  GstElement *video_mixer;
  GstElement *video_sink;
  GstVideoRectangle video_orect;        /* w/h/x/y of the output */

  GstElement *audio_mixer;
  GstElement *audio_sink;

  /* The duration of all items that have been played in ns.
   * Only updates when a new item is activated. */
  guint64 elapsed_duration;
} PlayoutApp;

typedef enum
{
  PLAYOUT_ITEM_STATE_NEW,       /* Newly created */
  PLAYOUT_ITEM_STATE_PREPARED,  /* Prepared and ready to activate */
  PLAYOUT_ITEM_STATE_ACTIVATED, /* Activated */
  PLAYOUT_ITEM_STATE_FIRST_VBUFFER,     /* First video buffer has gone through */
  PLAYOUT_ITEM_STATE_AGGREGATING,       /* Audio & video buffers are aggregating */
  PLAYOUT_ITEM_STATE_EOS        /* At least one pad is EOS */
} PlayoutItemState;

typedef struct
{
  PlayoutApp *app;
  PlayoutItemState state;

  gchar *fn;

  GstElement *decoder;          /* bin with uridecodebin + converters */

  /* We just use the first audio stream and ignore the rest (if there is audio) */
  GstPad *audio_pad;            /* decoder bin audio src ghostpad */
  GstPad *video_pad;            /* decoder bin video src ghostpad */
  GstVideoRectangle video_irect;        /* input w/h/x/y of the item */
  GstVideoRectangle video_orect;        /* output w/h/x/y of the item */

  /* When this item has finished preparing and all pads have been connected to
   * mixers, the pads will be blocked till it's this item's turn to be played */
  gulong audio_pad_probe_block_id;
  gulong video_pad_probe_block_id;

  /* The current running time of this item; updated with every audio buffer if
   * this item has audio; otherwise it's updated with very video buffer */
  guint64 running_time;
} PlayoutItem;

static PlayoutApp *playout_app_new (void);
static void playout_app_free (PlayoutApp * app);
static PlayoutItem *playout_item_new (PlayoutApp * app, const gchar * fn);
static void playout_item_free (PlayoutItem * item);

static void playout_app_add_item (PlayoutApp * app, const gchar * fn);
static gboolean playout_app_prepare_item (PlayoutItem * item);
static gboolean playout_app_activate_item (PlayoutItem * item);
static gboolean playout_app_activate_next_item (PlayoutApp * app);
static gboolean playout_app_activate_next_item_early (PlayoutApp * app);
static PlayoutItem *playout_app_get_current_item (PlayoutApp * app);
static gboolean playout_app_remove_item (PlayoutItem * item);

static void
playout_app_add_audio_sink (PlayoutApp * app)
{
  GstElement *audio_resample, *audio_conv, *queue;

  /* audiomixer doesn't do conversion yet, so we don't need an output capsfilter
   * for this branch. The output format is decided by the sink pads, which all
   * have to have the same format. */
  app->audio_mixer = gst_element_factory_make ("audiomixer", "audio_mixer");
  audio_conv = gst_element_factory_make ("audioconvert", "mixer_audioconvert");
  audio_resample = gst_element_factory_make ("audioresample",
      "audio_mixer_audioresample");
  queue = gst_element_factory_make ("queue", "asink_queue");
  app->audio_sink = gst_element_factory_make ("autoaudiosink", NULL);
  g_object_set (app->audio_sink, "async-handling", TRUE, NULL);
  gst_bin_add_many (GST_BIN (app->pipeline), app->audio_mixer, audio_conv,
      audio_resample, queue, app->audio_sink, NULL);
  gst_element_link_many (app->audio_mixer, audio_conv, audio_resample,
      queue, app->audio_sink, NULL);

  if (!gst_element_sync_state_with_parent (app->audio_mixer) ||
      !gst_element_sync_state_with_parent (audio_conv) ||
      !gst_element_sync_state_with_parent (audio_resample) ||
      !gst_element_sync_state_with_parent (queue) ||
      !gst_element_sync_state_with_parent (app->audio_sink))
    GST_ERROR ("app: unable to sync audio mixer + sink state with pipeline");
}

static PlayoutApp *
playout_app_new (void)
{
  GstElement *video_capsfilter, *queue;
  GstCaps *caps;
  PlayoutApp *app;

  app = g_new0 (PlayoutApp, 1);

  app->state = PLAYOUT_APP_STATE_READY;

  app->play_queue =
      g_ptr_array_new_with_free_func ((GDestroyNotify) playout_item_free);
  app->play_queue_current = -1;
  g_mutex_init (&app->play_queue_lock);

  app->main_loop = g_main_loop_new (NULL, FALSE);

  app->pipeline = gst_pipeline_new ("pipeline");

  /* It's best to set a caps filter for the video output format */
  app->video_orect.w = OUTPUT_VIDEO_WIDTH;
  app->video_orect.h = OUTPUT_VIDEO_HEIGHT;
  app->video_orect.x = 0;
  app->video_orect.y = 0;
  app->video_mixer = gst_element_factory_make ("compositor", "video_mixer");
  /* Set the background as black; faster while compositing, and allows us to
   * rescale videos with a different aspect ratio than the output in a way that
   * adds black borders automatically */
  g_object_set (app->video_mixer, "background", 1, NULL);
  queue = gst_element_factory_make ("queue", "vsink_queue");
  app->video_sink = gst_element_factory_make ("autovideosink", NULL);
  g_object_set (app->video_sink, "async-handling", TRUE, NULL);
  video_capsfilter = gst_element_factory_make ("capsfilter",
      "video_mixer_capsfilter");
  caps = gst_caps_from_string (RAW_VIDEO_CAPS_STR);
  g_object_set (video_capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);
  gst_bin_add_many (GST_BIN (app->pipeline), app->video_mixer, video_capsfilter,
      queue, app->video_sink, NULL);
  gst_element_link_many (app->video_mixer, video_capsfilter, queue,
      app->video_sink, NULL);

  return app;
}

static void
playout_app_free (PlayoutApp * app)
{
  GST_DEBUG ("Freeing app");
  g_ptr_array_unref (app->play_queue);
  g_main_loop_unref (app->main_loop);
  gst_element_set_state (app->pipeline, GST_STATE_NULL);
  gst_object_unref (app->pipeline);
  g_free (app);
}

static void
playout_app_eos (GstBus * bus, GstMessage * msg, PlayoutApp * app)
{
  g_print ("All streams EOS, exiting...\n");
  g_main_loop_quit (app->main_loop);
}

static PlayoutItem *
playout_item_new (PlayoutApp * app, const gchar * fn)
{
  PlayoutItem *item = g_new0 (PlayoutItem, 1);

  item->app = app;
  item->state = PLAYOUT_ITEM_STATE_NEW;
  item->fn = g_strdup (fn);

  return item;
}

/* Unlink and release the pad */
static gboolean
playout_remove_pad (GstPad * srcpad)
{
  GstPad *sinkpad;
  GstElement *mixer;

  sinkpad = gst_pad_get_peer (srcpad);
  if (!sinkpad)
    return FALSE;
  if (!gst_pad_unlink (srcpad, sinkpad))
    return FALSE;
  mixer = gst_pad_get_parent_element (sinkpad);
  gst_element_release_request_pad (mixer, sinkpad);
  GST_DEBUG ("Released some pad");

  gst_object_unref (sinkpad);
  gst_object_unref (mixer);
  return FALSE;
}

static GstPadProbeReturn
playout_item_pad_probe_blocked (GstPad * srcpad, GstPadProbeInfo * info,
    PlayoutItem * item)
{
  if (srcpad == item->audio_pad) {
    item->audio_pad_probe_block_id = GST_PAD_PROBE_INFO_ID (info);
  } else if (srcpad == item->video_pad) {
    item->video_pad_probe_block_id = GST_PAD_PROBE_INFO_ID (info);
  } else {
    g_assert_not_reached ();
  }

  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
playout_item_pad_probe_pad_running_time (GstPad * srcpad,
    GstPadProbeInfo * info, PlayoutItem * item)
{
  GstEvent *event;
  GstBuffer *buffer;
  guint64 running_time;
  const GstSegment *segment;

  buffer = GST_PAD_PROBE_INFO_BUFFER (info);
  event = gst_pad_get_sticky_event (srcpad, GST_EVENT_SEGMENT, 0);
  GST_TRACE ("%s: pad sticky event: %" GST_PTR_FORMAT, item->fn, event);

  if (event) {
    gst_event_parse_segment (event, &segment);
    gst_event_unref (event);
    running_time = gst_segment_to_running_time (segment, GST_FORMAT_TIME,
        GST_BUFFER_PTS (buffer));
  } else {
    GST_WARNING ("%s: unable to parse event for segment; falling back to pts. "
        "Output will probably have glitches.", item->fn);
    running_time = GST_BUFFER_PTS (buffer);
  }

  item->running_time = running_time + GST_BUFFER_DURATION (buffer);
  GST_TRACE ("%s: running time is %" G_GUINT64_FORMAT ", duration is %"
      G_GUINT64_FORMAT, item->fn, item->running_time,
      GST_BUFFER_DURATION (buffer));

  return GST_PAD_PROBE_PASS;
}

static GstPadProbeReturn
playout_item_pad_probe_video_pad_eos_on_buffer (GstPad * srcpad,
    GstPadProbeInfo * info, PlayoutItem * prev_item)
{
  PlayoutItem *current_item;

  current_item = playout_app_get_current_item (prev_item->app);

  if (!current_item)
    return GST_PAD_PROBE_REMOVE;

  /* Step through the item's states as buffers pass through. The first buffer
   * will be taken by the video_mixer, and kept till the audio running time
   * matches the video buffer running time. When the second buffer gets through,
   * we know that this pad has begun aggregating. */
  switch (current_item->state) {
    case PLAYOUT_ITEM_STATE_NEW:
    case PLAYOUT_ITEM_STATE_PREPARED:
      GST_DEBUG ("%s: new/prepared", current_item->fn);
      break;
    case PLAYOUT_ITEM_STATE_ACTIVATED:
      GST_DEBUG ("%s: activated -> first vbuffer", current_item->fn);
      current_item->state = PLAYOUT_ITEM_STATE_FIRST_VBUFFER;
      break;
    case PLAYOUT_ITEM_STATE_FIRST_VBUFFER:
      GST_DEBUG ("%s: first vbuffer -> aggregating", current_item->fn);
      current_item->state = PLAYOUT_ITEM_STATE_AGGREGATING;
      gst_pad_remove_probe (srcpad, GST_PAD_PROBE_INFO_ID (info));
      /* Item is aggregating, release the previous item's video pad */
      goto release;
      break;
    case PLAYOUT_ITEM_STATE_EOS:
      return GST_PAD_PROBE_REMOVE;
    default:
      g_assert_not_reached ();
  }

  return GST_PAD_PROBE_PASS;

release:
  {
    playout_remove_pad (prev_item->video_pad);
    GST_DEBUG ("%s: released video pad", prev_item->fn);
    prev_item->video_pad = NULL;

    /* If there's no audio pad, or if the audio pad is already EOS, we can
     * remove this item from the queue which will free it. Need to free the
     * item from the main thread because it causes the item's decoder bin
     * to be removed from the pipeline, which cannot be done in the
     * streaming thread */
    if (prev_item->audio_pad == NULL) {
      GST_DEBUG ("%s: queued item removal (last pad is video)", prev_item->fn);
      g_main_context_invoke (NULL, (GSourceFunc) playout_app_remove_item,
          prev_item);
    }

    /* Pad probe has already been removed above */
    return GST_PAD_PROBE_PASS;
  }
}

/* This is called on EOS for both item->audio_pad and item->video_pad
 *
 * FIXME: Add locking. Both pads could go EOS at the exact same time. */
static GstPadProbeReturn
playout_item_pad_probe_event (GstPad * srcpad, GstPadProbeInfo * info,
    PlayoutItem * item)
{
  GstEventType type;
  gboolean ret = TRUE;
  GstPadProbeReturn probe_ret = GST_PAD_PROBE_DROP;

  type = GST_EVENT_TYPE (GST_PAD_PROBE_INFO_DATA (info));
  if (type != GST_EVENT_EOS)
    return GST_PAD_PROBE_PASS;

  /* We might get two EOSes on this pad if we send an artificial EOS. Remove
   * the probe so this is only called once for each pad */
  gst_pad_remove_probe (srcpad, GST_PAD_PROBE_INFO_ID (info));

  GST_DEBUG ("%s: recvd some EOS", item->fn);

  if (item->state != PLAYOUT_ITEM_STATE_EOS) {
    /* We have more than one pad per item (video + audio item), and this is the
     * first pad to go EOS or we have only one pad per item, and that pad has
     * gone EOS. For the first case, the other pad might still have some buffers
     * to output before going EOS, but we need to activate the next item and
     * start outputting buffers from that immediately. */

    /* Update the total elapsed duration from the item's current running time */
    item->app->elapsed_duration += item->running_time;

    GST_DEBUG ("%s: activating next item", item->fn);
    /* Activate the next item if and only if this is the first pad to go EOS */
    ret = playout_app_activate_next_item (item->app);
    if (!ret) {
      GST_DEBUG ("%s: App is going EOS", item->fn);
      item->state = PLAYOUT_ITEM_STATE_EOS;
      item->app->state = PLAYOUT_APP_STATE_EOS;
      /* If we couldn't activate the next item, pass the EOS event onward,
       * ending the stream */
      probe_ret = GST_PAD_PROBE_PASS;
    }
  }

  g_assert (srcpad != NULL);

  if (srcpad == item->audio_pad) {
    GST_DEBUG ("%s: audio pad was EOS", item->fn);

    if (item->app->state != PLAYOUT_APP_STATE_EOS) {
      /* While activating the next item, we ensure that there's no offset mismatch
       * which would cause audiomixer to output silence, so we can release the
       * previous item's audio request pad here. We also unlink the audio pad;
       * nothing else is needed from it */
      playout_remove_pad (srcpad);
      GST_DEBUG ("%s: released audio pad", item->fn);

      /* If there's no video pad, or if the video pad is already EOS, we can
       * remove this item from the queue which will free it. Need to free the
       * item from the main thread because it causes the item's decoder bin
       * to be removed from the pipeline, which cannot be done in the
       * streaming thread */
      if (item->video_pad == NULL) {
        GST_DEBUG ("%s: queued item removal (last pad is audio)", item->fn);
        g_main_context_invoke (NULL, (GSourceFunc) playout_app_remove_item,
            item);
      }
    } else {
      /* If this is the last pad on audio_mixer, let the EOS go through
       * before unlinking/releasing the pad. This should happen within 500ms. */
      g_timeout_add (500, (GSourceFunc) playout_remove_pad, srcpad);
      GST_DEBUG ("%s: queued audio pad release", item->fn);

      if (item->video_pad == NULL) {
        /* Unlike above, we need to wait till the pad is removed before removing
         * the item from the app, so we queue it for 100ms afterwards */
        GST_DEBUG ("%s: queued last item removal (last pad is audio)",
            item->fn);
        g_timeout_add (600, (GSourceFunc) playout_app_remove_item, item);
      }
    }
    item->audio_pad = NULL;
  } else if (srcpad == item->video_pad) {

    GST_DEBUG ("%s: video pad was EOS", item->fn);

    if (item->audio_pad != NULL)
      GST_WARNING ("%s: video pad went EOS before audio pad! "
          "There will be audio/video glitches while switching.", item->fn);

    if (item->app->state != PLAYOUT_APP_STATE_EOS) {
      PlayoutItem *next_item;

      next_item = playout_app_get_current_item (item->app);
      GST_DEBUG ("%s: next item is %s, %i/%i", item->fn, next_item->fn,
          next_item->state, PLAYOUT_ITEM_STATE_ACTIVATED);

      g_assert (next_item != NULL);
      /* If there's another item being activated, release this video pad only
       * when the next item's video pad starts being aggregated; that happens
       * when this probe receives its 2nd buffer from the next item */
      gst_pad_add_probe (next_item->video_pad, GST_PAD_PROBE_TYPE_BUFFER,
          (GstPadProbeCallback) playout_item_pad_probe_video_pad_eos_on_buffer,
          item, NULL);
    } else {
      /* If this is the last pad on video_mixer, let the EOS go through
       * before unlinking/releasing the pad. This should happen within 500ms. */
      g_timeout_add (500, (GSourceFunc) playout_remove_pad, srcpad);
      GST_DEBUG ("%s: queued video pad release", item->fn);
      item->video_pad = NULL;
    }
    probe_ret = GST_PAD_PROBE_PASS;
  } else {
    g_assert_not_reached ();
  }

  item->state = PLAYOUT_ITEM_STATE_EOS;

  /* NOTE: If the srcpad has been unlinked, the return value is useless */
  return probe_ret;
}

/* On the "pad-added" signal of uridecodebin, add converters and connect to
 * audio/video mixers */
static void
playout_item_new_pad (GstElement * uridecodebin, GstPad * pad,
    PlayoutItem * item)
{
  GstStructure *s;
  GstCaps *caps;
  GstPad *sinkpad, *srcpad;
  GstElement *queue;
  GstPadProbeType block_probe_type;

  caps = gst_pad_get_current_caps (pad);
  s = gst_caps_get_structure (caps, 0);
  GST_DEBUG ("%s: new pad: %p, caps: %s", item->fn, pad,
      gst_structure_get_name (s));

  if (gst_structure_has_name (s, "audio/x-raw")) {
    if (item->audio_pad != NULL)
      /* Ignore all audio pads after the first one */
      goto out;
    goto audio;
  } else if (gst_structure_has_name (s, "video/x-raw")) {
    if (item->video_pad != NULL)
      /* Ignore all video pads after the first one */
      goto out;
    goto video;
  } else {
    goto out;
  }

audio:
  {
    GstCaps *wanted_caps;
    GstElement *audioconvert, *audioresample, *capsfilter;

    /* Audio pad found; add audio mixer and audio sink to the pipeline.
     * NOTE: If any items after this do not have an audio pad, the pipeline will
     * mess up because the audio sink will not receive any data. */
    if (item->app->audio_sink == NULL)
      playout_app_add_audio_sink (item->app);

    wanted_caps = gst_caps_from_string (RAW_AUDIO_CAPS_STR);

    if (!gst_caps_is_equal (caps, wanted_caps)) {
      GST_DEBUG ("%s: converting audio caps", item->fn);
      /* We need to convert the audio to the wanted format because
       * audiomixer doesn't do format conversion */
      audioresample = gst_element_factory_make ("audioresample", NULL);
      audioconvert = gst_element_factory_make ("audioconvert", NULL);
      capsfilter = gst_element_factory_make ("capsfilter", NULL);
      g_object_set (capsfilter, "caps", wanted_caps, NULL);
      queue = gst_element_factory_make ("queue", NULL);
      gst_bin_add_many (GST_BIN (item->decoder), audioresample, audioconvert,
          capsfilter, queue, NULL);

      sinkpad = gst_element_get_static_pad (audioresample, "sink");
      gst_pad_link (pad, sinkpad);
      gst_object_unref (sinkpad);
      gst_element_link_many (audioresample, audioconvert, capsfilter, queue,
          NULL);
      srcpad = gst_element_get_static_pad (queue, "src");

      if (!gst_element_sync_state_with_parent (audioresample) ||
          !gst_element_sync_state_with_parent (audioconvert) ||
          !gst_element_sync_state_with_parent (capsfilter) ||
          !gst_element_sync_state_with_parent (queue)) {
        GST_ERROR ("%s: unable to sync audio converter state with decoder",
            item->fn);
        goto out;
      }
    } else {
      queue = gst_element_factory_make ("queue", NULL);
      gst_bin_add (GST_BIN (item->decoder), queue);
      sinkpad = gst_element_get_static_pad (queue, "sink");
      gst_pad_link (pad, sinkpad);
      gst_object_unref (sinkpad);

      srcpad = gst_element_get_static_pad (queue, "src");

      if (!gst_element_sync_state_with_parent (queue)) {
        GST_ERROR ("%s: unable to sync audio queue state with decoder",
            item->fn);
        goto out;
      }
    }
    gst_caps_unref (wanted_caps);

    /* Convert the audioconvert src pad to a ghostpad on the bin */
    item->audio_pad = gst_ghost_pad_new (NULL, srcpad);
    gst_pad_set_active (item->audio_pad, TRUE);
    gst_element_add_pad (item->decoder, item->audio_pad);
    gst_object_unref (srcpad);

    srcpad = item->audio_pad;
    GST_DEBUG ("%s: created audio pad", item->fn);
    goto done;
  }

video:
  {
    if (!gst_structure_get_int (s, "width", &item->video_irect.w) ||
        !gst_structure_get_int (s, "height", &item->video_irect.h))
      GST_WARNING ("%s: unable to set width/height from caps", item->fn);
    item->video_irect.x = item->video_irect.y = 0;

    queue = gst_element_factory_make ("queue", "video-decoder-queue-%u");
    gst_bin_add (GST_BIN (item->decoder), queue);

    if (!gst_element_sync_state_with_parent (queue)) {
      GST_ERROR ("%s: unable to sync video queue state with decoder", item->fn);
      goto out;
    }

    sinkpad = gst_element_get_static_pad (queue, "sink");
    gst_pad_link (pad, sinkpad);
    gst_object_unref (sinkpad);

    /* Convert the queue src pad to a ghostpad on the bin */
    srcpad = gst_element_get_static_pad (queue, "src");
    item->video_pad = gst_ghost_pad_new (NULL, srcpad);
    gst_pad_set_active (item->video_pad, TRUE);
    gst_element_add_pad (item->decoder, item->video_pad);
    gst_object_unref (srcpad);

    srcpad = item->video_pad;
    GST_DEBUG ("%s: created video pad", item->fn);
    goto done;
  }

done:
  /* We let events and queries through */
  block_probe_type = GST_PAD_PROBE_TYPE_BLOCK |
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST;
  /* If the app is already playing an item, block everything except queries
   * till we need to play this item */
  if (item->app->state != PLAYOUT_APP_STATE_READY)
    gst_pad_add_probe (srcpad, block_probe_type,
        (GstPadProbeCallback) playout_item_pad_probe_blocked, item, NULL);
  /* Probe events for EOS */
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      (GstPadProbeCallback) playout_item_pad_probe_event, item, NULL);

out:
  gst_caps_unref (caps);
}

/* All pads on uridecodebin have finished being populated; the item has been
 * prepared and is ready to be activated */
static void
playout_item_no_more_pads (GstElement * uridecodebin, PlayoutItem * item)
{
  /* Set a buffer pad probe that constantly updates the item's
   * elapsed_duration using the duration of each audio buffer */
  if (item->audio_pad) {
    gst_pad_add_probe (item->audio_pad, GST_PAD_PROBE_TYPE_BUFFER,
        (GstPadProbeCallback) playout_item_pad_probe_pad_running_time,
        item, NULL);
  } else if (item->video_pad) {
    gst_pad_add_probe (item->video_pad, GST_PAD_PROBE_TYPE_BUFFER,
        (GstPadProbeCallback) playout_item_pad_probe_pad_running_time,
        item, NULL);
  } else {
    GST_ERROR ("%s: no pads were generated! Can't continue playing!", item->fn);
    return;
  }

  item->state = PLAYOUT_ITEM_STATE_PREPARED;
  GST_DEBUG ("%s: prepared", item->fn);

  if (item->app->state != PLAYOUT_APP_STATE_READY)
    /* This item will be activated when the previous one is EOS */
    return;

  GST_DEBUG ("Application isn't already playing; activate the item and prepare"
      " the next one");

  playout_app_activate_item (item);
  item->state = PLAYOUT_ITEM_STATE_ACTIVATED;
  item->app->state = PLAYOUT_APP_STATE_PLAYING;

  if (item->app->play_queue->len > 1)
    playout_app_prepare_item (g_ptr_array_index (item->app->play_queue, 1));
}

static GstElement *
playout_item_create_decoder (PlayoutItem * item)
{
  GstElement *bin, *dec;
  GError *err = NULL;
  gchar *uri;

  uri = gst_filename_to_uri (item->fn, &err);
  if (err != NULL) {
    GST_WARNING ("Could not convert '%s' to uri: %s", item->fn, err->message);
    g_clear_error (&err);
    return NULL;
  }

  bin = gst_bin_new (NULL);
  dec = gst_element_factory_make ("uridecodebin", NULL);
  g_object_set (dec, "uri", uri, NULL);
  g_free (uri);

  gst_bin_add (GST_BIN (bin), dec);

  g_signal_connect (dec, "pad-added", G_CALLBACK (playout_item_new_pad), item);
  g_signal_connect (dec, "no-more-pads", G_CALLBACK (playout_item_no_more_pads),
      item);

  return bin;
}

static void
playout_item_free (PlayoutItem * item)
{
  GST_DEBUG ("Entering free");
  switch (gst_element_set_state (item->decoder, GST_STATE_NULL)) {
    case GST_STATE_CHANGE_FAILURE:
      GST_ERROR ("%s: Unable to change state to NULL", item->fn);
      break;
    case GST_STATE_CHANGE_SUCCESS:
      GST_DEBUG ("%s: State change success", item->fn);
      break;
    default:
      GST_DEBUG ("%s: Some async/no-preroll", item->fn);
  }

  gst_bin_remove (GST_BIN (item->app->pipeline), item->decoder);
  GST_DEBUG ("%s: bin removed", item->fn);

  g_free (item->fn);
  g_free (item);
  GST_DEBUG ("item freed");
}

static guint64
playout_item_pad_get_segment_time (GstPad * srcpad)
{
  GstEvent *event;
  const GstSegment *segment;

  event = gst_pad_get_sticky_event (srcpad, GST_EVENT_SEGMENT, 0);
  if (!event)
    return 0;
  gst_event_parse_segment (event, &segment);
  gst_event_unref (event);
  return segment->time;
}

static void
playout_app_add_item (PlayoutApp * app, const gchar * fn)
{
  PlayoutItem *item;

  item = playout_item_new (app, fn);

  g_mutex_lock (&app->play_queue_lock);
  g_ptr_array_add (app->play_queue, item);
  g_mutex_unlock (&app->play_queue_lock);
}

static gboolean
playout_app_remove_item (PlayoutItem * item)
{
  PlayoutApp *app;
  GST_DEBUG ("%s: removing and freeing", item->fn);

  app = item->app;

  g_mutex_lock (&app->play_queue_lock);
  g_ptr_array_remove (app->play_queue, item);
  if (item->state >= PLAYOUT_ITEM_STATE_ACTIVATED)
    /* Removed item was playing; decrement the current-play-queue index */
    app->play_queue_current--;
  g_mutex_unlock (&app->play_queue_lock);

  /* Don't call this again */
  return FALSE;
}

static PlayoutItem *
playout_app_get_current_item (PlayoutApp * app)
{
  if (app->play_queue_current < 0 ||
      app->play_queue->len < (app->play_queue_current + 1))
    return NULL;

  return g_ptr_array_index (app->play_queue, app->play_queue_current);
}

static gboolean
playout_app_prepare_item (PlayoutItem * item)
{
  PlayoutApp *app = item->app;

  if (item->decoder != NULL)
    return TRUE;

  item->decoder = playout_item_create_decoder (item);

  if (item->decoder == NULL)
    return FALSE;

  gst_bin_add (GST_BIN (app->pipeline), item->decoder);

  if (!gst_element_sync_state_with_parent (item->decoder)) {
    GST_ERROR ("%s: unable to sync state with parent", item->fn);
    return FALSE;
  }

  GST_DEBUG ("%s: preparing", item->fn);

  /* All further processing is done in the "no-more-pads" callback of
   * uridecodebin */
  return TRUE;
}

/* Called exactly once for each item */
static gboolean
playout_app_activate_item (PlayoutItem * item)
{
  GstPad *sinkpad;
  guint64 segment_time;
  PlayoutApp *app = item->app;

  if (item->state != PLAYOUT_ITEM_STATE_PREPARED) {
    GST_ERROR ("Item %s is not ready to be activated!", item->fn);
    return FALSE;
  }

  if (!item->audio_pad && !item->video_pad) {
    GST_ERROR ("Item %s has no pads! Can't activate it!", item->fn);
    return FALSE;
  }

  /* Hook up to mixers and remove the probes blocking the pads */
  if (item->audio_pad) {
    GST_DEBUG ("%s: hooking up audio pad to mixer", item->fn);
    sinkpad = gst_element_request_pad_simple (app->audio_mixer, "sink_%u");
    gst_pad_link (item->audio_pad, sinkpad);

    segment_time = playout_item_pad_get_segment_time (item->audio_pad);
    if (segment_time > 0) {
      /* If the segment time is > 0, the new pad wants audiomixer to output audio
       * silence for that duration. This will cause audio glitches, so we  move
       * the pad offset back by that amount and tell audiomixer to start mixing
       * our buffers immediately. */
      GST_DEBUG ("%s: subtracting segment time %" G_GUINT64_FORMAT " from "
          "elapsed duration before setting it as the pad offset", item->fn,
          segment_time);
      if (app->elapsed_duration > segment_time)
        app->elapsed_duration -= segment_time;
      else
        app->elapsed_duration = 0;
    }

    if (app->elapsed_duration > 0) {
      GST_DEBUG ("%s: set audio pad offset to %" G_GUINT64_FORMAT "ms",
          item->fn, app->elapsed_duration / GST_MSECOND);
      gst_pad_set_offset (item->audio_pad, app->elapsed_duration);
    }

    if (item->audio_pad_probe_block_id > 0) {
      GST_DEBUG ("%s: removing audio pad block probe", item->fn);
      gst_pad_remove_probe (item->audio_pad, item->audio_pad_probe_block_id);
    }
    gst_object_unref (sinkpad);
  }

  if (item->video_pad) {
    GST_DEBUG ("%s: hooking up video pad to mixer", item->fn);
    sinkpad = gst_element_request_pad_simple (app->video_mixer, "sink_%u");

    /* Get new height/width/xpos/ypos such that the video scales up or down to
     * fit within the output video size without any cropping */
    gst_video_sink_center_rect (item->video_irect, item->app->video_orect,
        &item->video_orect, TRUE);
    GST_DEBUG ("%s: w: %i, h: %i, x: %i, y: %i", item->fn,
        item->video_orect.w, item->video_orect.h, item->video_orect.x,
        item->video_orect.y);
    g_object_set (sinkpad, "width", item->video_orect.w, "height",
        item->video_orect.h, "xpos", item->video_orect.x, "ypos",
        item->video_orect.y, NULL);

    /* If this is not the last item, on EOS, continue to aggregate using the
     * last buffer till the pad is released */
    if (item->app->play_queue->len != (item->app->play_queue_current + 2))
      g_object_set (sinkpad, "repeat-after-eos", TRUE, NULL);
    else
      GST_DEBUG ("%s: last item, not setting repeat-after-eos", item->fn);
    gst_pad_link (item->video_pad, sinkpad);

    if (app->elapsed_duration > 0) {
      GST_DEBUG ("%s: set video pad offset to %" G_GUINT64_FORMAT "ms",
          item->fn, app->elapsed_duration / GST_MSECOND);
      gst_pad_set_offset (item->video_pad, app->elapsed_duration);
    }

    if (item->video_pad_probe_block_id > 0) {
      GST_DEBUG ("%s: removing video pad block probe", item->fn);
      gst_pad_remove_probe (item->video_pad, item->video_pad_probe_block_id);
    }
    gst_object_unref (sinkpad);
  }

  item->state = PLAYOUT_ITEM_STATE_ACTIVATED;
  g_mutex_lock (&item->app->play_queue_lock);
  item->app->play_queue_current++;
  g_mutex_unlock (&item->app->play_queue_lock);

  GST_DEBUG ("%s: activated", item->fn);

  return TRUE;
}

/* Activate the next item, and prepare the one after that for later activation */
static gboolean
playout_app_activate_next_item (PlayoutApp * app)
{
  PlayoutItem *item;
  gboolean ret;

  if (app->play_queue->len < (app->play_queue_current + 2)) {
    g_print ("No more items to play\n");
    return FALSE;
  }

  item = g_ptr_array_index (app->play_queue, app->play_queue_current + 1);
  ret = playout_app_activate_item (item);
  if (!ret) {
    /* Tell caller, who can then decide whether to skip or error out */
    GST_ERROR ("%s: unable to activate", item->fn);
    return FALSE;
  }
  if (app->play_queue->len > (app->play_queue_current + 1)) {
    item = g_ptr_array_index (app->play_queue, app->play_queue_current + 1);
    /* FIXME: What if this fails? Prepare the next one in the queue? */
    ret = playout_app_prepare_item (item);
    if (!ret)
      GST_ERROR ("%s: unable to prepare", item->fn);
  }
  return ret;
}

static GstPadProbeReturn
playout_item_pad_probe_video_pad_running_time (GstPad * srcpad,
    GstPadProbeInfo * info, PlayoutItem * item)
{
  GstEvent *event;
  GstBuffer *buffer;
  guint64 running_time;
  const GstSegment *segment;

  buffer = GST_PAD_PROBE_INFO_BUFFER (info);
  event = gst_pad_get_sticky_event (srcpad, GST_EVENT_SEGMENT, 0);
  GST_TRACE ("%s: video sticky event: %" GST_PTR_FORMAT, item->fn, event);

  if (event) {
    gst_event_parse_segment (event, &segment);
    gst_event_unref (event);
    running_time = gst_segment_to_running_time (segment, GST_FORMAT_TIME,
        GST_BUFFER_PTS (buffer));
  } else {
    GST_WARNING ("%s: unable to parse video event for segment; falling back to "
        "pts", item->fn);
    running_time = GST_BUFFER_PTS (buffer);
  }

  if (running_time >= item->running_time) {
    /* The video buffer passing through video_mixer now matches the audio buffer
     * that passed through audio_mixer when the early switch was requested, so
     * this is the time to send an EOS to video_pad, which will complete the
     * switch */
    GST_DEBUG ("Sending video EOS to %s", item->fn);
    gst_pad_push_event (item->video_pad, gst_event_new_eos ());
    return GST_PAD_PROBE_DROP;
  } else {
    return GST_PAD_PROBE_PASS;
  }
}

static gboolean
playout_app_activate_next_item_early (PlayoutApp * app)
{
  PlayoutItem *item;

  item = playout_app_get_current_item (app);
  if (!item) {
    GST_WARNING ("Unable to switch early, no current item");
    return FALSE;
  }

  if (item->audio_pad) {
    /* If we have an audio pad, EOS audio first, always */
    GST_DEBUG ("Sending audio EOS to %s", item->fn);
    gst_pad_push_event (item->audio_pad, gst_event_new_eos ());
    /* We can't send the EOS to the video_pad yet because the running times for
     * both mixers are different due to buffering at the audio sink. So we wait
     * till the running time of the video_pad matches that of the audio_pad at
     * the time the audio EOS was sent, and then EOS video as well. */
    gst_pad_add_probe (item->video_pad, GST_PAD_PROBE_TYPE_BUFFER,
        (GstPadProbeCallback) playout_item_pad_probe_video_pad_running_time,
        item, NULL);
  } else if (item->video_pad) {
    /* If we have a video pad, EOS audio first, always */
    GST_DEBUG ("Sending video EOS to %s", item->fn);
    gst_pad_push_event (item->video_pad, gst_event_new_eos ());
  } else {
    g_assert_not_reached ();
  }

  /* Return FALSE so this function is called only once */
  return FALSE;
}

static gboolean
playout_app_play (PlayoutApp * app)
{
  PlayoutItem *item;

  item = app->play_queue->len ? g_ptr_array_index (app->play_queue, 0) : NULL;
  if (!item) {
    g_printerr ("Nothing to play\n");
    return FALSE;
  }

  playout_app_prepare_item (item);
  return TRUE;
}

/*
 * playout: An example application to sequentially and seamlessly play a list of
 * audio-video or video-only files.
 *
 * This example application uses the compositor and audiomixer elements combined
 * with pad probes to stitch together a list of A/V or V-only files in such
 * a way that audio and video glitching is minimised. Mixing A/V and V-only
 * files is not supported because it complicates the architecture quite a bit.
 *
 * Due to the fundamental difference in the representation of audio and video
 * data, unless constructed specifically for the purpose of being stitched back,
 * the audio and video tracks of files will rarely end at the same PTS. There is
 * usually a sync difference of a few frames. This application tries to stitch
 * together the audio tracks as perfectly as possible, and duplicates/drops
 * video frames if there is an underrun/overrun. Even when audio samples are
 * played back-to-back, there might be glitches due to quirks in the decoder.
 *
 * The list of PlayoutItems can be edited and added to dynamically; except the
 * currently-playing item and the next one (which has been prepared already).
 */
int
main (int argc, char **argv)
{
  GstBus *bus;
  gint switch_after_ms = 0;
  gchar **f, **filenames = NULL;
  GOptionEntry options[] = {
    {"switch-after", 's', 0, G_OPTION_ARG_INT, &switch_after_ms, "Time after "
          "which the next file will be forcibly activated", "MILLISECONDS"},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL,
        "FILENAME1 [FILENAME2] [FILENAME3] ..."},
    {NULL}
  };
  GOptionContext *ctx;
  PlayoutApp *app;
  GError *err = NULL;

  ctx = g_option_context_new (NULL);
  g_option_context_set_summary (ctx, "An example application to sequentially "
      "and seamlessly play a list of audio-video or video-only files.");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    if (err)
      g_printerr ("Error initializing: %s\n", err->message);
    else
      g_printerr ("Error initializing: Unknown error!\n");
    g_option_context_free (ctx);
    g_clear_error (&err);
    return 1;
  }

  if (filenames == NULL || *filenames == NULL) {
    g_printerr ("%s", g_option_context_get_help (ctx, TRUE, NULL));
    return 1;
  }

  g_option_context_free (ctx);

  GST_DEBUG_CATEGORY_INIT (playout, "playout", 0, "Playout example app");

  app = playout_app_new ();

  for (f = filenames; f != NULL && *f != NULL; ++f)
    playout_app_add_item (app, *f);

  g_strfreev (filenames);

  if (!playout_app_play (app))
    return 1;

  GST_DEBUG ("Setting pipeline to PLAYING");

  bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::eos", G_CALLBACK (playout_app_eos), app);
  gst_object_unref (bus);

  gst_element_set_state (app->pipeline, GST_STATE_PLAYING);

  if (switch_after_ms)
    g_timeout_add (switch_after_ms,
        (GSourceFunc) playout_app_activate_next_item_early, app);

  GST_DEBUG ("Running mainloop");
  g_main_loop_run (app->main_loop);

  playout_app_free (app);

  return 0;
}
