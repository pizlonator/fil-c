/*
 * dvbbasebin.c - 
 * Copyright (C) 2007 Alessandro Decina
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 * 
 * Authors:
 *   Alessandro Decina <alessandro.d@gmail.com>
 *   Reynaldo H. Verdejo Pinochet <reynaldo@osg.samsung.com>
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
#include <stdlib.h>
#include <string.h>
#include <gst/mpegts/mpegts.h>
#include "gstdvbelements.h"
#include "dvbbasebin.h"
#include "parsechannels.h"

GST_DEBUG_CATEGORY (dvb_base_bin_debug);
#define GST_CAT_DEFAULT dvb_base_bin_debug

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpegts, " "systemstream = (boolean) true ")
    );

static GstStaticPadTemplate program_template =
GST_STATIC_PAD_TEMPLATE ("program_%u", GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/mpegts, " "systemstream = (boolean) true ")
    );

enum
{
  /* FILL ME */
  SIGNAL_TUNING_START,
  SIGNAL_TUNING_DONE,
  SIGNAL_TUNING_FAIL,
  SIGNAL_TUNE,
  LAST_SIGNAL
};

enum
{
  ARG_0,
  PROP_ADAPTER,
  PROP_FRONTEND,
  PROP_DISEQC_SRC,
  PROP_FREQUENCY,
  PROP_POLARITY,
  PROP_SYMBOL_RATE,
  PROP_BANDWIDTH,
  PROP_CODE_RATE_HP,
  PROP_CODE_RATE_LP,
  PROP_GUARD,
  PROP_MODULATION,
  PROP_TRANS_MODE,
  PROP_HIERARCHY,
  PROP_INVERSION,
  PROP_PROGRAM_NUMBERS,
  PROP_STATS_REPORTING_INTERVAL,
  PROP_TUNING_TIMEOUT,
  PROP_DELSYS,
  PROP_PILOT,
  PROP_ROLLOFF,
  PROP_STREAM_ID,
  PROP_BANDWIDTH_HZ,
  PROP_ISDBT_LAYER_ENABLED,
  PROP_ISDBT_PARTIAL_RECEPTION,
  PROP_ISDBT_SOUND_BROADCASTING,
  PROP_ISDBT_SB_SUBCHANNEL_ID,
  PROP_ISDBT_SB_SEGMENT_IDX,
  PROP_ISDBT_SB_SEGMENT_COUNT,
  PROP_ISDBT_LAYERA_FEC,
  PROP_ISDBT_LAYERA_MODULATION,
  PROP_ISDBT_LAYERA_SEGMENT_COUNT,
  PROP_ISDBT_LAYERA_TIME_INTERLEAVING,
  PROP_ISDBT_LAYERB_FEC,
  PROP_ISDBT_LAYERB_MODULATION,
  PROP_ISDBT_LAYERB_SEGMENT_COUNT,
  PROP_ISDBT_LAYERB_TIME_INTERLEAVING,
  PROP_ISDBT_LAYERC_FEC,
  PROP_ISDBT_LAYERC_MODULATION,
  PROP_ISDBT_LAYERC_SEGMENT_COUNT,
  PROP_ISDBT_LAYERC_TIME_INTERLEAVING,
  PROP_LNB_SLOF,
  PROP_LNB_LOF1,
  PROP_LNB_LOF2,
  PROP_INTERLEAVING
};

typedef struct
{
  guint16 pid;
  guint usecount;
} DvbBaseBinStream;

typedef struct
{
  gint program_number;
  guint16 pmt_pid;
  guint16 pcr_pid;
  GstMpegtsSection *section;
  GstMpegtsSection *old_section;
  const GstMpegtsPMT *pmt;
  const GstMpegtsPMT *old_pmt;
  gboolean selected;
  gboolean pmt_active;
  gboolean active;
  GstPad *ghost;
} DvbBaseBinProgram;

static void dvb_base_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void dvb_base_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void dvb_base_bin_dispose (GObject * object);
static void dvb_base_bin_finalize (GObject * object);

static void dvb_base_bin_task (DvbBaseBin * basebin);

static GstStateChangeReturn dvb_base_bin_change_state (GstElement * element,
    GstStateChange transition);
static void dvb_base_bin_handle_message (GstBin * bin, GstMessage * message);
static void dvb_base_bin_pat_info_cb (DvbBaseBin * dvbbasebin,
    GstMpegtsSection * pat);
static void dvb_base_bin_pmt_info_cb (DvbBaseBin * dvbbasebin,
    GstMpegtsSection * pmt);
static GstPad *dvb_base_bin_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void dvb_base_bin_release_pad (GstElement * element, GstPad * pad);
static void dvb_base_bin_rebuild_filter (DvbBaseBin * dvbbasebin);
static void
dvb_base_bin_deactivate_program (DvbBaseBin * dvbbasebin,
    DvbBaseBinProgram * program);

static void dvb_base_bin_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

static void dvb_base_bin_program_destroy (gpointer data);

/* Proxy callbacks for dvbsrc signals */
static void tuning_start_signal_cb (GObject * object, DvbBaseBin * dvbbasebin);
static void tuning_done_signal_cb (GObject * object, DvbBaseBin * dvbbasebin);
static void tuning_fail_signal_cb (GObject * object, DvbBaseBin * dvbbasebin);

static void dvb_base_bin_do_tune (DvbBaseBin * dvbbasebin);

#define dvb_base_bin_parent_class parent_class
G_DEFINE_TYPE_EXTENDED (DvbBaseBin, dvb_base_bin, GST_TYPE_BIN,
    0,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        dvb_base_bin_uri_handler_init));
#define _do_init \
  GST_DEBUG_CATEGORY_INIT (dvb_base_bin_debug, "dvbbasebin", 0, "DVB bin"); \
  cam_init (); \
  dvb_element_init (plugin);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (dvbbasebin, "dvbbasebin", GST_RANK_NONE,
    GST_TYPE_DVB_BASE_BIN, _do_init);

static void
dvb_base_bin_ref_stream (DvbBaseBinStream * stream)
{
  g_return_if_fail (stream != NULL);
  stream->usecount++;
}

static void
dvb_base_bin_unref_stream (DvbBaseBinStream * stream)
{
  g_return_if_fail (stream != NULL);
  g_return_if_fail (stream->usecount > 0);
  stream->usecount--;
}

static DvbBaseBinStream *
dvb_base_bin_add_stream (DvbBaseBin * dvbbasebin, guint16 pid)
{
  DvbBaseBinStream *stream;

  stream = g_new0 (DvbBaseBinStream, 1);
  stream->pid = pid;
  stream->usecount = 0;

  g_hash_table_insert (dvbbasebin->streams,
      GINT_TO_POINTER ((gint) pid), stream);

  return stream;
}

static DvbBaseBinStream *
dvb_base_bin_get_stream (DvbBaseBin * dvbbasebin, guint16 pid)
{
  return (DvbBaseBinStream *) g_hash_table_lookup (dvbbasebin->streams,
      GINT_TO_POINTER ((gint) pid));
}

static DvbBaseBinProgram *
dvb_base_bin_add_program (DvbBaseBin * dvbbasebin, gint program_number)
{
  DvbBaseBinProgram *program;

  program = g_new0 (DvbBaseBinProgram, 1);
  program->program_number = program_number;
  program->selected = FALSE;
  program->active = FALSE;
  program->pmt_pid = G_MAXUINT16;
  program->pcr_pid = G_MAXUINT16;
  program->pmt = NULL;
  program->old_pmt = NULL;

  g_hash_table_insert (dvbbasebin->programs,
      GINT_TO_POINTER (program_number), program);

  return program;
}

static DvbBaseBinProgram *
dvb_base_bin_get_program (DvbBaseBin * dvbbasebin, gint program_number)
{
  return (DvbBaseBinProgram *) g_hash_table_lookup (dvbbasebin->programs,
      GINT_TO_POINTER (program_number));
}


static guint dvb_base_bin_signals[LAST_SIGNAL] = { 0 };

static void
tuning_start_signal_cb (GObject * object, DvbBaseBin * dvbbasebin)
{
  g_signal_emit (dvbbasebin, dvb_base_bin_signals[SIGNAL_TUNING_START], 0);
}

static void
tuning_done_signal_cb (GObject * object, DvbBaseBin * dvbbasebin)
{
  g_signal_emit (dvbbasebin, dvb_base_bin_signals[SIGNAL_TUNING_DONE], 0);
}

static void
tuning_fail_signal_cb (GObject * object, DvbBaseBin * dvbbasebin)
{
  g_signal_emit (dvbbasebin, dvb_base_bin_signals[SIGNAL_TUNING_FAIL], 0);
}

static void
dvb_base_bin_do_tune (DvbBaseBin * dvbbasebin)
{
  g_signal_emit_by_name (dvbbasebin->dvbsrc, "tune", NULL);
}

static void
dvb_base_bin_class_init (DvbBaseBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBinClass *bin_class;
  DvbBaseBinClass *dvbbasebin_class;
  GstElementFactory *dvbsrc_factory;
  GObjectClass *dvbsrc_class;
  typedef struct
  {
    guint prop_id;
    const gchar *prop_name;
  } ProxyedProperty;
  ProxyedProperty *walk;
  ProxyedProperty proxyed_properties[] = {
    {PROP_ADAPTER, "adapter"},
    {PROP_FRONTEND, "frontend"},
    {PROP_DISEQC_SRC, "diseqc-source"},
    {PROP_FREQUENCY, "frequency"},
    {PROP_POLARITY, "polarity"},
    {PROP_SYMBOL_RATE, "symbol-rate"},
#ifndef GST_REMOVE_DEPRECATED
    {PROP_BANDWIDTH, "bandwidth"},
#endif
    {PROP_CODE_RATE_HP, "code-rate-hp"},
    {PROP_CODE_RATE_LP, "code-rate-lp"},
    {PROP_GUARD, "guard"},
    {PROP_MODULATION, "modulation"},
    {PROP_TRANS_MODE, "trans-mode"},
    {PROP_HIERARCHY, "hierarchy"},
    {PROP_INVERSION, "inversion"},
    {PROP_STATS_REPORTING_INTERVAL, "stats-reporting-interval"},
    {PROP_TUNING_TIMEOUT, "tuning-timeout"},
    {PROP_DELSYS, "delsys"},
    {PROP_PILOT, "pilot"},
    {PROP_ROLLOFF, "rolloff"},
    {PROP_STREAM_ID, "stream-id"},
    {PROP_BANDWIDTH_HZ, "bandwidth-hz"},
    {PROP_ISDBT_LAYER_ENABLED, "isdbt-layer-enabled"},
    {PROP_ISDBT_PARTIAL_RECEPTION, "isdbt-partial-reception"},
    {PROP_ISDBT_SOUND_BROADCASTING, "isdbt-sound-broadcasting"},
    {PROP_ISDBT_SB_SUBCHANNEL_ID, "isdbt-sb-subchannel-id"},
    {PROP_ISDBT_SB_SEGMENT_IDX, "isdbt-sb-segment-idx"},
    {PROP_ISDBT_SB_SEGMENT_COUNT, "isdbt-sb-segment-count"},
    {PROP_ISDBT_LAYERA_FEC, "isdbt-layera-fec"},
    {PROP_ISDBT_LAYERA_MODULATION, "isdbt-layera-modulation"},
    {PROP_ISDBT_LAYERA_SEGMENT_COUNT, "isdbt-layera-segment-count"},
    {PROP_ISDBT_LAYERA_TIME_INTERLEAVING, "isdbt-layera-time-interleaving"},
    {PROP_ISDBT_LAYERB_FEC, "isdbt-layerb-fec"},
    {PROP_ISDBT_LAYERB_MODULATION, "isdbt-layerb-modulation"},
    {PROP_ISDBT_LAYERB_SEGMENT_COUNT, "isdbt-layerb-segment-count"},
    {PROP_ISDBT_LAYERB_TIME_INTERLEAVING, "isdbt-layerb-time-interleaving"},
    {PROP_ISDBT_LAYERC_FEC, "isdbt-layerc-fec"},
    {PROP_ISDBT_LAYERC_MODULATION, "isdbt-layerc-modulation"},
    {PROP_ISDBT_LAYERC_SEGMENT_COUNT, "isdbt-layerc-segment-count"},
    {PROP_ISDBT_LAYERC_TIME_INTERLEAVING, "isdbt-layerc-time-interleaving"},
    {PROP_LNB_SLOF, "lnb-slof"},
    {PROP_LNB_LOF1, "lnb-lof1"},
    {PROP_LNB_LOF2, "lnb-lof2"},
    {PROP_INTERLEAVING, "interleaving"},
    {0, NULL}
  };

  bin_class = GST_BIN_CLASS (klass);
  bin_class->handle_message = dvb_base_bin_handle_message;

  element_class = GST_ELEMENT_CLASS (klass);

  element_class->change_state = dvb_base_bin_change_state;
  element_class->request_new_pad = dvb_base_bin_request_new_pad;
  element_class->release_pad = dvb_base_bin_release_pad;

  gst_element_class_add_static_pad_template (element_class, &program_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class, "DVB bin",
      "Source/Bin/Video",
      "Access descramble and split DVB streams",
      "Alessandro Decina <alessandro.d@gmail.com>, "
      "Reynaldo H. Verdejo Pinochet <reynaldo@osg.samsung.com>");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = dvb_base_bin_set_property;
  gobject_class->get_property = dvb_base_bin_get_property;
  gobject_class->dispose = dvb_base_bin_dispose;
  gobject_class->finalize = dvb_base_bin_finalize;

  dvbbasebin_class = (DvbBaseBinClass *) klass;
  dvbbasebin_class->do_tune = dvb_base_bin_do_tune;

  /* install dvbsrc properties */
  dvbsrc_factory = gst_element_factory_find ("dvbsrc");
  dvbsrc_class =
      g_type_class_ref (gst_element_factory_get_element_type (dvbsrc_factory));
  walk = proxyed_properties;
  while (walk->prop_name != NULL) {
    GParamSpec *pspec;
    GParamSpec *our_pspec;

    pspec = g_object_class_find_property (dvbsrc_class, walk->prop_name);
    if (pspec != NULL) {
      GType param_type = G_PARAM_SPEC_TYPE (pspec);

      if (param_type == G_TYPE_PARAM_INT) {
        GParamSpecInt *src_pspec = G_PARAM_SPEC_INT (pspec);

        our_pspec = g_param_spec_int (g_param_spec_get_name (pspec),
            g_param_spec_get_nick (pspec), g_param_spec_get_blurb (pspec),
            src_pspec->minimum, src_pspec->maximum, src_pspec->default_value,
            pspec->flags);
      } else if (param_type == G_TYPE_PARAM_UINT) {
        GParamSpecUInt *src_pspec = G_PARAM_SPEC_UINT (pspec);

        our_pspec = g_param_spec_uint (g_param_spec_get_name (pspec),
            g_param_spec_get_nick (pspec), g_param_spec_get_blurb (pspec),
            src_pspec->minimum, src_pspec->maximum, src_pspec->default_value,
            pspec->flags);
      } else if (param_type == G_TYPE_PARAM_UINT64) {
        GParamSpecUInt64 *src_pspec = G_PARAM_SPEC_UINT64 (pspec);

        our_pspec = g_param_spec_uint64 (g_param_spec_get_name (pspec),
            g_param_spec_get_nick (pspec), g_param_spec_get_blurb (pspec),
            src_pspec->minimum, src_pspec->maximum, src_pspec->default_value,
            pspec->flags);
      } else if (param_type == G_TYPE_PARAM_STRING) {
        GParamSpecString *src_pspec = G_PARAM_SPEC_STRING (pspec);

        our_pspec = g_param_spec_string (g_param_spec_get_name (pspec),
            g_param_spec_get_nick (pspec), g_param_spec_get_blurb (pspec),
            src_pspec->default_value, pspec->flags);
      } else if (param_type == G_TYPE_PARAM_ENUM) {
        GParamSpecEnum *src_pspec = G_PARAM_SPEC_ENUM (pspec);

        our_pspec = g_param_spec_enum (g_param_spec_get_name (pspec),
            g_param_spec_get_nick (pspec), g_param_spec_get_blurb (pspec),
            pspec->value_type, src_pspec->default_value, pspec->flags);
      } else {
        GST_ERROR ("Unsupported property type %s for property %s",
            g_type_name (param_type), g_param_spec_get_name (pspec));
        ++walk;
        continue;
      }

      g_object_class_install_property (gobject_class, walk->prop_id, our_pspec);
    } else {
      g_warning ("dvbsrc has no property named %s", walk->prop_name);
    }
    ++walk;
  }
  g_type_class_unref (dvbsrc_class);

  g_object_class_install_property (gobject_class, PROP_PROGRAM_NUMBERS,
      g_param_spec_string ("program-numbers",
          "Program Numbers",
          "Colon separated list of programs", "", G_PARAM_READWRITE));
  /**
   * DvbBaseBin::tuning-start:
   * @dvbbasebin: the element on which the signal is emitted
   *
   * Signal emitted when the element first attempts to tune the
   * frontend tunner to a given frequency.
   */
  dvb_base_bin_signals[SIGNAL_TUNING_START] =
      g_signal_new ("tuning-start", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  /**
   * DvbBaseBin::tuning-done:
   * @dvbbasebin: the element on which the signal is emitted
   *
   * Signal emitted when the tunner has successfully got a lock on a signal.
   */
  dvb_base_bin_signals[SIGNAL_TUNING_DONE] =
      g_signal_new ("tuning-done", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  /**
   * DvbBaseBin::tuning-fail:
   * @dvbbasebin: the element on which the signal is emitted
   *
   * Signal emitted when the tunner failed to get a lock on the
   * signal.
   */
  dvb_base_bin_signals[SIGNAL_TUNING_FAIL] =
      g_signal_new ("tuning-fail", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * DvbBaseBin::tune:
   * @dvbbasesink: the element on which the signal is emitted
   *
   * Signal emitted from the application to the element, instructing it
   * to tune.
   */
  dvb_base_bin_signals[SIGNAL_TUNE] =
      g_signal_new ("tune", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (DvbBaseBinClass, do_tune), NULL, NULL, NULL,
      G_TYPE_NONE, 0);
}

static void
dvb_base_bin_reset (DvbBaseBin * dvbbasebin)
{
  if (dvbbasebin->hwcam) {
    cam_device_close (dvbbasebin->hwcam);
    cam_device_free (dvbbasebin->hwcam);
    dvbbasebin->hwcam = NULL;
  }
  dvbbasebin->trycam = TRUE;
}

static const gint16 initial_pids[] = { 0, 1, 0x10, 0x11, 0x12, 0x14, -1 };

static void
dvb_base_bin_init (DvbBaseBin * dvbbasebin)
{
  DvbBaseBinStream *stream;
  GstPad *ghost, *pad;
  int i;

  dvbbasebin->dvbsrc = gst_element_factory_make ("dvbsrc", NULL);
  dvbbasebin->buffer_queue = gst_element_factory_make ("queue", NULL);
  dvbbasebin->tsparse = gst_element_factory_make ("tsparse", NULL);

  g_object_set (dvbbasebin->buffer_queue, "max-size-buffers", 0,
      "max-size-bytes", 0, "max-size-time", (guint64) 0, NULL);

  gst_bin_add_many (GST_BIN (dvbbasebin), dvbbasebin->dvbsrc,
      dvbbasebin->buffer_queue, dvbbasebin->tsparse, NULL);

  gst_element_link_many (dvbbasebin->dvbsrc,
      dvbbasebin->buffer_queue, dvbbasebin->tsparse, NULL);

  /* Proxy dvbsrc signals */
  g_signal_connect (dvbbasebin->dvbsrc, "tuning-start",
      G_CALLBACK (tuning_start_signal_cb), dvbbasebin);
  g_signal_connect (dvbbasebin->dvbsrc, "tuning-done",
      G_CALLBACK (tuning_done_signal_cb), dvbbasebin);
  g_signal_connect (dvbbasebin->dvbsrc, "tuning-fail",
      G_CALLBACK (tuning_fail_signal_cb), dvbbasebin);

  /* Expose tsparse source pad */
  if (dvbbasebin->tsparse != NULL) {
    pad = gst_element_get_static_pad (dvbbasebin->tsparse, "src");
    ghost = gst_ghost_pad_new ("src", pad);
    gst_object_unref (pad);
  } else {
    ghost = gst_ghost_pad_new_no_target ("src", GST_PAD_SRC);
  }
  gst_element_add_pad (GST_ELEMENT (dvbbasebin), ghost);

  dvbbasebin->programs = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, dvb_base_bin_program_destroy);
  dvbbasebin->streams = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, g_free);

  dvbbasebin->pmtlist = NULL;
  dvbbasebin->pmtlist_changed = FALSE;

  dvbbasebin->disposed = FALSE;
  dvb_base_bin_reset (dvbbasebin);

  /* add PAT, CAT, NIT, SDT, EIT, TDT to pids filter for dvbsrc */
  i = 0;
  while (initial_pids[i] >= 0) {
    stream = dvb_base_bin_add_stream (dvbbasebin, (guint16) initial_pids[i]);
    dvb_base_bin_ref_stream (stream);
    i++;
  }
  dvb_base_bin_rebuild_filter (dvbbasebin);

  g_rec_mutex_init (&dvbbasebin->lock);
  dvbbasebin->task =
      gst_task_new ((GstTaskFunction) dvb_base_bin_task, dvbbasebin, NULL);
  gst_task_set_lock (dvbbasebin->task, &dvbbasebin->lock);
  dvbbasebin->poll = gst_poll_new (TRUE);
}

static void
dvb_base_bin_dispose (GObject * object)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (object);

  if (!dvbbasebin->disposed) {
    /* remove mpegtsparse BEFORE dvbsrc, since the mpegtsparse::pad-removed
     * signal handler uses dvbsrc */
    dvb_base_bin_reset (dvbbasebin);
    if (dvbbasebin->tsparse != NULL)
      gst_bin_remove (GST_BIN (dvbbasebin), dvbbasebin->tsparse);
    gst_bin_remove (GST_BIN (dvbbasebin), dvbbasebin->dvbsrc);
    gst_bin_remove (GST_BIN (dvbbasebin), dvbbasebin->buffer_queue);
    g_free (dvbbasebin->program_numbers);
    gst_poll_free (dvbbasebin->poll);
    gst_object_unref (dvbbasebin->task);
    g_rec_mutex_clear (&dvbbasebin->lock);
    dvbbasebin->disposed = TRUE;
  }

  if (G_OBJECT_CLASS (parent_class)->dispose)
    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
dvb_base_bin_finalize (GObject * object)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (object);

  g_hash_table_destroy (dvbbasebin->streams);
  g_hash_table_destroy (dvbbasebin->programs);
  g_list_free (dvbbasebin->pmtlist);

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
dvb_base_bin_set_program_numbers (DvbBaseBin * dvbbasebin, const gchar * pn)
{
  gchar **strv, **walk;
  DvbBaseBinProgram *program;

  /* Split up and update programs */
  strv = g_strsplit (pn, ":", 0);

  for (walk = strv; *walk; walk++) {
    gint program_number = strtol (*walk, NULL, 0);

    program = dvb_base_bin_get_program (dvbbasebin, program_number);
    if (program == NULL) {
      program = dvb_base_bin_add_program (dvbbasebin, program_number);
      program->selected = TRUE;
    }
  }

  g_strfreev (strv);

  /* FIXME : Deactivate programs no longer selected */

  g_free (dvbbasebin->program_numbers);
  dvbbasebin->program_numbers = g_strdup (pn);

  if (0)
    dvb_base_bin_deactivate_program (dvbbasebin, NULL);
}

static void
dvb_base_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (object);

  switch (prop_id) {
    case PROP_ADAPTER:
    case PROP_DISEQC_SRC:
    case PROP_FRONTEND:
    case PROP_FREQUENCY:
    case PROP_POLARITY:
    case PROP_SYMBOL_RATE:
    case PROP_BANDWIDTH:
    case PROP_CODE_RATE_HP:
    case PROP_CODE_RATE_LP:
    case PROP_GUARD:
    case PROP_MODULATION:
    case PROP_TRANS_MODE:
    case PROP_HIERARCHY:
    case PROP_INVERSION:
    case PROP_STATS_REPORTING_INTERVAL:
    case PROP_TUNING_TIMEOUT:
    case PROP_DELSYS:
    case PROP_PILOT:
    case PROP_ROLLOFF:
    case PROP_STREAM_ID:
    case PROP_BANDWIDTH_HZ:
    case PROP_ISDBT_LAYER_ENABLED:
    case PROP_ISDBT_PARTIAL_RECEPTION:
    case PROP_ISDBT_SOUND_BROADCASTING:
    case PROP_ISDBT_SB_SUBCHANNEL_ID:
    case PROP_ISDBT_SB_SEGMENT_IDX:
    case PROP_ISDBT_SB_SEGMENT_COUNT:
    case PROP_ISDBT_LAYERA_FEC:
    case PROP_ISDBT_LAYERA_MODULATION:
    case PROP_ISDBT_LAYERA_SEGMENT_COUNT:
    case PROP_ISDBT_LAYERA_TIME_INTERLEAVING:
    case PROP_ISDBT_LAYERB_FEC:
    case PROP_ISDBT_LAYERB_MODULATION:
    case PROP_ISDBT_LAYERB_SEGMENT_COUNT:
    case PROP_ISDBT_LAYERB_TIME_INTERLEAVING:
    case PROP_ISDBT_LAYERC_FEC:
    case PROP_ISDBT_LAYERC_MODULATION:
    case PROP_ISDBT_LAYERC_SEGMENT_COUNT:
    case PROP_ISDBT_LAYERC_TIME_INTERLEAVING:
    case PROP_LNB_SLOF:
    case PROP_LNB_LOF1:
    case PROP_LNB_LOF2:
    case PROP_INTERLEAVING:
      /* FIXME: check if we can tune (state < PLAYING || program-numbers == "") */
      g_object_set_property (G_OBJECT (dvbbasebin->dvbsrc), pspec->name, value);
      break;
    case PROP_PROGRAM_NUMBERS:
      dvb_base_bin_set_program_numbers (dvbbasebin, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
dvb_base_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (object);

  switch (prop_id) {
    case PROP_ADAPTER:
    case PROP_FRONTEND:
    case PROP_DISEQC_SRC:
    case PROP_FREQUENCY:
    case PROP_POLARITY:
    case PROP_SYMBOL_RATE:
    case PROP_BANDWIDTH:
    case PROP_CODE_RATE_HP:
    case PROP_CODE_RATE_LP:
    case PROP_GUARD:
    case PROP_MODULATION:
    case PROP_TRANS_MODE:
    case PROP_HIERARCHY:
    case PROP_INVERSION:
    case PROP_STATS_REPORTING_INTERVAL:
    case PROP_TUNING_TIMEOUT:
    case PROP_DELSYS:
    case PROP_PILOT:
    case PROP_ROLLOFF:
    case PROP_STREAM_ID:
    case PROP_BANDWIDTH_HZ:
    case PROP_ISDBT_LAYER_ENABLED:
    case PROP_ISDBT_PARTIAL_RECEPTION:
    case PROP_ISDBT_SOUND_BROADCASTING:
    case PROP_ISDBT_SB_SUBCHANNEL_ID:
    case PROP_ISDBT_SB_SEGMENT_IDX:
    case PROP_ISDBT_SB_SEGMENT_COUNT:
    case PROP_ISDBT_LAYERA_FEC:
    case PROP_ISDBT_LAYERA_MODULATION:
    case PROP_ISDBT_LAYERA_SEGMENT_COUNT:
    case PROP_ISDBT_LAYERA_TIME_INTERLEAVING:
    case PROP_ISDBT_LAYERB_FEC:
    case PROP_ISDBT_LAYERB_MODULATION:
    case PROP_ISDBT_LAYERB_SEGMENT_COUNT:
    case PROP_ISDBT_LAYERB_TIME_INTERLEAVING:
    case PROP_ISDBT_LAYERC_FEC:
    case PROP_ISDBT_LAYERC_MODULATION:
    case PROP_ISDBT_LAYERC_SEGMENT_COUNT:
    case PROP_ISDBT_LAYERC_TIME_INTERLEAVING:
    case PROP_LNB_SLOF:
    case PROP_LNB_LOF1:
    case PROP_LNB_LOF2:
    case PROP_INTERLEAVING:
      g_object_get_property (G_OBJECT (dvbbasebin->dvbsrc), pspec->name, value);
      break;
    case PROP_PROGRAM_NUMBERS:
      g_value_set_string (value, dvbbasebin->program_numbers);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static GstPad *
dvb_base_bin_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (element);
  GstPad *pad;
  GstPad *ghost;
  gchar *pad_name;

  GST_DEBUG_OBJECT (dvbbasebin, "New pad requested %s", GST_STR_NULL (name));

  if (dvbbasebin->tsparse == NULL)
    return NULL;

  if (name == NULL)
    name = GST_PAD_TEMPLATE_NAME_TEMPLATE (templ);

  pad = gst_element_request_pad_simple (dvbbasebin->tsparse, name);
  if (pad == NULL)
    return NULL;

  pad_name = gst_pad_get_name (pad);
  ghost = gst_ghost_pad_new (pad_name, pad);
  gst_object_unref (pad);
  g_free (pad_name);
  gst_element_add_pad (element, ghost);

  return ghost;
}

static void
dvb_base_bin_release_pad (GstElement * element, GstPad * pad)
{
  GstGhostPad *ghost;
  GstPad *target;

  g_return_if_fail (GST_IS_DVB_BASE_BIN (element));

  ghost = GST_GHOST_PAD (pad);
  target = gst_ghost_pad_get_target (ghost);
  gst_element_release_request_pad (GST_ELEMENT (GST_DVB_BASE_BIN
          (element)->tsparse), target);
  gst_object_unref (target);

  gst_element_remove_pad (element, pad);
}

static void
dvb_base_bin_reset_pmtlist (DvbBaseBin * dvbbasebin)
{
  CamConditionalAccessPmtFlag flag;
  GList *walk;
  GstMpegtsPMT *pmt;

  walk = dvbbasebin->pmtlist;
  while (walk) {
    if (walk->prev == NULL) {
      if (walk->next == NULL)
        flag = CAM_CONDITIONAL_ACCESS_PMT_FLAG_ONLY;
      else
        flag = CAM_CONDITIONAL_ACCESS_PMT_FLAG_FIRST;
    } else {
      if (walk->next == NULL)
        flag = CAM_CONDITIONAL_ACCESS_PMT_FLAG_LAST;
      else
        flag = CAM_CONDITIONAL_ACCESS_PMT_FLAG_MORE;
    }

    pmt = (GstMpegtsPMT *) walk->data;
    cam_device_set_pmt (dvbbasebin->hwcam, pmt, flag);

    walk = walk->next;
  }

  dvbbasebin->pmtlist_changed = FALSE;
}

static void
dvb_base_bin_init_cam (DvbBaseBin * dvbbasebin)
{
  gint adapter;
  gchar *ca_file;

  g_object_get (dvbbasebin->dvbsrc, "adapter", &adapter, NULL);
  /* TODO: handle multiple cams */
  ca_file = g_strdup_printf ("/dev/dvb/adapter%d/ca0", adapter);
  if (g_file_test (ca_file, G_FILE_TEST_EXISTS)) {
    dvbbasebin->hwcam = cam_device_new ();
    /* device_open() can block up to 5s ! */
    if (!cam_device_open (dvbbasebin->hwcam, ca_file)) {
      GST_ERROR_OBJECT (dvbbasebin, "could not open %s", ca_file);
      cam_device_free (dvbbasebin->hwcam);
      dvbbasebin->hwcam = NULL;
    }
  }

  dvbbasebin->trycam = FALSE;

  g_free (ca_file);
}

static GstStateChangeReturn
dvb_base_bin_change_state (GstElement * element, GstStateChange transition)
{
  DvbBaseBin *dvbbasebin;
  GstStateChangeReturn ret;

  dvbbasebin = GST_DVB_BASE_BIN (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (dvbbasebin->tsparse == NULL) {
        GST_ELEMENT_ERROR (dvbbasebin, CORE, MISSING_PLUGIN, (NULL),
            ("No 'tsparse' element, check your GStreamer installation."));
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_poll_set_flushing (dvbbasebin->poll, FALSE);
      g_rec_mutex_lock (&dvbbasebin->lock);
      gst_task_start (dvbbasebin->task);
      g_rec_mutex_unlock (&dvbbasebin->lock);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_poll_set_flushing (dvbbasebin->poll, TRUE);
      g_rec_mutex_lock (&dvbbasebin->lock);
      gst_task_stop (dvbbasebin->task);
      g_rec_mutex_unlock (&dvbbasebin->lock);
      dvb_base_bin_reset (dvbbasebin);
      break;
    default:
      break;
  }

  return ret;
}

static void
foreach_stream_build_filter (gpointer key, gpointer value, gpointer user_data)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (user_data);
  DvbBaseBinStream *stream = (DvbBaseBinStream *) value;
  gchar *tmp, *pid;

  GST_DEBUG ("stream %d usecount %d", stream->pid, stream->usecount);

  if (stream->usecount > 0) {
    /* TODO: use g_strjoinv FTW */
    tmp = dvbbasebin->filter;
    pid = g_strdup_printf ("%d", stream->pid);
    dvbbasebin->filter = g_strjoin (":", pid, dvbbasebin->filter, NULL);

    g_free (pid);
    g_free (tmp);
  }
}

static void
dvb_base_bin_rebuild_filter (DvbBaseBin * dvbbasebin)
{
  g_hash_table_foreach (dvbbasebin->streams,
      foreach_stream_build_filter, dvbbasebin);

  if (dvbbasebin->filter == NULL)
    /* fix dvbsrc to handle NULL filter */
    dvbbasebin->filter = g_strdup ("");

  GST_INFO_OBJECT (dvbbasebin, "rebuilt filter %s", dvbbasebin->filter);

  /* FIXME: find a way to not add unwanted pids controlled by app */
  g_object_set (dvbbasebin->dvbsrc, "pids", dvbbasebin->filter, NULL);
  g_free (dvbbasebin->filter);
  dvbbasebin->filter = NULL;
}

static void
dvb_base_bin_remove_pmt_streams (DvbBaseBin * dvbbasebin,
    const GstMpegtsPMT * pmt)
{
  gint i;
  DvbBaseBinStream *stream;

  for (i = 0; i < pmt->streams->len; i++) {
    GstMpegtsPMTStream *pmtstream = g_ptr_array_index (pmt->streams, i);

    stream = dvb_base_bin_get_stream (dvbbasebin, pmtstream->pid);
    if (stream == NULL) {
      GST_WARNING_OBJECT (dvbbasebin, "removing unknown stream %d ??",
          pmtstream->pid);
      continue;
    }

    dvb_base_bin_unref_stream (stream);
  }
}

static void
dvb_base_bin_add_pmt_streams (DvbBaseBin * dvbbasebin, const GstMpegtsPMT * pmt)
{
  DvbBaseBinStream *stream;
  gint i;

  for (i = 0; i < pmt->streams->len; i++) {
    GstMpegtsPMTStream *pmtstream = g_ptr_array_index (pmt->streams, i);

    GST_DEBUG ("filtering stream %d stream_type %d", pmtstream->pid,
        pmtstream->stream_type);

    stream = dvb_base_bin_get_stream (dvbbasebin, pmtstream->pid);
    if (stream == NULL)
      stream = dvb_base_bin_add_stream (dvbbasebin, pmtstream->pid);
    dvb_base_bin_ref_stream (stream);
  }
}

static void
dvb_base_bin_activate_program (DvbBaseBin * dvbbasebin,
    DvbBaseBinProgram * program)
{
  DvbBaseBinStream *stream;

  if (program->old_pmt) {
    dvb_base_bin_remove_pmt_streams (dvbbasebin, program->old_pmt);
    dvbbasebin->pmtlist = g_list_remove (dvbbasebin->pmtlist, program->old_pmt);
  }

  /* activate the PMT and PCR streams. If the PCR stream is in the PMT its
   * usecount will be incremented by 2 here and decremented by 2 when the
   * program is deactivated */
  if (!program->pmt_active) {
    stream = dvb_base_bin_get_stream (dvbbasebin, program->pmt_pid);
    if (stream == NULL)
      stream = dvb_base_bin_add_stream (dvbbasebin, program->pmt_pid);
    dvb_base_bin_ref_stream (stream);
    program->pmt_active = TRUE;
  }

  if (program->pmt) {
    guint16 old_pcr_pid;

    old_pcr_pid = program->pcr_pid;
    program->pcr_pid = program->pmt->pcr_pid;
    if (old_pcr_pid != G_MAXUINT16 && old_pcr_pid != program->pcr_pid) {
      dvb_base_bin_unref_stream (dvb_base_bin_get_stream (dvbbasebin,
              old_pcr_pid));
    }

    stream = dvb_base_bin_get_stream (dvbbasebin, program->pcr_pid);
    if (stream == NULL)
      stream = dvb_base_bin_add_stream (dvbbasebin, program->pcr_pid);
    dvb_base_bin_ref_stream (stream);

    dvb_base_bin_add_pmt_streams (dvbbasebin, program->pmt);
    dvbbasebin->pmtlist =
        g_list_append (dvbbasebin->pmtlist, (gpointer) program->pmt);
    dvbbasebin->pmtlist_changed = TRUE;
    program->active = TRUE;
  }

  dvb_base_bin_rebuild_filter (dvbbasebin);
}

static void
dvb_base_bin_deactivate_program (DvbBaseBin * dvbbasebin,
    DvbBaseBinProgram * program)
{
  DvbBaseBinStream *stream;

  stream = dvb_base_bin_get_stream (dvbbasebin, program->pmt_pid);
  if (stream != NULL) {
    dvb_base_bin_unref_stream (stream);
  }

  stream = dvb_base_bin_get_stream (dvbbasebin, program->pcr_pid);
  if (stream != NULL) {
    dvb_base_bin_unref_stream (stream);
  }

  if (program->pmt) {
    dvb_base_bin_remove_pmt_streams (dvbbasebin, program->pmt);
    dvbbasebin->pmtlist = g_list_remove (dvbbasebin->pmtlist, program->pmt);
    dvbbasebin->pmtlist_changed = TRUE;
  }

  dvb_base_bin_rebuild_filter (dvbbasebin);
  program->pmt_active = FALSE;
  program->active = FALSE;
}

static void
dvb_base_bin_handle_message (GstBin * bin, GstMessage * message)
{
  DvbBaseBin *dvbbasebin;

  dvbbasebin = GST_DVB_BASE_BIN (bin);

  /* note: message->src might be a GstPad, so use element cast w/o typecheck */
  if (GST_ELEMENT_CAST (message->src) == dvbbasebin->tsparse) {
    GstMpegtsSection *section = gst_message_parse_mpegts_section (message);

    if (section) {
      switch (GST_MPEGTS_SECTION_TYPE (section)) {
        case GST_MPEGTS_SECTION_PAT:
          dvb_base_bin_pat_info_cb (dvbbasebin, section);
          break;
        case GST_MPEGTS_SECTION_PMT:
          dvb_base_bin_pmt_info_cb (dvbbasebin, section);
          break;
        default:
          break;
      }
      gst_mpegts_section_unref (section);
    }
  }

  /* chain up */
  GST_BIN_CLASS (parent_class)->handle_message (bin, message);
}



static void
dvb_base_bin_pat_info_cb (DvbBaseBin * dvbbasebin, GstMpegtsSection * section)
{
  GPtrArray *pat;
  DvbBaseBinProgram *program;
  DvbBaseBinStream *stream;
  guint old_pmt_pid;
  gint i;
  gboolean rebuild_filter = FALSE;

  if (!(pat = gst_mpegts_section_get_pat (section))) {
    GST_WARNING_OBJECT (dvbbasebin, "got invalid PAT");
    return;
  }

  for (i = 0; i < pat->len; i++) {
    GstMpegtsPatProgram *patp = g_ptr_array_index (pat, i);

    program = dvb_base_bin_get_program (dvbbasebin, patp->program_number);
    if (program == NULL)
      program = dvb_base_bin_add_program (dvbbasebin, patp->program_number);

    old_pmt_pid = program->pmt_pid;
    program->pmt_pid = patp->network_or_program_map_PID;

    if (program->selected) {
      /* PAT update */
      if (old_pmt_pid != G_MAXUINT16 && old_pmt_pid != program->pmt_pid) {
        dvb_base_bin_unref_stream (dvb_base_bin_get_stream (dvbbasebin,
                old_pmt_pid));
      }

      stream = dvb_base_bin_get_stream (dvbbasebin, program->pmt_pid);
      if (stream == NULL)
        stream = dvb_base_bin_add_stream (dvbbasebin, program->pmt_pid);

      dvb_base_bin_ref_stream (stream);

      rebuild_filter = TRUE;
    }
  }
  g_ptr_array_unref (pat);

  if (rebuild_filter)
    dvb_base_bin_rebuild_filter (dvbbasebin);
}

static void
dvb_base_bin_pmt_info_cb (DvbBaseBin * dvbbasebin, GstMpegtsSection * section)
{
  const GstMpegtsPMT *pmt;
  DvbBaseBinProgram *program;
  guint program_number;

  pmt = gst_mpegts_section_get_pmt (section);
  if (G_UNLIKELY (pmt == NULL)) {
    GST_WARNING_OBJECT (dvbbasebin, "Received invalid PMT");
    return;
  }

  program_number = section->subtable_extension;

  program = dvb_base_bin_get_program (dvbbasebin, program_number);
  if (program == NULL) {
    GST_WARNING ("got PMT for program %d but program not in PAT",
        program_number);
    program = dvb_base_bin_add_program (dvbbasebin, program_number);
  }

  program->old_pmt = program->pmt;
  program->old_section = program->section;
  program->pmt = pmt;
  program->section = gst_mpegts_section_ref (section);

  /* activate the program if it's selected and either it's not active or its pmt
   * changed */
  if (program->selected && (!program->active || program->old_pmt != NULL))
    dvb_base_bin_activate_program (dvbbasebin, program);

  if (program->old_pmt) {
    gst_mpegts_section_unref (program->old_section);
    program->old_pmt = NULL;
  }
}

static guint
dvb_base_bin_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
dvb_base_bin_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "dvb", NULL };

  return protocols;
}

static gchar *
dvb_base_bin_uri_get_uri (GstURIHandler * handler)
{
  return g_strdup ("dvb://");
}

static gboolean
dvb_base_bin_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  DvbBaseBin *dvbbasebin = GST_DVB_BASE_BIN (handler);
  GError *err = NULL;
  gchar *location;

  location = gst_uri_get_location (uri);

  if (location == NULL)
    goto no_location;

  /* FIXME: here is where we parse channels.conf */
  if (!set_properties_for_channel (GST_ELEMENT (dvbbasebin), location, &err))
    goto set_properties_failed;

  g_free (location);
  return TRUE;

post_error_and_exit:
  {
    gst_element_message_full (GST_ELEMENT (dvbbasebin), GST_MESSAGE_ERROR,
        err->domain, err->code, g_strdup (err->message), NULL, __FILE__,
        GST_FUNCTION, __LINE__);
    g_propagate_error (error, err);
    return FALSE;
  }
no_location:
  {
    g_set_error (&err, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "No details to DVB URI");
    goto post_error_and_exit;
  }
set_properties_failed:
  {
    g_free (location);
    if (!err)
      g_set_error (&err, GST_URI_ERROR, GST_URI_ERROR_BAD_REFERENCE,
          "Could not find information for channel");
    goto post_error_and_exit;
  }
}

static void
dvb_base_bin_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = dvb_base_bin_uri_get_type;
  iface->get_protocols = dvb_base_bin_uri_get_protocols;
  iface->get_uri = dvb_base_bin_uri_get_uri;
  iface->set_uri = dvb_base_bin_uri_set_uri;
}


static void
dvb_base_bin_program_destroy (gpointer data)
{
  DvbBaseBinProgram *program;

  program = (DvbBaseBinProgram *) data;

  if (program->pmt) {
    program->pmt = NULL;
    gst_mpegts_section_unref (program->section);
  }

  g_free (program);
}

static void
dvb_base_bin_task (DvbBaseBin * basebin)
{
  gint pollres;

  GST_DEBUG_OBJECT (basebin, "In task");

  /* If we haven't tried to open the cam, try now */
  if (G_UNLIKELY (basebin->trycam))
    dvb_base_bin_init_cam (basebin);

  /* poll with timeout */
  pollres = gst_poll_wait (basebin->poll, GST_SECOND / 4);

  if (G_UNLIKELY (pollres == -1)) {
    gst_task_stop (basebin->task);
    return;
  }
  if (basebin->hwcam) {
    cam_device_poll (basebin->hwcam);

    if (basebin->pmtlist_changed) {
      if (cam_device_ready (basebin->hwcam)) {
        GST_DEBUG_OBJECT (basebin, "pmt list changed");
        dvb_base_bin_reset_pmtlist (basebin);
      } else {
        GST_DEBUG_OBJECT (basebin, "pmt list changed but CAM not ready");
      }
    }
  }
}
