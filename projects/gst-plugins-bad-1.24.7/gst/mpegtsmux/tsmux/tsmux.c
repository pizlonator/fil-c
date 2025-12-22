/*
 * Copyright 2006 BBC and Fluendo S.A.
 *
 * This library is licensed under 3 different licenses and you
 * can choose to use it under the terms of any one of them. The
 * three licenses are the MPL 1.1, the LGPL, and the MIT license.
 *
 * MPL:
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * LGPL:
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
 *
 * MIT:
 *
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MPL-1.1 OR MIT OR LGPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/mpegts/mpegts.h>

#include "tsmux.h"
#include "tsmuxstream.h"

#define GST_CAT_DEFAULT gst_base_ts_mux_debug

/* Maximum total data length for a PAT section is 1024 bytes, minus an
 * 8 byte header, then the length of each program entry is 32 bits,
 * then finally a 32 bit CRC. Thus the maximum number of programs in this mux
 * is (1024 - 8 - 4) / 4 = 253 because it only supports single section PATs */
#define TSMUX_MAX_PROGRAMS 253

#define TSMUX_SECTION_HDR_SIZE 8

#define TSMUX_DEFAULT_NETWORK_ID 0x0001
#define TSMUX_DEFAULT_TS_ID 0x0001

/* The last byte of the PCR in the header defines the byte position
 * at which PCR should be calculated */
#define PCR_BYTE_OFFSET 11

/* HACK: We use a fixed buffering offset for the PCR at the moment -
 * this is the amount 'in advance' of the stream that the PCR sits.
 * 1/8 second atm */
#define TSMUX_PCR_OFFSET (TSMUX_CLOCK_FREQ / 8)

/* Base for all written PCR and DTS/PTS,
 * so we have some slack to go backwards */
#define CLOCK_BASE (TSMUX_CLOCK_FREQ * 10 * 360)

static gboolean tsmux_write_pat (TsMux * mux);
static gboolean tsmux_write_pmt (TsMux * mux, TsMuxProgram * program);
static gboolean tsmux_write_scte_null (TsMux * mux, TsMuxProgram * program);
static gint64 get_next_pcr (TsMux * mux, gint64 cur_ts);
static gint64 get_current_pcr (TsMux * mux, gint64 cur_ts);
static gint64 write_new_pcr (TsMux * mux, TsMuxStream * stream, gint64 cur_pcr,
    gint64 next_pcr);
static gboolean tsmux_write_ts_header (TsMux * mux, guint8 * buf,
    TsMuxPacketInfo * pi, guint stream_avail, guint * payload_len_out,
    guint * payload_offset_out);

static void
tsmux_section_free (TsMuxSection * section)
{
  gst_mpegts_section_unref (section->section);
  g_free (section);
}

static TsMuxStream *
tsmux_new_stream_default (guint16 pid, guint stream_type, guint stream_number,
    gpointer user_data)
{
  return tsmux_stream_new (pid, stream_type, stream_number);
}

/**
 * tsmux_new:
 *
 * Create a new muxer session.
 *
 * Returns: A new #TsMux object.
 */
TsMux *
tsmux_new (void)
{
  TsMux *mux;

  mux = g_new0 (TsMux, 1);

  mux->transport_id = TSMUX_DEFAULT_TS_ID;

  mux->next_pgm_no = TSMUX_START_PROGRAM_ID;
  mux->next_pmt_pid = TSMUX_START_PMT_PID;
  mux->next_stream_pid = TSMUX_START_ES_PID;

  mux->pat_changed = TRUE;
  mux->next_pat_pcr = -1;
  mux->pat_interval = TSMUX_DEFAULT_PAT_INTERVAL;

  mux->si_changed = TRUE;
  mux->si_interval = TSMUX_DEFAULT_SI_INTERVAL;

  mux->pcr_interval = TSMUX_DEFAULT_PCR_INTERVAL;

  mux->next_si_pcr = -1;

  mux->si_sections = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) tsmux_section_free);

  mux->new_stream_func = tsmux_new_stream_default;
  mux->new_stream_data = NULL;

  mux->first_pcr_ts = G_MININT64;

  return mux;
}

/**
 * tsmux_set_write_func:
 * @mux: a #TsMux
 * @func: a user callback function
 * @user_data: user data passed to @func
 *
 * Set the callback function and user data to be called when @mux has output to
 * produce. @user_data will be passed as user data in @func.
 */
void
tsmux_set_write_func (TsMux * mux, TsMuxWriteFunc func, void *user_data)
{
  g_return_if_fail (mux != NULL);

  mux->write_func = func;
  mux->write_func_data = user_data;
}

/**
 * tsmux_set_alloc_func:
 * @mux: a #TsMux
 * @func: a user callback function
 * @user_data: user data passed to @func
 *
 * Set the callback function and user data to be called when @mux needs
 * a new buffer to write a packet into.
 * @user_data will be passed as user data in @func.
 */
void
tsmux_set_alloc_func (TsMux * mux, TsMuxAllocFunc func, void *user_data)
{
  g_return_if_fail (mux != NULL);

  mux->alloc_func = func;
  mux->alloc_func_data = user_data;
}

/**
 * tsmux_set_new_stream_func:
 * @mux: a #TsMux
 * @func: a user callback function
 * @user_data: user data passed to @func
 *
 * Set the callback function and user data to be called when @mux needs
 * to create a new stream.
 * @user_data will be passed as user data in @func.
 */
void
tsmux_set_new_stream_func (TsMux * mux, TsMuxNewStreamFunc func,
    void *user_data)
{
  g_return_if_fail (mux != NULL);

  mux->new_stream_func = func;
  mux->new_stream_data = user_data;
}

/**
 * tsmux_set_pat_interval:
 * @mux: a #TsMux
 * @freq: a new PAT interval
 *
 * Set the interval (in cycles of the 90kHz clock) for writing out the PAT table.
 *
 * Many transport stream clients might have problems if the PAT table is not
 * inserted in the stream at regular intervals, especially when initially trying
 * to figure out the contents of the stream.
 */
void
tsmux_set_pat_interval (TsMux * mux, guint freq)
{
  g_return_if_fail (mux != NULL);

  mux->pat_interval = freq;
}

/**
 * tsmux_set_pcr_interval:
 * @mux: a #TsMux
 * @freq: a new PCR interval
 *
 * Set the interval (in cycles of the 90kHz clock) for writing the PCR.
 */
void
tsmux_set_pcr_interval (TsMux * mux, guint freq)
{
  g_return_if_fail (mux != NULL);

  mux->pcr_interval = freq;
}

/**
 * tsmux_get_pat_interval:
 * @mux: a #TsMux
 *
 * Get the configured PAT interval. See also tsmux_set_pat_interval().
 *
 * Returns: the configured PAT interval
 */
guint
tsmux_get_pat_interval (TsMux * mux)
{
  g_return_val_if_fail (mux != NULL, 0);

  return mux->pat_interval;
}

/**
 * tsmux_resend_pat:
 * @mux: a #TsMux
 *
 * Resends the PAT before the next stream packet.
 */
void
tsmux_resend_pat (TsMux * mux)
{
  g_return_if_fail (mux != NULL);

  mux->next_pat_pcr = -1;
}

/**
 * tsmux_set_si_interval:
 * @mux: a #TsMux
 * @freq: a new SI table interval
 *
 * Set the interval (in cycles of the 90kHz clock) for writing out the SI tables.
 *
 */
void
tsmux_set_si_interval (TsMux * mux, guint freq)
{
  g_return_if_fail (mux != NULL);

  mux->si_interval = freq;
}

/**
 * tsmux_get_si_interval:
 * @mux: a #TsMux
 *
 * Get the configured SI table interval. See also tsmux_set_si_interval().
 *
 * Returns: the configured SI interval
 */
guint
tsmux_get_si_interval (TsMux * mux)
{
  g_return_val_if_fail (mux != NULL, 0);

  return mux->si_interval;
}

/**
 * tsmux_resend_si:
 * @mux: a #TsMux
 *
 * Resends the SI tables before the next stream packet.
 *
 */
void
tsmux_resend_si (TsMux * mux)
{
  g_return_if_fail (mux != NULL);

  mux->next_si_pcr = -1;
}

/**
 * tsmux_add_mpegts_si_section:
 * @mux: a #TsMux
 * @section: (transfer full): a #GstMpegtsSection to add
 *
 * Add a Service Information #GstMpegtsSection to the stream
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean
tsmux_add_mpegts_si_section (TsMux * mux, GstMpegtsSection * section)
{
  TsMuxSection *tsmux_section;

  g_return_val_if_fail (mux != NULL, FALSE);
  g_return_val_if_fail (section != NULL, FALSE);
  g_return_val_if_fail (mux->si_sections != NULL, FALSE);

  tsmux_section = g_new0 (TsMuxSection, 1);

  GST_DEBUG ("Adding mpegts section with type %d to mux",
      section->section_type);

  tsmux_section->section = section;
  tsmux_section->pi.pid = section->pid;

  g_hash_table_insert (mux->si_sections,
      GINT_TO_POINTER (section->section_type), tsmux_section);

  mux->si_changed = TRUE;

  return TRUE;
}


/**
 * tsmux_free:
 * @mux: a #TsMux
 *
 * Free all resources associated with @mux. After calling this function @mux can
 * not be used anymore.
 */
void
tsmux_free (TsMux * mux)
{
  GList *cur;

  g_return_if_fail (mux != NULL);

  /* Free PAT section */
  if (mux->pat.section)
    gst_mpegts_section_unref (mux->pat.section);

  /* Free all programs */
  for (cur = mux->programs; cur; cur = cur->next) {
    TsMuxProgram *program = (TsMuxProgram *) cur->data;

    tsmux_program_free (program);
  }
  g_list_free (mux->programs);

  /* Free all streams */
  for (cur = mux->streams; cur; cur = cur->next) {
    TsMuxStream *stream = (TsMuxStream *) cur->data;

    tsmux_stream_free (stream);
  }
  g_list_free (mux->streams);

  /* Free SI table sections */
  g_hash_table_unref (mux->si_sections);

  g_free (mux);
}

static gint
tsmux_program_compare (TsMuxProgram * program, gint * needle)
{
  return (program->pgm_number - *needle);
}

/**
 * tsmux_program_new:
 * @mux: a #TsMux
 *
 * Create a new program in the missing session @mux.
 *
 * Returns: a new #TsMuxProgram or %NULL when the maximum number of programs has
 * been reached.
 */
TsMuxProgram *
tsmux_program_new (TsMux * mux, gint prog_id)
{
  TsMuxProgram *program;

  g_return_val_if_fail (mux != NULL, NULL);

  /* Ensure we have room for another program */
  if (mux->nb_programs == TSMUX_MAX_PROGRAMS)
    return NULL;

  program = g_new0 (TsMuxProgram, 1);

  program->pmt_changed = TRUE;
  program->pmt_interval = TSMUX_DEFAULT_PMT_INTERVAL;

  program->next_pmt_pcr = -1;
  program->next_pcr = -1;

  if (prog_id == 0) {
    program->pgm_number = mux->next_pgm_no++;
    while (g_list_find_custom (mux->programs, &program->pgm_number,
            (GCompareFunc) tsmux_program_compare) != NULL) {
      program->pgm_number = mux->next_pgm_no++;
    }
  } else {
    program->pgm_number = prog_id;
    while (g_list_find_custom (mux->programs, &program->pgm_number,
            (GCompareFunc) tsmux_program_compare) != NULL) {
      program->pgm_number++;
    }
  }

  program->pmt_pid = mux->next_pmt_pid++;
  program->pcr_stream = NULL;
  program->pcr_pid = 0;

  /* SCTE35 is disabled by default */
  program->scte35_pid = 0;
  program->scte35_null_interval = TSMUX_DEFAULT_SCTE_35_NULL_INTERVAL;
  program->next_scte35_pcr = -1;

  /* mux->streams owns the streams */
  program->streams = g_ptr_array_new_full (1, NULL);

  mux->programs = g_list_prepend (mux->programs, program);
  mux->nb_programs++;
  mux->pat_changed = TRUE;

  return program;
}

gboolean
tsmux_program_delete (TsMux * mux, TsMuxProgram * program)
{
  g_return_val_if_fail (mux != NULL, FALSE);

  if (mux->nb_programs == 0)
    return FALSE;

  if (!program)
    return FALSE;

  mux->programs = g_list_remove (mux->programs, program);
  mux->nb_programs--;
  mux->pat_changed = TRUE;
  tsmux_program_free ((TsMuxProgram *) program);

  return TRUE;
}

/**
 * tsmux_set_pmt_interval:
 * @program: a #TsMuxProgram
 * @freq: a new PMT interval
 *
 * Set the interval (in cycles of the 90kHz clock) for writing out the PMT table.
 *
 * Many transport stream clients might have problems if the PMT table is not
 * inserted in the stream at regular intervals, especially when initially trying
 * to figure out the contents of the stream.
 */
void
tsmux_set_pmt_interval (TsMuxProgram * program, guint freq)
{
  g_return_if_fail (program != NULL);

  program->pmt_interval = freq;
}

/**
 * tsmux_get_pmt_interval:
 * @program: a #TsMuxProgram
 *
 * Get the configured PMT interval. See also tsmux_set_pmt_interval().
 *
 * Returns: the configured PMT interval
 */
guint
tsmux_get_pmt_interval (TsMuxProgram * program)
{
  g_return_val_if_fail (program != NULL, 0);

  return program->pmt_interval;
}

/**
 * tsmux_program_set_scte35_interval:
 * @program: a #TsMuxProgram
 * @freq: a new SCTE-35 NULL interval
 *
 * Set the interval (in cycles of the 90kHz clock) for sending out the SCTE-35
 * NULL command. This is only effective is the SCTE-35 PID is not 0.
 */
void
tsmux_program_set_scte35_interval (TsMuxProgram * program, guint interval)
{
  g_return_if_fail (program != NULL);

  program->scte35_null_interval = interval;
}

/**
 * tsmux_resend_pmt:
 * @program: a #TsMuxProgram
 *
 * Resends the PMT before the next stream packet.
 */
void
tsmux_resend_pmt (TsMuxProgram * program)
{
  g_return_if_fail (program != NULL);

  program->next_pmt_pcr = -1;
}

/**
 * tsmux_program_set_scte35_pid:
 * @program: a #TsMuxProgram
 * @pid: The pid to use, or 0 to deactivate usage.
 *
 * Set the @pid to use for sending SCTE-35 packets on the given
 * @program.
 *
 * This needs to be called as early as possible if SCTE-35 sections
 * are even going to be used with the given @program so that the PMT
 * can be properly configured.
 */
void
tsmux_program_set_scte35_pid (TsMuxProgram * program, guint16 pid)
{
  TsMuxSection *section;
  GstMpegtsSCTESIT *sit;
  g_return_if_fail (program != NULL);

  program->scte35_pid = pid;
  /* Create/Update the section */
  if (program->scte35_null_section) {
    tsmux_section_free (program->scte35_null_section);
    program->scte35_null_section = NULL;
  }
  if (pid != 0) {
    program->scte35_null_section = section = g_new0 (TsMuxSection, 1);
    section->pi.pid = pid;
    sit = gst_mpegts_scte_null_new ();
    section->section = gst_mpegts_section_from_scte_sit (sit, pid);
  }
}

/**
 * tsmux_program_get_scte35_pid:
 * @program: a #TsMuxProgram
 *
 * Get the PID configured for sending SCTE-35 packets.
 *
 * Returns: the configured SCTE-35 PID, or 0 if not active.
 */
guint16
tsmux_program_get_scte35_pid (TsMuxProgram * program)
{
  g_return_val_if_fail (program != NULL, 0);

  return program->scte35_pid;
}

/**
 * tsmux_program_add_stream:
 * @program: a #TsMuxProgram
 * @stream: a #TsMuxStream
 *
 * Add @stream to @program.
 */
void
tsmux_program_add_stream (TsMuxProgram * program, TsMuxStream * stream)
{
  GPtrArray *streams;
  guint i;
  gint pmt_index, array_index = -1 /* append */ ;
  guint16 pid;

  g_return_if_fail (program != NULL);
  g_return_if_fail (stream != NULL);

  streams = program->streams;
  pmt_index = stream->pmt_index;
  pid = tsmux_stream_get_pid (stream);

  if (pmt_index >= 0) {
    /* Insert into streams with known indices */
    for (i = 0; i < streams->len; i++) {
      TsMuxStream *s = g_ptr_array_index (streams, i);

      if (s->pmt_index < 0 || pmt_index < s->pmt_index) {
        array_index = i;
        GST_DEBUG ("PID 0x%04x: Using known-order index %d/%u",
            pid, array_index, streams->len);
        break;
      }
    }
  } else {
    /* Insert after streams with known indices, sorted by PID */
    for (i = 0; i < streams->len; i++) {
      TsMuxStream *s = g_ptr_array_index (streams, i);

      if (s->pmt_index < 0 && pid < tsmux_stream_get_pid (s)) {
        array_index = i;
        GST_DEBUG ("PID 0x%04x: Using PID-order index %d/%u",
            pid, array_index, streams->len);
        break;
      }
    }
  }

  g_ptr_array_insert (streams, array_index, stream);
  program->pmt_changed = TRUE;
}

/**
 * tsmux_program_set_pcr_stream:
 * @program: a #TsMuxProgram
 * @stream: a #TsMuxStream
 *
 * Set @stream as the PCR stream for @program, overwriting the previously
 * configured PCR stream. When @stream is NULL, program will have no PCR stream
 * configured.
 */
void
tsmux_program_set_pcr_stream (TsMuxProgram * program, TsMuxStream * stream)
{
  g_return_if_fail (program != NULL);

  if (program->pcr_stream == stream)
    return;

  program->pcr_pid = 0;
  if (program->pcr_stream != NULL)
    tsmux_stream_pcr_unref (program->pcr_stream);
  if (stream)
    tsmux_stream_pcr_ref (stream);
  program->pcr_stream = stream;

  program->pmt_changed = TRUE;
}

/**
 * tsmux_program_set_pcr_pid:
 * @program: a #TsMuxProgram
 * @pid: a PID
 *
 * Set @pid as the PCR PID for @program, overwriting the previously
 * configured PCR PID. When pid == 0, program will have no PCR PID configured.
 */
void
tsmux_program_set_pcr_pid (TsMuxProgram * program, guint16 pid)
{
  g_return_if_fail (program != NULL);

  program->pcr_pid = pid;
  program->pmt_changed = TRUE;
}

/**
 * tsmux_get_new_pid:
 * @mux: a #TsMux
 *
 * Get a new free PID.
 *
 * Returns: a new free PID.
 */
guint16
tsmux_get_new_pid (TsMux * mux)
{
  g_return_val_if_fail (mux != NULL, -1);

  /* make sure this PID is free
   * (and not taken by a specific earlier request) */
  do {
    mux->next_stream_pid++;
  } while (tsmux_find_stream (mux, mux->next_stream_pid));

  return mux->next_stream_pid;
}

/**
 * tsmux_create_stream:
 * @mux: a #TsMux
 * @stream_type: the stream type
 * @stream_number: stream number
 * @pid: the PID of the new stream.
 *
 * Create a new stream of @stream_type with @stream_number in the muxer session @mux.
 *
 * When @pid is set to #TSMUX_PID_AUTO, a new free PID will automatically
 * be allocated for the new stream.
 *
 * Returns: a new #TsMuxStream.
 */
TsMuxStream *
tsmux_create_stream (TsMux * mux, guint stream_type, guint stream_number,
    guint16 pid, gchar * language, guint bitrate, guint max_bitrate)
{
  TsMuxStream *stream;
  guint16 new_pid;

  g_return_val_if_fail (mux != NULL, NULL);
  g_return_val_if_fail (mux->new_stream_func != NULL, NULL);

  if (pid == TSMUX_PID_AUTO) {
    new_pid = tsmux_get_new_pid (mux);
  } else {
    new_pid = pid & 0x1FFF;
  }

  /* Ensure we're not creating a PID collision */
  if (tsmux_find_stream (mux, new_pid))
    return NULL;

  stream =
      mux->new_stream_func (new_pid, stream_type, stream_number,
      mux->new_stream_data);

  mux->streams = g_list_prepend (mux->streams, stream);
  mux->nb_streams++;

  if (language) {
    strncpy (stream->language, language, 4);
    stream->language[3] = 0;
  } else {
    stream->language[0] = 0;
  }

  stream->max_bitrate = max_bitrate;
  /* ignored if it's not audio */
  stream->audio_bitrate = bitrate;

  return stream;
}

/**
 * tsmux_find_stream:
 * @mux: a #TsMux
 * @pid: the PID to find.
 *
 * Find the stream associated with PID.
 *
 * Returns: a #TsMuxStream with @pid or NULL when the stream was not found.
 */
TsMuxStream *
tsmux_find_stream (TsMux * mux, guint16 pid)
{
  TsMuxStream *found = NULL;
  GList *cur;

  g_return_val_if_fail (mux != NULL, NULL);

  for (cur = mux->streams; cur; cur = cur->next) {
    TsMuxStream *stream = (TsMuxStream *) cur->data;

    if (tsmux_stream_get_pid (stream) == pid) {
      found = stream;
      break;
    }
  }
  return found;
}

static gboolean
tsmux_program_remove_stream (TsMuxProgram * program, TsMuxStream * stream)
{
  GPtrArray *streams = program->streams;

  if (!g_ptr_array_remove (streams, stream)) {
    g_warn_if_reached ();
    return FALSE;
  }

  return streams->len == 0;
}


gboolean
tsmux_remove_stream (TsMux * mux, guint16 pid, TsMuxProgram * program)
{
  GList *cur;
  gboolean ret = FALSE;

  g_return_val_if_fail (mux != NULL, FALSE);

  for (cur = mux->streams; cur; cur = cur->next) {
    TsMuxStream *stream = (TsMuxStream *) cur->data;

    if (tsmux_stream_get_pid (stream) == pid) {
      ret = tsmux_program_remove_stream (program, stream);
      mux->streams = g_list_remove (mux->streams, stream);
      tsmux_stream_free (stream);
      break;
    }
  }

  if (ret)
    tsmux_program_delete (mux, program);

  return ret;
}

static gboolean
tsmux_get_buffer (TsMux * mux, GstBuffer ** buf)
{
  g_return_val_if_fail (buf, FALSE);

  if (G_UNLIKELY (!mux->alloc_func))
    return FALSE;

  mux->alloc_func (buf, mux->alloc_func_data);

  if (!*buf)
    return FALSE;

  g_assert (gst_buffer_get_size (*buf) == TSMUX_PACKET_LENGTH);
  return TRUE;
}

static gboolean
tsmux_packet_out (TsMux * mux, GstBuffer * buf, gint64 pcr)
{
  g_return_val_if_fail (buf, FALSE);

  if (G_UNLIKELY (mux->write_func == NULL)) {
    gst_buffer_unref (buf);
    return TRUE;
  }

  if (mux->bitrate) {
    GST_BUFFER_PTS (buf) =
        gst_util_uint64_scale (mux->n_bytes * 8, GST_SECOND, mux->bitrate);

    /* Check and insert a PCR observation for each program if needed,
     * but only for programs that have written their SI at least once,
     * so the stream starts with PAT/PMT */
    if (mux->first_pcr_ts != G_MININT64) {
      GList *cur;

      for (cur = mux->programs; cur; cur = cur->next) {
        TsMuxProgram *program = (TsMuxProgram *) cur->data;
        TsMuxStream *stream = program->pcr_stream;
        gint64 cur_pcr, next_pcr, new_pcr;

        if (!program->wrote_si)
          continue;

        cur_pcr = get_current_pcr (mux, 0);
        next_pcr = get_next_pcr (mux, 0);
        new_pcr = write_new_pcr (mux, stream, cur_pcr, next_pcr);

        if (new_pcr != -1) {
          GstBuffer *pcr_buf = NULL;
          GstMapInfo map;

          if (!tsmux_get_buffer (mux, &pcr_buf)) {
            goto error;
          }

          gst_buffer_map (pcr_buf, &map, GST_MAP_WRITE);
          tsmux_write_ts_header (mux, map.data, &stream->pi, 0, NULL, NULL);
          gst_buffer_unmap (pcr_buf, &map);

          stream->pi.flags &= TSMUX_PACKET_FLAG_PES_FULL_HEADER;
          if (!tsmux_packet_out (mux, pcr_buf, new_pcr))
            goto error;
        }
      }
    }
  }

  mux->n_bytes += gst_buffer_get_size (buf);

  return mux->write_func (buf, mux->write_func_data, pcr);

error:
  gst_buffer_unref (buf);
  return FALSE;
}

/*
 * adaptation_field() {
 *   adaptation_field_length                              8 uimsbf
 *   if(adaptation_field_length >0) {
 *     discontinuity_indicator                            1 bslbf
 *     random_access_indicator                            1 bslbf
 *     elementary_stream_priority_indicator               1 bslbf
 *     PCR_flag                                           1 bslbf
 *     OPCR_flag                                          1 bslbf
 *     splicing_point_flag                                1 bslbf
 *     transport_private_data_flag                        1 bslbf
 *     adaptation_field_extension_flag                    1 bslbf
 *     if(PCR_flag == '1') {
 *       program_clock_reference_base                    33 uimsbf
 *       reserved                                         6 bslbf
 *       program_clock_reference_extension                9 uimsbf
 *     }
 *     if(OPCR_flag == '1') {
 *       original_program_clock_reference_base           33 uimsbf
 *       reserved                                         6 bslbf
 *       original_program_clock_reference_extension       9 uimsbf
 *     }
 *     if (splicing_point_flag == '1') {
 *       splice_countdown                                 8 tcimsbf
 *     }
 *     if(transport_private_data_flag == '1') {
 *       transport_private_data_length                    8 uimsbf
 *       for (i=0; i<transport_private_data_length;i++){
 *         private_data_byte                              8 bslbf
 *       }
 *     }
 *     if (adaptation_field_extension_flag == '1' ) {
 *       adaptation_field_extension_length                8 uimsbf
 *       ltw_flag                                         1 bslbf
 *       piecewise_rate_flag                              1 bslbf
 *       seamless_splice_flag                             1 bslbf
 *       reserved                                         5 bslbf
 *       if (ltw_flag == '1') {
 *         ltw_valid_flag                                 1 bslbf
 *         ltw_offset                                    15 uimsbf
 *       }
 *       if (piecewise_rate_flag == '1') {
 *         reserved                                       2 bslbf
 *         piecewise_rate                                22 uimsbf
 *       }
 *       if (seamless_splice_flag == '1'){
 *         splice_type                                    4 bslbf
 *         DTS_next_AU[32..30]                            3 bslbf
 *         marker_bit                                     1 bslbf
 *         DTS_next_AU[29..15]                           15 bslbf
 *         marker_bit                                     1 bslbf
 *         DTS_next_AU[14..0]                            15 bslbf
 *         marker_bit                                     1 bslbf
 *       }
 *       for ( i=0;i<N;i++) {
 *         reserved                                       8 bslbf
 *       }
 *     }
 *     for (i=0;i<N;i++){
 *       stuffing_byte                                    8 bslbf
 *     }
 *   }
 * }
 */
static gboolean
tsmux_write_adaptation_field (guint8 * buf,
    TsMuxPacketInfo * pi, guint8 min_length, guint8 * written)
{
  guint8 pos = 2;
  guint8 flags = 0;

  g_assert (min_length <= TSMUX_PAYLOAD_LENGTH);

  /* Write out all the fields from the packet info only if the
   * user set the flag to request the adaptation field - if the flag
   * isn't set, we're just supposed to write stuffing bytes */
  if (pi->flags & TSMUX_PACKET_FLAG_ADAPTATION) {
    TS_DEBUG ("writing adaptation fields");
    if (pi->flags & TSMUX_PACKET_FLAG_DISCONT)
      flags |= 0x80;
    if (pi->flags & TSMUX_PACKET_FLAG_RANDOM_ACCESS)
      flags |= 0x40;
    if (pi->flags & TSMUX_PACKET_FLAG_PRIORITY)
      flags |= 0x20;
    if (pi->flags & TSMUX_PACKET_FLAG_WRITE_PCR) {
      guint64 pcr_base;
      guint32 pcr_ext;

      pcr_base = (pi->pcr / 300);
      pcr_ext = (pi->pcr % 300);

      flags |= 0x10;
      TS_DEBUG ("Writing PCR %" G_GUINT64_FORMAT " + ext %u", pcr_base,
          pcr_ext);
      buf[pos++] = (pcr_base >> 25) & 0xff;
      buf[pos++] = (pcr_base >> 17) & 0xff;
      buf[pos++] = (pcr_base >> 9) & 0xff;
      buf[pos++] = (pcr_base >> 1) & 0xff;
      buf[pos++] = ((pcr_base << 7) & 0x80) | 0x7e | ((pcr_ext >> 8) & 0x01);   /* set 6 reserve bits to 1 */
      buf[pos++] = (pcr_ext) & 0xff;
    }
    if (pi->flags & TSMUX_PACKET_FLAG_WRITE_OPCR) {
      guint64 opcr_base;
      guint32 opcr_ext;

      opcr_base = (pi->opcr / 300);
      opcr_ext = (pi->opcr % 300);

      flags |= 0x08;
      TS_DEBUG ("Writing OPCR");
      buf[pos++] = (opcr_base >> 25) & 0xff;
      buf[pos++] = (opcr_base >> 17) & 0xff;
      buf[pos++] = (opcr_base >> 9) & 0xff;
      buf[pos++] = (opcr_base >> 1) & 0xff;
      buf[pos++] = ((opcr_base << 7) & 0x80) | 0x7e | ((opcr_ext >> 8) & 0x01); /* set 6 reserve bits to 1 */
      buf[pos++] = (opcr_ext) & 0xff;
    }
    if (pi->flags & TSMUX_PACKET_FLAG_WRITE_SPLICE) {
      flags |= 0x04;
      buf[pos++] = pi->splice_countdown;
    }
    if (pi->private_data_len > 0) {
      flags |= 0x02;
      /* Private data to write, ensure we have enough room */
      if ((1 + pi->private_data_len) > (TSMUX_PAYLOAD_LENGTH - pos))
        return FALSE;
      buf[pos++] = pi->private_data_len;
      memcpy (&(buf[pos]), pi->private_data, pi->private_data_len);
      pos += pi->private_data_len;
      TS_DEBUG ("%u bytes of private data", pi->private_data_len);
    }
    if (pi->flags & TSMUX_PACKET_FLAG_WRITE_ADAPT_EXT) {
      flags |= 0x01;
      TS_DEBUG ("FIXME: write Adaptation extension");
      /* Write an empty extension for now */
      buf[pos++] = 1;
      buf[pos++] = 0x1f;        /* lower 5 bits are reserved, and should be all 1 */
    }
  }
  /* Write the flags at the start */
  buf[1] = flags;

  /* Stuffing bytes if needed */
  while (pos < min_length)
    buf[pos++] = 0xff;

  /* Write the adaptation field length, which doesn't include its own byte */
  buf[0] = pos - 1;

  if (written)
    *written = pos;

  return TRUE;
}

static gboolean
tsmux_write_ts_header (TsMux * mux, guint8 * buf, TsMuxPacketInfo * pi,
    guint stream_avail, guint * payload_len_out, guint * payload_offset_out)
{
  guint8 *tmp;
  guint8 adaptation_flag = 0;
  guint8 adapt_min_length = 0;
  guint8 adapt_len = 0;
  guint payload_len;
  gboolean write_adapt = FALSE;

  /* Sync byte */
  buf[0] = TSMUX_SYNC_BYTE;

  TS_DEBUG ("PID 0x%04x, counter = 0x%01x, %u bytes avail", pi->pid,
      mux->pid_packet_counts[pi->pid] & 0x0f, stream_avail);

  /* 3 bits:
   *   transport_error_indicator
   *   payload_unit_start_indicator
   *   transport_priority: (00)
   * 13 bits: PID
   */
  tmp = buf + 1;
  if (pi->packet_start_unit_indicator) {
    tsmux_put16 (&tmp, 0x4000 | pi->pid);
  } else
    tsmux_put16 (&tmp, pi->pid);

  /* 2 bits: scrambling_control (NOT SUPPORTED) (00)
   * 2 bits: adaptation field control (1x has_adaptation_field | x1 has_payload)
   * 4 bits: continuity counter (xxxx)
   */

  if (pi->flags & TSMUX_PACKET_FLAG_ADAPTATION) {
    write_adapt = TRUE;
  }

  if (stream_avail < TSMUX_PAYLOAD_LENGTH) {
    /* Need an adaptation field regardless for stuffing */
    adapt_min_length = TSMUX_PAYLOAD_LENGTH - stream_avail;
    write_adapt = TRUE;
  }

  if (write_adapt) {
    gboolean res;

    /* Flag the adaptation field presence */
    adaptation_flag |= 0x20;
    res = tsmux_write_adaptation_field (buf + TSMUX_HEADER_LENGTH,
        pi, adapt_min_length, &adapt_len);
    if (G_UNLIKELY (res == FALSE))
      return FALSE;

    /* Should have written at least the number of bytes we requested */
    g_assert (adapt_len >= adapt_min_length);
  }

  /* The amount of packet data we wrote is the remaining space after
   * the adaptation field */
  payload_len = TSMUX_PAYLOAD_LENGTH - adapt_len;

  if (payload_len_out)
    *payload_len_out = payload_len;
  else
    g_assert (payload_len == 0);

  if (payload_offset_out)
    *payload_offset_out = TSMUX_HEADER_LENGTH + adapt_len;

  /* Now if we are going to write out some payload, flag that fact */
  if (payload_len > 0 && stream_avail > 0) {
    /* Flag the presence of a payload */
    adaptation_flag |= 0x10;

    /* We must have enough data to fill the payload, or some calculation
     * went wrong */
    g_assert (payload_len <= stream_avail);

    /* Packet with payload, increment the continuity counter */
    mux->pid_packet_counts[pi->pid]++;
  }

  adaptation_flag |= mux->pid_packet_counts[pi->pid] & 0x0f;

  /* Write the byte of transport_scrambling_control, adaptation_field_control
   * + continuity counter out */
  buf[3] = adaptation_flag;


  if (write_adapt) {
    TS_DEBUG ("Adaptation field of size >= %d + %d bytes payload",
        adapt_len, payload_len);
  } else {
    TS_DEBUG ("Payload of %d bytes only", payload_len);
  }

  return TRUE;
}

static gboolean
tsmux_section_write_packet (TsMux * mux, TsMuxSection * section)
{
  guint8 *data;
  gsize data_size;
  guint payload_written = 0;
  gboolean ret = FALSE;

  g_return_val_if_fail (section != NULL, FALSE);
  g_return_val_if_fail (mux != NULL, FALSE);

  data = gst_mpegts_section_packetize (section->section, &data_size);
  if (!data) {
    GST_WARNING ("Could not packetize section");
    return FALSE;
  }

  /* Mark the start of new PES unit */
  section->pi.packet_start_unit_indicator = TRUE;

  /* Mark payload data size */
  section->pi.stream_avail = data_size;

  while (section->pi.stream_avail > 0) {
    GstBuffer *buf;
    GstMapInfo map;
    guint len, offset;

    if (!tsmux_get_buffer (mux, &buf))
      goto done;

    if (!gst_buffer_map (buf, &map, GST_MAP_WRITE)) {
      gst_buffer_unref (buf);
      goto done;
    }

    if (section->pi.packet_start_unit_indicator) {
      /* We need room for a pointer byte */
      if (!tsmux_write_ts_header (mux, map.data, &section->pi,
              section->pi.stream_avail + 1, &len, &offset)) {
        gst_buffer_unmap (buf, &map);
        gst_buffer_unref (buf);
        goto done;
      }

      /* Write the pointer byte */
      map.data[offset++] = 0x00;
      len--;
    } else if (!tsmux_write_ts_header (mux, map.data, &section->pi,
            section->pi.stream_avail, &len, &offset)) {
      gst_buffer_unmap (buf, &map);
      gst_buffer_unref (buf);
      goto done;
    }

    GST_DEBUG ("Creating section packet for offset %u with length %u; %u bytes"
        " remaining", payload_written, len, section->pi.stream_avail - len);

    memcpy (map.data + offset, data + payload_written, len);
    gst_buffer_unmap (buf, &map);

    /* Push the packet without PCR */
    if (G_UNLIKELY (!tsmux_packet_out (mux, buf, -1)))
      goto done;

    section->pi.stream_avail -= len;
    payload_written += len;
    section->pi.packet_start_unit_indicator = FALSE;
  }

  ret = TRUE;

done:
  return ret;
}

/**
 * tsmux_send_section:
 * @mux: a #TsMux
 * @section: (transfer full): a #GstMpegtsSection to add
 *
 * Send a @section immediately on the stream.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean
tsmux_send_section (TsMux * mux, GstMpegtsSection * section)
{
  gboolean ret;
  TsMuxSection tsmux_section;

  g_return_val_if_fail (mux != NULL, FALSE);
  g_return_val_if_fail (section != NULL, FALSE);

  memset (&tsmux_section, 0, sizeof (tsmux_section));

  GST_DEBUG ("Sending mpegts section with type %d to mux",
      section->section_type);

  tsmux_section.section = section;
  tsmux_section.pi.pid = section->pid;

  ret = tsmux_section_write_packet (mux, &tsmux_section);
  gst_mpegts_section_unref (section);

  return ret;
}

static void
tsmux_write_si_foreach (gpointer key, gpointer value, gpointer user_data)
{
  GstMpegtsSectionType section_type = GPOINTER_TO_INT (key);
  TsMuxSection *section = value;
  TsMux *mux = user_data;

  if (!tsmux_section_write_packet (mux, section))
    GST_WARNING ("Failed to send SI section (type %d)", section_type);
}

static gboolean
tsmux_write_si (TsMux * mux)
{
  g_hash_table_foreach (mux->si_sections, tsmux_write_si_foreach, mux);
  mux->si_changed = FALSE;
  return TRUE;
}

static void
tsmux_write_null_ts_header (guint8 * buf)
{
  *buf++ = TSMUX_SYNC_BYTE;
  *buf++ = 0x1f;
  *buf++ = 0xff;
  *buf++ = 0x10;
}

static gint64
ts_to_pcr (gint64 ts)
{
  if (ts == G_MININT64) {
    return 0;
  }

  return (ts - TSMUX_PCR_OFFSET) * (TSMUX_SYS_CLOCK_FREQ / TSMUX_CLOCK_FREQ);
}

/* Calculate the PCR to write into the current packet */
static gint64
get_current_pcr (TsMux * mux, gint64 cur_ts)
{
  if (!mux->bitrate)
    return ts_to_pcr (cur_ts);

  if (mux->first_pcr_ts == G_MININT64) {
    g_assert (cur_ts != G_MININT64);
    mux->first_pcr_ts = cur_ts;
    GST_DEBUG ("First PCR offset is %" G_GUINT64_FORMAT, cur_ts);
  }

  return ts_to_pcr (mux->first_pcr_ts) +
      gst_util_uint64_scale ((mux->n_bytes + PCR_BYTE_OFFSET) * 8,
      TSMUX_SYS_CLOCK_FREQ, mux->bitrate);
}

/* Predict the PCR at the next packet if possible */
static gint64
get_next_pcr (TsMux * mux, gint64 cur_ts)
{
  if (!mux->bitrate)
    return ts_to_pcr (cur_ts);

  if (mux->first_pcr_ts == G_MININT64) {
    g_assert (cur_ts != G_MININT64);
    mux->first_pcr_ts = cur_ts;
    GST_DEBUG ("First PCR offset is %" G_GUINT64_FORMAT, cur_ts);
  }

  return ts_to_pcr (mux->first_pcr_ts) +
      gst_util_uint64_scale ((mux->n_bytes + TSMUX_PACKET_LENGTH +
          PCR_BYTE_OFFSET) * 8, TSMUX_SYS_CLOCK_FREQ, mux->bitrate);
}

static gint64
write_new_pcr (TsMux * mux, TsMuxStream * stream, gint64 cur_pcr,
    gint64 next_pcr)
{
  if (stream->next_pcr == -1 || next_pcr > stream->next_pcr) {
    stream->pi.flags |=
        TSMUX_PACKET_FLAG_ADAPTATION | TSMUX_PACKET_FLAG_WRITE_PCR;
    stream->pi.pcr = cur_pcr;

    if (mux->bitrate && stream->next_pcr != -1 && cur_pcr >= stream->next_pcr) {
      GST_WARNING ("Writing PCR %" G_GUINT64_FORMAT " missed the target %"
          G_GUINT64_FORMAT " by %f ms", cur_pcr, stream->next_pcr,
          (double) (cur_pcr - stream->next_pcr) / 27000.0);
    }
    /* Next PCR deadline is now plus the scheduled interval */
    stream->next_pcr = cur_pcr + mux->pcr_interval * 300;
  } else {
    cur_pcr = -1;
  }

  return cur_pcr;
}

static gboolean
rewrite_si (TsMux * mux, gint64 cur_ts)
{
  gboolean write_pat;
  gboolean write_si;
  GList *cur;
  gint64 next_pcr;

  next_pcr = get_next_pcr (mux, cur_ts);

  /* check if we need to rewrite pat */
  if (mux->next_pat_pcr == -1 || mux->pat_changed)
    write_pat = TRUE;
  else if (next_pcr > mux->next_pat_pcr)
    write_pat = TRUE;
  else
    write_pat = FALSE;

  if (write_pat) {
    if (mux->next_pat_pcr == -1)
      mux->next_pat_pcr = next_pcr + mux->pat_interval * 300;
    else
      mux->next_pat_pcr += mux->pat_interval * 300;

    if (!tsmux_write_pat (mux))
      return FALSE;

    next_pcr = get_next_pcr (mux, cur_ts);
  }

  /* check if we need to rewrite sit */
  if (mux->next_si_pcr == -1 || mux->si_changed)
    write_si = TRUE;
  else if (next_pcr > mux->next_si_pcr)
    write_si = TRUE;
  else
    write_si = FALSE;

  if (write_si) {
    if (mux->next_si_pcr == -1)
      mux->next_si_pcr = next_pcr + mux->si_interval * 300;
    else
      mux->next_si_pcr += mux->si_interval * 300;

    if (!tsmux_write_si (mux))
      return FALSE;

    next_pcr = get_current_pcr (mux, cur_ts);
  }

  /* check if we need to rewrite any of the current pmts */
  for (cur = mux->programs; cur; cur = cur->next) {
    TsMuxProgram *program = (TsMuxProgram *) cur->data;
    gboolean write_pmt;

    if (program->next_pmt_pcr == -1 || program->pmt_changed)
      write_pmt = TRUE;
    else if (next_pcr > program->next_pmt_pcr)
      write_pmt = TRUE;
    else
      write_pmt = FALSE;

    if (write_pmt) {
      if (program->next_pmt_pcr == -1)
        program->next_pmt_pcr = next_pcr + program->pmt_interval * 300;
      else
        program->next_pmt_pcr += program->pmt_interval * 300;

      if (!tsmux_write_pmt (mux, program))
        return FALSE;

      next_pcr = get_current_pcr (mux, cur_ts);
    }

    if (program->scte35_pid != 0) {
      gboolean write_scte_null = FALSE;
      if (program->next_scte35_pcr == -1)
        write_scte_null = TRUE;
      else if (next_pcr > program->next_scte35_pcr)
        write_scte_null = TRUE;

      if (write_scte_null) {
        GST_DEBUG ("next scte35 pcr %" G_GINT64_FORMAT,
            program->next_scte35_pcr);
        if (program->next_scte35_pcr == -1)
          program->next_scte35_pcr =
              next_pcr + program->scte35_null_interval * 300;
        else
          program->next_scte35_pcr += program->scte35_null_interval * 300;
        GST_DEBUG ("next scte35 NOW pcr %" G_GINT64_FORMAT,
            program->next_scte35_pcr);

        if (!tsmux_write_scte_null (mux, program))
          return FALSE;

        next_pcr = get_current_pcr (mux, cur_ts);
      }
    }

    program->wrote_si = TRUE;
  }

  return TRUE;
}

static gboolean
pad_stream (TsMux * mux, TsMuxStream * stream, gint64 cur_ts)
{
  guint64 bitrate;
  GstBuffer *buf = NULL;
  GstMapInfo map;
  gboolean ret = TRUE;
  GstClockTimeDiff diff;
  guint64 start_n_bytes;

  if (!mux->bitrate)
    goto done;

  if (!GST_CLOCK_STIME_IS_VALID (cur_ts))
    goto done;

  if (!GST_CLOCK_STIME_IS_VALID (stream->first_ts))
    stream->first_ts = cur_ts;

  diff = GST_CLOCK_DIFF (stream->first_ts, cur_ts);
  if (diff == 0)
    goto done;

  ret = FALSE;
  start_n_bytes = mux->n_bytes;
  do {
    GST_LOG ("Transport stream bitrate: %" G_GUINT64_FORMAT " over %"
        G_GUINT64_FORMAT " bytes, duration %" GST_TIME_FORMAT,
        gst_util_uint64_scale (mux->n_bytes * 8, TSMUX_CLOCK_FREQ, diff),
        mux->n_bytes, GST_TIME_ARGS (diff * GST_SECOND / TSMUX_CLOCK_FREQ));

    /* calculate what the overall bitrate will be if we add 1 more packet */
    bitrate =
        gst_util_uint64_scale ((mux->n_bytes + TSMUX_PACKET_LENGTH) * 8,
        TSMUX_CLOCK_FREQ, diff);

    if (bitrate <= mux->bitrate) {
      gint64 new_pcr;

      if (!tsmux_get_buffer (mux, &buf))
        goto done;

      if (!gst_buffer_map (buf, &map, GST_MAP_WRITE)) {
        gst_buffer_unref (buf);
        goto done;
      }

      new_pcr = write_new_pcr (mux, stream, get_current_pcr (mux, cur_ts),
          get_next_pcr (mux, cur_ts));
      if (new_pcr != -1) {
        GST_LOG ("Writing PCR-only packet on PID 0x%04x", stream->pi.pid);
        tsmux_write_ts_header (mux, map.data, &stream->pi, 0, NULL, NULL);
      } else {
        GST_LOG ("Writing null stuffing packet");
        if (!rewrite_si (mux, cur_ts)) {
          gst_buffer_unmap (buf, &map);
          gst_buffer_unref (buf);
          goto done;
        }
        tsmux_write_null_ts_header (map.data);
        memset (map.data + TSMUX_HEADER_LENGTH, 0xFF, TSMUX_PAYLOAD_LENGTH);
      }

      gst_buffer_unmap (buf, &map);

      stream->pi.flags &= TSMUX_PACKET_FLAG_PES_FULL_HEADER;
      if (!tsmux_packet_out (mux, buf, new_pcr))
        goto done;
    }
  } while (bitrate < mux->bitrate);

  if (mux->n_bytes != start_n_bytes) {
    GST_LOG ("Finished padding the mux");
  }

  ret = TRUE;

done:
  return ret;
}

static gint64
write_new_prog_pcr (TsMux * mux, TsMuxProgram * prog, gint64 cur_pcr)
{
  if (prog->next_pcr == -1 || cur_pcr > prog->next_pcr) {
    prog->pi.flags |=
        TSMUX_PACKET_FLAG_ADAPTATION | TSMUX_PACKET_FLAG_WRITE_PCR;
    prog->pi.pcr = cur_pcr;

    if (prog->next_pcr == -1)
      prog->next_pcr = cur_pcr + mux->pcr_interval * 300;
    else
      prog->next_pcr += mux->pcr_interval * 300;
  } else {
    cur_pcr = -1;
  }

  return cur_pcr;
}

/**
 * tsmux_write_stream_packet:
 * @mux: a #TsMux
 * @stream: a #TsMuxStream
 *
 * Write a packet of @stream.
 *
 * Returns: TRUE if the packet could be written.
 */
gboolean
tsmux_write_stream_packet (TsMux * mux, TsMuxStream * stream)
{
  guint payload_len, payload_offs;
  TsMuxPacketInfo *pi = &stream->pi;
  gboolean res;
  gint64 new_pcr = -1;
  GstBuffer *buf = NULL;
  GstMapInfo map;

  g_return_val_if_fail (mux != NULL, FALSE);
  g_return_val_if_fail (stream != NULL, FALSE);

  if (tsmux_stream_is_pcr (stream) || stream->program->pcr_pid) {
    gint64 cur_ts = CLOCK_BASE;
    if (tsmux_stream_get_dts (stream) != G_MININT64)
      cur_ts += tsmux_stream_get_dts (stream);
    else
      cur_ts += tsmux_stream_get_pts (stream);

    if (!rewrite_si (mux, cur_ts))
      goto fail;

    if (!pad_stream (mux, stream, cur_ts))
      goto fail;

    new_pcr =
        write_new_pcr (mux, stream, get_current_pcr (mux, cur_ts),
        get_next_pcr (mux, cur_ts));

    if (stream->program->pcr_pid) {
      /* this should only enter block when time to send a PCR packet */
      new_pcr = write_new_prog_pcr (mux, stream->program, get_current_pcr (mux,
              cur_ts));
      if (new_pcr != -1) {
        if (!tsmux_get_buffer (mux, &buf))
          return FALSE;

        if (!gst_buffer_map (buf, &map, GST_MAP_WRITE))
          goto fail_unmapped;

        if (!tsmux_write_ts_header (mux, map.data, &stream->program->pi, 0,
                NULL, NULL))
          goto fail;

        gst_buffer_unmap (buf, &map);
        stream->program->pi.pid = stream->program->pcr_pid;
        stream->program->pi.flags &= TSMUX_PACKET_FLAG_PES_FULL_HEADER;
        if (!tsmux_packet_out (mux, buf, new_pcr))
          return FALSE;
      }
    }
  }

  pi->packet_start_unit_indicator = tsmux_stream_at_pes_start (stream);
  if (pi->packet_start_unit_indicator) {
    tsmux_stream_initialize_pes_packet (stream);
    if (stream->dts != G_MININT64)
      stream->dts += CLOCK_BASE;
    if (stream->pts != G_MININT64)
      stream->pts += CLOCK_BASE;
  }
  pi->stream_avail = tsmux_stream_bytes_avail (stream);

  /* obtain buffer */
  if (!tsmux_get_buffer (mux, &buf))
    return FALSE;

  gst_buffer_map (buf, &map, GST_MAP_WRITE);

  if (!tsmux_write_ts_header (mux, map.data, pi, pi->stream_avail, &payload_len,
          &payload_offs))
    goto fail;


  if (!tsmux_stream_get_data (stream, map.data + payload_offs, payload_len))
    goto fail;

  gst_buffer_unmap (buf, &map);

  GST_DEBUG ("Writing PES of size %d", (int) gst_buffer_get_size (buf));
  res = tsmux_packet_out (mux, buf, new_pcr);

  /* Reset all dynamic flags */
  stream->pi.flags &= TSMUX_PACKET_FLAG_PES_FULL_HEADER;

  return res;

  /* ERRORS */
fail:
  {
    if (buf) {
      gst_buffer_unmap (buf, &map);
      gst_buffer_unref (buf);
    }
    return FALSE;
  }
fail_unmapped:
  gst_clear_buffer (&buf);
  return FALSE;
}

/**
 * tsmux_program_free:
 * @program: a #TsMuxProgram
 *
 * Free the resources of @program. After this call @program can not be used
 * anymore.
 */
void
tsmux_program_free (TsMuxProgram * program)
{
  g_return_if_fail (program != NULL);

  /* Free PMT section */
  if (program->pmt.section)
    gst_mpegts_section_unref (program->pmt.section);
  if (program->scte35_null_section)
    tsmux_section_free (program->scte35_null_section);

  g_ptr_array_free (program->streams, TRUE);
  g_free (program);
}

/**
 * tsmux_program_set_pmt_pid:
 * @program: A #TsmuxProgram
 * @pmt_pid: PID to write PMT for this program
 */
void
tsmux_program_set_pmt_pid (TsMuxProgram * program, guint16 pmt_pid)
{
  g_return_if_fail (program != NULL);

  program->pmt_pid = pmt_pid;
  program->pmt_changed = TRUE;
}

static gint
compare_program_number (gconstpointer a, gconstpointer b)
{
  const GstMpegtsPatProgram *pgm1 = *(const GstMpegtsPatProgram * const *) a;
  const GstMpegtsPatProgram *pgm2 = *(const GstMpegtsPatProgram * const *) b;
  gint num1 = pgm1->program_number, num2 = pgm2->program_number;

  return num1 - num2;
}

static gboolean
tsmux_write_pat (TsMux * mux)
{

  if (mux->pat_changed) {
    /* program_association_section ()
     * for (i = 0; i < N; i++) {
     *    program_number                         16   uimsbf
     *    reserved                                3   bslbf
     *    network_PID_or_program_map_PID         13   uimbsf
     * }
     * CRC_32                                    32   rbchof
     */
    GList *cur;
    GPtrArray *pat;

    pat = gst_mpegts_pat_new ();

    for (cur = mux->programs; cur; cur = cur->next) {
      GstMpegtsPatProgram *pat_pgm;
      TsMuxProgram *program = (TsMuxProgram *) cur->data;

      pat_pgm = gst_mpegts_pat_program_new ();
      pat_pgm->program_number = program->pgm_number;
      pat_pgm->network_or_program_map_PID = program->pmt_pid;

      g_ptr_array_add (pat, pat_pgm);
    }

    g_ptr_array_sort (pat, compare_program_number);

    if (mux->pat.section)
      gst_mpegts_section_unref (mux->pat.section);

    mux->pat.section = gst_mpegts_section_from_pat (pat, mux->transport_id);

    mux->pat.section->version_number = mux->pat_version++;

    TS_DEBUG ("PAT has %d programs", mux->nb_programs);
    mux->pat_changed = FALSE;
  }

  return tsmux_section_write_packet (mux, &mux->pat);
}

static gboolean
tsmux_write_pmt (TsMux * mux, TsMuxProgram * program)
{

  if (program->pmt_changed) {
    /* program_association_section ()
     * reserved                                   3   bslbf
     * PCR_PID                                   13   uimsbf
     * reserved                                   4   bslbf
     * program_info_length                       12   uimsbf
     * for (i = 0; i < N; i++)
     *   descriptor ()
     *
     * for (i = 0; i < N1; i++) {
     *    stream_type                             8   uimsbf
     *    reserved                                3   bslbf
     *    elementary_PID                         13   uimbsf
     *    reserved                                4   bslbf
     *    ES_info_length                         12   uimbsf
     *    for (i = 0; i < N1; i++) {
     *      descriptor ();
     *    }
     * }
     */
    GstMpegtsDescriptor *descriptor;
    GstMpegtsPMT *pmt;
#if 0
    /* See note about bluray descriptors below */
    guint8 desc[] = { 0x0F, 0xFF, 0xFC, 0xFC };
#endif
    guint i;

    pmt = gst_mpegts_pmt_new ();

    if ((program->pcr_stream == NULL) && (program->pcr_pid == 0))
      pmt->pcr_pid = 0x1FFF;
    else if (program->pcr_pid != 0)
      pmt->pcr_pid = program->pcr_pid;
    else
      pmt->pcr_pid = tsmux_stream_get_pid (program->pcr_stream);

#if 0
    /* FIXME : These two descriptors should not be added in all PMT
     * but only in "bluray-compatible" mpeg-ts output. I even have my
     * doubt whether the DTCP descriptor is even needed */
    descriptor = gst_mpegts_descriptor_from_registration ("HDMV", NULL, 0);
    g_ptr_array_add (pmt->descriptors, descriptor);

    /* DTCP descriptor, see
     * http://www.dtcp.com/documents/dtcp/info-20150204-dtcp-v1-rev%201-71.pdf */
    descriptor = gst_mpegts_descriptor_from_custom (0x88, desc, 4);
    g_ptr_array_add (pmt->descriptors, descriptor);
#endif

    /* Will SCTE-35 be potentially used ? */
    if (program->scte35_pid != 0) {
      descriptor = gst_mpegts_descriptor_from_registration ("CUEI", NULL, 0);
      g_ptr_array_add (pmt->descriptors, descriptor);
    }

    /* Write out the entries */
    for (i = 0; i < program->streams->len; i++) {
      GstMpegtsPMTStream *pmt_stream;
      TsMuxStream *stream = g_ptr_array_index (program->streams, i);

      pmt_stream = gst_mpegts_pmt_stream_new ();

      /* FIXME: Use API to retrieve this from the stream */
      pmt_stream->stream_type = stream->stream_type;
      pmt_stream->pid = tsmux_stream_get_pid (stream);

      /* Write any ES descriptors needed */
      tsmux_stream_get_es_descrs (stream, pmt_stream);
      g_ptr_array_add (pmt->streams, pmt_stream);
    }

    /* Will SCTE-35 be potentially used ? */
    if (program->scte35_pid != 0) {
      GstMpegtsPMTStream *pmt_stream = gst_mpegts_pmt_stream_new ();
      pmt_stream->stream_type = GST_MPEGTS_STREAM_TYPE_SCTE_SIT;
      pmt_stream->pid = program->scte35_pid;
      g_ptr_array_add (pmt->streams, pmt_stream);
    }

    TS_DEBUG ("PMT for program %d has %d streams",
        program->pgm_number, program->streams->len);

    pmt->program_number = program->pgm_number;

    program->pmt.pi.pid = program->pmt_pid;
    program->pmt_changed = FALSE;

    if (program->pmt.section)
      gst_mpegts_section_unref (program->pmt.section);

    program->pmt.section = gst_mpegts_section_from_pmt (pmt, program->pmt_pid);
    program->pmt.section->version_number = program->pmt_version++;
  }

  return tsmux_section_write_packet (mux, &program->pmt);
}

static gboolean
tsmux_write_scte_null (TsMux * mux, TsMuxProgram * program)
{
  /* SCTE-35 NULL section is created when PID is set */
  GST_LOG ("Writing SCTE NULL packet");
  return tsmux_section_write_packet (mux, program->scte35_null_section);
}

void
tsmux_set_bitrate (TsMux * mux, guint64 bitrate)
{
  mux->bitrate = bitrate;
}
