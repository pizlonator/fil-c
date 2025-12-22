/* Gstreamer
 * Copyright (C) <2014> Jesper Larsen <knorr.jesper@gmail.com>
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

#include <gst/check/gstcheck.h>
#include <gst/mpegts/mpegts.h>

static const guint8 pat_data_check[] = {
  0x00, 0xB0, 0x11, 0x00, 0x00, 0xc1, 0x00,
  0x00, 0x00, 0x00, 0xe0, 0x30, 0x00, 0x01,
  0xe0, 0x31, 0x98, 0xdf, 0x37, 0xc4
};

static const guint8 pmt_data_check[] = {
  0x02, 0xb0, 0x29, 0x00, 0x01, 0xc1, 0x00,
  0x00, 0xff, 0xff, 0xf0, 0x06, 0x05, 0x04,
  0x48, 0x44, 0x4d, 0x56, 0x1b, 0xe0, 0x40,
  0xF0, 0x06, 0x05, 0x04, 0x48, 0x44, 0x4d,
  0x56, 0x1b, 0xe0, 0x41, 0xF0, 0x06, 0x05,
  0x04, 0x48, 0x44, 0x4d, 0x56, 0x15, 0x41,
  0x5f, 0x5b
};

static const guint8 nit_data_check[] = {
  0x40, 0xf0, 0x49, 0x1f, 0xff, 0xc1, 0x00,
  0x00, 0xf0, 0x0e, 0x40, 0x0c, 0x4e, 0x65,
  0x74, 0x77, 0x6f, 0x72, 0x6b, 0x20, 0x6e,
  0x61, 0x6d, 0x65, 0xf0, 0x2e, 0x1f, 0xff,
  0x1f, 0xfe, 0xf0, 0x11, 0x40, 0x0f, 0x41,
  0x6e, 0x6f, 0x74, 0x68, 0x65, 0x72, 0x20,
  0x6e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b,
  0x1f, 0xff, 0x1f, 0xfe, 0xf0, 0x11, 0x40,
  0x0f, 0x41, 0x6e, 0x6f, 0x74, 0x68, 0x65,
  0x72, 0x20, 0x6e, 0x65, 0x74, 0x77, 0x6f,
  0x72, 0x6b, 0xce, 0x03, 0xf5, 0x94
};

static const guint8 sdt_data_check[] = {
  0x42, 0xf0, 0x38, 0x1f, 0xff, 0xc1, 0x00,
  0x00, 0x1f, 0xff, 0xff, 0x00, 0x00, 0xFF,
  0x90, 0x11, 0x48, 0x0f, 0x01, 0x08, 0x50,
  0x72, 0x6f, 0x76, 0x69, 0x64, 0x65, 0x72,
  0x04, 0x4e, 0x61, 0x6d, 0x65, 0x00, 0x01,
  0xFF, 0xB0, 0x11, 0x48, 0x0f, 0x01, 0x08,
  0x50, 0x72, 0x6f, 0x76, 0x69, 0x64, 0x65,
  0x72, 0x04, 0x4e, 0x61, 0x6d, 0x65, 0x25,
  0xe5, 0x02, 0xd9
};

static const guint8 stt_data_check[] = {
  0xcd, 0xf0, 0x11, 0x00, 0x00, 0xc1, 0x00,
  0x00, 0x00, 0x23, 0xb4, 0xe6, 0x5C, 0x0c,
  0xc0, 0x00, 0xc4, 0x86, 0x56, 0xa5
};

static gboolean
_has_iso6937_iconv (void)
{
  gboolean supported;
  GIConv test = g_iconv_open ("iso6937", "utf-8");
  supported = (test != (GIConv) - 1);
  g_iconv_close (test);
  return supported;
}

GST_START_TEST (test_scte_sit)
{
  GstMpegtsSCTESIT *sit;
  GstMpegtsSection *sit_section;
  GstMpegtsSCTESpliceEvent *event;
  guint8 *data;
  gsize data_size;

  /* Try a simple NULL command before anything else */
  sit = gst_mpegts_scte_sit_new ();
  sit->tier = 123;
  sit->pts_adjustment = 0x1fedcba12;
  sit->splice_command_type = GST_MTS_SCTE_SPLICE_COMMAND_NULL;

  sit_section = gst_mpegts_section_from_scte_sit (sit, 456);
  fail_if (sit_section == NULL);
  fail_unless (sit_section->short_section);

  /* Serialize and check that we can re-parse it into something valid */
  data = gst_mpegts_section_packetize (sit_section, &data_size);
  fail_if (data == NULL);
  GST_MEMDUMP ("section", data, data_size);

  GST_LOG ("here");
  sit_section->destroy_parsed (sit_section->cached_parsed);
  sit_section->cached_parsed = NULL;

  sit = (GstMpegtsSCTESIT *) gst_mpegts_section_get_scte_sit (sit_section);
  fail_if (sit == NULL);
  /* Check the values */
  fail_unless (sit->encrypted_packet == FALSE);
  fail_unless (sit->pts_adjustment == 0x1fedcba12);
  fail_unless (sit->tier == 123);
  fail_unless (sit->splice_command_type == GST_MTS_SCTE_SPLICE_COMMAND_NULL);

  gst_mpegts_section_unref (sit_section);


  /* Same thing but now with an insert command */
  sit = gst_mpegts_scte_sit_new ();
  sit->tier = 123;
  sit->pts_adjustment = 0x1fedcba12;
  sit->splice_command_type = GST_MTS_SCTE_SPLICE_COMMAND_INSERT;

  event = gst_mpegts_scte_splice_event_new ();
  event->insert_event = TRUE;
  event->splice_event_id = 4285;
  event->program_splice_flag = TRUE;
  event->duration_flag = TRUE;
  event->splice_immediate_flag = FALSE;

  event->program_splice_time_specified = TRUE;
  event->program_splice_time = 0x1fdecba12;

  event->break_duration_auto_return = TRUE;
  event->break_duration = 590000;
  event->unique_program_id = 4256;
  event->avail_num = 2;
  event->avails_expected = 2;
  g_ptr_array_add (sit->splices, event);

  sit_section = gst_mpegts_section_from_scte_sit (sit, 456);
  fail_if (sit_section == NULL);
  fail_unless (sit_section->short_section);

  /* Serialize and check that we can re-parse it into something valid */
  data = gst_mpegts_section_packetize (sit_section, &data_size);
  fail_if (data == NULL);
  GST_MEMDUMP ("section", data, data_size);

  GST_LOG ("here");
  sit_section->destroy_parsed (sit_section->cached_parsed);
  sit_section->cached_parsed = NULL;

  sit = (GstMpegtsSCTESIT *) gst_mpegts_section_get_scte_sit (sit_section);
  fail_if (sit == NULL);
  /* Check the values */
  fail_unless (sit->encrypted_packet == FALSE);
  fail_unless (sit->pts_adjustment == 0x1fedcba12);
  fail_unless (sit->tier == 123);
  fail_unless (sit->pts_adjustment == 0x1fedcba12);
  fail_unless (sit->splice_command_type == GST_MTS_SCTE_SPLICE_COMMAND_INSERT);

  event = g_ptr_array_index (sit->splices, 0);
  fail_unless (event->insert_event == TRUE);
  fail_unless (event->splice_event_id == 4285);
  fail_unless (event->program_splice_flag == TRUE);
  fail_unless (event->duration_flag == TRUE);
  fail_unless (event->splice_immediate_flag == FALSE);

  fail_unless (event->program_splice_time_specified == TRUE);
  fail_unless (event->program_splice_time == 0x1fdecba12);

  fail_unless (event->break_duration_auto_return == TRUE);
  fail_unless (event->break_duration == 590000);
  fail_unless (event->unique_program_id == 4256);
  fail_unless (event->avail_num == 2);
  fail_unless (event->avails_expected == 2);


  gst_mpegts_section_unref (sit_section);


}

GST_END_TEST;

GST_START_TEST (test_mpegts_pat)
{
  GstMpegtsPatProgram *program;
  GPtrArray *pat;
  GstMpegtsSection *pat_section;
  gint i;
  guint8 *data;
  gsize data_size;

  /* Check creation of PAT */
  pat = gst_mpegts_pat_new ();

  for (i = 0; i < 2; i++) {
    program = gst_mpegts_pat_program_new ();

    program->program_number = i;
    program->network_or_program_map_PID = 0x30 + i;

    g_ptr_array_add (pat, program);
  }

  pat_section = gst_mpegts_section_from_pat (pat, 0);
  fail_if (pat_section == NULL);

  /* Re-parse the PAT from section */
  pat = gst_mpegts_section_get_pat (pat_section);

  fail_unless (pat->len == 2);

  for (i = 0; i < 2; i++) {
    program = g_ptr_array_index (pat, i);

    assert_equals_int (program->program_number, i);
    assert_equals_int (program->network_or_program_map_PID, 0x30 + i);
  }
  g_ptr_array_unref (pat);
  pat = NULL;

  /* Packetize the section, and check the data integrity */
  data = gst_mpegts_section_packetize (pat_section, &data_size);
  fail_if (data == NULL);

  for (i = 0; i < data_size; i++) {
    if (data[i] != pat_data_check[i])
      fail ("0x%X != 0x%X in byte %d of PAT section", data[i],
          pat_data_check[i], i);
  }

  /* Check assertion on bad CRC. Reset parsed data, and make the CRC corrupt */
  pat_section->data[pat_section->section_length - 1]++;
  pat_section->destroy_parsed (pat_section->cached_parsed);
  pat_section->cached_parsed = NULL;

  pat = gst_mpegts_section_get_pat (pat_section);
  fail_unless (pat == NULL);

  gst_mpegts_section_unref (pat_section);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_pmt)
{
  GstMpegtsPMT *pmt;
  GstMpegtsPMTStream *stream;
  GstMpegtsDescriptor *desc;
  GstMpegtsSection *pmt_section;
  guint8 *data;
  gsize data_size;
  gint i;

  /* Check creation of PMT */
  pmt = gst_mpegts_pmt_new ();

  pmt->pcr_pid = 0x1FFF;
  pmt->program_number = 1;

  desc = gst_mpegts_descriptor_from_registration ("HDMV", NULL, 0);
  g_ptr_array_add (pmt->descriptors, desc);

  for (i = 0; i < 2; i++) {
    stream = gst_mpegts_pmt_stream_new ();

    stream->stream_type = GST_MPEGTS_STREAM_TYPE_VIDEO_H264;
    stream->pid = 0x40 + i;

    desc = gst_mpegts_descriptor_from_registration ("HDMV", NULL, 0);

    g_ptr_array_add (stream->descriptors, desc);
    g_ptr_array_add (pmt->streams, stream);
  }

  pmt_section = gst_mpegts_section_from_pmt (pmt, 0x30);
  fail_if (pmt_section == NULL);

  /* Re-parse the PMT from section */
  pmt = (GstMpegtsPMT *) gst_mpegts_section_get_pmt (pmt_section);

  fail_unless (pmt->pcr_pid == 0x1FFF);
  fail_unless (pmt->program_number == 1);
  fail_unless (pmt->descriptors->len == 1);
  fail_unless (pmt->streams->len == 2);

  desc = (GstMpegtsDescriptor *) gst_mpegts_find_descriptor (pmt->descriptors,
      GST_MTS_DESC_REGISTRATION);
  fail_if (desc == NULL);

  for (i = 0; i < 2; i++) {
    stream = g_ptr_array_index (pmt->streams, i);

    fail_unless (stream->stream_type == GST_MPEGTS_STREAM_TYPE_VIDEO_H264);
    fail_unless (stream->pid == 0x40 + i);
    fail_unless (stream->descriptors->len == 1);

    desc =
        (GstMpegtsDescriptor *) gst_mpegts_find_descriptor (stream->descriptors,
        GST_MTS_DESC_REGISTRATION);
    fail_if (desc == NULL);
  }

  /* Packetize the section, and check data integrity */
  data = gst_mpegts_section_packetize (pmt_section, &data_size);

  for (i = 0; i < data_size; i++) {
    if (data[i] != pmt_data_check[i])
      fail ("0x%X != 0x%X in byte %d of PMT section", data[i],
          pmt_data_check[i], i);
  }

  /* Check assertion on bad CRC. Reset parsed data, and make the CRC corrupt */
  pmt_section->data[pmt_section->section_length - 1]++;
  pmt_section->destroy_parsed (pmt_section->cached_parsed);
  pmt_section->cached_parsed = NULL;
  pmt = (GstMpegtsPMT *) gst_mpegts_section_get_pmt (pmt_section);

  fail_unless (pmt == NULL);

  gst_mpegts_section_unref (pmt_section);
}

GST_END_TEST;


GST_START_TEST (test_mpegts_nit)
{
  GstMpegtsNITStream *stream;
  GstMpegtsNIT *nit;
  GstMpegtsDescriptor *desc;
  GstMpegtsSection *nit_section;
  gchar *name;
  guint8 *data;
  gsize data_size;
  gint i;

  /* Check creation of NIT */
  nit = gst_mpegts_nit_new ();

  nit->actual_network = TRUE;
  nit->network_id = 0x1FFF;

  desc = gst_mpegts_descriptor_from_dvb_network_name ("Network name");

  fail_if (desc == NULL);

  g_ptr_array_add (nit->descriptors, desc);

  for (i = 0; i < 2; i++) {
    stream = gst_mpegts_nit_stream_new ();
    stream->transport_stream_id = 0x1FFF;
    stream->original_network_id = 0x1FFE;

    desc = gst_mpegts_descriptor_from_dvb_network_name ("Another network");

    g_ptr_array_add (stream->descriptors, desc);
    g_ptr_array_add (nit->streams, stream);
  }

  nit_section = gst_mpegts_section_from_nit (nit);
  fail_if (nit_section == NULL);

  /* Re-parse the NIT from section */
  nit = (GstMpegtsNIT *) gst_mpegts_section_get_nit (nit_section);

  fail_unless (nit->descriptors->len == 1);
  fail_unless (nit->streams->len == 2);
  fail_unless (nit->actual_network == TRUE);
  fail_unless (nit->network_id == 0x1FFF);

  desc = (GstMpegtsDescriptor *) gst_mpegts_find_descriptor (nit->descriptors,
      GST_MTS_DESC_DVB_NETWORK_NAME);

  fail_if (desc == NULL);

  fail_unless (gst_mpegts_descriptor_parse_dvb_network_name (desc,
          &name) == TRUE);

  g_free (name);

  for (i = 0; i < 2; i++) {
    stream = g_ptr_array_index (nit->streams, i);

    fail_unless (stream->transport_stream_id == 0x1FFF);
    fail_unless (stream->original_network_id == 0x1FFE);
    fail_unless (stream->descriptors->len == 1);

    desc =
        (GstMpegtsDescriptor *) gst_mpegts_find_descriptor (stream->descriptors,
        GST_MTS_DESC_DVB_NETWORK_NAME);

    fail_unless (gst_mpegts_descriptor_parse_dvb_network_name (desc,
            &name) == TRUE);
    g_free (name);
  }

  /* Packetize the section, and check data integrity */
  data = gst_mpegts_section_packetize (nit_section, &data_size);

  fail_if (data == NULL);

  if (_has_iso6937_iconv ()) {
    for (i = 0; i < data_size; i++) {
      if (data[i] != nit_data_check[i])
        fail ("0x%X != 0x%X in byte %d of NIT section", data[i],
            nit_data_check[i], i);
    }
  }

  /* Check assertion on bad CRC. Reset parsed data, and make the CRC corrupt */
  nit_section->data[nit_section->section_length - 1]++;
  nit_section->destroy_parsed (nit_section->cached_parsed);
  nit_section->cached_parsed = NULL;
  nit = (GstMpegtsNIT *) gst_mpegts_section_get_nit (nit_section);

  fail_unless (nit == NULL);

  gst_mpegts_section_unref (nit_section);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_sdt)
{
  GstMpegtsSDTService *service;
  GstMpegtsSDT *sdt;
  GstMpegtsDescriptor *desc;
  GstMpegtsSection *sdt_section;
  guint8 *data;
  gsize data_size;
  gint i;

  /* Check creation of SDT */
  sdt = gst_mpegts_sdt_new ();

  sdt->actual_ts = TRUE;
  sdt->original_network_id = 0x1FFF;
  sdt->transport_stream_id = 0x1FFF;

  for (i = 0; i < 2; i++) {
    service = gst_mpegts_sdt_service_new ();
    service->service_id = i;
    service->EIT_schedule_flag = TRUE;
    service->EIT_present_following_flag = TRUE;
    service->running_status = GST_MPEGTS_RUNNING_STATUS_RUNNING + i;
    service->free_CA_mode = TRUE;

    desc = gst_mpegts_descriptor_from_dvb_service
        (GST_DVB_SERVICE_DIGITAL_TELEVISION, "Name", "Provider");

    g_ptr_array_add (service->descriptors, desc);
    g_ptr_array_add (sdt->services, service);
  }

  sdt_section = gst_mpegts_section_from_sdt (sdt);
  fail_if (sdt_section == NULL);

  /* Re-parse the SDT from section */
  sdt = (GstMpegtsSDT *) gst_mpegts_section_get_sdt (sdt_section);

  fail_unless (sdt->services->len == 2);
  fail_unless (sdt->actual_ts == TRUE);
  fail_unless (sdt->original_network_id == 0x1FFF);
  fail_unless (sdt->transport_stream_id == 0x1FFF);

  for (i = 0; i < 2; i++) {
    service = g_ptr_array_index (sdt->services, i);

    fail_if (service == NULL);
    fail_unless (service->descriptors->len == 1);
    fail_unless (service->service_id == i);
    fail_unless (service->EIT_schedule_flag == TRUE);
    fail_unless (service->EIT_present_following_flag == TRUE);
    fail_unless (service->running_status ==
        GST_MPEGTS_RUNNING_STATUS_RUNNING + i);
    fail_unless (service->free_CA_mode == TRUE);

    desc = (GstMpegtsDescriptor *)
        gst_mpegts_find_descriptor (service->descriptors,
        GST_MTS_DESC_DVB_SERVICE);

    fail_if (desc == NULL);

    fail_unless (gst_mpegts_descriptor_parse_dvb_service (desc,
            NULL, NULL, NULL) == TRUE);
  }

  /* Packetize the section, and check data integrity */
  data = gst_mpegts_section_packetize (sdt_section, &data_size);

  fail_if (data == NULL);

  if (_has_iso6937_iconv ()) {
    for (i = 0; i < data_size; i++) {
      if (data[i] != sdt_data_check[i])
        fail ("0x%X != 0x%X in byte %d of SDT section", data[i],
            sdt_data_check[i], i);
    }
  }

  /* Check assertion on bad CRC. Reset parsed data, and make the CRC corrupt */
  sdt_section->data[sdt_section->section_length - 1]++;
  sdt_section->destroy_parsed (sdt_section->cached_parsed);
  sdt_section->cached_parsed = NULL;
  sdt = (GstMpegtsSDT *) gst_mpegts_section_get_sdt (sdt_section);

  fail_unless (sdt == NULL);

  gst_mpegts_section_unref (sdt_section);
}

GST_END_TEST;

GST_START_TEST (test_mpegts_atsc_stt)
{
  const GstMpegtsAtscSTT *stt;
  GstMpegtsSection *section;
  guint8 *data;
  GstDateTime *dt;

  data = g_memdup2 (stt_data_check, 20);

  section = gst_mpegts_section_new (0x1ffb, data, 20);
  stt = gst_mpegts_section_get_atsc_stt (section);
  fail_if (stt == NULL);

  fail_unless (stt->protocol_version == 0);
  fail_unless (stt->system_time == 0x23b4e65c);
  fail_unless (stt->gps_utc_offset == 12);
  fail_unless (stt->ds_status == 1);
  fail_unless (stt->ds_dayofmonth == 0);
  fail_unless (stt->ds_hour == 0);

  dt = gst_mpegts_atsc_stt_get_datetime_utc ((GstMpegtsAtscSTT *) stt);
  fail_unless (gst_date_time_get_day (dt) == 30);
  fail_unless (gst_date_time_get_month (dt) == 12);
  fail_unless (gst_date_time_get_year (dt) == 1998);
  fail_unless (gst_date_time_get_hour (dt) == 13);
  fail_unless (gst_date_time_get_minute (dt) == 0);
  fail_unless (gst_date_time_get_second (dt) == 0);

  gst_date_time_unref (dt);
  gst_mpegts_section_unref (section);
}

GST_END_TEST;


static const guint8 registration_descriptor[] = {
  0x05, 0x04, 0x48, 0x44, 0x4d, 0x56
};

GST_START_TEST (test_mpegts_descriptors)
{
  GstMpegtsDescriptor *desc;
  guint i;

  /*
   * Registration descriptor (0x05)
   */

  /* Check creation of descriptor */
  desc = gst_mpegts_descriptor_from_registration ("HDMV", NULL, 0);
  fail_if (desc == NULL);
  fail_unless (desc->length == 4);
  fail_unless (desc->tag == 0x05);
  for (i = 0; i < 6; i++) {
    if (registration_descriptor[i] != desc->data[i])
      fail ("0x%X != 0x%X in byte %d of registration descriptor",
          desc->data[i], registration_descriptor[i], i);
  }
  gst_mpegts_descriptor_free (desc);
}

GST_END_TEST;

static const guint8 network_name_descriptor[] = {
  0x40, 0x04, 0x4e, 0x61, 0x6d, 0x65
};

static const guint8 service_descriptor[] = {
  0x48, 0x0f, 0x01, 0x08, 0x50, 0x72, 0x6f,
  0x76, 0x69, 0x64, 0x65, 0x72, 0x04, 0x4e,
  0x61, 0x6d, 0x65
};

GST_START_TEST (test_mpegts_dvb_descriptors)
{
  GstMpegtsDescriptor *desc;
  GstMpegtsDVBServiceType service_type;
  gchar *string, *provider;
  gchar long_string[257];
  gboolean ret;
  guint i;

  /*
   * Network name descriptor (0x40)
   */

  /* Check creation of descriptor */
  desc = gst_mpegts_descriptor_from_dvb_network_name ("Name");
  fail_if (desc == NULL);
  if (_has_iso6937_iconv ())
    fail_unless (desc->length == 4);
  fail_unless (desc->tag == 0x40);

  if (_has_iso6937_iconv ()) {
    for (i = 0; i < 6; i++) {
      if (desc->data[i] != network_name_descriptor[i])
        fail ("0x%X != 0x%X in byte %d of network name descriptor",
            desc->data[i], network_name_descriptor[i], i);
    }
  }

  /* Check parsing of descriptor */
  ret = gst_mpegts_descriptor_parse_dvb_network_name (desc, &string);
  fail_unless (ret == TRUE);
  fail_unless (strcmp (string, "Name") == 0);
  g_free (string);
  gst_mpegts_descriptor_free (desc);

  /* Descriptor should fail if string is more than 255 bytes */
  memset (long_string, 0x41, 256);
  long_string[256] = 0x00;
  fail_if (gst_mpegts_descriptor_from_dvb_network_name (long_string) != NULL);

  /*
   * Service descriptor (0x48)
   */

  /* Check creation of descriptor with data */
  desc = gst_mpegts_descriptor_from_dvb_service
      (GST_DVB_SERVICE_DIGITAL_TELEVISION, "Name", "Provider");
  fail_if (desc == NULL);
  if (_has_iso6937_iconv ())
    fail_unless (desc->length == 15);
  fail_unless (desc->tag == 0x48);

  if (_has_iso6937_iconv ()) {
    for (i = 0; i < 17; i++) {
      if (desc->data[i] != service_descriptor[i])
        fail ("0x%X != 0x%X in byte %d of service descriptor",
            desc->data[i], service_descriptor[i], i);
    }
  }

  /* Check parsing of descriptor with data */
  ret = gst_mpegts_descriptor_parse_dvb_service
      (desc, &service_type, &string, &provider);
  fail_unless (ret == TRUE);
  fail_unless (service_type == GST_DVB_SERVICE_DIGITAL_TELEVISION);
  fail_unless (strcmp (string, "Name") == 0);
  fail_unless (strcmp (provider, "Provider") == 0);
  g_free (string);
  g_free (provider);
  gst_mpegts_descriptor_free (desc);

  /* Check creation of descriptor without data */
  desc = gst_mpegts_descriptor_from_dvb_service
      (GST_DVB_SERVICE_DIGITAL_TELEVISION, NULL, NULL);
  fail_if (desc == NULL);
  fail_unless (desc->length == 3);
  fail_unless (desc->tag == 0x48);

  /* Check parsing of descriptor without data */
  ret = gst_mpegts_descriptor_parse_dvb_service (desc, NULL, NULL, NULL);
  fail_unless (ret == TRUE);
  gst_mpegts_descriptor_free (desc);

  /* Descriptor should fail if string is more than 255 bytes */
  memset (long_string, 0x41, 256);
  long_string[256] = 0x00;
  desc =
      gst_mpegts_descriptor_from_dvb_service
      (GST_DVB_SERVICE_DIGITAL_TELEVISION, long_string, NULL);
  fail_if (desc != NULL);
  desc =
      gst_mpegts_descriptor_from_dvb_service
      (GST_DVB_SERVICE_DIGITAL_TELEVISION, NULL, long_string);
  fail_if (desc != NULL);
}

GST_END_TEST;

static Suite *
mpegts_suite (void)
{
  Suite *s = suite_create ("MPEG Transport Stream helper library");

  TCase *tc_chain = tcase_create ("general");

  gst_mpegts_initialize ();

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_scte_sit);
  tcase_add_test (tc_chain, test_mpegts_pat);
  tcase_add_test (tc_chain, test_mpegts_pmt);
  tcase_add_test (tc_chain, test_mpegts_nit);
  tcase_add_test (tc_chain, test_mpegts_sdt);
  tcase_add_test (tc_chain, test_mpegts_atsc_stt);
  tcase_add_test (tc_chain, test_mpegts_descriptors);
  tcase_add_test (tc_chain, test_mpegts_dvb_descriptors);

  return s;
}

GST_CHECK_MAIN (mpegts);
