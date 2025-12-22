/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Copyright (C) 2023 Pengutronix e.K. - www.pengutronix.de
 *
 */

#pragma once

#include "linux/usb/g_uvc.h"
#include "linux/usb/video.h"
#include "linux/videodev2.h"

#include <gst/gst.h>

#include "configfs.h"

G_BEGIN_DECLS GST_DEBUG_CATEGORY_EXTERN (uvcsink_debug);

#define GST_TYPE_UVCSINK (gst_uvc_sink_get_type())
G_DECLARE_FINAL_TYPE (GstUvcSink, gst_uvc_sink, GST, UVCSINK, GstBin)

GST_ELEMENT_REGISTER_DECLARE (uvcsink);

struct _GstUvcSink
{
  GstBin bin;
  GstElement *fakesink;
  GstElement *v4l2sink;
  GstPad *sinkpad;
  GstPad *fakesinkpad;
  GstPad *v4l2sinkpad;

  /* streaming status */
  gboolean streaming;

  GstCaps *probed_caps;
  GstCaps *cur_caps;

  /* a poll for video_fd */
  GstPoll *poll;
  GstPollFD pollfd;

  struct uvc_function_config *fc;

  struct {
    int bFrameIndex;
    int bFormatIndex;
    unsigned int dwFrameInterval;
  } cur;

  struct uvc_streaming_control probe;
  struct uvc_streaming_control commit;

  int control;

  /* probes */
  int buffer_peer_probe_id;
  int idle_probe_id;

  GstClock *v4l2_clock;

  int caps_changed;
  int streamon;
  int streamoff;
};

#define UVCSINK_MSG_LOCK(v) g_mutex_lock(&(v)->msg_lock)
#define UVCSINK_MSG_UNLOCK(v) g_mutex_unlock(&(v)->msg_lock)

int uvc_events_process_data(GstUvcSink * self,
                                   const struct uvc_request_data *data);
int uvc_events_process_setup(GstUvcSink * self,
                         const struct usb_ctrlrequest *ctrl,
                         struct uvc_request_data *resp);
int uvc_fill_streaming_control(GstUvcSink * self,
                           struct uvc_streaming_control *ctrl,
                           int iframe, int iformat, unsigned int dwival);
G_END_DECLS
