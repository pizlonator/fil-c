/* GStreamer unit tests for libgstpbutils
 *
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
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
# include <config.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/pbutils/pbutils.h>
#include <gst/base/gstbitwriter.h>

#include <stdio.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>          /* for chmod() and getpid () */
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>           /* for chmod() */
#endif

#ifdef G_OS_UNIX
#include <unistd.h>             /* for getpid() */
#endif

static void
missing_msg_check_getters (GstMessage * msg)
{
  gchar *str;

  str = gst_missing_plugin_message_get_installer_detail (msg);
  fail_unless (str != NULL);
  fail_unless (*str != '\0');
  fail_unless (g_str_has_prefix (str, "gstreamer|"));
  g_free (str);

  str = gst_missing_plugin_message_get_description (msg);
  fail_unless (str != NULL);
  fail_unless (*str != '\0');
  g_free (str);
}

GST_START_TEST (test_pb_utils_post_missing_messages)
{
  const GstStructure *s;
  GstElement *pipeline;
  GstMessage *msg;
  GstCaps *caps;
  GstBus *bus;

  gst_pb_utils_init ();

  pipeline = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (pipeline);

  /* first, test common assertion failure cases */
  ASSERT_CRITICAL (msg = gst_missing_uri_source_message_new (NULL, "http"));
  ASSERT_CRITICAL (gst_missing_uri_source_message_new (pipeline, NULL));

  ASSERT_CRITICAL (gst_missing_uri_sink_message_new (NULL, "http"));
  ASSERT_CRITICAL (gst_missing_uri_sink_message_new (pipeline, NULL));

  ASSERT_CRITICAL (gst_missing_element_message_new (NULL, "rgbfyltr"));
  ASSERT_CRITICAL (gst_missing_element_message_new (pipeline, NULL));

  caps = gst_caps_new_empty_simple ("audio/x-dontexist");

  ASSERT_CRITICAL (gst_missing_decoder_message_new (NULL, caps));
  ASSERT_CRITICAL (gst_missing_decoder_message_new (pipeline, NULL));

  ASSERT_CRITICAL (gst_missing_encoder_message_new (NULL, caps));
  ASSERT_CRITICAL (gst_missing_encoder_message_new (pipeline, NULL));

  gst_caps_unref (caps);

  /* URI source (with existing protocol) */
  msg = gst_missing_uri_source_message_new (pipeline, "http");
  fail_unless (msg != NULL);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (gst_message_get_structure (msg) != NULL);
  s = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "urisource");
  fail_unless (gst_structure_has_field_typed (s, "detail", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "detail"), "http");
  missing_msg_check_getters (msg);
  gst_message_unref (msg);

  /* URI sink (with existing protocol) */
  msg = gst_missing_uri_sink_message_new (pipeline, "smb");
  fail_unless (msg != NULL);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (gst_message_get_structure (msg) != NULL);
  s = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "urisink");
  fail_unless (gst_structure_has_field_typed (s, "detail", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "detail"), "smb");
  missing_msg_check_getters (msg);
  gst_message_unref (msg);

  /* URI source (with bogus protocol) */
  msg = gst_missing_uri_source_message_new (pipeline, "chchck");
  fail_unless (msg != NULL);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (gst_message_get_structure (msg) != NULL);
  s = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "urisource");
  fail_unless (gst_structure_has_field_typed (s, "detail", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "detail"), "chchck");
  missing_msg_check_getters (msg);
  gst_message_unref (msg);

  /* URI sink (with bogus protocol) */
  msg = gst_missing_uri_sink_message_new (pipeline, "chchck");
  fail_unless (msg != NULL);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (gst_message_get_structure (msg) != NULL);
  s = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "urisink");
  fail_unless (gst_structure_has_field_typed (s, "detail", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "detail"), "chchck");
  missing_msg_check_getters (msg);
  gst_message_unref (msg);

  /* element */
  msg = gst_missing_element_message_new (pipeline, "foobar");
  fail_unless (msg != NULL);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (gst_message_get_structure (msg) != NULL);
  s = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "element");
  fail_unless (gst_structure_has_field_typed (s, "detail", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "detail"), "foobar");
  missing_msg_check_getters (msg);
  gst_message_unref (msg);

  /* create bogus caps that don't exist */
  caps = gst_caps_new_simple ("do/x-not", "exist", G_TYPE_BOOLEAN, FALSE, NULL);

  /* decoder (with unknown caps) */
  msg = gst_missing_decoder_message_new (pipeline, caps);
  fail_unless (msg != NULL);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (gst_message_get_structure (msg) != NULL);
  s = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "decoder");
  fail_unless (gst_structure_has_field_typed (s, "detail", GST_TYPE_CAPS));
  missing_msg_check_getters (msg);
  gst_message_unref (msg);

  /* encoder (with unknown caps) */
  msg = gst_missing_encoder_message_new (pipeline, caps);
  fail_unless (msg != NULL);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (gst_message_get_structure (msg) != NULL);
  s = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "encoder");
  fail_unless (gst_structure_has_field_typed (s, "detail", GST_TYPE_CAPS));
  missing_msg_check_getters (msg);
  gst_message_unref (msg);

  gst_caps_unref (caps);

  /* create caps that exist */
  caps = gst_caps_new_empty_simple ("video/x-matroska");
  /* decoder (with known caps) */
  msg = gst_missing_decoder_message_new (pipeline, caps);
  fail_unless (msg != NULL);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (gst_message_get_structure (msg) != NULL);
  s = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "decoder");
  fail_unless (gst_structure_has_field_typed (s, "detail", GST_TYPE_CAPS));
  fail_unless (gst_structure_has_field_typed (s, "name", G_TYPE_STRING));
  fail_unless (gst_structure_get_string (s, "name") != NULL);
  missing_msg_check_getters (msg);
  gst_message_unref (msg);

  /* encoder (with known caps) */
  msg = gst_missing_encoder_message_new (pipeline, caps);
  fail_unless (msg != NULL);
  fail_unless_equals_int (GST_MESSAGE_TYPE (msg), GST_MESSAGE_ELEMENT);
  fail_unless (gst_message_get_structure (msg) != NULL);
  s = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (s, "missing-plugin"));
  fail_unless (gst_structure_has_field_typed (s, "type", G_TYPE_STRING));
  fail_unless_equals_string (gst_structure_get_string (s, "type"), "encoder");
  fail_unless (gst_structure_has_field_typed (s, "detail", GST_TYPE_CAPS));
  fail_unless (gst_structure_has_field_typed (s, "name", G_TYPE_STRING));
  fail_unless (gst_structure_get_string (s, "name") != NULL);
  missing_msg_check_getters (msg);
  gst_message_unref (msg);

  gst_caps_unref (caps);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  gst_object_unref (bus);
}

GST_END_TEST;

GST_START_TEST (test_pb_utils_init)
{
  /* should be fine to call multiple times */
  gst_pb_utils_init ();
  gst_pb_utils_init ();
  gst_pb_utils_init ();
  gst_pb_utils_init ();
}

GST_END_TEST;

#define F_AUDIO GST_PBUTILS_CAPS_DESCRIPTION_FLAG_AUDIO
#define F_VIDEO GST_PBUTILS_CAPS_DESCRIPTION_FLAG_VIDEO
#define F_SUB GST_PBUTILS_CAPS_DESCRIPTION_FLAG_SUBTITLE
#define F_IMAGE GST_PBUTILS_CAPS_DESCRIPTION_FLAG_IMAGE
#define F_AV (F_AUDIO | F_VIDEO)
#define F_AVS (F_AUDIO | F_VIDEO | F_SUB)
#define F_AVSI (F_AUDIO | F_VIDEO | F_SUB | F_IMAGE)
#define F_CONTAINER GST_PBUTILS_CAPS_DESCRIPTION_FLAG_CONTAINER
#define F_AV_CONTAINER (F_CONTAINER | F_AV)
#define F_AVS_CONTAINER (F_CONTAINER | F_AVS)
#define F_AVSI_CONTAINER (F_CONTAINER | F_AVSI)
#define F_META GST_PBUTILS_CAPS_DESCRIPTION_FLAG_METADATA
#define F_TAG GST_PBUTILS_CAPS_DESCRIPTION_FLAG_TAG

/* *INDENT-OFF* */
static const struct FlagDescEntry
{
  const gchar *caps_string;
  GstPbUtilsCapsDescriptionFlags flags;
} flag_descs[] = {
  {"application/x-binary", 0},
  {"audio/x-wav", F_AUDIO | F_CONTAINER},
  {"video/quicktime", F_AVSI_CONTAINER},
  {"video/x-flv", F_AV_CONTAINER},
  {"video/x-h264", F_VIDEO},
  {"audio/mpeg,mpegversion=4", F_AUDIO},
  {"image/jpeg", F_IMAGE | F_VIDEO},
  {"meta/x-klv", F_META},
  {"application/x-onvif-metadata", F_META},
  {"random/x-nonsense, sense=false", 0},
};
/* *INDENT-ON* */

GST_START_TEST (test_pb_utils_get_caps_description_flags)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (flag_descs); ++i) {
    GstPbUtilsCapsDescriptionFlags flags;
    const struct FlagDescEntry *e;
    GstCaps *caps;

    e = &flag_descs[i];
    caps = gst_caps_from_string (e->caps_string);
    flags = gst_pb_utils_get_caps_description_flags (caps);
    gst_caps_unref (caps);
    GST_DEBUG ("%s: expecting 0x%x, got 0x%x", e->caps_string, e->flags, flags);
    fail_unless_equals_int (flags, e->flags);
  }
}

GST_END_TEST;

static const gchar *caps_strings[] = {
  /* formats with static descriptions */
  "application/ogg", "application/vnd.rn-realmedia", "video/x-fli",
  "video/x-flv", "video/x-matroska", "video/x-ms-asf", "video/x-msvideo",
  "video/x-quicktime", "video/quicktime", "audio/x-ac3", "audio/ac3",
  "audio/x-private-ac3", "audio/x-private1-ac3", "audio/x-adpcm",
  "audio/aiff", "audio/x-alaw", "audio/amr", "audio/AMR", "audio/AMR-WB",
  "audio/iLBC-sh", "audio/ms-gsm", "audio/qcelp", "audio/x-adpcm",
  "audio/x-aiff", "audio/x-alac", "audio/x-amr-nb-sh", "audio/x-amr-wb-sh",
  "audio/x-au", "audio/x-cinepak", "audio/x-dpcm", "audio/x-dts",
  "audio/x-dv", "audio/x-flac", "audio/x-gsm", "audio/x-iec958",
  "audio/x-iLBC", "audio/x-ircam", "audio/x-lpcm", "audio/x-private1-lpcm",
  "audio/x-m4a", "audio/x-mod", "audio/x-mulaw", "audio/x-musepack",
  "audio/x-nist", "audio/x-nsf", "audio/x-paris", "audio/x-qdm2",
  "audio/x-ralf-mpeg4-generic", "audio/x-sds", "audio/x-shorten",
  "audio/x-sid", "audio/x-sipro", "audio/x-spc", "audio/x-speex",
  "audio/x-svx", "audio/x-tta", "audio/x-ttafile",
  "audio/x-vnd.sony.atrac3", "audio/x-vorbis", "audio/x-voc", "audio/x-w64",
  "audio/x-wav", "audio/x-wavpack", "audio/x-wavpack-correction",
  "audio/x-wms", "audio/x-voxware", "audio/x-xi", "video/sp5x", "video/vivo",
  "video/x-4xm", "video/x-apple-video", "video/x-camtasia",
  "video/x-cdxa", "video/x-cinepak", "video/x-cirrus-logic-accupak",
  "video/x-compressed-yuv", "subpicture/x-dvd", "video/x-ffv",
  "video/x-ffvhuff", "video/x-flash-screen", "video/x-flash-video",
  "video/x-h261", "video/x-huffyuv", "video/x-intel-h263", "video/x-jpeg",
  "video/x-mjpeg", "video/x-mjpeg-b", "video/mpegts", "video/x-mng",
  "video/x-mszh", "video/x-msvideocodec", "video/x-mve", "video/x-nut",
  "video/x-nuv", "video/x-qdrw", "video/x-raw", "video/x-smc",
  "video/x-smoke", "video/x-tarkin", "video/x-theora", "video/x-rle",
  "video/x-ultimotion", "video/x-vcd", "video/x-vmnc", "video/x-vp3",
  "video/x-vp5", "video/x-vp6", "video/x-vp6-flash", "video/x-vp7",
  "video/x-zlib", "image/bmp", "image/x-bmp",
  "image/x-MS-bmp", "image/gif", "image/jpeg", "image/jng", "image/png",
  "image/pbm", "image/ppm", "image/svg+xml", "image/tiff",
  "image/x-cmu-raster", "image/x-icon", "image/x-xcf", "image/x-pixmap",
  "image/x-xpixmap", "image/x-quicktime", "image/x-sun-raster",
  "image/x-tga", "video/x-dv", "video/x-dv",
  /* some RTP formats */
  "application/x-rtp, media=(string)video, encoding-name=(string)TimVCodec",
  "application/x-rtp, media=(string)audio, encoding-name=(string)TimACodec",
  "application/x-rtp, media=(string)application, encoding-name=(string)TimMux",
  "application/x-rtp, media=(string)woohoo, encoding-name=(string)TPM",
  /* incomplete RTP formats */
  "application/x-rtp, media=(string)woohoo",
  "application/x-rtp, encoding-name=(string)TPM",
  "application/x-rtp, media=(string)woohoo",
  /* formats with dynamic descriptions */
  "audio/x-adpcm",
  "audio/x-adpcm, layout=(string)dvi",
  "audio/x-adpcm, layout=(string)swf",
  "audio/x-adpcm, layout=(string)microsoft",
  "audio/x-adpcm, layout=(string)quicktime",
  "audio/mpeg, mpegversion=(int)4",
  "audio/mpeg, mpegversion=(int)1, layer=(int)1",
  "audio/mpeg, mpegversion=(int)1, layer=(int)2",
  "audio/mpeg, mpegversion=(int)1, layer=(int)3",
  "audio/mpeg, mpegversion=(int)1, layer=(int)99",
  "audio/mpeg, mpegversion=(int)99",
  "video/mpeg, mpegversion=(int)2, systemstream=(boolean)TRUE",
  "video/mpeg, systemstream=(boolean)FALSE",
  "video/mpeg, mpegversion=(int)2",
  "video/mpeg, mpegversion=(int)1, systemstream=(boolean)FALSE",
  "video/mpeg, mpegversion=(int)2, systemstream=(boolean)FALSE",
  "video/mpeg, mpegversion=(int)4, systemstream=(boolean)FALSE",
  "video/mpeg, mpegversion=(int)99, systemstream=(boolean)TRUE",
  "video/mpeg, mpegversion=(int)99, systemstream=(boolean)FALSE",
  "video/mpeg, mpegversion=(int)4, systemstream=(boolean)FALSE, profile=main",
  "video/mpeg, mpegversion=(int)4, systemstream=(boolean)FALSE, profile=adsfad",
  "video/mpeg",
  "video/x-indeo, indeoversion=(int)3",
  "video/x-indeo, indeoversion=(int)5",
  "video/x-indeo",
  "video/x-wmv, wmvversion=(int)1",
  "video/x-wmv, wmvversion=(int)2",
  "video/x-wmv, wmvversion=(int)3",
  "video/x-wmv, wmvversion=(int)99",
  "video/x-wmv",
  "audio/x-wma, wmaversion=(int)1",
  "audio/x-wma, wmaversion=(int)2",
  "audio/x-wma, wmaversion=(int)3",
  "audio/x-wma, wmaversion=(int)99",
  "audio/x-wma",
  "video/x-dirac",
  "video/x-dirac, profile=(string)vc2-low-delay",
  "video/x-dirac, profile=(string)vc2-simple",
  "video/x-dirac, profile=(string)vc2-main",
  "video/x-dirac, profile=(string)main",
  "video/x-dirac, profile=(string)czvja",
  "video/x-divx, divxversion=(int)3",
  "video/x-divx, divxversion=(int)4",
  "video/x-divx, divxversion=(int)5",
  "video/x-divx, divxversion=(int)99",
  "video/x-divx",
  "video/x-svq, svqversion=(int)1",
  "video/x-svq, svqversion=(int)3",
  "video/x-svq, svqversion=(int)99",
  "video/x-svq",
  "video/x-h265, profile=(string)main",
  "video/x-h265, profile=(string)xafasdf",
  "video/x-h265",
  "video/x-h264, variant=(string)itu",
  "video/x-h264, variant=(string)videosoft",
  "video/x-h264, variant=(string)foobar",
  "video/x-h264",
  "video/x-h264, profile=(string)foobar",
  "video/x-h264, profile=(string)high-4:4:4-intra",
  "video/x-h264, profile=(string)high",
  "video/x-h263, variant=(string)itu",
  "video/x-h263, variant=(string)lead",
  "video/x-h263, variant=(string)microsoft",
  "video/x-h263, variant=(string)vdolive",
  "video/x-h263, variant=(string)vivo",
  "video/x-h263, variant=(string)xirlink",
  "video/x-h263, variant=(string)foobar",
  "video/x-h263",
  "video/x-msmpeg, msmpegversion=(int)41",
  "video/x-msmpeg, msmpegversion=(int)42",
  "video/x-msmpeg, msmpegversion=(int)43",
  "video/x-msmpeg, msmpegversion=(int)99",
  "video/x-msmpeg",
  "video/x-pn-realvideo, rmversion=(int)1",
  "video/x-pn-realvideo, rmversion=(int)2",
  "video/x-pn-realvideo, rmversion=(int)3",
  "video/x-pn-realvideo, rmversion=(int)4",
  "video/x-pn-realvideo, rmversion=(int)99",
  "video/x-pn-realvideo",
  "audio/x-pn-realaudio, raversion=(int)1",
  "audio/x-pn-realaudio, raversion=(int)2",
  "audio/x-pn-realaudio, raversion=(int)99",
  "audio/x-pn-realaudio",
  "audio/x-mace, maceversion=(int)3",
  "audio/x-mace, maceversion=(int)6",
  "audio/x-mace, maceversion=(int)99",
  "audio/x-mace",
  "video/x-truemotion, trueversion=(int)1",
  "video/x-truemotion, trueversion=(int)2",
  "video/x-truemotion, trueversion=(int)99",
  "video/x-truemotion",
  "video/x-asus, asusversion=(int)1",
  "video/x-asus, asusversion=(int)2",
  "video/x-asus, asusversion=(int)99",
  "video/x-asus",
  "video/x-xan, wcversion=(int)1",
  "video/x-xan, wcversion=(int)99",
  "video/x-xan",
  "video/x-ati-vcr, vcrversion=(int)1",
  "video/x-ati-vcr, vcrversion=(int)2",
  "video/x-ati-vcr, vcrversion=(int)99",
  "video/x-ati-vcr",
  /* raw audio */
  "audio/x-raw, format=(string)S16LE, rate=(int)44100, channels=(int)2",
  "audio/x-raw, format=(string)F32,rate=(int)22050, channels=(int)2",
  /* raw video */
  "video/x-raw, format=(string)RGB16, width=(int)320, height=(int)240, framerate=(fraction)30/1, pixel-aspect-ratio=(fraction)1/1",
  "video/x-raw, format=(string)YUY2, width=(int)320, height=(int)240, framerate=(fraction)30/1",
  "video/x-raw, format=(string)I420, width=(int)320, height=(int)240, framerate=(fraction)30/1",
  "video/x-raw, format=(string)AYUV, width=(int)320, height=(int)240, framerate=(fraction)30/1",
  /* and a made-up format */
  "video/x-tpm"
};

static gboolean
validate_video_subsampling (GstCaps * caps, gchar * desc)
{
  GstStructure *st;
  const gchar *format;

  st = gst_caps_get_structure (caps, 0);
  if (!gst_structure_has_name (st, "video/x-raw"))
    return TRUE;

  format = gst_structure_get_string (st, "format");
  if (!format)
    return TRUE;

  if (g_strcmp0 (format, "YUY2") == 0)
    return g_str_has_suffix (desc, "4:2:2");
  if (g_strcmp0 (format, "I420") == 0)
    return g_str_has_suffix (desc, "4:2:0");
  if (g_strcmp0 (format, "AYUV") == 0)
    return g_str_has_suffix (desc, "4:4:4");

  return TRUE;
}

GST_START_TEST (test_pb_utils_get_codec_description)
{
  gint i;

  gst_pb_utils_init ();

  for (i = 0; i < G_N_ELEMENTS (caps_strings); ++i) {
    GstCaps *caps;
    gchar *desc;

    caps = gst_caps_from_string (caps_strings[i]);
    fail_unless (caps != NULL, "could not create caps from string '%s'",
        caps_strings[i]);
    GST_LOG ("Caps %s:", caps_strings[i]);
    desc = gst_pb_utils_get_codec_description (caps);
    fail_unless (desc != NULL);
    GST_LOG (" - codec   : %s", desc);
    fail_unless (g_utf8_validate (desc, -1, NULL));
    fail_unless (validate_video_subsampling (caps, desc));
    g_free (desc);
    desc = gst_pb_utils_get_decoder_description (caps);
    fail_unless (desc != NULL);
    GST_LOG (" - decoder : %s", desc);
    fail_unless (g_utf8_validate (desc, -1, NULL));
    g_free (desc);
    desc = gst_pb_utils_get_encoder_description (caps);
    fail_unless (desc != NULL);
    GST_LOG (" - encoder : %s", desc);
    fail_unless (g_utf8_validate (desc, -1, NULL));
    g_free (desc);
    gst_caps_unref (caps);
  }
}

GST_END_TEST;


GST_START_TEST (test_pb_utils_taglist_add_codec_info)
{
  GstTagList *list;
  GstCaps *caps, *bogus_caps;
  gchar *res;

  gst_pb_utils_init ();
  list = gst_tag_list_new_empty ();
  caps = gst_caps_new_empty_simple ("video/x-theora");
  ASSERT_CRITICAL (fail_if
      (gst_pb_utils_add_codec_description_to_tag_list (NULL,
              GST_TAG_VIDEO_CODEC, caps)));
  ASSERT_CRITICAL (fail_if
      (gst_pb_utils_add_codec_description_to_tag_list (list, "asdfa", caps)));
  ASSERT_CRITICAL (fail_if
      (gst_pb_utils_add_codec_description_to_tag_list (list,
              GST_TAG_IMAGE, caps)));
  ASSERT_CRITICAL (fail_if
      (gst_pb_utils_add_codec_description_to_tag_list (list,
              GST_TAG_VIDEO_CODEC, NULL)));

  /* Try adding bogus caps (should fail) */
  bogus_caps = gst_caps_new_empty_simple ("bogus/format");
  fail_if (gst_pb_utils_add_codec_description_to_tag_list (list,
          GST_TAG_VIDEO_CODEC, bogus_caps));
  gst_caps_unref (bogus_caps);

  /* Try adding valid caps with known tag */
  fail_unless (gst_pb_utils_add_codec_description_to_tag_list (list,
          GST_TAG_VIDEO_CODEC, caps));
  fail_if (gst_tag_list_is_empty (list));
  fail_unless (gst_tag_list_get_string (list, GST_TAG_VIDEO_CODEC, &res));
  g_free (res);
  gst_tag_list_unref (list);

  /* Try adding valid caps with auto-tag (for video, audio, subtitle, generic) */
  list = gst_tag_list_new_empty ();
  fail_unless (gst_pb_utils_add_codec_description_to_tag_list (list, NULL,
          caps));
  fail_if (gst_tag_list_is_empty (list));
  fail_unless (gst_tag_list_get_string (list, GST_TAG_VIDEO_CODEC, &res));
  g_free (res);
  gst_tag_list_unref (list);
  gst_caps_unref (caps);

  list = gst_tag_list_new_empty ();
  caps = gst_caps_new_empty_simple ("audio/x-vorbis");
  fail_unless (gst_pb_utils_add_codec_description_to_tag_list (list, NULL,
          caps));
  fail_if (gst_tag_list_is_empty (list));
  fail_unless (gst_tag_list_get_string (list, GST_TAG_AUDIO_CODEC, &res));
  g_free (res);
  gst_tag_list_unref (list);
  gst_caps_unref (caps);

  list = gst_tag_list_new_empty ();
  caps = gst_caps_new_empty_simple ("subtitle/x-kate");
  fail_unless (gst_pb_utils_add_codec_description_to_tag_list (list, NULL,
          caps));
  fail_if (gst_tag_list_is_empty (list));
  fail_unless (gst_tag_list_get_string (list, GST_TAG_SUBTITLE_CODEC, &res));
  g_free (res);
  gst_tag_list_unref (list);
  gst_caps_unref (caps);

  list = gst_tag_list_new_empty ();
  caps = gst_caps_new_empty_simple ("application/ogg");
  fail_unless (gst_pb_utils_add_codec_description_to_tag_list (list, NULL,
          caps));
  fail_if (gst_tag_list_is_empty (list));
  fail_unless (gst_tag_list_get_string (list, GST_TAG_CONTAINER_FORMAT, &res));
  g_free (res);
  gst_tag_list_unref (list);
  gst_caps_unref (caps);

  list = gst_tag_list_new_empty ();
  caps = gst_caps_new_empty_simple ("image/bmp");
  fail_unless (gst_pb_utils_add_codec_description_to_tag_list (list, NULL,
          caps));
  fail_if (gst_tag_list_is_empty (list));
  fail_unless (gst_tag_list_get_string (list, GST_TAG_CODEC, &res));
  g_free (res);
  gst_tag_list_unref (list);
  gst_caps_unref (caps);
}

GST_END_TEST;

static gint marker;

static void
result_cb (GstInstallPluginsReturn result, gpointer user_data)
{
  GST_LOG ("result = %u, user_data = %p", result, user_data);

  fail_unless (user_data == (gpointer) & marker);

  marker = result;
}

#define SCRIPT_NO_XID \
    "#!/bin/sh\n"                                  \
    "if test x$1 != xdetail1; then exit 21; fi;\n" \
    "if test x$2 != xdetail2; then exit 22; fi;\n" \
    "exit 1\n"

#define SCRIPT_WITH_XID \
    "#!/bin/sh\n"                                  \
    "if test x$1 != 'x--transient-for=42'; then exit 21; fi;\n"      \
    "if test x$2 != xdetail1; then exit 22; fi;\n" \
    "if test x$3 != xdetail2; then exit 23; fi;\n" \
    "exit 0\n"

/* make sure our script gets called with the right parameters */
static void
test_pb_utils_install_plugins_do_callout (const gchar * const *details,
    GstInstallPluginsContext * ctx, const gchar * script,
    GstInstallPluginsReturn expected_result)
{
#ifdef G_OS_UNIX
  GstInstallPluginsReturn ret;
  GError *err = NULL;
  gchar *path;

  path = g_strdup_printf ("%s/gst-plugins-base-unit-test-helper.%s.%lu",
      g_get_tmp_dir (), (g_get_user_name ())? g_get_user_name () : "nobody",
      (gulong) getpid ());

  if (!g_file_set_contents (path, script, -1, &err)) {
    GST_DEBUG ("Failed to write test script to %s: %s", path, err->message);
    g_error_free (err);
    goto done;
  }

  if (chmod (path, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
    GST_DEBUG ("Could not set mode u+rwx on '%s'", path);
    goto done;
  }

  /* test gst_install_plugins_supported() I */
  g_setenv ("GST_INSTALL_PLUGINS_HELPER", "/i/do/not/ex.ist!", 1);
  fail_if (gst_install_plugins_supported ());

  GST_LOG ("setting GST_INSTALL_PLUGINS_HELPER to '%s'", path);
  g_setenv ("GST_INSTALL_PLUGINS_HELPER", path, 1);

  /* test gst_install_plugins_supported() II */
  fail_unless (gst_install_plugins_supported ());

  /* test sync callout */
  ret = gst_install_plugins_sync (details, ctx);
  fail_unless (ret == GST_INSTALL_PLUGINS_HELPER_MISSING ||
      ret == expected_result,
      "gst_install_plugins_sync() failed with unexpected ret %d, which is "
      "neither HELPER_MISSING nor %d", ret, expected_result);

  /* test async callout */
  marker = -333;
  ret = gst_install_plugins_async (details, ctx, result_cb,
      (gpointer) & marker);
  fail_unless (ret == GST_INSTALL_PLUGINS_HELPER_MISSING ||
      ret == GST_INSTALL_PLUGINS_STARTED_OK,
      "gst_install_plugins_async() failed with unexpected ret %d", ret);
  if (ret == GST_INSTALL_PLUGINS_STARTED_OK) {
    while (marker == -333) {
      g_usleep (500);
      g_main_context_iteration (NULL, FALSE);
    }
    /* and check that the callback was called with the expected code */
    fail_unless_equals_int (marker, expected_result);
  }

done:

  g_unlink (path);
  g_free (path);
#endif /* G_OS_UNIX */
}

GST_START_TEST (test_pb_utils_install_plugins)
{
  GstInstallPluginsContext *ctx;
  GstInstallPluginsReturn ret;
  const gchar *details[] = { "detail1", "detail2", NULL };
  const gchar *details_multi[] = { "detail1", "detail1", "detail2", NULL };

  ctx = gst_install_plugins_context_new ();

  ASSERT_CRITICAL (ret = gst_install_plugins_sync (NULL, ctx));
  ASSERT_CRITICAL (ret =
      gst_install_plugins_async (NULL, ctx, result_cb, (gpointer) & marker));
  ASSERT_CRITICAL (ret =
      gst_install_plugins_async (details, ctx, NULL, (gpointer) & marker));

  /* make sure the functions return the right error code if the helper does
   * not exist */
  g_setenv ("GST_INSTALL_PLUGINS_HELPER", "/does/not/ex/is.t", 1);
  ret = gst_install_plugins_sync (details, NULL);
  fail_unless_equals_int (ret, GST_INSTALL_PLUGINS_HELPER_MISSING);

  marker = -333;
  ret =
      gst_install_plugins_async (details, NULL, result_cb, (gpointer) & marker);
  fail_unless_equals_int (ret, GST_INSTALL_PLUGINS_HELPER_MISSING);
  /* and check that the callback wasn't called */
  fail_unless_equals_int (marker, -333);

  /* now make sure our scripts are actually called as expected (if possible) */
  test_pb_utils_install_plugins_do_callout (details, NULL, SCRIPT_NO_XID,
      GST_INSTALL_PLUGINS_NOT_FOUND);

  /* and again with context */
  gst_install_plugins_context_set_xid (ctx, 42);
  test_pb_utils_install_plugins_do_callout (details, ctx, SCRIPT_WITH_XID,
      GST_INSTALL_PLUGINS_SUCCESS);

  /* and make sure that duplicate detail strings get dropped */
  test_pb_utils_install_plugins_do_callout (details_multi, NULL, SCRIPT_NO_XID,
      GST_INSTALL_PLUGINS_NOT_FOUND);

  /* and the same again with context */
  gst_install_plugins_context_set_xid (ctx, 42);
  test_pb_utils_install_plugins_do_callout (details_multi, ctx, SCRIPT_WITH_XID,
      GST_INSTALL_PLUGINS_SUCCESS);

  /* and free the context now that we don't need it any longer */
  gst_install_plugins_context_free (ctx);

  /* completely silly test to check gst_install_plugins_return_get_name()
   * is somewhat well-behaved */
  {
    gint i;

    for (i = -99; i < 16738; ++i) {
      const gchar *s;

      s = gst_install_plugins_return_get_name ((GstInstallPluginsReturn) i);
      fail_unless (s != NULL);
      /* GST_LOG ("%5d = %s", i, s); */
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_pb_utils_installer_details)
{
  GstMessage *msg;
  GstElement *el;
  GstCaps *caps;
  gchar *detail1, *detail2;

  el = gst_pipeline_new ("dummy-element");

  /* uri source */
  detail1 = gst_missing_uri_source_installer_detail_new ("http");
  fail_unless (detail1 != NULL);
  fail_unless (g_str_has_prefix (detail1, "gstreamer|1.0|"));
  fail_unless (g_str_has_suffix (detail1, "|urisource-http"));
  msg = gst_missing_uri_source_message_new (el, "http");
  fail_unless (msg != NULL);
  detail2 = gst_missing_plugin_message_get_installer_detail (msg);
  fail_unless (detail2 != NULL);
  gst_message_unref (msg);
  fail_unless_equals_string (detail1, detail2);
  g_free (detail1);
  g_free (detail2);

  /* uri sink */
  detail1 = gst_missing_uri_sink_installer_detail_new ("http");
  fail_unless (detail1 != NULL);
  fail_unless (g_str_has_prefix (detail1, "gstreamer|1.0|"));
  fail_unless (g_str_has_suffix (detail1, "|urisink-http"));
  msg = gst_missing_uri_sink_message_new (el, "http");
  fail_unless (msg != NULL);
  detail2 = gst_missing_plugin_message_get_installer_detail (msg);
  fail_unless (detail2 != NULL);
  gst_message_unref (msg);
  fail_unless_equals_string (detail1, detail2);
  g_free (detail1);
  g_free (detail2);

  /* element */
  detail1 = gst_missing_element_installer_detail_new ("deinterlace");
  fail_unless (detail1 != NULL);
  fail_unless (g_str_has_prefix (detail1, "gstreamer|1.0|"));
  fail_unless (g_str_has_suffix (detail1, "|element-deinterlace"));
  msg = gst_missing_element_message_new (el, "deinterlace");
  fail_unless (msg != NULL);
  detail2 = gst_missing_plugin_message_get_installer_detail (msg);
  fail_unless (detail2 != NULL);
  gst_message_unref (msg);
  fail_unless_equals_string (detail1, detail2);
  g_free (detail1);
  g_free (detail2);

  /* decoder */
  caps = gst_caps_new_simple ("audio/x-spiffy", "spiffyversion", G_TYPE_INT,
      2, "channels", G_TYPE_INT, 6, NULL);
  detail1 = gst_missing_decoder_installer_detail_new (caps);
  fail_unless (detail1 != NULL);
  fail_unless (g_str_has_prefix (detail1, "gstreamer|1.0|"));
  fail_unless (g_str_has_suffix (detail1,
          "|decoder-audio/x-spiffy, spiffyversion=(int)2"));
  msg = gst_missing_decoder_message_new (el, caps);
  fail_unless (msg != NULL);
  detail2 = gst_missing_plugin_message_get_installer_detail (msg);
  fail_unless (detail2 != NULL);
  gst_message_unref (msg);
  gst_caps_unref (caps);
  fail_unless_equals_string (detail1, detail2);
  g_free (detail1);
  g_free (detail2);

  /* encoder */
  caps = gst_caps_new_simple ("audio/x-spiffy", "spiffyversion", G_TYPE_INT,
      2, "channels", G_TYPE_INT, 6, NULL);
  detail1 = gst_missing_encoder_installer_detail_new (caps);
  fail_unless (g_str_has_prefix (detail1, "gstreamer|1.0|"));
  fail_unless (g_str_has_suffix (detail1,
          "|encoder-audio/x-spiffy, spiffyversion=(int)2"));
  fail_unless (detail1 != NULL);
  msg = gst_missing_encoder_message_new (el, caps);
  fail_unless (msg != NULL);
  detail2 = gst_missing_plugin_message_get_installer_detail (msg);
  fail_unless (detail2 != NULL);
  gst_message_unref (msg);
  gst_caps_unref (caps);
  fail_unless_equals_string (detail1, detail2);
  g_free (detail1);
  g_free (detail2);

  gst_object_unref (el);
}

GST_END_TEST;

GST_START_TEST (test_pb_utils_versions)
{
  gchar *s;
  guint maj, min, mic, nano;

  gst_plugins_base_version (NULL, NULL, NULL, NULL);
  gst_plugins_base_version (&maj, &min, &mic, &nano);
  fail_unless_equals_int (maj, GST_PLUGINS_BASE_VERSION_MAJOR);
  fail_unless_equals_int (min, GST_PLUGINS_BASE_VERSION_MINOR);
  fail_unless_equals_int (mic, GST_PLUGINS_BASE_VERSION_MICRO);
  fail_unless_equals_int (nano, GST_PLUGINS_BASE_VERSION_NANO);

  s = gst_plugins_base_version_string ();
  if (GST_PLUGINS_BASE_VERSION_NANO == 0) {
    fail_if (strstr (s, "GIT") || strstr (s, "git") || strstr (s, "prerel"));
  }
  if (GST_PLUGINS_BASE_VERSION_NANO == 1) {
    fail_unless (strstr (s, "GIT") || strstr (s, "git"));
  }
  if (GST_PLUGINS_BASE_VERSION_NANO >= 2) {
    fail_unless (strstr (s, "Prerelease") || strstr (s, "prerelease"));
  }
  g_free (s);
}

GST_END_TEST;

GST_START_TEST (test_pb_utils_aac_get_profile)
{
  const guint8 aac_config[] = { 0x11, 0x90, 0x56, 0xE5, 0x00 };
  const guint8 aac_config_sre[] = { 0x17, 0x80, 0x91, 0xA2, 0x82, 0x00 };
  const guint8 heaac_config[] = { 0x2B, 0x11, 0x88, 0x00, 0x06, 0x01, 0x02 };
  const gchar *profile, *level;
  guint sample_rate;
  GstBitWriter *wr;
  guint8 *buf;
  guint buf_len;

  profile = gst_codec_utils_aac_get_profile (aac_config, sizeof (aac_config));
  fail_unless (profile != NULL);
  fail_unless_equals_string (profile, "lc");

  level = gst_codec_utils_aac_get_level (aac_config, sizeof (aac_config));
  fail_unless_equals_string (level, "2");

  sample_rate =
      gst_codec_utils_aac_get_sample_rate (aac_config, sizeof (aac_config));
  fail_unless_equals_int (sample_rate, 48000);

  sample_rate =
      gst_codec_utils_aac_get_sample_rate (aac_config_sre,
      sizeof (aac_config_sre));
  fail_unless_equals_int (sample_rate, 0x12345);

  profile =
      gst_codec_utils_aac_get_profile (heaac_config, sizeof (heaac_config));
  fail_unless (profile != NULL);
  fail_unless_equals_string (profile, "lc");

  level = gst_codec_utils_aac_get_level (heaac_config, sizeof (heaac_config));
  fail_unless_equals_string (level, "2");

  sample_rate =
      gst_codec_utils_aac_get_sample_rate (heaac_config, sizeof (heaac_config));
  fail_unless_equals_int (sample_rate, 48000);

  wr = gst_bit_writer_new ();
  fail_if (wr == NULL);
  gst_bit_writer_put_bits_uint8 (wr, 5, 5);     /* object_type = 5 (SBR) */
  gst_bit_writer_put_bits_uint8 (wr, 3, 4);     /* freq_index = 3 (48KHz) */
  gst_bit_writer_put_bits_uint8 (wr, 2, 4);     /* channel_config = 2 (L&R) */
  gst_bit_writer_put_bits_uint8 (wr, 0x0f, 4);  /* freq_index extension */
  gst_bit_writer_put_bits_uint32 (wr, 87654, 24);       /* freq */
  gst_bit_writer_put_bits_uint8 (wr, 2, 5);     /* object_type = 2 (LC) */

  buf = gst_bit_writer_get_data (wr);
  buf_len = gst_bit_writer_get_size (wr);
  profile = gst_codec_utils_aac_get_profile (buf, buf_len);
  fail_unless (profile != NULL);
  fail_unless_equals_string (profile, "lc");
  level = gst_codec_utils_aac_get_level (buf, buf_len);
  fail_unless (level != NULL);
  fail_unless_equals_string (level, "5");
  sample_rate = gst_codec_utils_aac_get_sample_rate (buf, buf_len);
  fail_unless_equals_int (sample_rate, 87654);
  gst_bit_writer_free (wr);
}

GST_END_TEST;

#define SPS_LEN 3
#define SPS_CONSTRAINT_SET_FLAG_0 1 << 7
#define SPS_CONSTRAINT_SET_FLAG_1 (1 << 6)
#define SPS_CONSTRAINT_SET_FLAG_2 (1 << 5)
#define SPS_CONSTRAINT_SET_FLAG_3 (1 << 4)
#define SPS_CONSTRAINT_SET_FLAG_4 (1 << 3)
#define SPS_CONSTRAINT_SET_FLAG_5 (1 << 2)

static void
fill_h264_sps (guint8 * sps,
    guint8 profile_idc, guint constraint_set_flags, guint8 level_idc)
{
  memset (sps, 0x0, SPS_LEN);
  /*
   * * Bit 0:7   - Profile indication
   * * Bit 8     - constraint_set0_flag
   * * Bit 9     - constraint_set1_flag
   * * Bit 10    - constraint_set2_flag
   * * Bit 11    - constraint_set3_flag
   * * Bit 12    - constraint_set4_flag
   * * Bit 13    - constraint_set5_flag
   * * Bit 14:15 - Reserved
   * * Bit 16:24 - Level indication
   * */
  sps[0] = profile_idc;
  sps[1] |= constraint_set_flags;
  sps[2] = level_idc;
}

GST_START_TEST (test_pb_utils_h264_profiles)
{
  guint8 sps[SPS_LEN] = { 0, };
  const gchar *profile;

  fill_h264_sps (sps, 66, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "baseline");

  fill_h264_sps (sps, 66, SPS_CONSTRAINT_SET_FLAG_1, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "constrained-baseline");

  fill_h264_sps (sps, 77, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "main");

  fill_h264_sps (sps, 88, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "extended");

  fill_h264_sps (sps, 100, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "high");

  fill_h264_sps (sps, 100,
      SPS_CONSTRAINT_SET_FLAG_4 | SPS_CONSTRAINT_SET_FLAG_5, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "constrained-high");

  fill_h264_sps (sps, 100, SPS_CONSTRAINT_SET_FLAG_4, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "progressive-high");

  fill_h264_sps (sps, 110, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "high-10");

  fill_h264_sps (sps, 110, SPS_CONSTRAINT_SET_FLAG_3, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "high-10-intra");

  fill_h264_sps (sps, 110, SPS_CONSTRAINT_SET_FLAG_4, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "progressive-high-10");

  fill_h264_sps (sps, 122, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "high-4:2:2");

  fill_h264_sps (sps, 122, SPS_CONSTRAINT_SET_FLAG_3, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "high-4:2:2-intra");

  fill_h264_sps (sps, 244, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "high-4:4:4");

  fill_h264_sps (sps, 244, SPS_CONSTRAINT_SET_FLAG_3, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "high-4:4:4-intra");

  fill_h264_sps (sps, 44, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "cavlc-4:4:4-intra");

  fill_h264_sps (sps, 118, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "multiview-high");

  fill_h264_sps (sps, 128, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "stereo-high");

  fill_h264_sps (sps, 83, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "scalable-baseline");

  fill_h264_sps (sps, 83, SPS_CONSTRAINT_SET_FLAG_5, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "scalable-constrained-baseline");

  fill_h264_sps (sps, 86, 0, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "scalable-high");

  fill_h264_sps (sps, 86, SPS_CONSTRAINT_SET_FLAG_3, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "scalable-high-intra");

  fill_h264_sps (sps, 86, SPS_CONSTRAINT_SET_FLAG_5, 0);
  profile = gst_codec_utils_h264_get_profile (sps, SPS_LEN);
  fail_unless_equals_string (profile, "scalable-constrained-high");

}

GST_END_TEST;

GST_START_TEST (test_pb_utils_h264_get_profile_flags_level)
{
  gboolean ret = FALSE;
  guint codec_data_len = 7;
  guint8 codec_data[] = { 0x01, 0x64, 0x00, 0x32, 0x00, 0x00, 0x00 };
  guint8 codec_data_bad_version[] =
      { 0x00, 0x64, 0x00, 0x32, 0x00, 0x00, 0x00 };
  guint8 profile;
  guint8 flags;
  guint8 level;

  /* happy path */
  ret =
      gst_codec_utils_h264_get_profile_flags_level (codec_data, codec_data_len,
      &profile, &flags, &level);
  fail_unless (ret == TRUE);
  fail_unless (profile == 0x64);
  fail_unless (flags == 0x00);
  fail_unless (level == 0x32);

  /* happy path, return locations null */
  ret =
      gst_codec_utils_h264_get_profile_flags_level (codec_data, codec_data_len,
      NULL, NULL, NULL);
  fail_unless (ret == TRUE);

  /* data too short */
  ret =
      gst_codec_utils_h264_get_profile_flags_level (codec_data, 6, &profile,
      &flags, &level);
  fail_unless (ret == FALSE);

  /* wrong codec version */
  ret =
      gst_codec_utils_h264_get_profile_flags_level (codec_data_bad_version,
      codec_data_len, &profile, &flags, &level);
  fail_unless (ret == FALSE);
}

GST_END_TEST;

#define PROFILE_TIER_LEVEL_LEN 11

static void
fill_h265_profile (guint8 * profile_tier_level,
    guint8 profile_idc, guint8 max_14bit_flag, guint8 max_12bit_flag,
    guint8 max_10bit_flag, guint8 max_8bit_flag, guint8 max_422_flag,
    guint8 max_420_flag, guint8 max_mono_flag, guint8 intra_flag,
    guint8 one_pic_flag, guint8 lower_bit_rate_flag)
{
  /* Bit 0:1   - general_profile_space
   * Bit 2     - general_tier_flag
   * Bit 3:7   - general_profile_idc
   * Bit 8:39  - gernal_profile_compatibility_flags
   * Bit 40    - general_progressive_source_flag
   * Bit 41    - general_interlaced_source_flag
   * Bit 42    - general_non_packed_constraint_flag
   * Bit 43    - general_frame_only_constraint_flag
   */

  memset (profile_tier_level, 0x0, PROFILE_TIER_LEVEL_LEN);

  profile_tier_level[0] = profile_idc;

  if (profile_idc < 4)
    return;

  profile_tier_level[5] |= (max_12bit_flag << 3);
  profile_tier_level[5] |= (max_10bit_flag << 2);
  profile_tier_level[5] |= (max_8bit_flag << 1);
  profile_tier_level[5] |= max_422_flag;
  profile_tier_level[6] |= (max_420_flag << 7);
  profile_tier_level[6] |= (max_mono_flag << 6);
  profile_tier_level[6] |= (intra_flag << 5);
  profile_tier_level[6] |= (one_pic_flag << 4);
  profile_tier_level[6] |= (lower_bit_rate_flag << 3);
  profile_tier_level[6] |= (max_14bit_flag << 2);
}

GST_START_TEST (test_pb_utils_h265_profiles)
{
  guint8 profile_tier_level[PROFILE_TIER_LEVEL_LEN] = { 0, };
  const gchar *profile;

  fill_h265_profile (profile_tier_level, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main");

  fill_h265_profile (profile_tier_level, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-10");

  fill_h265_profile (profile_tier_level, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-still-picture");

  /* Format range extensions profiles */
  fill_h265_profile (profile_tier_level, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless (profile == NULL);

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "monochrome");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "monochrome-10");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "monochrome-12");

  fill_h265_profile (profile_tier_level, 4, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "monochrome-16");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-12");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-422-10");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-422-12");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-444");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-444-10");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-444-12");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-intra");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-10-intra");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-12-intra");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-422-10-intra");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-422-12-intra");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-444-intra");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-444-10-intra");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-444-12-intra");

  fill_h265_profile (profile_tier_level, 4, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-444-16-intra");

  fill_h265_profile (profile_tier_level, 4, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-444-still-picture");

  fill_h265_profile (profile_tier_level, 4, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "main-444-16-still-picture");

  /* High Throughput profiles */
  fill_h265_profile (profile_tier_level, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless (profile == NULL);

  fill_h265_profile (profile_tier_level, 5, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "high-throughput-444");

  fill_h265_profile (profile_tier_level, 5, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "high-throughput-444-10");

  fill_h265_profile (profile_tier_level, 5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "high-throughput-444-14");

  fill_h265_profile (profile_tier_level, 5, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "high-throughput-444-16-intra");

  /* Multiview Main profile */
  fill_h265_profile (profile_tier_level, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless (profile == NULL);

  fill_h265_profile (profile_tier_level, 6, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "multiview-main");

  /* Scalable Main profiles */
  fill_h265_profile (profile_tier_level, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless (profile == NULL);

  fill_h265_profile (profile_tier_level, 7, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "scalable-main");

  fill_h265_profile (profile_tier_level, 7, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "scalable-main-10");

  /* 3D Main profile */
  fill_h265_profile (profile_tier_level, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless (profile == NULL);

  fill_h265_profile (profile_tier_level, 8, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "3d-main");

  /* Screen content coding extensions profiles */
  fill_h265_profile (profile_tier_level, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless (profile == NULL);

  fill_h265_profile (profile_tier_level, 9, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "screen-extended-main");

  fill_h265_profile (profile_tier_level, 9, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "screen-extended-main-10");

  fill_h265_profile (profile_tier_level, 9, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "screen-extended-main-444");

  fill_h265_profile (profile_tier_level, 9, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "screen-extended-main-444-10");

  fill_h265_profile (profile_tier_level, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "screen-extended-high-throughput-444-14");

  /* Scalable format range extensions profiles */
  fill_h265_profile (profile_tier_level, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless (profile == NULL);

  fill_h265_profile (profile_tier_level, 10, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "scalable-monochrome");

  fill_h265_profile (profile_tier_level, 10, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "scalable-monochrome-12");

  fill_h265_profile (profile_tier_level, 10, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "scalable-monochrome-16");

  fill_h265_profile (profile_tier_level, 10, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "scalable-main-444");

  fill_h265_profile (profile_tier_level, 11, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "screen-extended-main-444-10");

  fill_h265_profile (profile_tier_level, 11, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "screen-extended-main-444");

  fill_h265_profile (profile_tier_level, 11, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1);
  profile = gst_codec_utils_h265_get_profile (profile_tier_level,
      sizeof (profile_tier_level));
  fail_unless_equals_string (profile, "screen-extended-high-throughput-444-14");
}

GST_END_TEST;

static const guint8 h265_sample_codec_data[] = {
  0x01, 0x01, 0x60, 0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5d,
  0xf0, 0x00, 0xfc,
  0xfd, 0xf8, 0xf8, 0x00, 0x00, 0x0f, 0x03, 0x20, 0x00, 0x01, 0x00, 0x18, 0x40,
  0x01, 0x0c, 0x01,
  0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00,
  0x00, 0x03, 0x00,
  0x5d, 0x15, 0xc0, 0x90, 0x21, 0x00, 0x01, 0x00, 0x22, 0x42, 0x01, 0x01, 0x01,
  0x60, 0x00, 0x00,
  0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x5d, 0xa0, 0x0a,
  0x08, 0x0f, 0x16,
  0x20, 0x57, 0xb9, 0x16, 0x55, 0x35, 0x01, 0x01, 0x01, 0x00, 0x80, 0x22, 0x00,
  0x01, 0x00, 0x07,
  0x44, 0x01, 0xc0, 0x2c, 0xbc, 0x14, 0xc9
};

GST_START_TEST (test_pb_utils_caps_mime_codec)
{
  GstCaps *caps = NULL;
  GstCaps *caps2 = NULL;
  gchar *mime_codec = NULL;
  GstBuffer *buffer = NULL;
  guint8 *codec_data = NULL;
  gsize codec_data_len;

  /* h264 without codec data */
  caps = gst_caps_new_empty_simple ("video/x-h264");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "avc1");
  caps2 = gst_codec_utils_caps_from_mime_codec (mime_codec);
  fail_unless (gst_caps_is_equal_fixed (caps, caps2));
  gst_caps_unref (caps2);
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* h264 with codec data */
  codec_data_len = sizeof (guint8) * 7;
  codec_data = g_malloc0 (codec_data_len);
  codec_data[0] = 0x01;
  codec_data[1] = 0x64;
  codec_data[2] = 0x00;
  codec_data[3] = 0x32;
  /* seven bytes is the minumum for a valid h264 codec_data, but in
   * gst_codec_utils_h264_get_profile_flags_level we only parse the first four
   * bytes */
  buffer = gst_buffer_new_wrapped (codec_data, codec_data_len);
  caps =
      gst_caps_new_simple ("video/x-h264", "codec_data", GST_TYPE_BUFFER,
      buffer, NULL);
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "avc1.640032");
  g_free (mime_codec);
  gst_caps_unref (caps);
  gst_buffer_unref (buffer);

  /* h265 */
  buffer =
      gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      (gpointer) h265_sample_codec_data, sizeof (h265_sample_codec_data), 0,
      sizeof (h265_sample_codec_data), NULL, NULL);
  caps =
      gst_caps_new_simple ("video/x-h265", "stream-format", G_TYPE_STRING,
      "hvc1", "codec_data", GST_TYPE_BUFFER, buffer, NULL);
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "hvc1.1.6.L93.B0");
  g_free (mime_codec);
  gst_caps_unref (caps);
  gst_buffer_unref (buffer);

  /* av1 */
  caps = gst_caps_new_empty_simple ("video/x-av1");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "av01");
  caps2 = gst_codec_utils_caps_from_mime_codec (mime_codec);
  fail_unless (gst_caps_is_equal_fixed (caps, caps2));
  gst_caps_unref (caps2);
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* vp8 */
  caps = gst_caps_new_empty_simple ("video/x-vp8");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "vp08");
  caps2 = gst_codec_utils_caps_from_mime_codec (mime_codec);
  fail_unless (gst_caps_is_equal_fixed (caps, caps2));
  gst_caps_unref (caps2);
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* vp9 */
  caps = gst_caps_new_empty_simple ("video/x-vp9");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "vp09");
  caps2 = gst_codec_utils_caps_from_mime_codec (mime_codec);
  fail_unless (gst_caps_is_equal_fixed (caps, caps2));
  gst_caps_unref (caps2);
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* vp9 with default chroma subsampling, color primaries, color transfer, color
   * matrix and luma/chroma encoded in the "legal" range*/
  caps =
      gst_caps_from_string
      ("video/x-vp9, width=(int)640, height=(int)480, pixel-aspect-ratio=(fraction)1/1, framerate=(fraction)30/1, chroma-format=(string)4:2:0, bit-depth-luma=(uint)8, bit-depth-chroma=(uint)8, colorimetry=(string)bt709, alignment=(string)super-frame, profile=(string)0, codec-alpha=(boolean)false");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "vp09.00.10.08");
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* vp9 with non-default chroma subsampling */
  caps = gst_caps_from_string ("video/x-vp9, width=(int)640, height=(int)480, "
      "pixel-aspect-ratio=(fraction)1/1, framerate=(fraction)30/1, "
      "chroma-format=(string)4:2:2, bit-depth-luma=(uint)8, "
      "bit-depth-chroma=(uint)8, colorimetry=(string)bt709, "
      "alignment=(string)super-frame, profile=(string)0, "
      "codec-alpha=(boolean)false");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "vp09.00.10.08.02.01.01.01.00");
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* mjpeg */
  caps = gst_caps_new_empty_simple ("image/jpeg");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "mjpg");
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* aac without codec data */
  caps = gst_caps_new_empty_simple ("audio/mpeg");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "mp4a.40");
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* aac with codec data */
  codec_data_len = sizeof (guint8) * 2;
  codec_data = g_malloc0 (codec_data_len);
  codec_data[0] = 0x11;
  codec_data[1] = 0x88;
  buffer = gst_buffer_new_wrapped (codec_data, codec_data_len);
  caps =
      gst_caps_new_simple ("audio/mpeg", "codec_data", GST_TYPE_BUFFER, buffer,
      NULL);
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "mp4a.40.2");
  g_free (mime_codec);
  gst_caps_unref (caps);
  gst_buffer_unref (buffer);

  /* opus */
  caps = gst_caps_new_empty_simple ("audio/x-opus");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "opus");
  caps2 = gst_codec_utils_caps_from_mime_codec (mime_codec);
  fail_unless (gst_caps_is_equal_fixed (caps, caps2));
  gst_caps_unref (caps2);
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* mulaw */
  caps = gst_caps_new_empty_simple ("audio/x-mulaw");
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "ulaw");
  caps2 = gst_codec_utils_caps_from_mime_codec (mime_codec);
  fail_unless (gst_caps_is_equal_fixed (caps, caps2));
  gst_caps_unref (caps2);
  g_free (mime_codec);
  gst_caps_unref (caps);

  /* g726 */
  caps =
      gst_caps_new_simple ("audio/x-adpcm", "layout", G_TYPE_STRING, "g726",
      NULL);
  mime_codec = gst_codec_utils_caps_get_mime_codec (caps);
  fail_unless_equals_string (mime_codec, "g726");
  caps2 = gst_codec_utils_caps_from_mime_codec (mime_codec);
  fail_unless (gst_caps_is_equal_fixed (caps, caps2));
  gst_caps_unref (caps2);
  g_free (mime_codec);
  gst_caps_unref (caps);
}

GST_END_TEST;

static Suite *
libgstpbutils_suite (void)
{
  Suite *s = suite_create ("pbutils library");
  TCase *tc_chain = tcase_create ("general");

  gst_pb_utils_init ();

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_pb_utils_init);
  tcase_add_test (tc_chain, test_pb_utils_post_missing_messages);
  tcase_add_test (tc_chain, test_pb_utils_taglist_add_codec_info);
  tcase_add_test (tc_chain, test_pb_utils_get_caps_description_flags);
  tcase_add_test (tc_chain, test_pb_utils_get_codec_description);
  tcase_add_test (tc_chain, test_pb_utils_install_plugins);
  tcase_add_test (tc_chain, test_pb_utils_installer_details);
  tcase_add_test (tc_chain, test_pb_utils_versions);
  tcase_add_test (tc_chain, test_pb_utils_aac_get_profile);
  tcase_add_test (tc_chain, test_pb_utils_h264_profiles);
  tcase_add_test (tc_chain, test_pb_utils_h264_get_profile_flags_level);
  tcase_add_test (tc_chain, test_pb_utils_h265_profiles);
  tcase_add_test (tc_chain, test_pb_utils_caps_mime_codec);
  return s;
}

GST_CHECK_MAIN (libgstpbutils);
