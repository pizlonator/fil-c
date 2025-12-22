/* GStreamer
 * Copyright (C) 2012 Fluendo S.A. <support@fluendo.com>
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
#  include <config.h>
#endif

#include <string.h>

#include "opensles.h"
#include "openslesringbuffer.h"

GST_DEBUG_CATEGORY_STATIC (opensles_ringbuffer_debug);
#define GST_CAT_DEFAULT opensles_ringbuffer_debug

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (opensles_ringbuffer_debug, \
      "opensles_ringbuffer", 0, "OpenSL ES ringbuffer");

#define parent_class gst_opensles_ringbuffer_parent_class
G_DEFINE_TYPE_WITH_CODE (GstOpenSLESRingBuffer, gst_opensles_ringbuffer,
    GST_TYPE_AUDIO_RING_BUFFER, _do_init);

/*
 * Some generic helper functions
 */

static inline SLuint32
_opensles_sample_rate (guint rate)
{
  switch (rate) {
    case 8000:
      return SL_SAMPLINGRATE_8;
    case 11025:
      return SL_SAMPLINGRATE_11_025;
    case 12000:
      return SL_SAMPLINGRATE_12;
    case 16000:
      return SL_SAMPLINGRATE_16;
    case 22050:
      return SL_SAMPLINGRATE_22_05;
    case 24000:
      return SL_SAMPLINGRATE_24;
    case 32000:
      return SL_SAMPLINGRATE_32;
    case 44100:
      return SL_SAMPLINGRATE_44_1;
    case 48000:
      return SL_SAMPLINGRATE_48;
    case 64000:
      return SL_SAMPLINGRATE_64;
    case 88200:
      return SL_SAMPLINGRATE_88_2;
    case 96000:
      return SL_SAMPLINGRATE_96;
    case 192000:
      return SL_SAMPLINGRATE_192;
    default:
      return 0;
  }
}

static inline SLuint32
_opensles_channel_mask (GstAudioRingBufferSpec * spec)
{
  switch (spec->info.channels) {
    case 1:
      return (SL_SPEAKER_FRONT_CENTER);
    case 2:
      return (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);
    default:
      return 0;
  }
}

static inline void
_opensles_format (GstAudioRingBufferSpec * spec, SLDataFormat_PCM * format)
{
  format->formatType = SL_DATAFORMAT_PCM;
  format->numChannels = spec->info.channels;
  format->samplesPerSec = _opensles_sample_rate (spec->info.rate);
  format->bitsPerSample = spec->info.finfo->depth;
  format->containerSize = spec->info.finfo->width;
  format->channelMask = _opensles_channel_mask (spec);
  format->endianness =
      ((spec->info.finfo->endianness ==
          G_BIG_ENDIAN) ? SL_BYTEORDER_BIGENDIAN : SL_BYTEORDER_LITTLEENDIAN);
}

/* 
 * Recorder related functions
 */

static gboolean
_opensles_recorder_acquire (GstAudioRingBuffer * rb,
    GstAudioRingBufferSpec * spec)
{
  GstOpenSLESRingBuffer *thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  SLresult result;
  SLDataFormat_PCM format;
  SLAndroidConfigurationItf config;

  /* Configure audio source */
  SLDataLocator_IODevice loc_dev = {
    SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
    SL_DEFAULTDEVICEID_AUDIOINPUT, NULL
  };
  SLDataSource audioSrc = { &loc_dev, NULL };

  /* Configure audio sink */
  SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
    SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
  };
  SLDataSink audioSink = { &loc_bq, &format };

  /* Required optional interfaces */
  const SLInterfaceID ids[2] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
    SL_IID_ANDROIDCONFIGURATION
  };
  const SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_FALSE };

  /* Define the audio format in OpenSL ES terminology */
  _opensles_format (spec, &format);

  /* Create the audio recorder object (requires the RECORD_AUDIO permission) */
  result = (*thiz->engineEngine)->CreateAudioRecorder (thiz->engineEngine,
      &thiz->recorderObject, &audioSrc, &audioSink, 2, ids, req);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "engine.CreateAudioRecorder failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Set the recording preset if we have one */
  if (thiz->preset != GST_OPENSLES_RECORDING_PRESET_NONE) {
    SLint32 preset = gst_to_opensles_recording_preset (thiz->preset);

    result = (*thiz->recorderObject)->GetInterface (thiz->recorderObject,
        SL_IID_ANDROIDCONFIGURATION, &config);

    if (result == SL_RESULT_SUCCESS) {
      result = (*config)->SetConfiguration (config,
          SL_ANDROID_KEY_RECORDING_PRESET, &preset, sizeof (preset));

      if (result != SL_RESULT_SUCCESS) {
        GST_WARNING_OBJECT (thiz, "Failed to set playback stream type (0x%08x)",
            (guint32) result);
      }
    } else {
      GST_WARNING_OBJECT (thiz,
          "Could not get configuration interface 0x%08x", (guint32) result);
    }
  }

  /* Realize the audio recorder object */
  result =
      (*thiz->recorderObject)->Realize (thiz->recorderObject, SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "recorder.Realize failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Get the record interface */
  result = (*thiz->recorderObject)->GetInterface (thiz->recorderObject,
      SL_IID_RECORD, &thiz->recorderRecord);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "recorder.GetInterface(Record) failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Get the buffer queue interface */
  result =
      (*thiz->recorderObject)->GetInterface (thiz->recorderObject,
      SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &thiz->bufferQueue);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "recorder.GetInterface(BufferQueue) failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  return TRUE;

failed:
  return FALSE;
}

/* This callback function is executed when the ringbuffer is started to preroll
 * the output buffer queue with empty buffers, from app thread, and each time
 * there's a filled buffer, from audio device processing thread,
 * the callback behaviour.
 */
static void
_opensles_recorder_cb (SLAndroidSimpleBufferQueueItf bufferQueue, void *context)
{
  GstAudioRingBuffer *rb = GST_AUDIO_RING_BUFFER_CAST (context);
  GstOpenSLESRingBuffer *thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  SLresult result;
  guint8 *ptr;
  gint seg;
  gint len;

  /* Advance only when we are called by the callback function */
  if (bufferQueue) {
    gst_audio_ring_buffer_advance (rb, 1);
  }

  /* Get a segment form the GStreamer ringbuffer to write in */
  if (!gst_audio_ring_buffer_prepare_read (rb, &seg, &ptr, &len)) {
    GST_WARNING_OBJECT (rb, "No segment available");
    return;
  }

  GST_LOG_OBJECT (thiz, "enqueue: %p size %d segment: %d", ptr, len, seg);

  /* Enqueue the sefment as buffer to be written */
  result = (*thiz->bufferQueue)->Enqueue (thiz->bufferQueue, ptr, len);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "bufferQueue.Enqueue failed(0x%08x)",
        (guint32) result);
    return;
  }
}

static gboolean
_opensles_recorder_start (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  SLresult result;

  /* Register callback on the buffer queue */
  if (!thiz->is_queue_callback_registered) {
    result = (*thiz->bufferQueue)->RegisterCallback (thiz->bufferQueue,
        _opensles_recorder_cb, rb);
    if (result != SL_RESULT_SUCCESS) {
      GST_ERROR_OBJECT (thiz, "bufferQueue.RegisterCallback failed(0x%08x)",
          (guint32) result);
      return FALSE;
    }
    thiz->is_queue_callback_registered = TRUE;
  }

  /* Preroll one buffer */
  _opensles_recorder_cb (NULL, rb);

  /* Start recording */
  result =
      (*thiz->recorderRecord)->SetRecordState (thiz->recorderRecord,
      SL_RECORDSTATE_RECORDING);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "recorder.SetRecordState failed(0x%08x)",
        (guint32) result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
_opensles_recorder_stop (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  SLresult result;

  /* Stop recording */
  result =
      (*thiz->recorderRecord)->SetRecordState (thiz->recorderRecord,
      SL_RECORDSTATE_STOPPED);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "recorder.SetRecordState failed(0x%08x)",
        (guint32) result);
    return FALSE;
  }

  /* Unregister callback on the buffer queue */
  result = (*thiz->bufferQueue)->RegisterCallback (thiz->bufferQueue,
      NULL, NULL);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "bufferQueue.RegisterCallback failed(0x%08x)",
        (guint32) result);
    return FALSE;
  }
  thiz->is_queue_callback_registered = FALSE;

  /* Reset the queue */
  result = (*thiz->bufferQueue)->Clear (thiz->bufferQueue);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "bufferQueue.Clear failed(0x%08x)",
        (guint32) result);
    return FALSE;
  }

  return TRUE;
}

/*
 * Player related functions
 */

static gboolean
_opensles_player_change_volume (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;
  SLresult result;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);

  if (thiz->playerVolume) {
    gint millibel = (1.0 - thiz->volume) * -5000.0;
    result =
        (*thiz->playerVolume)->SetVolumeLevel (thiz->playerVolume, millibel);
    if (result != SL_RESULT_SUCCESS) {
      GST_ERROR_OBJECT (thiz, "player.SetVolumeLevel failed(0x%08x)",
          (guint32) result);
      return FALSE;
    }
    GST_DEBUG_OBJECT (thiz, "changed volume to %d", millibel);
  }

  return TRUE;
}

static gboolean
_opensles_player_change_mute (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;
  SLresult result;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);

  if (thiz->playerVolume) {
    result = (*thiz->playerVolume)->SetMute (thiz->playerVolume, thiz->mute);
    if (result != SL_RESULT_SUCCESS) {
      GST_ERROR_OBJECT (thiz, "player.SetMute failed(0x%08x)",
          (guint32) result);
      return FALSE;
    }
    GST_DEBUG_OBJECT (thiz, "changed mute to %d", thiz->mute);
  }

  return TRUE;
}

/* This is a callback function invoked by the playback device thread and
 * it's used to monitor position changes */
static void
_opensles_player_event_cb (SLPlayItf caller, void *context, SLuint32 event)
{
  if (event & SL_PLAYEVENT_HEADATNEWPOS) {
    SLmillisecond position;

    (*caller)->GetPosition (caller, &position);
    GST_LOG_OBJECT (context, "at position=%u ms", (guint) position);
  }
}

static gboolean
_opensles_player_acquire (GstAudioRingBuffer * rb,
    GstAudioRingBufferSpec * spec)
{
  GstOpenSLESRingBuffer *thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  SLresult result;
  SLDataFormat_PCM format;
  SLAndroidConfigurationItf config;

  /* Configure audio source
   * 4 buffers is the "typical" size as optimized inside Android's
   * OpenSL ES, see frameworks/wilhelm/src/itfstruct.h BUFFER_HEADER_TYPICAL
   *
   * Also only use half of our segment size to make sure that there's always
   * some more queued up in our ringbuffer and we don't start to read silence.
   */
  SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
    SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, MIN (4, MAX (spec->segtotal >> 1,
            1))
  };
  SLDataSource audioSrc = { &loc_bufq, &format };

  /* Configure audio sink */
  SLDataLocator_OutputMix loc_outmix = {
    SL_DATALOCATOR_OUTPUTMIX, thiz->outputMixObject
  };
  SLDataSink audioSink = { &loc_outmix, NULL };

  /* Define the required interfaces */
  const SLInterfaceID ids[3] = { SL_IID_BUFFERQUEUE, SL_IID_VOLUME,
    SL_IID_ANDROIDCONFIGURATION
  };
  const SLboolean req[3] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
    SL_BOOLEAN_FALSE
  };

  /* Define the format in OpenSL ES terminology */
  _opensles_format (spec, &format);

  /* Create the player object */
  result = (*thiz->engineEngine)->CreateAudioPlayer (thiz->engineEngine,
      &thiz->playerObject, &audioSrc, &audioSink, 3, ids, req);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "engine.CreateAudioPlayer failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Set the stream type if we have one */
  if (thiz->stream_type != GST_OPENSLES_STREAM_TYPE_NONE) {
    SLint32 stream_type = gst_to_opensles_stream_type (thiz->stream_type);

    result = (*thiz->playerObject)->GetInterface (thiz->playerObject,
        SL_IID_ANDROIDCONFIGURATION, &config);

    if (result == SL_RESULT_SUCCESS) {
      result = (*config)->SetConfiguration (config,
          SL_ANDROID_KEY_STREAM_TYPE, &stream_type, sizeof (stream_type));

      if (result != SL_RESULT_SUCCESS) {
        GST_WARNING_OBJECT (thiz, "Failed to set playback stream type (0x%08x)",
            (guint32) result);
      }
    } else {
      GST_WARNING_OBJECT (thiz,
          "Could not get configuration interface 0x%08x", (guint32) result);
    }
  }

  /* Realize the player object */
  result =
      (*thiz->playerObject)->Realize (thiz->playerObject, SL_BOOLEAN_FALSE);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.Realize failed(0x%08x)", (guint32) result);
    goto failed;
  }

  /* Get the play interface */
  result = (*thiz->playerObject)->GetInterface (thiz->playerObject,
      SL_IID_PLAY, &thiz->playerPlay);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.GetInterface(Play) failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Get the buffer queue interface */
  result = (*thiz->playerObject)->GetInterface (thiz->playerObject,
      SL_IID_BUFFERQUEUE, &thiz->bufferQueue);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.GetInterface(BufferQueue) failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Get the volume interface */
  result = (*thiz->playerObject)->GetInterface (thiz->playerObject,
      SL_IID_VOLUME, &thiz->playerVolume);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.GetInterface(Volume) failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Request position update events at each 20 ms */
  result = (*thiz->playerPlay)->SetPositionUpdatePeriod (thiz->playerPlay, 20);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.SetPositionUpdatePeriod failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Define the event mask to be monitorized */
  result = (*thiz->playerPlay)->SetCallbackEventsMask (thiz->playerPlay,
      SL_PLAYEVENT_HEADATNEWPOS);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.SetCallbackEventsMask failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Register a callback to process the events */
  result = (*thiz->playerPlay)->RegisterCallback (thiz->playerPlay,
      _opensles_player_event_cb, thiz);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.RegisterCallback(event_cb) failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  /* Configure the volume and mute state */
  _opensles_player_change_volume (rb);
  _opensles_player_change_mute (rb);

  /* Allocate the queue associated ringbuffer memory */
  thiz->data_segtotal = loc_bufq.numBuffers;
  thiz->data_size = spec->segsize * thiz->data_segtotal;
  thiz->data = g_malloc0 (thiz->data_size);
  g_atomic_int_set (&thiz->segqueued, 0);
  g_atomic_int_set (&thiz->is_prerolled, 0);
  thiz->cursor = 0;

  return TRUE;

failed:
  return FALSE;
}

/* This callback function is executed when the ringbuffer is started to preroll
 * the input buffer queue with few buffers, from app thread, and each time
 * that rendering of one buffer finishes, from audio device processing thread,
 * the callback behaviour.
 *
 * We wrap the queue behaviour with an appropriate chunk of memory (queue len *
 * ringbuffer segment size) which is used to hold the audio data while it's 
 * being processed in the queue. The memory region is used whit a ringbuffer
 * behaviour.
 */
static void
_opensles_player_cb (SLAndroidSimpleBufferQueueItf bufferQueue, void *context)
{
  GstAudioRingBuffer *rb = GST_AUDIO_RING_BUFFER_CAST (context);
  GstOpenSLESRingBuffer *thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  SLresult result;
  guint8 *ptr, *cur;
  gint seg;
  gint len;

  /* Get a segment form the GStreamer ringbuffer to read some samples */
  if (!gst_audio_ring_buffer_prepare_read (rb, &seg, &ptr, &len)) {
    GST_WARNING_OBJECT (rb, "No segment available");
    return;
  }

  /* copy the segment data to our queue associated ringbuffer memory */
  cur = thiz->data + (thiz->cursor * rb->spec.segsize);
  memcpy (cur, ptr, len);
  g_atomic_int_inc (&thiz->segqueued);

  GST_LOG_OBJECT (thiz, "enqueue: %p size %d segment: %d in queue[%d]",
      cur, len, seg, thiz->cursor);
  /* advance the cursor in our queue associated ringbuffer */
  thiz->cursor = (thiz->cursor + 1) % thiz->data_segtotal;

  /* Enqueue the buffer to be rendered */
  result = (*thiz->bufferQueue)->Enqueue (thiz->bufferQueue, cur, len);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "bufferQueue.Enqueue failed(0x%08x)",
        (guint32) result);
    return;
  }

  /* Fill with silence samples the segment of the GStreamer ringbuffer */
  gst_audio_ring_buffer_clear (rb, seg);
  /* Make the segment reusable */
  gst_audio_ring_buffer_advance (rb, 1);
}

static gboolean
_opensles_player_start (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  SLresult result;

  /* Register callback on the buffer queue */
  if (!thiz->is_queue_callback_registered) {
    result = (*thiz->bufferQueue)->RegisterCallback (thiz->bufferQueue,
        _opensles_player_cb, rb);
    if (result != SL_RESULT_SUCCESS) {
      GST_ERROR_OBJECT (thiz, "bufferQueue.RegisterCallback failed(0x%08x)",
          (guint32) result);
      return FALSE;
    }
    thiz->is_queue_callback_registered = TRUE;
  }

  /* Fill the queue by enqueing a buffer */
  if (!g_atomic_int_get (&thiz->is_prerolled)) {
    _opensles_player_cb (NULL, rb);
    g_atomic_int_set (&thiz->is_prerolled, 1);
  }

  /* Change player state into PLAYING */
  result =
      (*thiz->playerPlay)->SetPlayState (thiz->playerPlay,
      SL_PLAYSTATE_PLAYING);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.SetPlayState failed(0x%08x)",
        (guint32) result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
_opensles_player_pause (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  SLresult result;

  result =
      (*thiz->playerPlay)->SetPlayState (thiz->playerPlay, SL_PLAYSTATE_PAUSED);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.SetPlayState failed(0x%08x)",
        (guint32) result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
_opensles_player_stop (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  SLresult result;

  /* Change player state into STOPPED */
  result =
      (*thiz->playerPlay)->SetPlayState (thiz->playerPlay,
      SL_PLAYSTATE_STOPPED);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "player.SetPlayState failed(0x%08x)",
        (guint32) result);
    return FALSE;
  }

  /* Unregister callback on the buffer queue */
  result = (*thiz->bufferQueue)->RegisterCallback (thiz->bufferQueue,
      NULL, NULL);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "bufferQueue.RegisterCallback failed(0x%08x)",
        (guint32) result);
    return FALSE;
  }
  thiz->is_queue_callback_registered = FALSE;

  /* Reset the queue */
  result = (*thiz->bufferQueue)->Clear (thiz->bufferQueue);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "bufferQueue.Clear failed(0x%08x)",
        (guint32) result);
    return FALSE;
  }

  /* Reset our state */
  g_atomic_int_set (&thiz->segqueued, 0);
  thiz->cursor = 0;

  return TRUE;
}

/*
 * OpenSL ES ringbuffer wrapper
 */

GstAudioRingBuffer *
gst_opensles_ringbuffer_new (RingBufferMode mode)
{
  GstOpenSLESRingBuffer *thiz;

  g_return_val_if_fail (mode > RB_MODE_NONE && mode < RB_MODE_LAST, NULL);

  thiz = g_object_new (GST_TYPE_OPENSLES_RING_BUFFER, NULL);

  if (thiz) {
    thiz->mode = mode;
    if (mode == RB_MODE_SRC) {
      thiz->acquire = _opensles_recorder_acquire;
      thiz->start = _opensles_recorder_start;
      thiz->pause = _opensles_recorder_stop;
      thiz->stop = _opensles_recorder_stop;
      thiz->change_volume = NULL;
    } else if (mode == RB_MODE_SINK_PCM) {
      thiz->acquire = _opensles_player_acquire;
      thiz->start = _opensles_player_start;
      thiz->pause = _opensles_player_pause;
      thiz->stop = _opensles_player_stop;
      thiz->change_volume = _opensles_player_change_volume;
    }
  }

  GST_DEBUG_OBJECT (thiz, "ringbuffer created");

  return GST_AUDIO_RING_BUFFER (thiz);
}

void
gst_opensles_ringbuffer_set_volume (GstAudioRingBuffer * rb, gfloat volume)
{
  GstOpenSLESRingBuffer *thiz;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);

  thiz->volume = volume;

  if (thiz->change_volume) {
    thiz->change_volume (rb);
  }
}

void
gst_opensles_ringbuffer_set_mute (GstAudioRingBuffer * rb, gboolean mute)
{
  GstOpenSLESRingBuffer *thiz;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);

  thiz->mute = mute;

  if (thiz->change_mute) {
    thiz->change_mute (rb);
  }
}

static gboolean
gst_opensles_ringbuffer_open_device (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;
  SLresult result;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);

  /* Create and realize the engine object */
  thiz->engineObject = gst_opensles_get_engine ();
  if (!thiz->engineObject) {
    GST_ERROR_OBJECT (thiz, "Failed to get engine object");
    goto failed;
  }

  /* Get the engine interface, which is needed in order to create other objects */
  result = (*thiz->engineObject)->GetInterface (thiz->engineObject,
      SL_IID_ENGINE, &thiz->engineEngine);
  if (result != SL_RESULT_SUCCESS) {
    GST_ERROR_OBJECT (thiz, "engine.GetInterface(Engine) failed(0x%08x)",
        (guint32) result);
    goto failed;
  }

  if (thiz->mode == RB_MODE_SINK_PCM) {
    SLOutputMixItf outputMix;

    /* Create an output mixer object */
    result = (*thiz->engineEngine)->CreateOutputMix (thiz->engineEngine,
        &thiz->outputMixObject, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
      GST_ERROR_OBJECT (thiz, "engine.CreateOutputMix failed(0x%08x)",
          (guint32) result);
      goto failed;
    }

    /* Realize the output mixer object */
    result = (*thiz->outputMixObject)->Realize (thiz->outputMixObject,
        SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
      GST_ERROR_OBJECT (thiz, "outputMix.Realize failed(0x%08x)",
          (guint32) result);
      goto failed;
    }

    /* Get the mixer interface */
    result = (*thiz->outputMixObject)->GetInterface (thiz->outputMixObject,
        SL_IID_OUTPUTMIX, &outputMix);
    if (result != SL_RESULT_SUCCESS) {
      GST_WARNING_OBJECT (thiz, "outputMix.GetInterface failed(0x%08x)",
          (guint32) result);
    } else {
      SLint32 numDevices = MAX_NUMBER_OUTPUT_DEVICES;
      SLuint32 deviceIDs[MAX_NUMBER_OUTPUT_DEVICES];
      gint i;

      /* Query the list of output devices */
      (*outputMix)->GetDestinationOutputDeviceIDs (outputMix, &numDevices,
          deviceIDs);
      GST_DEBUG_OBJECT (thiz, "Found %d output devices", (gint) numDevices);
      for (i = 0; i < numDevices; i++) {
        GST_DEBUG_OBJECT (thiz, "  DeviceID: %08x", (guint) deviceIDs[i]);
      }
    }
  }

  GST_DEBUG_OBJECT (thiz, "device opened");
  return TRUE;

failed:
  return FALSE;
}

static gboolean
gst_opensles_ringbuffer_close_device (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);

  /* Destroy the output mix object */
  if (thiz->outputMixObject) {
    (*thiz->outputMixObject)->Destroy (thiz->outputMixObject);
    thiz->outputMixObject = NULL;
  }

  /* Destroy the engine object and invalidate all associated interfaces */
  if (thiz->engineObject) {
    gst_opensles_release_engine (thiz->engineObject);
    thiz->engineObject = NULL;
    thiz->engineEngine = NULL;
  }

  thiz->bufferQueue = NULL;

  GST_DEBUG_OBJECT (thiz, "device closed");
  return TRUE;
}

static gboolean
gst_opensles_ringbuffer_acquire (GstAudioRingBuffer * rb,
    GstAudioRingBufferSpec * spec)
{
  GstOpenSLESRingBuffer *thiz;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);

  /* Instantiate and configure the OpenSL ES interfaces */
  if (!thiz->acquire (rb, spec)) {
    return FALSE;
  }

  /* Initialize our ringbuffer memory region */
  rb->size = spec->segtotal * spec->segsize;
  rb->memory = g_malloc0 (rb->size);

  GST_DEBUG_OBJECT (thiz, "ringbuffer acquired");
  return TRUE;
}

static gboolean
gst_opensles_ringbuffer_release (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;

  thiz = GST_OPENSLES_RING_BUFFER (rb);

  /* XXX: We need to sleep a bit before destroying the player object
   * because of a bug in Android in versions < 4.2.
   *
   * OpenSLES is using AudioTrack for rendering the sound. AudioTrack
   * has a thread that pulls raw audio from the buffer queue and then
   * passes it forward to AudioFlinger (AudioTrack::processAudioBuffer()).
   * This thread is calling various callbacks on events, e.g. when
   * an underrun happens or to request data. OpenSLES sets this callback
   * on AudioTrack (audioTrack_callBack_pullFromBuffQueue() from
   * android_AudioPlayer.cpp). Among other things this is taking a lock
   * on the player interface.
   *
   * Now if we destroy the player interface object, it will first of all
   * take the player interface lock (IObject_Destroy()). Then it destroys
   * the audio player instance (android_audioPlayer_destroy()) which then
   * calls stop() on the AudioTrack and deletes it. Now the destructor of
   * AudioTrack will wait until the rendering thread (AudioTrack::processAudioBuffer())
   * has finished.
   *
   * If all this happens with bad timing it can happen that the rendering
   * thread is currently e.g. handling underrun but did not lock the player
   * interface object yet. Then destroying happens and takes the lock and waits
   * for the thread to finish. Then the thread tries to take the lock and waits
   * forever.
   *
   * We wait a bit before destroying the player object to make sure that
   * the rendering thread finished whatever it was doing, and then stops
   * (note: we called gst_opensles_ringbuffer_stop() before this already).
   */

  /* Destroy audio player object, and invalidate all associated interfaces */
  if (thiz->playerObject) {
    g_usleep (50000);
    (*thiz->playerObject)->Destroy (thiz->playerObject);
    thiz->playerObject = NULL;
    thiz->playerPlay = NULL;
    thiz->playerVolume = NULL;
  }

  /* Destroy audio recorder object, and invalidate all associated interfaces */
  if (thiz->recorderObject) {
    g_usleep (50000);
    (*thiz->recorderObject)->Destroy (thiz->recorderObject);
    thiz->recorderObject = NULL;
    thiz->recorderRecord = NULL;
  }

  if (thiz->data) {
    g_free (thiz->data);
    thiz->data = NULL;
  }

  if (rb->memory) {
    g_free (rb->memory);
    rb->memory = NULL;
    rb->size = 0;
  }

  GST_DEBUG_OBJECT (thiz, "ringbuffer released");
  return TRUE;
}

static gboolean
gst_opensles_ringbuffer_start (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;
  gboolean res;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  res = thiz->start (rb);

  GST_DEBUG_OBJECT (thiz, "ringbuffer %s started", (res ? "" : "not"));
  return res;
}

static gboolean
gst_opensles_ringbuffer_pause (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;
  gboolean res;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  res = thiz->pause (rb);

  GST_DEBUG_OBJECT (thiz, "ringbuffer %s paused", (res ? "" : "not"));
  return res;
}

static gboolean
gst_opensles_ringbuffer_stop (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;
  gboolean res;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);
  res = thiz->stop (rb);

  GST_DEBUG_OBJECT (thiz, "ringbuffer %s stopped", (res ? " " : "not"));
  return res;
}

static guint
gst_opensles_ringbuffer_delay (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;
  guint res = 0;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);

  if (thiz->playerPlay) {
    SLuint32 state;
    SLmillisecond position;
    guint64 playedpos = 0, queuedpos = 0;
    (*thiz->playerPlay)->GetPlayState (thiz->playerPlay, &state);
    if (state == SL_PLAYSTATE_PLAYING) {
      (*thiz->playerPlay)->GetPosition (thiz->playerPlay, &position);
      playedpos =
          gst_util_uint64_scale_round (position, rb->spec.info.rate, 1000);
      queuedpos = g_atomic_int_get (&thiz->segqueued) * rb->samples_per_seg;
      if (queuedpos < playedpos) {
        res = 0;
        GST_ERROR_OBJECT (thiz,
            "Queued position smaller than playback position (%" G_GUINT64_FORMAT
            " < %" G_GUINT64_FORMAT ")", queuedpos, playedpos);
      } else {
        res = queuedpos - playedpos;
      }
    }

    GST_LOG_OBJECT (thiz, "queued samples %" G_GUINT64_FORMAT " position %u ms "
        "(%" G_GUINT64_FORMAT " samples) delay %u samples",
        queuedpos, (guint) position, playedpos, res);
  }

  return res;
}

static void
gst_opensles_ringbuffer_clear_all (GstAudioRingBuffer * rb)
{
  GstOpenSLESRingBuffer *thiz;

  thiz = GST_OPENSLES_RING_BUFFER_CAST (rb);

  if (thiz->data) {
    SLresult result;

    memset (thiz->data, 0, thiz->data_size);
    g_atomic_int_set (&thiz->segqueued, 0);
    thiz->cursor = 0;
    /* Reset the queue */
    result = (*thiz->bufferQueue)->Clear (thiz->bufferQueue);
    if (result != SL_RESULT_SUCCESS) {
      GST_WARNING_OBJECT (thiz, "bufferQueue.Clear failed(0x%08x)",
          (guint32) result);
    }
    g_atomic_int_set (&thiz->is_prerolled, 0);
  }

  GST_CALL_PARENT (GST_AUDIO_RING_BUFFER_CLASS, clear_all, (rb));
}

static void
gst_opensles_ringbuffer_dispose (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_opensles_ringbuffer_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_opensles_ringbuffer_class_init (GstOpenSLESRingBufferClass * klass)
{
  GObjectClass *gobject_class;
  GstAudioRingBufferClass *gstringbuffer_class;

  gobject_class = (GObjectClass *) klass;
  gstringbuffer_class = (GstAudioRingBufferClass *) klass;

  gobject_class->dispose = gst_opensles_ringbuffer_dispose;
  gobject_class->finalize = gst_opensles_ringbuffer_finalize;

  gstringbuffer_class->open_device =
      GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_open_device);
  gstringbuffer_class->close_device =
      GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_close_device);
  gstringbuffer_class->acquire =
      GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_acquire);
  gstringbuffer_class->release =
      GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_release);
  gstringbuffer_class->start =
      GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_start);
  gstringbuffer_class->pause =
      GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_pause);
  gstringbuffer_class->resume =
      GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_start);
  gstringbuffer_class->stop = GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_stop);
  gstringbuffer_class->delay =
      GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_delay);
  gstringbuffer_class->clear_all =
      GST_DEBUG_FUNCPTR (gst_opensles_ringbuffer_clear_all);
}

static void
gst_opensles_ringbuffer_init (GstOpenSLESRingBuffer * thiz)
{
  thiz->mode = RB_MODE_NONE;
  thiz->engineObject = NULL;
  thiz->outputMixObject = NULL;
  thiz->playerObject = NULL;
  thiz->recorderObject = NULL;
  thiz->is_queue_callback_registered = FALSE;
}
