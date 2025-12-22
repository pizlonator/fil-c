/*
 * WebRTC Audio Processing Elements
 *
 *  Copyright 2016 Collabora Ltd
 *    @author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

/**
 * SECTION:element-webrtcechoprobe
 *
 * This echo probe is to be used with the webrtcdsp element. See #webrtcdsp
 * documentation for more details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstwebrtcechoprobe.h"

#include <modules/audio_processing/include/audio_processing.h>

#include <gst/audio/audio.h>

GST_DEBUG_CATEGORY_EXTERN (webrtc_dsp_debug);
#define GST_CAT_DEFAULT (webrtc_dsp_debug)

#define MAX_ADAPTER_SIZE (1*1024*1024)

static GstStaticPadTemplate gst_webrtc_echo_probe_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) { 48000, 32000, 16000, 8000 }, "
        "channels = (int) [1, MAX];"
        "audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (F32) ", "
        "layout = (string) non-interleaved, "
        "rate = (int) { 48000, 32000, 16000, 8000 }, "
        "channels = (int) [1, MAX]")
    );

static GstStaticPadTemplate gst_webrtc_echo_probe_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) { 48000, 32000, 16000, 8000 }, "
        "channels = (int) [1, MAX];"
        "audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (F32) ", "
        "layout = (string) non-interleaved, "
        "rate = (int) { 48000, 32000, 16000, 8000 }, "
        "channels = (int) [1, MAX]")
    );

G_LOCK_DEFINE_STATIC (gst_aec_probes);
static GList *gst_aec_probes = NULL;

G_DEFINE_TYPE (GstWebrtcEchoProbe, gst_webrtc_echo_probe,
    GST_TYPE_AUDIO_FILTER);
GST_ELEMENT_REGISTER_DEFINE (webrtcechoprobe, "webrtcechoprobe",
    GST_RANK_NONE, GST_TYPE_WEBRTC_ECHO_PROBE);

static gboolean
gst_webrtc_echo_probe_setup (GstAudioFilter * filter, const GstAudioInfo * info)
{
  GstWebrtcEchoProbe *self = GST_WEBRTC_ECHO_PROBE (filter);

  GST_LOG_OBJECT (self, "setting format to %s with %i Hz and %i channels",
      info->finfo->description, info->rate, info->channels);

  GST_WEBRTC_ECHO_PROBE_LOCK (self);

  self->info = *info;
  self->interleaved = (info->layout == GST_AUDIO_LAYOUT_INTERLEAVED);

  if (!self->interleaved)
    gst_planar_audio_adapter_configure (self->padapter, info);

  /* WebRTC library works with 10ms buffers, compute once this size */
  self->period_samples = info->rate / 100;
  self->period_size = self->period_samples * info->bpf;

  if (self->interleaved &&
      (MAX_DATA_SIZE_SAMPLES * 2) < self->period_size)
    goto period_too_big;

  GST_WEBRTC_ECHO_PROBE_UNLOCK (self);

  return TRUE;

period_too_big:
  GST_WEBRTC_ECHO_PROBE_UNLOCK (self);
  GST_WARNING_OBJECT (self, "webrtcdsp format produce too big period "
      "(maximum is %d samples and we have %u samples), "
      "reduce the number of channels or the rate.",
      MAX_DATA_SIZE_SAMPLES, self->period_size / 2);
  return FALSE;
}

static gboolean
gst_webrtc_echo_probe_stop (GstBaseTransform * btrans)
{
  GstWebrtcEchoProbe *self = GST_WEBRTC_ECHO_PROBE (btrans);

  GST_WEBRTC_ECHO_PROBE_LOCK (self);
  gst_adapter_clear (self->adapter);
  gst_planar_audio_adapter_clear (self->padapter);
  GST_WEBRTC_ECHO_PROBE_UNLOCK (self);

  return TRUE;
}

static gboolean
gst_webrtc_echo_probe_src_event (GstBaseTransform * btrans, GstEvent * event)
{
  GstBaseTransformClass *klass;
  GstWebrtcEchoProbe *self = GST_WEBRTC_ECHO_PROBE (btrans);
  GstClockTime latency;
  GstClockTime upstream_latency = 0;
  GstQuery *query;

  klass = GST_BASE_TRANSFORM_CLASS (gst_webrtc_echo_probe_parent_class);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_LATENCY:
      gst_event_parse_latency (event, &latency);
      query = gst_query_new_latency ();

      if (gst_pad_query (btrans->srcpad, query)) {
        gst_query_parse_latency (query, NULL, &upstream_latency, NULL);

        if (!GST_CLOCK_TIME_IS_VALID (upstream_latency))
          upstream_latency = 0;
      }

      GST_WEBRTC_ECHO_PROBE_LOCK (self);
      self->latency = latency;
      self->delay = upstream_latency / GST_MSECOND;
      GST_WEBRTC_ECHO_PROBE_UNLOCK (self);

      GST_DEBUG_OBJECT (self, "We have a latency of %" GST_TIME_FORMAT
          " and delay of %ims", GST_TIME_ARGS (latency),
          (gint) (upstream_latency / GST_MSECOND));
      break;
    default:
      break;
  }

  return klass->src_event (btrans, event);
}

static GstFlowReturn
gst_webrtc_echo_probe_transform_ip (GstBaseTransform * btrans,
    GstBuffer * buffer)
{
  GstWebrtcEchoProbe *self = GST_WEBRTC_ECHO_PROBE (btrans);
  GstBuffer *newbuf = NULL;

  GST_WEBRTC_ECHO_PROBE_LOCK (self);
  newbuf = gst_buffer_copy (buffer);
  /* Moves the buffer timestamp to be in Running time */
  GST_BUFFER_PTS (newbuf) = gst_segment_to_running_time (&btrans->segment,
      GST_FORMAT_TIME, GST_BUFFER_PTS (buffer));

  if (self->interleaved) {
    gst_adapter_push (self->adapter, newbuf);

    if (gst_adapter_available (self->adapter) > MAX_ADAPTER_SIZE)
      gst_adapter_flush (self->adapter,
          gst_adapter_available (self->adapter) - MAX_ADAPTER_SIZE);
  } else {
    gsize available;

    gst_planar_audio_adapter_push (self->padapter, newbuf);
    available =
        gst_planar_audio_adapter_available (self->padapter) * self->info.bpf;
    if (available > MAX_ADAPTER_SIZE)
      gst_planar_audio_adapter_flush (self->padapter,
          (available - MAX_ADAPTER_SIZE) / self->info.bpf);
  }

  GST_WEBRTC_ECHO_PROBE_UNLOCK (self);

  return GST_FLOW_OK;
}

static void
gst_webrtc_echo_probe_finalize (GObject * object)
{
  GstWebrtcEchoProbe *self = GST_WEBRTC_ECHO_PROBE (object);

  G_LOCK (gst_aec_probes);
  gst_aec_probes = g_list_remove (gst_aec_probes, self);
  G_UNLOCK (gst_aec_probes);

  gst_object_unref (self->adapter);
  gst_object_unref (self->padapter);
  self->adapter = NULL;
  self->padapter = NULL;

  G_OBJECT_CLASS (gst_webrtc_echo_probe_parent_class)->finalize (object);
}

static void
gst_webrtc_echo_probe_init (GstWebrtcEchoProbe * self)
{
  self->adapter = gst_adapter_new ();
  self->padapter = gst_planar_audio_adapter_new ();
  gst_audio_info_init (&self->info);
  g_mutex_init (&self->lock);

  self->latency = GST_CLOCK_TIME_NONE;

  G_LOCK (gst_aec_probes);
  gst_aec_probes = g_list_prepend (gst_aec_probes, self);
  G_UNLOCK (gst_aec_probes);
}

static void
gst_webrtc_echo_probe_class_init (GstWebrtcEchoProbeClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *btrans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstAudioFilterClass *audiofilter_class = GST_AUDIO_FILTER_CLASS (klass);

  gobject_class->finalize = gst_webrtc_echo_probe_finalize;

  btrans_class->passthrough_on_same_caps = TRUE;
  btrans_class->src_event = GST_DEBUG_FUNCPTR (gst_webrtc_echo_probe_src_event);
  btrans_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_webrtc_echo_probe_transform_ip);
  btrans_class->stop = GST_DEBUG_FUNCPTR (gst_webrtc_echo_probe_stop);

  audiofilter_class->setup = GST_DEBUG_FUNCPTR (gst_webrtc_echo_probe_setup);

  gst_element_class_add_static_pad_template (element_class,
      &gst_webrtc_echo_probe_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_webrtc_echo_probe_sink_template);

  gst_element_class_set_static_metadata (element_class,
      "Acoustic Echo Canceller probe",
      "Generic/Audio",
      "Gathers playback buffers for webrtcdsp",
      "Nicolas Dufresne <nicolas.dufrsesne@collabora.com>");
}


GstWebrtcEchoProbe *
gst_webrtc_acquire_echo_probe (const gchar * name)
{
  GstWebrtcEchoProbe *ret = NULL;
  GList *l;

  G_LOCK (gst_aec_probes);
  for (l = gst_aec_probes; l; l = l->next) {
    GstWebrtcEchoProbe *probe = GST_WEBRTC_ECHO_PROBE (l->data);

    GST_WEBRTC_ECHO_PROBE_LOCK (probe);
    if (!probe->acquired && g_strcmp0 (GST_OBJECT_NAME (probe), name) == 0) {
      probe->acquired = TRUE;
      ret = GST_WEBRTC_ECHO_PROBE (gst_object_ref (probe));
      GST_WEBRTC_ECHO_PROBE_UNLOCK (probe);
      break;
    }
    GST_WEBRTC_ECHO_PROBE_UNLOCK (probe);
  }
  G_UNLOCK (gst_aec_probes);

  return ret;
}

void
gst_webrtc_release_echo_probe (GstWebrtcEchoProbe * probe)
{
  GST_WEBRTC_ECHO_PROBE_LOCK (probe);
  probe->acquired = FALSE;
  GST_WEBRTC_ECHO_PROBE_UNLOCK (probe);
  gst_object_unref (probe);
}

gint
gst_webrtc_echo_probe_read (GstWebrtcEchoProbe * self, GstClockTime rec_time,
    GstBuffer ** buf, GstAudioInfo * info, gboolean * interleaved)
{
  GstClockTimeDiff diff;
  gsize avail, skip, offset, size = 0;
  gint delay = -1;

  GST_WEBRTC_ECHO_PROBE_LOCK (self);

  /* We always return a buffer -- if don't have data (size == 0), we generate a
   * silence buffer */

  if (!GST_CLOCK_TIME_IS_VALID (self->latency))
    goto copy;

  /* If we have a format, use that, else generate silence in input format */
  if (!GST_AUDIO_INFO_IS_VALID (&self->info)) {
    goto copy;
  } else {
   *info = self->info;
   *interleaved = self->interleaved;
  }

  if (self->interleaved)
    avail = gst_adapter_available (self->adapter) / self->info.bpf;
  else
    avail = gst_planar_audio_adapter_available (self->padapter);

  /* In delay agnostic mode, just return 10ms of data */
  if (!GST_CLOCK_TIME_IS_VALID (rec_time)) {
    if (avail < self->period_samples)
      goto copy;

    size = self->period_samples;
    skip = 0;
    offset = 0;

    goto copy;
  }

  if (avail == 0) {
    diff = G_MAXINT64;
  } else {
    GstClockTime play_time;
    guint64 distance;

    if (self->interleaved) {
      play_time = gst_adapter_prev_pts (self->adapter, &distance);
      distance /= self->info.bpf;
    } else {
      play_time = gst_planar_audio_adapter_prev_pts (self->padapter, &distance);
    }

    if (GST_CLOCK_TIME_IS_VALID (play_time)) {
      play_time += gst_util_uint64_scale_int (distance, GST_SECOND,
          self->info.rate);
      play_time += self->latency;

      diff = GST_CLOCK_DIFF (rec_time, play_time) / GST_MSECOND;
    } else {
      /* We have no timestamp, assume perfect delay */
      diff = self->delay;
    }
  }

  if (diff > self->delay) {
    skip = (diff - self->delay) * self->info.rate / 1000;
    skip = MIN (self->period_samples, skip);
    offset = 0;
  } else {
    skip = 0;
    offset = (self->delay - diff) * self->info.rate / 1000;
    offset = MIN (avail, offset);
  }

  size = MIN (avail - offset, self->period_samples - skip);

copy:
  if (!size) {
    /* No data, provide a period's worth of silence, using our format if we have
     * it, or the provided format if we don't */
    guint period_samples = info->rate / 100;
    guint period_size = period_samples * info->bpf;

    *buf = gst_buffer_new_allocate (NULL, period_size, NULL);
    gst_buffer_memset (*buf, 0, 0, period_size);
    gst_buffer_add_audio_meta (*buf, info, period_samples, NULL);
  } else {
    /* We have some actual data, pop period_samples' worth if have it, else pad
     * with silence and provide what we do have */
    GstBuffer *ret, *taken, *tmp;

    if (self->interleaved) {
      skip *= self->info.bpf;
      offset *= self->info.bpf;
      size *= self->info.bpf;

      gst_adapter_flush (self->adapter, offset);

      /* we need to fill silence at the beginning and/or the end of the
       * buffer in order to have period_samples in the buffer */
      if (size < self->period_size) {
        gsize padding = self->period_size - (skip + size);

        taken = gst_adapter_take_buffer (self->adapter, size);
        ret = gst_buffer_new ();

        /* need some silence at the beginning */
        if (skip) {
          tmp = gst_buffer_new_allocate (NULL, skip, NULL);
          gst_buffer_memset (tmp, 0, 0, skip);
          ret = gst_buffer_append (ret, tmp);
        }

        ret = gst_buffer_append (ret, taken);

        /* need some silence at the end */
        if (padding) {
          tmp = gst_buffer_new_allocate (NULL, padding, NULL);
          gst_buffer_memset (tmp, 0, 0, padding);
          ret = gst_buffer_append (ret, tmp);
        }
      } else {
        ret = gst_adapter_take_buffer (self->adapter, size);
      }
    } else {
      gst_planar_audio_adapter_flush (self->padapter, offset);

      /* we need to fill silence at the beginning and/or the end of each
       * channel plane in order to have exactly period_samples in the buffer */
      if (size < self->period_samples) {
        GstAudioMeta *meta;
        gint bps = self->info.finfo->width / 8;
        gsize padding = self->period_samples - (skip + size);
        gint c;

        taken = gst_planar_audio_adapter_take_buffer (self->padapter, size,
            GST_MAP_READ);
        meta = gst_buffer_get_audio_meta (taken);
        ret = gst_buffer_new ();

        for (c = 0; c < meta->info.channels; c++) {
          /* need some silence at the beginning */
          if (skip) {
            tmp = gst_buffer_new_allocate (NULL, skip * bps, NULL);
            gst_buffer_memset (tmp, 0, 0, skip * bps);
            ret = gst_buffer_append (ret, tmp);
          }

          tmp = gst_buffer_copy_region (taken, GST_BUFFER_COPY_MEMORY,
              meta->offsets[c], size * bps);
          ret = gst_buffer_append (ret, tmp);

          /* need some silence at the end */
          if (padding) {
            tmp = gst_buffer_new_allocate (NULL, padding * bps, NULL);
            gst_buffer_memset (tmp, 0, 0, padding * bps);
            ret = gst_buffer_append (ret, tmp);
          }
        }

        gst_buffer_unref (taken);
        gst_buffer_add_audio_meta (ret, &self->info, self->period_samples,
            NULL);
      } else {
        ret = gst_planar_audio_adapter_take_buffer (self->padapter, size,
          GST_MAP_READWRITE);
      }
    }

    *buf = ret;
  }

  delay = self->delay;

  GST_WEBRTC_ECHO_PROBE_UNLOCK (self);

  return delay;
}
