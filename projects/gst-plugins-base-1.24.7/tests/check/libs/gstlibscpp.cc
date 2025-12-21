/* GStreamer
 * Copyright (C) 2011 Tim-Philipp Müller <tim centricular net>
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
#include <gst/check/gstcheck.h>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <gst/audio/audio-enumtypes.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiocdsrc.h>
#include <gst/audio/gstaudioclock.h>
#include <gst/audio/gstaudiofilter.h>
#include <gst/audio/gstaudiosink.h>
#include <gst/audio/gstaudiosrc.h>
#include <gst/audio/gstaudiobasesink.h>
#include <gst/audio/gstaudiobasesrc.h>
#include <gst/audio/gstaudioringbuffer.h>
#include <gst/audio/streamvolume.h>

#include <gst/fft/gstfftf32.h>
#include <gst/fft/gstfftf64.h>
#include <gst/fft/gstfft.h>
#include <gst/fft/gstffts16.h>
#include <gst/fft/gstffts32.h>

#include <gst/pbutils/codec-utils.h>
#include <gst/pbutils/descriptions.h>
#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/encoding-target.h>
#include <gst/pbutils/gstdiscoverer.h>
#include <gst/pbutils/gstpluginsbaseversion.h>
#include <gst/pbutils/install-plugins.h>
#include <gst/pbutils/missing-plugins.h>
#include <gst/pbutils/pbutils-enumtypes.h>
#include <gst/pbutils/pbutils.h>

#include <gst/riff/riff-ids.h>
#include <gst/riff/riff-media.h>
#include <gst/riff/riff-read.h>

#include <gst/rtp/gstrtpbaseaudiopayload.h>
#include <gst/rtp/gstrtpbasedepayload.h>
#include <gst/rtp/gstrtpbasepayload.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtppayloads.h>

#include <gst/rtsp/gstrtspconnection.h>
#include <gst/rtsp/gstrtspdefs.h>
#include <gst/rtsp/gstrtsp-enumtypes.h>
#include <gst/rtsp/gstrtspextension.h>
#include <gst/rtsp/gstrtsp.h>
#include <gst/rtsp/gstrtspmessage.h>
#include <gst/rtsp/gstrtsprange.h>
#include <gst/rtsp/gstrtsptransport.h>
#include <gst/rtsp/gstrtspurl.h>

#include <gst/sdp/gstsdp.h>
#include <gst/sdp/gstsdpmessage.h>
#include <gst/tag/gsttagdemux.h>

#include <gst/tag/tag.h>

#include <gst/video/gstvideofilter.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video-enumtypes.h>
#include <gst/video/video.h>
#include <gst/video/colorbalancechannel.h>
#include <gst/video/colorbalance.h>
#include <gst/video/videodirection.h>
#include <gst/video/videoorientation.h>
#include <gst/video/videooverlay.h>
#include <gst/video/navigation.h>

/* we mostly just want to make sure that our library headers don't
 * contain anything a C++ compiler might not like */
GST_START_TEST (test_nothing)
{
  gst_init (NULL, NULL);
}

GST_END_TEST;

GST_START_TEST (test_init_macros)
{
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  fail_unless_equals_int (rtp.size[0], 0)
  fail_unless_equals_int (rtcp.map.size, 0);
}

GST_END_TEST;

static Suite *
libscpp_suite (void)
{
  Suite *s = suite_create ("GstLibsCpp");
  TCase *tc_chain = tcase_create ("C++ libs header tests");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_nothing);
  tcase_add_test (tc_chain, test_init_macros);

  return s;
}

GST_CHECK_MAIN (libscpp);
