/* GStreamer unit tests for the SDP support library
 *
 * Copyright (C) 2013 Jose Antonio Santos Cadenas <santoscadenas@gmail.com>
 * Copyright (C) 2013 Kurento
 * Contact: Miguel París Díaz <mparisdiaz@gmail.com>
 * Contact: José Antonio Santos Cadenas <santoscadenas@kurento.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/sdp/gstsdpmessage.h>

/* *INDENT-OFF* */
static const gchar *sdp = "v=0\r\n"
    "o=- 123456 0 IN IP4 127.0.0.1\r\n"
    "s=TestSessionToCopy\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "a=sendrecv\r\n"
    "m=video 3434 RTP/AVP 96 97 99\r\n"
    "a=rtpmap:96 MP4V-ES/90000\r\n"
    "a=rtpmap:97 H263-1998/90000\r\n"
    "a=rtpmap:99 H263/90000\r\n"
    "a=sendrecv\r\n"
    "m=video 6565 RTP/AVP 98\r\n"
    "a=rtpmap:98 VP8/90000\r\n"
    "a=sendrecv\r\n"
    "m=audio 4545 RTP/AVP 14\r\n"
    "a=sendrecv\r\n"
    "m=audio 1010 TCP 14\r\n";

static const gchar *h264_sdp = "v=0\r\n"
    "o=- 992782775729845470 2 IN IP4 127.0.0.1\r\n"
    "s=TestH264\r\n"
    "t=0 0\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=recvonly\r\n"
    "a=rtpmap:96 H264/90000\r\n"
    "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n";

static const gchar *h264_sdp_with_source_filter = "v=0\r\n"
    "o=- 992782775729845470 2 IN IP4 127.0.0.1\r\n"
    "s=TestH264\r\n"
    "t=0 0\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=recvonly\r\n"
    "a=rtpmap:96 H264/90000\r\n"
    "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n"
    "a=source-filter: incl IN IP4 127.0.0.2 127.0.0.1\r\n";

static const gchar caps_video_string1[] =
    "application/x-unknown, media=(string)video, payload=(int)96, "
    "clock-rate=(int)90000, encoding-name=(string)MP4V-ES";

static const gchar caps_video_string2[] =
    "application/x-unknown, media=(string)video, payload=(int)97, "
    "clock-rate=(int)90000, encoding-name=(string)H263-1998";

static const gchar caps_audio_string[] =
    "application/x-unknown, media=(string)audio, payload=(int)14, "
    "clock-rate=(int)90000";

static const gchar * sdp_rtcp_fb = "v=0\r\n"
    "o=- 123456 2 IN IP4 127.0.0.1 \r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=maxptime:60\r\n"
    "a=sendrecv\r\n"
    "m=video 1 UDP/TLS/RTP/SAVPF 100 101 102\r\n"
    "c=IN IP4 1.1.1.1\r\n"
    "a=rtpmap:100 VP8/90000\r\n"
    "a=rtcp-fb:100 nack\r\n"
    "a=rtcp-fb:100 nack pli\r\n"
    "a=rtcp-fb:100 ccm fir\r\n"
    "a=rtpmap:101 VP9/90000\r\n"
    "a=rtcp-fb:101 nack pli\r\n"
    "a=rtpmap:102 H264/90000\r\n"
    "a=rtcp-fb:102    ccm fir\r\n"; /* incorrect spacing */

static const gchar caps_video_rtcp_fb_pt_100[] =
    "application/x-unknown, media=(string)video, payload=(int)100, "
    "clock-rate=(int)90000, encoding-name=(string)VP8, "
    "rtcp-fb-ccm-fir=(boolean)true, rtcp-fb-nack=(boolean)true, "
    "rtcp-fb-nack-pli=(boolean)true";

static const gchar caps_video_rtcp_fb_pt_101[] =
    "application/x-unknown, media=(string)video, payload=(int)101, "
    "clock-rate=(int)90000, encoding-name=(string)VP9, "
    "rtcp-fb-nack-pli=(boolean)true";

static const gchar caps_video_rtcp_fb_pt_102[] =
    "application/x-unknown, media=(string)video, payload=(int)102, "
    "clock-rate=(int)90000, encoding-name=(string)H264, "
    "rtcp-fb-ccm-fir=(boolean)true";

static const gchar *sdp_rtcp_fb_all = "v=0\r\n"
    "o=- 123456 2 IN IP4 127.0.0.1 \r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=maxptime:60\r\n"
    "a=sendrecv\r\n"
    "m=video 1 UDP/TLS/RTP/SAVPF 100 101 102\r\n"
    "c=IN IP4 1.1.1.1\r\n"
    "a=rtpmap:100 VP8/90000\r\n"
    "a=rtcp-fb:* nack\r\n"
    "a=rtcp-fb:* nack pli\r\n"
    "a=rtcp-fb:100 ccm fir\r\n"
    "a=rtpmap:101 VP9/90000\r\n"
    "a=rtcp-fb:101 ccm fir\r\n"
    "a=rtpmap:102 H264/90000\r\n";

static const gchar caps_video_rtcp_fb_all_pt_100[] =
    "application/x-unknown, media=(string)video, payload=(int)100, "
    "clock-rate=(int)90000, encoding-name=(string)VP8, "
    "rtcp-fb-ccm-fir=(boolean)true, rtcp-fb-nack=(boolean)true, "
    "rtcp-fb-nack-pli=(boolean)true";

static const gchar caps_video_rtcp_fb_all_pt_101[] =
    "application/x-unknown, media=(string)video, payload=(int)101, "
    "clock-rate=(int)90000, encoding-name=(string)VP9, "
    "rtcp-fb-ccm-fir=(boolean)true, rtcp-fb-nack=(boolean)true, "
    "rtcp-fb-nack-pli=(boolean)true";

static const gchar caps_video_rtcp_fb_all_pt_102[] =
    "application/x-unknown, media=(string)video, payload=(int)102, "
    "clock-rate=(int)90000, encoding-name=(string)H264, "
    "rtcp-fb-nack=(boolean)true, rtcp-fb-nack-pli=(boolean)true";

static const gchar * sdp_extmap = "v=0\r\n"
    "o=- 123456 2 IN IP4 127.0.0.1 \r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=maxptime:60\r\n"
    "a=sendrecv\r\n"
    "m=video 1 UDP/TLS/RTP/SAVPF 100 101 102\r\n"
    "c=IN IP4 1.1.1.1\r\n"
    "a=rtpmap:100 VP8/90000\r\n"
    "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n"
    "a=extmap:3/recvonly http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
    "a=extmap:4 urn:3gpp:video-orientation attributes\r\n";

static const gchar caps_video_extmap_pt_100[] =
    "application/x-unknown, media=(string)video, payload=(int)100, "
    "clock-rate=(int)90000, encoding-name=(string)VP8, "
    "extmap-2=urn:ietf:params:rtp-hdrext:toffset, "
    "extmap-3=(string)<\"recvonly\",\"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\",\"\">, "
    "extmap-4=(string)<\"\",\"urn:3gpp:video-orientation\",\"attributes\">";

static const gchar * sdp_fmtp = "v=0\r\n"
    "o=ali 1122334455 1122334466 IN IP4 fec.example.com\r\n"
    "s=Raptor RTP FEC Example\r\n"
    "t=0 0\r\n"
    "a=group:FEC-FR S1 R1\r\n"
    "m=video 30000 RTP/AVP 100\r\n"
    "c=IN IP4 233.252.0.1/127\r\n"
    "a=rtpmap:100 MP2T/90000\r\n"
    "a=fec-source-flow: id=0\r\n"
    "a=mid:S1\r\n"
    "m=application 30000 RTP/AVP 110\r\n"
    "c=IN IP4 233.252.0.2/127\r\n"
    "a=rtpmap:110 raptorfec/90000\r\n"
    "a=fmtp:110 raptor-scheme-id=1; Kmax=8192; T=128; P=A; repair-window=200000\r\n"
    "a=mid:R1\r\n";

static const gchar caps_video_raptor_fec_pt_100[] =
    "application/x-unknown, media=(string)video, payload=(int)100, "
    "clock-rate=(int)90000, encoding-name=(string)MP2T, "
    "a-fec-source-flow=(string)\"\\ id\\=0\", a-mid=(string)S1";

static const gchar caps_application_raptor_fec_pt_110[] =
    "application/x-unknown, media=(string)application, payload=(int)110, "
    "clock-rate=(int)90000, encoding-name=(string)RAPTORFEC, "
    "raptor-scheme-id=(string)1, kmax=(string)8192, t=(string)128, p=(string)A, repair-window=(string)200000, "
    "a-mid=(string)R1";

static const gchar caps_multiple_rid[] =
    "application/x-unknown, media=(string)video, payload=(int)96, "
    "clock-rate=(int)90000, encoding-name=(string)VP8, "
    "rid-h=(string)\"send\", "
    "rid-m=(string)\"send\", "
    "rid-l=(string)\"send\", "
    "a-simulcast=(string)\"send\\ h\\;m\\;l\"";

static const gchar caps_rid_params[] =
    "application/x-unknown, media=(string)video, payload=(int)96, "
    "clock-rate=(int)90000, encoding-name=(string)VP8, "
    "rid-0=(string)<\"send\",\"max-width=1920\",\"max-height=1080\">, "
    "rid-1=(string)<\"send\",\"max-width=1280\",\"max-height=720\">";

/* *INDENT-ON* */

GST_START_TEST (boxed)
{
  GValue value = G_VALUE_INIT;
  GValue value_copy = G_VALUE_INIT;
  GstSDPMessage *message, *copy;
  gchar *message1_str, *message2_str, *copy_str;
  const gchar *repeat1[] = { "789", "012", NULL };

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp, -1, message);

  gst_sdp_message_add_time (message, "123", "456", repeat1);

  g_value_init (&value, GST_TYPE_SDP_MESSAGE);
  g_value_init (&value_copy, GST_TYPE_SDP_MESSAGE);

  g_value_set_boxed (&value, message);
  message1_str = gst_sdp_message_as_text (message);
  GST_DEBUG ("message1:\n%s", message1_str);
  gst_sdp_message_free (message);

  message = g_value_get_boxed (&value);
  message2_str = gst_sdp_message_as_text (message);
  GST_DEBUG ("message2:\n%s", message2_str);

  fail_if (g_strcmp0 (message1_str, message2_str) != 0);

  g_value_copy (&value, &value_copy);
  g_value_reset (&value);

  copy = g_value_dup_boxed (&value_copy);
  g_value_reset (&value_copy);

  copy_str = gst_sdp_message_as_text (copy);
  gst_sdp_message_free (copy);
  GST_DEBUG ("copy:\n%s", copy_str);

  fail_if (g_strcmp0 (message1_str, copy_str));

  g_free (message1_str);
  g_free (message2_str);
  g_free (copy_str);
}

GST_END_TEST
GST_START_TEST (copy)
{
  GstSDPMessage *message, *copy;
  glong length = -1;
  gchar *message_str, *copy_str;
  const gchar *repeat1[] = { "789", "012", NULL };
  const gchar *repeat2[] = { "987", "210", NULL };

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp, length, message);

  gst_sdp_message_add_time (message, "123", "456", repeat1);
  gst_sdp_message_add_time (message, "321", "654", repeat2);

  gst_sdp_message_copy (message, &copy);

  message_str = gst_sdp_message_as_text (message);
  GST_DEBUG ("Original:\n%s", message_str);
  gst_sdp_message_free (message);
  copy_str = gst_sdp_message_as_text (copy);
  gst_sdp_message_free (copy);
  GST_DEBUG ("Copy:\n%s", copy_str);

  fail_if (g_strcmp0 (copy_str, message_str) != 0);
  g_free (copy_str);
  g_free (message_str);
}

GST_END_TEST
GST_START_TEST (modify)
{
  GstSDPMessage *message;
  glong length = -1;
  const GstSDPMedia *media;
  const gchar *old_val;
  const gchar *result;
  GstSDPAttribute attr;

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp, length, message);

  /* modify session attribute */
  fail_unless (gst_sdp_message_add_attribute (message,
          "test_attr_session", "param1=val1") == GST_SDP_OK);

  old_val = gst_sdp_message_get_attribute_val (message, "test_attr_session");

  fail_unless (old_val != NULL);
  attr.key = g_strdup ("test_attr_session");
  attr.value = g_strdup_printf ("%s;param2=val2", old_val);

  fail_unless (gst_sdp_message_replace_attribute (message, 0,
          &attr) == GST_SDP_OK);

  result = gst_sdp_message_get_attribute_val (message, "test_attr_session");
  fail_unless (result != NULL);
  fail_unless (g_strcmp0 (result, "param1=val1;param2=val2") == 0);


  /* modify media attribute */
  media = gst_sdp_message_get_media (message, 0);
  fail_unless (media != NULL);

  fail_unless (gst_sdp_media_add_attribute ((GstSDPMedia *) media,
          "test_attr_media", "param3=val3") == GST_SDP_OK);

  old_val =
      gst_sdp_media_get_attribute_val ((GstSDPMedia *) media,
      "test_attr_media");

  fail_unless (old_val != NULL);
  attr.key = g_strdup ("test_attr_media");
  attr.value = g_strdup ("myparam=myval");

  fail_unless (gst_sdp_media_replace_attribute ((GstSDPMedia *) media,
          0, &attr) == GST_SDP_OK);

  result =
      gst_sdp_media_get_attribute_val ((GstSDPMedia *) media,
      "test_attr_media");
  fail_unless (result != NULL);
  fail_unless (g_strcmp0 (result, "myparam=myval") == 0);

  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (null)
{
  GstSDPMessage *message;
  const GstSDPMedia *media;
  glong length = -1;
  const gchar *val;

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp, length, message);

  fail_unless (gst_sdp_message_add_attribute (message,
          "test_attr_session", NULL) == GST_SDP_OK);

  val = gst_sdp_message_get_attribute_val (message, "test_attr_session");
  fail_unless (val == NULL);

  media = gst_sdp_message_get_media (message, 0);

  fail_unless (gst_sdp_media_add_attribute ((GstSDPMedia *) media,
          "test_attr_media", NULL) == GST_SDP_OK);

  val = gst_sdp_media_get_attribute_val (media, "test_attr_media");
  fail_unless (val == NULL);

  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (caps_from_media)
{
  GstSDPMessage *message;
  glong length = -1;
  const GstSDPMedia *media1, *media2, *media3;
  GstCaps *caps_video1, *caps_video2, *caps_audio;
  GstCaps *result_video1, *result_video2, *result_audio;

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp, length, message);

  media1 = gst_sdp_message_get_media (message, 0);
  fail_unless (media1 != NULL);

  media2 = gst_sdp_message_get_media (message, 1);
  fail_unless (media2 != NULL);

  media3 = gst_sdp_message_get_media (message, 2);
  fail_unless (media2 != NULL);

  caps_video1 = gst_sdp_media_get_caps_from_media (media1, 96);
  caps_video2 = gst_sdp_media_get_caps_from_media (media1, 97);
  caps_audio = gst_sdp_media_get_caps_from_media (media3, 14);

  result_video1 = gst_caps_from_string (caps_video_string1);
  fail_unless (gst_caps_is_strictly_equal (caps_video1, result_video1));
  gst_caps_unref (result_video1);
  gst_caps_unref (caps_video1);

  result_video2 = gst_caps_from_string (caps_video_string2);
  fail_unless (gst_caps_is_strictly_equal (caps_video2, result_video2));
  gst_caps_unref (result_video2);
  gst_caps_unref (caps_video2);

  result_audio = gst_caps_from_string (caps_audio_string);
  fail_unless (gst_caps_is_strictly_equal (caps_audio, result_audio));
  gst_caps_unref (result_audio);
  gst_caps_unref (caps_audio);

  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (media_from_caps)
{
  GstSDPResult ret = GST_SDP_OK;
  GstSDPMessage *message;
  glong length = -1;
  GstSDPMedia *media_video, *media_audio;
  const GstSDPMedia *result_video, *result_audio;
  GstCaps *caps_video, *caps_audio;
  const gchar *media1_text, *media2_text, *media3_text, *media4_text;

  caps_video = gst_caps_from_string (caps_video_string1);
  caps_audio = gst_caps_from_string (caps_audio_string);

  gst_sdp_media_new (&media_video);
  fail_unless (media_video != NULL);
  gst_sdp_media_new (&media_audio);
  fail_unless (media_audio != NULL);

  ret = gst_sdp_media_set_media_from_caps (caps_video, media_video);
  fail_unless (ret == GST_SDP_OK);
  gst_caps_unref (caps_video);
  ret = gst_sdp_media_set_media_from_caps (caps_audio, media_audio);
  fail_unless (ret == GST_SDP_OK);
  gst_caps_unref (caps_audio);

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp, length, message);

  result_video = gst_sdp_message_get_media (message, 0);
  fail_unless (result_video != NULL);

  result_audio = gst_sdp_message_get_media (message, 2);
  fail_unless (result_audio != NULL);

  media1_text = gst_sdp_media_get_attribute_val (media_video, "rtpmap");
  media2_text = gst_sdp_media_get_attribute_val (result_video, "rtpmap");
  media3_text = gst_sdp_media_get_format (media_audio, 0);
  media4_text = gst_sdp_media_get_format (result_audio, 0);

  fail_if (g_strcmp0 (media1_text, media2_text) != 0);
  fail_if (g_strcmp0 (media3_text, media4_text) != 0);

  gst_sdp_media_free (media_video);
  gst_sdp_media_free (media_audio);
  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (caps_from_media_rtcp_fb)
{
  GstSDPMessage *message;
  glong length = -1;
  const GstSDPMedia *media1;
  GstCaps *caps1, *caps2, *caps3;
  GstCaps *result1, *result2, *result3;

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp_rtcp_fb, length, message);

  media1 = gst_sdp_message_get_media (message, 0);
  fail_unless (media1 != NULL);

  caps1 = gst_sdp_media_get_caps_from_media (media1, 100);
  result1 = gst_caps_from_string (caps_video_rtcp_fb_pt_100);
  fail_unless (gst_caps_is_strictly_equal (caps1, result1));

  gst_caps_unref (result1);
  gst_caps_unref (caps1);

  caps2 = gst_sdp_media_get_caps_from_media (media1, 101);
  result2 = gst_caps_from_string (caps_video_rtcp_fb_pt_101);
  fail_unless (gst_caps_is_strictly_equal (caps2, result2));

  gst_caps_unref (result2);
  gst_caps_unref (caps2);

  caps3 = gst_sdp_media_get_caps_from_media (media1, 102);
  result3 = gst_caps_from_string (caps_video_rtcp_fb_pt_102);

  fail_unless (gst_caps_is_strictly_equal (caps3, result3));

  gst_caps_unref (result3);
  gst_caps_unref (caps3);

  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (caps_from_media_rtcp_fb_all)
{
  GstSDPMessage *message;
  glong length = -1;
  const GstSDPMedia *media1;
  GstCaps *caps1, *caps2, *caps3;
  GstCaps *result1, *result2, *result3;

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp_rtcp_fb_all, length, message);

  media1 = gst_sdp_message_get_media (message, 0);
  fail_unless (media1 != NULL);

  caps1 = gst_sdp_media_get_caps_from_media (media1, 100);
  result1 = gst_caps_from_string (caps_video_rtcp_fb_all_pt_100);
  fail_unless (gst_caps_is_strictly_equal (caps1, result1));

  gst_caps_unref (result1);
  gst_caps_unref (caps1);

  caps2 = gst_sdp_media_get_caps_from_media (media1, 101);
  result2 = gst_caps_from_string (caps_video_rtcp_fb_all_pt_101);
  fail_unless (gst_caps_is_strictly_equal (caps2, result2));

  gst_caps_unref (result2);
  gst_caps_unref (caps2);

  caps3 = gst_sdp_media_get_caps_from_media (media1, 102);
  result3 = gst_caps_from_string (caps_video_rtcp_fb_all_pt_102);

  fail_unless (gst_caps_is_strictly_equal (caps3, result3));

  gst_caps_unref (result3);
  gst_caps_unref (caps3);

  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (media_from_caps_rtcp_fb_pt_100)
{
  GstSDPResult ret = GST_SDP_OK;
  GstSDPMessage *message;
  glong length = -1;
  GstSDPMedia *media_caps;
  const GstSDPMedia *media_sdp;
  GstCaps *caps;
  const gchar *attr_val_caps1, *attr_val_caps2, *attr_val_caps3;
  const gchar *attr_val_sdp1, *attr_val_sdp2, *attr_val_sdp3;

  caps = gst_caps_from_string (caps_video_rtcp_fb_pt_100);

  gst_sdp_media_new (&media_caps);
  fail_unless (media_caps != NULL);

  ret = gst_sdp_media_set_media_from_caps (caps, media_caps);
  fail_unless (ret == GST_SDP_OK);
  gst_caps_unref (caps);

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp_rtcp_fb, length, message);

  media_sdp = gst_sdp_message_get_media (message, 0);
  fail_unless (media_sdp != NULL);

  attr_val_caps1 = gst_sdp_media_get_attribute_val_n (media_caps, "rtcp-fb", 0);
  attr_val_caps2 = gst_sdp_media_get_attribute_val_n (media_caps, "rtcp-fb", 1);
  attr_val_caps3 = gst_sdp_media_get_attribute_val_n (media_caps, "rtcp-fb", 2);

  attr_val_sdp1 = gst_sdp_media_get_attribute_val_n (media_sdp, "rtcp-fb", 0);
  attr_val_sdp2 = gst_sdp_media_get_attribute_val_n (media_sdp, "rtcp-fb", 1);
  attr_val_sdp3 = gst_sdp_media_get_attribute_val_n (media_sdp, "rtcp-fb", 2);

  fail_if (g_strcmp0 (attr_val_caps1, attr_val_sdp1) != 0);
  fail_if (g_strcmp0 (attr_val_caps2, attr_val_sdp2) != 0);
  fail_if (g_strcmp0 (attr_val_caps3, attr_val_sdp3) != 0);

  gst_sdp_media_free (media_caps);
  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (media_from_caps_rtcp_fb_pt_101)
{
  GstSDPResult ret = GST_SDP_OK;
  GstSDPMessage *message;
  glong length = -1;
  GstSDPMedia *media_caps;
  const GstSDPMedia *media_sdp;
  GstCaps *caps;
  const gchar *attr_val_caps1, *attr_val_sdp1;

  caps = gst_caps_from_string (caps_video_rtcp_fb_pt_101);

  gst_sdp_media_new (&media_caps);
  fail_unless (media_caps != NULL);

  ret = gst_sdp_media_set_media_from_caps (caps, media_caps);
  fail_unless (ret == GST_SDP_OK);
  gst_caps_unref (caps);

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp_rtcp_fb, length, message);

  media_sdp = gst_sdp_message_get_media (message, 0);
  fail_unless (media_sdp != NULL);

  attr_val_caps1 = gst_sdp_media_get_attribute_val (media_caps, "rtcp-fb");
  attr_val_sdp1 = gst_sdp_media_get_attribute_val_n (media_sdp, "rtcp-fb", 3);

  fail_if (g_strcmp0 (attr_val_caps1, attr_val_sdp1) != 0);

  gst_sdp_media_free (media_caps);
  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (caps_from_media_extmap)
{
  GstSDPMessage *message;
  glong length = -1;
  const GstSDPMedia *media1;
  GstCaps *caps1;
  GstCaps *result1;

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp_extmap, length, message);

  media1 = gst_sdp_message_get_media (message, 0);
  fail_unless (media1 != NULL);

  caps1 = gst_sdp_media_get_caps_from_media (media1, 100);
  gst_sdp_media_attributes_to_caps (media1, caps1);
  result1 = gst_caps_from_string (caps_video_extmap_pt_100);
  fail_unless (gst_caps_is_strictly_equal (caps1, result1));

  gst_caps_unref (result1);
  gst_caps_unref (caps1);

  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (caps_from_media_fmtp)
{
  GstSDPMessage *message;
  glong length = -1;
  const GstSDPMedia *media1, *media2;
  GstCaps *caps1, *caps2;
  GstCaps *result1, *result2;

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp_fmtp, length, message);

  media1 = gst_sdp_message_get_media (message, 0);
  fail_unless (media1 != NULL);

  caps1 = gst_sdp_media_get_caps_from_media (media1, 100);
  gst_sdp_media_attributes_to_caps (media1, caps1);
  result1 = gst_caps_from_string (caps_video_raptor_fec_pt_100);
  fail_unless (gst_caps_is_strictly_equal (caps1, result1));

  gst_caps_unref (result1);
  gst_caps_unref (caps1);

  media2 = gst_sdp_message_get_media (message, 1);
  fail_unless (media1 != NULL);

  caps2 = gst_sdp_media_get_caps_from_media (media2, 110);
  gst_sdp_media_attributes_to_caps (media2, caps2);
  result2 = gst_caps_from_string (caps_application_raptor_fec_pt_110);
  fail_unless (gst_caps_is_strictly_equal (caps2, result2));

  gst_caps_unref (result2);
  gst_caps_unref (caps2);
  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (media_from_caps_extmap_pt_100)
{
  GstSDPResult ret = GST_SDP_OK;
  GstSDPMessage *message;
  glong length = -1;
  GstSDPMedia *media_caps;
  const GstSDPMedia *media_sdp;
  GstCaps *caps;
  const gchar *attr_val_caps1, *attr_val_caps2, *attr_val_caps3;
  const gchar *attr_val_sdp1, *attr_val_sdp2, *attr_val_sdp3;

  caps = gst_caps_from_string (caps_video_extmap_pt_100);

  gst_sdp_media_new (&media_caps);
  fail_unless (media_caps != NULL);

  ret = gst_sdp_media_set_media_from_caps (caps, media_caps);
  fail_unless (ret == GST_SDP_OK);
  gst_caps_unref (caps);

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp_extmap, length, message);

  media_sdp = gst_sdp_message_get_media (message, 0);
  fail_unless (media_sdp != NULL);

  attr_val_caps1 = gst_sdp_media_get_attribute_val_n (media_caps, "extmap", 0);
  attr_val_caps2 = gst_sdp_media_get_attribute_val_n (media_caps, "extmap", 1);
  attr_val_caps3 = gst_sdp_media_get_attribute_val_n (media_caps, "extmap", 2);

  attr_val_sdp1 = gst_sdp_media_get_attribute_val_n (media_sdp, "extmap", 0);
  attr_val_sdp2 = gst_sdp_media_get_attribute_val_n (media_sdp, "extmap", 1);
  attr_val_sdp3 = gst_sdp_media_get_attribute_val_n (media_sdp, "extmap", 2);

  fail_if (g_strcmp0 (attr_val_caps1, attr_val_sdp1) != 0);
  fail_if (g_strcmp0 (attr_val_caps2, attr_val_sdp2) != 0);
  fail_if (g_strcmp0 (attr_val_caps3, attr_val_sdp3) != 0);

  gst_sdp_media_free (media_caps);
  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (caps_from_media_really_const)
{
  GstSDPMessage *message;
  glong length = -1;
  const GstSDPMedia *media1;
  gchar *serialized;
  GstCaps *caps;

  /* BUG: gst_sdp_media_get_caps_from_media() used to modify the media passed
   * thus violating the const tag */

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) sdp, length, message);

  serialized = gst_sdp_message_as_text (message);
  fail_unless (g_strcmp0 (serialized, sdp) == 0);
  g_free (serialized);

  media1 = gst_sdp_message_get_media (message, 0);
  fail_unless (media1 != NULL);

  caps = gst_sdp_media_get_caps_from_media (media1, 96);

  serialized = gst_sdp_message_as_text (message);
  fail_unless (g_strcmp0 (serialized, sdp) == 0);
  g_free (serialized);

  gst_caps_unref (caps);

  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (media_from_caps_h264_with_profile_asymmetry_allowed)
{
  GstSDPMessage *message;
  glong length = -1;
  const GstSDPMedia *result_video;
  GstStructure *s_video;
  GstCaps *caps_video;

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) h264_sdp, length, message);


  result_video = gst_sdp_message_get_media (message, 0);
  fail_unless (result_video != NULL);
  caps_video = gst_sdp_media_get_caps_from_media (result_video, 96);

  s_video = gst_caps_get_structure (caps_video, 0);
  fail_if (gst_structure_has_field (s_video, "level-asymmetry-allowed"));
  fail_if (gst_structure_has_field (s_video, "profile-level-id"));
  fail_unless_equals_string (gst_structure_get_string (s_video, "profile"),
      "constrained-baseline");

  gst_caps_unref (caps_video);
  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (caps_multiple_rid_parse)
{
  GstSDPMedia media, media2;
  GstCaps *caps, *expected;

  /* BUG: gst_sdp_media_add_attributes_to_caps() would only set a single rid
   * string attribute key/value in caps */

  memset (&media, 0, sizeof (media));
  fail_unless_equals_int (gst_sdp_media_init (&media), GST_SDP_OK);

  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_set_media (&media, "video"));
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_media_add_format (&media, "96"));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_add_attribute (&media, "rtpmap", "96 VP8/90000"));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_add_attribute (&media, "rid", "h send"));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_add_attribute (&media, "rid", "m send"));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_add_attribute (&media, "rid", "l send"));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_add_attribute (&media, "simulcast", "send h;m;l"));

  expected = gst_caps_from_string (caps_multiple_rid);
  fail_unless (gst_caps_is_fixed (expected));
  fail_unless (expected != NULL);

  caps = gst_sdp_media_get_caps_from_media (&media, 96);
  fail_unless (caps != NULL);
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_attributes_to_caps (&media, caps));
  fail_unless (gst_caps_is_fixed (caps));

  GST_DEBUG ("    caps %" GST_PTR_FORMAT, caps);
  GST_DEBUG ("expected %" GST_PTR_FORMAT, expected);
  fail_unless (gst_caps_is_equal (caps, expected));

  memset (&media2, 0, sizeof (media2));
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_media_init (&media2));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_set_media_from_caps (caps, &media2));

  gst_clear_caps (&caps);

  caps = gst_sdp_media_get_caps_from_media (&media, 96);
  fail_unless (caps != NULL);
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_attributes_to_caps (&media, caps));
  fail_unless (gst_caps_is_fixed (caps));

  GST_DEBUG ("    caps %" GST_PTR_FORMAT, caps);
  GST_DEBUG ("expected %" GST_PTR_FORMAT, expected);
  fail_unless (gst_caps_is_equal (caps, expected));

  gst_sdp_media_uninit (&media);
  gst_sdp_media_uninit (&media2);

  gst_clear_caps (&caps);
  gst_clear_caps (&expected);
}

GST_END_TEST
GST_START_TEST (caps_multiple_rid_parse_with_params)
{
  GstSDPMedia media, media2;
  GstCaps *caps, *expected;

  /* BUG: gst_sdp_media_add_attributes_to_caps() would only set a single rid
   * string attribute key/value in caps */

  memset (&media, 0, sizeof (media));
  fail_unless_equals_int (gst_sdp_media_init (&media), GST_SDP_OK);

  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_set_media (&media, "video"));
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_media_add_format (&media, "96"));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_add_attribute (&media, "rtpmap", "96 VP8/90000"));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_add_attribute (&media, "rid",
          "0 send max-width=1920;max-height=1080"));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_add_attribute (&media, "rid",
          "1 send max-width=1280;max-height=720"));

  expected = gst_caps_from_string (caps_rid_params);
  fail_unless (gst_caps_is_fixed (expected));
  fail_unless (expected != NULL);

  caps = gst_sdp_media_get_caps_from_media (&media, 96);
  fail_unless (caps != NULL);
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_attributes_to_caps (&media, caps));
  fail_unless (gst_caps_is_fixed (caps));

  GST_DEBUG ("    caps %" GST_PTR_FORMAT, caps);
  GST_DEBUG ("expected %" GST_PTR_FORMAT, expected);
  fail_unless (gst_caps_is_equal (caps, expected));

  memset (&media2, 0, sizeof (media2));
  fail_unless_equals_int (GST_SDP_OK, gst_sdp_media_init (&media2));
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_set_media_from_caps (caps, &media2));

  gst_clear_caps (&caps);

  caps = gst_sdp_media_get_caps_from_media (&media, 96);
  fail_unless (caps != NULL);
  fail_unless_equals_int (GST_SDP_OK,
      gst_sdp_media_attributes_to_caps (&media, caps));
  fail_unless (gst_caps_is_fixed (caps));

  GST_DEBUG ("    caps %" GST_PTR_FORMAT, caps);
  GST_DEBUG ("expected %" GST_PTR_FORMAT, expected);
  fail_unless (gst_caps_is_equal (caps, expected));

  gst_sdp_media_uninit (&media);
  gst_sdp_media_uninit (&media2);

  gst_clear_caps (&caps);
  gst_clear_caps (&expected);
}

GST_END_TEST
GST_START_TEST (media_from_caps_with_source_filters)
{
  GstSDPMessage *message;
  glong length = -1;
  const GstSDPMedia *result_video;
  GstStructure *s_video;
  GstCaps *caps_video;

  gst_sdp_message_new (&message);
  gst_sdp_message_parse_buffer ((guint8 *) h264_sdp_with_source_filter,
      length, message);

  result_video = gst_sdp_message_get_media (message, 0);
  fail_unless (result_video != NULL);
  caps_video = gst_sdp_media_get_caps_from_media (result_video, 96);
  gst_sdp_media_attributes_to_caps (result_video, caps_video);

  s_video = gst_caps_get_structure (caps_video, 0);
  fail_if (gst_structure_has_field (s_video, "a-source-filter"));

  gst_caps_unref (caps_video);
  gst_sdp_message_free (message);
}

GST_END_TEST
GST_START_TEST (media_remove)
{
  GstSDPResult ret = GST_SDP_OK;
  GstSDPMessage *message;
  const gchar *media;
  GstSDPMedia *media_video, *media_audio;
  const GstSDPMedia *result_audio;
  GstCaps *caps_video, *caps_audio;

  /* Add two medias, firts a video and then audio */
  caps_video = gst_caps_from_string (caps_video_string1);
  caps_audio = gst_caps_from_string (caps_audio_string);

  gst_sdp_media_new (&media_video);
  fail_unless (media_video != NULL);
  gst_sdp_media_new (&media_audio);
  fail_unless (media_audio != NULL);

  ret = gst_sdp_media_set_media_from_caps (caps_video, media_video);
  fail_unless (ret == GST_SDP_OK);
  gst_caps_unref (caps_video);
  ret = gst_sdp_media_set_media_from_caps (caps_audio, media_audio);
  fail_unless (ret == GST_SDP_OK);
  gst_caps_unref (caps_audio);

  gst_sdp_message_new (&message);
  gst_sdp_message_add_media (message, media_video);
  gst_sdp_message_add_media (message, media_audio);

  fail_unless_equals_int (gst_sdp_message_medias_len (message), 2);

  /* Remove the first media (video) */
  ret = gst_sdp_message_remove_media (message, 0);
  fail_unless (ret == GST_SDP_OK);

  /* Audio media is the only one left */
  fail_unless_equals_int (gst_sdp_message_medias_len (message), 1);

  result_audio = gst_sdp_message_get_media (message, 0);
  fail_unless (result_audio != NULL);

  media = gst_sdp_media_get_media (result_audio);
  fail_unless_equals_string (media, "audio");

  gst_sdp_media_free (media_audio);
  gst_sdp_media_free (media_video);
  gst_sdp_message_free (message);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
sdp_suite (void)
{
  Suite *s = suite_create ("sdp");
  TCase *tc_chain = tcase_create ("sdp");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, copy);
  tcase_add_test (tc_chain, boxed);
  tcase_add_test (tc_chain, modify);
  tcase_add_test (tc_chain, null);
  tcase_add_test (tc_chain, caps_from_media);
  tcase_add_test (tc_chain, caps_from_media_really_const);
  tcase_add_test (tc_chain, media_from_caps);
  tcase_add_test (tc_chain, caps_from_media_rtcp_fb);
  tcase_add_test (tc_chain, caps_from_media_rtcp_fb_all);
  tcase_add_test (tc_chain, caps_from_media_extmap);
  tcase_add_test (tc_chain, caps_from_media_fmtp);
  tcase_add_test (tc_chain, media_from_caps_rtcp_fb_pt_100);
  tcase_add_test (tc_chain, media_from_caps_rtcp_fb_pt_101);
  tcase_add_test (tc_chain, media_from_caps_extmap_pt_100);
  tcase_add_test (tc_chain,
      media_from_caps_h264_with_profile_asymmetry_allowed);
  tcase_add_test (tc_chain, caps_multiple_rid_parse);
  tcase_add_test (tc_chain, caps_multiple_rid_parse_with_params);
  tcase_add_test (tc_chain, media_from_caps_with_source_filters);
  tcase_add_test (tc_chain, media_remove);

  return s;
}

GST_CHECK_MAIN (sdp);
