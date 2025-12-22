/* GStreamer
 * Copyright (C) 2024 Tim-Philipp Müller <tim centricular net>
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

#include <gst/analytics/analytics.h>

#include <gst/audio/gstnonstreamaudiodecoder.h>
#include <gst/audio/gstplanaraudioadapter.h>

#include <gst/basecamerabinsrc/gstbasecamerasrc.h>
#include <gst/basecamerabinsrc/gstcamerabinpreview.h>
/*
#include <gst/codecparsers/x>
#include <gst/codecs/x>
#include <gst/cuda/x>
*/
#include <gst/insertbin/gstinsertbin.h>
#include <gst/interfaces/photography.h>
#include <gst/mpegts/mpegts.h>
#include <gst/mse/mse.h>
/*
#include <gst/opencv/x>
*/
#include <gst/play/play.h>
#include <gst/player/player.h>
// #include <gst/sctp/x>
#include <gst/transcoder/gsttranscoder.h>
/*
#include <gst/va/x>
#include <gst/vulkan/x>
#include <gst/wayland/x>
*/
#include <gst/webrtc/webrtc.h>

/* we mostly just want to make sure that our library headers don't
 * contain anything a C++ compiler might not like */
GST_START_TEST (test_nothing)
{
  gst_init (NULL, NULL);
}

GST_END_TEST;

static Suite *
libscpp_suite (void)
{
  Suite *s = suite_create ("BadLibsCpp");
  TCase *tc_chain = tcase_create ("C++ bad libs header tests");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_nothing);

  return s;
}

GST_CHECK_MAIN (libscpp);
