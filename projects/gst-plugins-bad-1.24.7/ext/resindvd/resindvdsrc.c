/* GStreamer
 * Copyright (C) 2008-2009 Jan Schmidt <thaytan@noraisin.net>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gmodule.h>
#include <gst/gst.h>
#include <gst/glib-compat-private.h>
#include <glib/gi18n-lib.h>
#include <gst/video/video.h>
#include <gst/video/navigation.h>
#include <gst/tag/tag.h>

#include "resindvdsrc.h"

GST_DEBUG_CATEGORY_STATIC (rsndvdsrc_debug);
#define GST_CAT_DEFAULT rsndvdsrc_debug

#define DEFAULT_DEVICE "/dev/dvd"
#define DEFAULT_FASTSTART TRUE
#define DEFAULT_LANG "en"

#define GST_FLOW_WOULD_BLOCK GST_FLOW_CUSTOM_SUCCESS

#define CLOCK_BASE 9LL
#define CLOCK_FREQ CLOCK_BASE * 10000

#define MPEGTIME_TO_GSTTIME(time) (gst_util_uint64_scale ((time), \
            GST_MSECOND/10, CLOCK_BASE))
#define GSTTIME_TO_MPEGTIME(time) (gst_util_uint64_scale ((time), \
            CLOCK_BASE, GST_MSECOND/10))

typedef enum
{
  RSN_NAV_RESULT_NONE,
  RSN_NAV_RESULT_HIGHLIGHT,
  RSN_NAV_RESULT_BRANCH,
  RSN_NAV_RESULT_BRANCH_AND_HIGHLIGHT
} RsnNavResult;

typedef enum
{
  RSN_BTN_NONE = 0x00,
  RSN_BTN_LEFT = 0x01,
  RSN_BTN_RIGHT = 0x02,
  RSN_BTN_UP = 0x04,
  RSN_BTN_DOWN = 0x08
} RsnBtnMask;

enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_DEVICE,
  ARG_FASTSTART
};

typedef struct
{
  GstBuffer *buffer;
  GstClockTime ts;
  GstClockTime running_ts;
} RsnDvdPendingNav;

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    // GST_STATIC_CAPS ("video/mpeg,mpegversion=2,systemstream=true")
    GST_STATIC_CAPS ("application/x-resin-dvd")
    );

/* Private seek format for private flushing */
static GstFormat rsndvd_format;
/* Title/chapter formats */
static GstFormat title_format;
static GstFormat chapter_format;

static void rsn_dvdsrc_register_extra (GType rsn_dvdsrc_type);

#define rsn_dvdsrc_parent_class parent_class
G_DEFINE_TYPE_EXTENDED (resinDvdSrc, rsn_dvdsrc, GST_TYPE_BASE_SRC,
    0, rsn_dvdsrc_register_extra (g_define_type_id));

static gboolean read_vts_info (resinDvdSrc * src);

static void rsn_dvdsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void rsn_dvdsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void rsn_dvdsrc_finalize (GObject * object);

static gboolean rsn_dvdsrc_start (GstBaseSrc * bsrc);
static gboolean rsn_dvdsrc_stop (GstBaseSrc * bsrc);
static gboolean rsn_dvdsrc_unlock (GstBaseSrc * bsrc);
static gboolean rsn_dvdsrc_unlock_stop (GstBaseSrc * bsrc);

static gboolean rsn_dvdsrc_is_seekable (GstBaseSrc * bsrc);
static gboolean rsn_dvdsrc_prepare_seek (GstBaseSrc * bsrc,
    GstEvent * event, GstSegment * segment);
static gboolean rsn_dvdsrc_do_seek (GstBaseSrc * bsrc, GstSegment * segment);
static GstStateChangeReturn rsn_dvdsrc_change_state (GstElement * element,
    GstStateChange transition);

static void rsn_dvdsrc_prepare_spu_stream_event (resinDvdSrc * src,
    guint8 logical_stream, guint8 phys_stream, gboolean forced_only);
static void rsn_dvdsrc_prepare_audio_stream_event (resinDvdSrc * src,
    guint8 logical_stream, guint8 phys_stream);
static gboolean rsn_dvdsrc_prepare_streamsinfo_event (resinDvdSrc * src);
static void rsn_dvdsrc_prepare_clut_change_event (resinDvdSrc * src,
    const guint32 * clut);
static void rsn_dvdsrc_update_highlight (resinDvdSrc * src);

static void rsn_dvdsrc_enqueue_nav_block (resinDvdSrc * src,
    GstBuffer * nav_buf, GstClockTime ts);
static void rsn_dvdsrc_activate_nav_block (resinDvdSrc * src,
    GstBuffer * nav_buf);
static void rsn_dvdsrc_clear_nav_blocks (resinDvdSrc * src);
static void rsn_dvdsrc_check_nav_blocks (resinDvdSrc * src);
static void rsn_dvdsrc_schedule_nav_cb (resinDvdSrc * src,
    RsnDvdPendingNav * next_nav);

static GstFlowReturn rsn_dvdsrc_create (GstBaseSrc * bsrc, guint64 offset,
    guint length, GstBuffer ** buf);
static gboolean rsn_dvdsrc_src_event (GstBaseSrc * basesrc, GstEvent * event);
static gboolean rsn_dvdsrc_src_query (GstBaseSrc * basesrc, GstQuery * query);

static GstClockTime ifotime_to_gsttime (dvd_time_t * ifo_time);
static void rsn_dvdsrc_send_commands_changed (resinDvdSrc * src);

static GstClockTime
ifotime_to_gsttime (dvd_time_t * ifo_time)
{
  GstClockTime ts;
  guint frames;

  ts = 36000 * GST_SECOND * ((ifo_time->hour & 0xf0) >> 4);
  ts += 3600 * GST_SECOND * (ifo_time->hour & 0x0f);
  ts += 600 * GST_SECOND * ((ifo_time->minute & 0xf0) >> 4);
  ts += 60 * GST_SECOND * (ifo_time->minute & 0x0f);
  ts += 10 * GST_SECOND * ((ifo_time->second & 0xf0) >> 4);
  ts += GST_SECOND * (ifo_time->second & 0x0f);

  frames = ((ifo_time->frame_u >> 4) & 0x3) * 10;
  frames += (ifo_time->frame_u & 0xf);

  if (ifo_time->frame_u & 0x80)
    ts += GST_SECOND * frames / 30;
  else
    ts += GST_SECOND * frames / 25;

  return ts;
}

static void
rsn_dvdsrc_register_extra (GType rsn_dvdsrc_type)
{
  GST_DEBUG_CATEGORY_INIT (rsndvdsrc_debug, "rsndvdsrc", 0,
      "Resin DVD source element based on libdvdnav");

  rsndvd_format = gst_format_register ("rsndvdsrc-internal",
      "private Resin DVD src format");

  title_format = gst_format_register ("title", "DVD title format");
  chapter_format = gst_format_register ("chapter", "DVD chapter format");
}

static void
rsn_dvdsrc_class_init (resinDvdSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

  gobject_class->finalize = rsn_dvdsrc_finalize;
  gobject_class->set_property = rsn_dvdsrc_set_property;
  gobject_class->get_property = rsn_dvdsrc_get_property;

  gstelement_class->change_state = rsn_dvdsrc_change_state;

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (rsn_dvdsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (rsn_dvdsrc_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (rsn_dvdsrc_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (rsn_dvdsrc_unlock_stop);
  gstbasesrc_class->event = GST_DEBUG_FUNCPTR (rsn_dvdsrc_src_event);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (rsn_dvdsrc_src_query);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (rsn_dvdsrc_is_seekable);
  gstbasesrc_class->prepare_seek_segment =
      GST_DEBUG_FUNCPTR (rsn_dvdsrc_prepare_seek);
  gstbasesrc_class->do_seek = GST_DEBUG_FUNCPTR (rsn_dvdsrc_do_seek);

  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (rsn_dvdsrc_create);

  g_object_class_install_property (gobject_class, ARG_DEVICE,
      g_param_spec_string ("device", "Device", "DVD device location",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, ARG_FASTSTART,
      g_param_spec_boolean ("fast-start", "Fast start",
          "Skip straight to the DVD menu on start", DEFAULT_FASTSTART,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_set_static_metadata (gstelement_class, "Resin DVD Src",
      "Source/DVD", "DVD source element", "Jan Schmidt <thaytan@noraisin.net>");
}

static void
rsn_dvdsrc_init (resinDvdSrc * rsndvdsrc)
{
  const gchar *envvar;

  envvar = g_getenv ("DVDFASTSTART");
  if (envvar)
    rsndvdsrc->faststart = (strcmp (envvar, "0") && strcmp (envvar, "no"));
  else
    rsndvdsrc->faststart = DEFAULT_FASTSTART;

  rsndvdsrc->device = g_strdup (DEFAULT_DEVICE);
  g_mutex_init (&rsndvdsrc->dvd_lock);
  g_mutex_init (&rsndvdsrc->branch_lock);
  rsndvdsrc->branching = FALSE;
  g_cond_init (&rsndvdsrc->still_cond);

  gst_base_src_set_format (GST_BASE_SRC (rsndvdsrc), GST_FORMAT_TIME);
}

static void
rsn_dvdsrc_finalize (GObject * object)
{
  resinDvdSrc *src = RESINDVDSRC (object);

  g_mutex_clear (&src->dvd_lock);
  g_mutex_clear (&src->branch_lock);
  g_cond_clear (&src->still_cond);
  g_free (src->device);

  gst_buffer_replace (&src->alloc_buf, NULL);
  gst_buffer_replace (&src->next_buf, NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
rsn_dvdsrc_unlock (GstBaseSrc * bsrc)
{
  resinDvdSrc *src = RESINDVDSRC (bsrc);

  g_mutex_lock (&src->branch_lock);
  src->branching = TRUE;
  g_cond_broadcast (&src->still_cond);
  g_mutex_unlock (&src->branch_lock);

  return TRUE;
}

static gboolean
rsn_dvdsrc_unlock_stop (GstBaseSrc * bsrc)
{
  resinDvdSrc *src = RESINDVDSRC (bsrc);

  g_mutex_lock (&src->branch_lock);
  src->branching = FALSE;
  g_mutex_unlock (&src->branch_lock);

  return TRUE;
}

static void
rsn_dvdsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  resinDvdSrc *src = RESINDVDSRC (object);

  switch (prop_id) {
    case ARG_DEVICE:
      GST_OBJECT_LOCK (src);
      g_free (src->device);
      if (g_value_get_string (value) == NULL)
        src->device = g_strdup (DEFAULT_DEVICE);
      else
        src->device = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (src);
      break;
    case ARG_FASTSTART:
      GST_OBJECT_LOCK (src);
      src->faststart = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (src);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
rsn_dvdsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  resinDvdSrc *src = RESINDVDSRC (object);

  switch (prop_id) {
    case ARG_DEVICE:
      GST_OBJECT_LOCK (src);
      g_value_set_string (value, src->device);
      GST_OBJECT_UNLOCK (src);
      break;
    case ARG_FASTSTART:
      GST_OBJECT_LOCK (src);
      g_value_set_boolean (value, src->faststart);
      GST_OBJECT_UNLOCK (src);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
rsn_dvdsrc_start (GstBaseSrc * bsrc)
{
  resinDvdSrc *src = RESINDVDSRC (bsrc);
  const gchar *const *langs, *const *cur;
  const char *disc_name;
  gchar lang[8];

  g_mutex_lock (&src->dvd_lock);
  if (!read_vts_info (src)) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        (_("Could not read title information for DVD.")), GST_ERROR_SYSTEM);
    goto fail;
  }

  if (dvdnav_open (&src->dvdnav, src->device) != DVDNAV_STATUS_OK) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        (_("Failed to open DVD device '%s'."), src->device));
    goto fail;
  }

  if (dvdnav_set_PGC_positioning_flag (src->dvdnav, 1) != DVDNAV_STATUS_OK) {
    GST_ELEMENT_ERROR (src, LIBRARY, FAILED,
        (_("Failed to set PGC based seeking.")), GST_ERROR_SYSTEM);
    goto fail;
  }

  /* Attempt to set DVD menu, audio and spu languages */
  langs = g_get_language_names ();
  strncpy (lang, DEFAULT_LANG, 8);
  for (cur = langs; *cur != NULL; cur++) {
    /* Look for a 2 char iso-639 lang */
    if (strlen (*cur) == 2) {
      strncpy (lang, *cur, 8);
      break;
    }
  }
  /* Set the user's preferred language */
  dvdnav_menu_language_select (src->dvdnav, lang);
  dvdnav_audio_language_select (src->dvdnav, lang);
  dvdnav_spu_language_select (src->dvdnav, lang);

  if (src->faststart) {
    if (dvdnav_title_play (src->dvdnav, 1) != DVDNAV_STATUS_OK ||
        (dvdnav_menu_call (src->dvdnav, DVD_MENU_Title) != DVDNAV_STATUS_OK &&
            dvdnav_menu_call (src->dvdnav,
                DVD_MENU_Root) != DVDNAV_STATUS_OK)) {
      /* Fast start failed. Do normal start */
      dvdnav_reset (src->dvdnav);
    }
  }

  /* Get disc name and convert to UTF-8 */
  g_free (src->disc_name);
  dvdnav_get_title_string (src->dvdnav, &disc_name);
  if (disc_name != NULL && *disc_name != '\0')
    src->disc_name = gst_tag_freeform_string_to_utf8 (disc_name, -1, NULL);
  else
    src->disc_name = NULL;

  src->first_seek = TRUE;
  src->running = TRUE;
  src->branching = FALSE;
  src->discont = TRUE;
  src->need_segment = TRUE;
  src->need_tag_update = TRUE;

  src->cur_position = GST_CLOCK_TIME_NONE;
  src->pgc_duration = GST_CLOCK_TIME_NONE;
  src->cur_start_ts = GST_CLOCK_TIME_NONE;
  src->cur_end_ts = GST_CLOCK_TIME_NONE;
  src->cur_vobu_base_ts = GST_CLOCK_TIME_NONE;

  src->vts_n = 0;
  src->in_menu = FALSE;
  src->title_n = -1;
  src->part_n = -1;

  src->active_button = -1;
  src->cur_btn_mask = RSN_BTN_NONE;

  src->angles_changed = FALSE;
  src->n_angles = 0;
  src->cur_angle = 0;

  src->commands_changed = TRUE;

  src->cur_spu_phys_stream = -1;
  src->cur_spu_forced_only = FALSE;
  memset (src->cur_clut, 0, sizeof (guint32) * 16);
  src->cur_audio_phys_stream = -1;

  g_mutex_unlock (&src->dvd_lock);

  return TRUE;

fail:
  if (src->dvdnav) {
    dvdnav_close (src->dvdnav);
    src->dvdnav = NULL;
  }
  g_mutex_unlock (&src->dvd_lock);
  return FALSE;
}

/* Use libdvdread to read and cache info from the IFO file about
 * streams in each VTS */
static gboolean
read_vts_info (resinDvdSrc * src)
{
  gint n_vts;

  if (src->vts_attrs) {
    g_array_free (src->vts_attrs, TRUE);
    src->vts_attrs = NULL;
  }

  if (src->dvdread)
    DVDClose (src->dvdread);

  src->dvdread = DVDOpen (src->device);
  if (src->dvdread == NULL)
    return FALSE;

  if (!(src->vmg_file = ifoOpen (src->dvdread, 0))) {
    GST_ERROR ("Can't open VMG ifo");
    return FALSE;
  }
  if (!src->vmg_file->vts_atrt) {
    GST_INFO ("No vts_atrt - odd, but apparently OK");
    g_array_set_size (src->vts_attrs, 0);
    src->vts_attrs = NULL;
    return TRUE;
  }
  n_vts = src->vmg_file->vts_atrt->nr_of_vtss;
  memcpy (&src->vmgm_attr, src->vmg_file->vmgi_mat, sizeof (vmgi_mat_t));

  GST_DEBUG ("Reading IFO info for %d VTSs", n_vts);
  src->vts_attrs =
      g_array_sized_new (FALSE, TRUE, sizeof (vtsi_mat_t), n_vts + 1);
  if (!src->vts_attrs)
    return FALSE;
  g_array_set_size (src->vts_attrs, n_vts + 1);

  return TRUE;
}

static vtsi_mat_t *
get_vts_attr (resinDvdSrc * src, gint n)
{
  vtsi_mat_t *vts_attr;

  if (src->vts_attrs == NULL || n >= src->vts_attrs->len) {
    if (src->vts_attrs)
      GST_ERROR_OBJECT (src, "No stream info for VTS %d (have %d)", n,
          src->vts_attrs->len);
    else
      GST_ERROR_OBJECT (src, "No stream info");
    return NULL;
  }

  vts_attr = &g_array_index (src->vts_attrs, vtsi_mat_t, src->vts_n);

  /* Check if we have read this VTS ifo yet */
  if (vts_attr->vtsm_vobs == 0) {
    ifo_handle_t *ifo = ifoOpen (src->dvdread, n);

    if (!ifo) {
      GST_ERROR ("Can't open VTS %d", n);
      return NULL;
    }

    GST_DEBUG ("VTS %d, Menu has %d audio %d subpictures. "
        "Title has %d and %d", n,
        ifo->vtsi_mat->nr_of_vtsm_audio_streams,
        ifo->vtsi_mat->nr_of_vtsm_subp_streams,
        ifo->vtsi_mat->nr_of_vts_audio_streams,
        ifo->vtsi_mat->nr_of_vts_subp_streams);

    memcpy (&g_array_index (src->vts_attrs, vtsi_mat_t, n),
        ifo->vtsi_mat, sizeof (vtsi_mat_t));

    ifoClose (ifo);
  };

  return vts_attr;
}

static gboolean
rsn_dvdsrc_stop (GstBaseSrc * bsrc)
{
  resinDvdSrc *src = RESINDVDSRC (bsrc);
  gboolean ret = TRUE;
  GstMessage *mouse_over_msg = NULL;

  g_mutex_lock (&src->dvd_lock);

  if (src->nav_clock_id) {
    gst_clock_id_unschedule (src->nav_clock_id);
    gst_clock_id_unref (src->nav_clock_id);
    src->nav_clock_id = NULL;
  }
  rsn_dvdsrc_clear_nav_blocks (src);
  src->have_pci = FALSE;

  if (src->was_mouse_over) {
    mouse_over_msg =
        gst_navigation_message_new_mouse_over ((GstObject *) src, FALSE);
    src->was_mouse_over = FALSE;
  }

  /* Clear any allocated output buffer */
  gst_buffer_replace (&src->alloc_buf, NULL);
  gst_buffer_replace (&src->next_buf, NULL);
  src->running = FALSE;

  if (src->streams_event) {
    gst_event_unref (src->streams_event);
    src->streams_event = NULL;
  }
  if (src->clut_event) {
    gst_event_unref (src->clut_event);
    src->clut_event = NULL;
  }
  if (src->spu_select_event) {
    gst_event_unref (src->spu_select_event);
    src->spu_select_event = NULL;
  }
  if (src->audio_select_event) {
    gst_event_unref (src->audio_select_event);
    src->audio_select_event = NULL;
  }
  if (src->highlight_event) {
    gst_event_unref (src->highlight_event);
    src->highlight_event = NULL;
  }

  g_free (src->disc_name);
  src->disc_name = NULL;

  if (src->dvdnav) {
    if (dvdnav_close (src->dvdnav) != DVDNAV_STATUS_OK) {
      GST_ELEMENT_ERROR (src, RESOURCE, CLOSE, (NULL),
          ("dvdnav_close failed: %s", dvdnav_err_to_string (src->dvdnav)));
      ret = FALSE;
    }
    src->dvdnav = NULL;
  }

  if (src->vmg_file) {
    ifoClose (src->vmg_file);
    src->vmg_file = NULL;
  }
  if (src->vts_file) {
    ifoClose (src->vts_file);
    src->vts_file = NULL;
  }
  if (src->dvdread) {
    DVDClose (src->dvdread);
    src->dvdread = NULL;
  }

  g_mutex_unlock (&src->dvd_lock);

  if (mouse_over_msg)
    gst_element_post_message (GST_ELEMENT_CAST (src), mouse_over_msg);
  return ret;
}

/* handle still events. Call with dvd_lock */
static gboolean
rsn_dvdsrc_do_still (resinDvdSrc * src, int duration)
{
  GstEvent *still_event;
  GstEvent *hl_event;
  gboolean cmds_changed;
  GstEvent *seg_event;
  GstSegment *segment = &(GST_BASE_SRC (src)->segment);

  if (src->in_still_state == FALSE) {
    GST_DEBUG_OBJECT (src, "**** Start STILL FRAME. Duration %d ****",
        duration);

    if (duration == 255)
      src->still_time_remaining = GST_CLOCK_TIME_NONE;
    else
      src->still_time_remaining = GST_SECOND * duration;

    /* Send a close-segment event, and a still-frame start
     * event, then sleep */
    still_event = gst_video_event_new_still_frame (TRUE);

    segment->stop = segment->position = src->cur_end_ts;
    GST_LOG_OBJECT (src, "Segment position now %" GST_TIME_FORMAT,
        GST_TIME_ARGS (segment->position));

    seg_event = gst_event_new_segment (segment);

    /* Grab any pending highlight event to send too */
    hl_event = src->highlight_event;
    src->highlight_event = NULL;
    cmds_changed = src->commands_changed;
    src->commands_changed = FALSE;

    /* Now, send the events. We need to drop the dvd lock while doing so,
     * and then check after if we got flushed */
    g_mutex_unlock (&src->dvd_lock);
    gst_pad_push_event (GST_BASE_SRC_PAD (src), still_event);
    gst_pad_push_event (GST_BASE_SRC_PAD (src), seg_event);
    if (hl_event) {
      GST_LOG_OBJECT (src, "Sending highlight event before still");
      gst_pad_push_event (GST_BASE_SRC_PAD (src), hl_event);
    }
    if (cmds_changed)
      rsn_dvdsrc_send_commands_changed (src);

    g_mutex_lock (&src->dvd_lock);

    g_mutex_lock (&src->branch_lock);

    src->in_still_state = TRUE;
  } else {
    GST_DEBUG_OBJECT (src,
        "Re-entering still wait with %" GST_TIME_FORMAT " remaining",
        GST_TIME_ARGS (src->still_time_remaining));
    g_mutex_lock (&src->branch_lock);
  }

  if (src->branching) {
    GST_INFO_OBJECT (src, "Branching - aborting still");
    g_mutex_unlock (&src->branch_lock);
    return TRUE;
  }

  if (duration == 255) {
    /*
     * The only way to get woken from this still is by a flushing
     * seek or a user action. Either one will clear the still, so
     * don't skip it
     */
    src->need_segment = TRUE;

    g_mutex_unlock (&src->dvd_lock);
    GST_LOG_OBJECT (src, "Entering cond_wait still");
    g_cond_wait (&src->still_cond, &src->branch_lock);
    GST_LOG_OBJECT (src, "cond_wait still over, branching = %d",
        src->branching);

    if (src->branching) {
      g_mutex_unlock (&src->branch_lock);
      g_mutex_lock (&src->dvd_lock);
      return TRUE;
    }
    src->in_still_state = FALSE;

    g_mutex_unlock (&src->branch_lock);
    g_mutex_lock (&src->dvd_lock);
  } else {
    gboolean was_signalled;

    if (src->still_time_remaining > 0) {
      gint64 end_time;

      end_time =
          g_get_monotonic_time () + src->still_time_remaining / GST_USECOND;

      /* Implement timed stills by sleeping, possibly
       * in multiple steps if we get paused/unpaused */
      g_mutex_unlock (&src->dvd_lock);
      GST_LOG_OBJECT (src, "cond_timed_wait still for %d sec", duration);
      was_signalled =
          g_cond_wait_until (&src->still_cond, &src->branch_lock, end_time);
      was_signalled |= src->branching;

      g_mutex_unlock (&src->branch_lock);
      g_mutex_lock (&src->dvd_lock);

      if (was_signalled) {
        /* Signalled - must be flushing */
        gint64 cur_time;
        GstClockTimeDiff remain;

        cur_time = g_get_monotonic_time ();
        remain = (end_time - cur_time) * GST_USECOND;
        if (remain < 0)
          src->still_time_remaining = 0;
        else
          src->still_time_remaining = remain;

        GST_LOG_OBJECT (src,
            "cond_timed_wait still aborted by signal with %" GST_TIME_FORMAT
            " remaining. branching = %d",
            GST_TIME_ARGS (src->still_time_remaining), src->branching);

        return TRUE;
      }
    }

    /* Else timed out, end the still */
    GST_DEBUG_OBJECT (src,
        "Timed still of %d secs over, calling dvdnav_still_skip", duration);

    if (dvdnav_still_skip (src->dvdnav) != DVDNAV_STATUS_OK) {
      return FALSE;
    }

    /* Tell downstream the still is over.
     * We only do this if the still isn't interrupted: */
    still_event = gst_video_event_new_still_frame (FALSE);

    /* If the segment was too short in a timed still, it may need extending */
    if (segment->position < segment->start + GST_SECOND * duration) {
      segment->position = segment->start + (GST_SECOND * duration);
      if (segment->stop != -1 && segment->position > segment->stop)
        segment->stop = segment->position;

      GST_LOG_OBJECT (src, "Extended segment position to %" GST_TIME_FORMAT,
          GST_TIME_ARGS (segment->position));
    }

    g_mutex_unlock (&src->dvd_lock);
    gst_pad_push_event (GST_BASE_SRC_PAD (src), still_event);
    g_mutex_lock (&src->dvd_lock);
  }

  return TRUE;
}

static pgc_t *
get_current_pgc (resinDvdSrc * src)
{
  gint title, part, pgc_n;
  gint32 vts_ttn;
  pgc_t *pgc;

  if (dvdnav_is_domain_fp (src->dvdnav)) {
    return src->vmg_file->first_play_pgc;
  }

  if (src->vts_n == 0 || src->in_menu) {
    /* FIXME: look up current menu PGC */
    return NULL;
  }

  if (dvdnav_current_title_info (src->dvdnav, &title, &part) !=
      DVDNAV_STATUS_OK)
    return NULL;

  /* To find the right PGC, we need the title number within this VTS (vts_ttn)
   * from the VMG tt_srpt table... */
  if (title < 1 || title > src->vmg_file->tt_srpt->nr_of_srpts)
    return NULL;

  /* We must be in the correct VTS for any of this to succeed... */
  if (src->vts_n != src->vmg_file->tt_srpt->title[title - 1].title_set_nr)
    return NULL;

  /* We must also be in the VTS domain to use the tmap table */
  if (src->vts_n == 0)
    return NULL;

  vts_ttn = src->vmg_file->tt_srpt->title[title - 1].vts_ttn;

  if (vts_ttn < 1 || vts_ttn > src->vts_file->vts_ptt_srpt->nr_of_srpts)
    return NULL;

  if (src->vts_file->vts_ptt_srpt->title[vts_ttn - 1].nr_of_ptts == 0)
    return NULL;

  pgc_n = src->vts_file->vts_ptt_srpt->title[vts_ttn - 1].ptt[0].pgcn;
  if (pgc_n > src->vts_file->vts_pgcit->nr_of_pgci_srp)
    return NULL;

  pgc = src->vts_file->vts_pgcit->pgci_srp[pgc_n - 1].pgc;

  return pgc;
}

static GstTagList *
update_title_info (resinDvdSrc * src, gboolean force)
{
  gint n_angles, cur_agl;
  gint title_n, part_n;

  if (dvdnav_get_angle_info (src->dvdnav, &cur_agl,
          &n_angles) == DVDNAV_STATUS_OK && src->n_angles != n_angles) {
    /* Make sure we send an angles-changed message soon */
    src->angles_changed = TRUE;
  }

  if (dvdnav_current_title_info (src->dvdnav, &title_n,
          &part_n) != DVDNAV_STATUS_OK) {
    if (!src->in_menu)
      return NULL;              /* Can't update now */
    /* Must be in the first play sequence */
    title_n = -1;
    part_n = 0;
  }

  if (title_n != src->title_n || part_n != src->part_n ||
      src->n_angles != n_angles || src->cur_angle != cur_agl || force) {
    gchar *title_str = NULL;

    src->title_n = title_n;
    src->part_n = part_n;
    src->n_angles = n_angles;
    src->cur_angle = cur_agl;

    if (title_n == 0) {
      /* In a menu */
      title_str = g_strdup ("DVD Menu");
    } else if (title_n > 0) {
      /* In a title */
      if (n_angles > 1) {
        title_str = g_strdup_printf ("Title %i, Chapter %i, Angle %i of %i",
            title_n, part_n, cur_agl, n_angles);

      } else {
        title_str = g_strdup_printf ("Title %i, Chapter %i", title_n, part_n);
      }
    }

    if (src->disc_name && src->disc_name[0]) {
      /* We have a name for this disc, publish it */
      if (title_str) {
        gchar *new_title_str =
            g_strdup_printf ("%s, %s", title_str, src->disc_name);
        g_free (title_str);
        title_str = new_title_str;
      } else {
        title_str = g_strdup (src->disc_name);
      }
    }
    if (title_str) {
      GstTagList *tags = gst_tag_list_new (GST_TAG_TITLE, title_str, NULL);
      g_free (title_str);
      return tags;
    }
  }

  return NULL;
}

/* we don't cache the result on purpose */
static gboolean
rsn_descrambler_available (void)
{
  GModule *module;
  gpointer sym;
  gsize res;

  module = g_module_open ("libdvdcss", 0);
  if (module != NULL) {
    res = g_module_symbol (module, "dvdcss_open", &sym);
    g_module_close (module);
  } else {
    res = FALSE;
  }

  return res;
}

static GstFlowReturn
rsn_dvdsrc_step (resinDvdSrc * src, gboolean have_dvd_lock)
{
  GstFlowReturn ret = GST_FLOW_OK;
  dvdnav_status_t dvdnav_ret;
  gint event, len;
  GstMapInfo mmap;

  /* Allocate an output buffer if there isn't a pending one */
  if (src->alloc_buf == NULL)
    src->alloc_buf = gst_buffer_new_allocate (NULL, DVD_VIDEO_LB_LEN, NULL);

  gst_buffer_map (src->alloc_buf, &mmap, GST_MAP_WRITE);

  len = DVD_VIDEO_LB_LEN;

  dvdnav_ret = dvdnav_get_next_block (src->dvdnav, mmap.data, &event, &len);
  if (dvdnav_ret != DVDNAV_STATUS_OK)
    goto read_error;
  g_mutex_lock (&src->branch_lock);
  if (src->branching)
    goto branching;
  g_mutex_unlock (&src->branch_lock);

  switch (event) {
    case DVDNAV_BLOCK_OK:
      /* Data block that needs outputting */
      gst_buffer_unmap (src->alloc_buf, &mmap);
      src->next_buf = src->alloc_buf;
      src->alloc_buf = NULL;

      src->next_is_nav_block = FALSE;
      src->next_nav_ts = GST_CLOCK_TIME_NONE;
      src->in_still_state = FALSE;
      break;
    case DVDNAV_NAV_PACKET:
    {
      pci_t *pci = dvdnav_get_current_nav_pci (src->dvdnav);
      GstClockTime new_start_ptm = MPEGTIME_TO_GSTTIME (pci->pci_gi.vobu_s_ptm);
      GstClockTime new_end_ptm = MPEGTIME_TO_GSTTIME (pci->pci_gi.vobu_e_ptm);
      GstClockTimeDiff new_base_time = ifotime_to_gsttime (&pci->pci_gi.e_eltm);
      gboolean discont = FALSE;

      src->in_still_state = FALSE;

      if (new_start_ptm != src->cur_end_ts) {
        /* Hack because libdvdnav seems to lose a NAV packet during
         * angle block changes, triggering a false discont */
        GstClockTimeDiff diff = GST_CLOCK_DIFF (src->cur_end_ts, new_start_ptm);
        if (src->cur_end_ts == GST_CLOCK_TIME_NONE || diff > 2 * GST_SECOND ||
            diff < 0) {
          discont = TRUE;
          GST_DEBUG_OBJECT (src, "Discont NAV packet start TS %" GST_TIME_FORMAT
              " != end TS %" GST_TIME_FORMAT,
              GST_TIME_ARGS (new_start_ptm), GST_TIME_ARGS (src->cur_end_ts));
        }
      }

      GST_LOG_OBJECT (src, "NAV packet start TS %" GST_TIME_FORMAT
          " end TS %" GST_TIME_FORMAT " base %" GST_STIME_FORMAT " %s",
          GST_TIME_ARGS (new_start_ptm), GST_TIME_ARGS (new_end_ptm),
          GST_STIME_ARGS (new_base_time), discont ? "discont" : "");

#if 0
      g_print ("NAV packet start TS %" GST_TIME_FORMAT
          " end TS %" GST_TIME_FORMAT " base %" G_GINT64_FORMAT " %s\n",
          GST_TIME_ARGS (new_start_ptm), GST_TIME_ARGS (new_end_ptm),
          new_base_time, discont ? "discont" : "");
#endif

      if (discont) {
        GST_DEBUG_OBJECT (src,
            "NAV packet discont: cur_end_ts %" GST_TIME_FORMAT " != "
            " vobu_start_ptm: %" GST_TIME_FORMAT " base %" GST_TIME_FORMAT,
            GST_TIME_ARGS (src->cur_end_ts),
            GST_TIME_ARGS (new_start_ptm), GST_TIME_ARGS (new_base_time));
        src->need_segment = TRUE;
      }

      src->cur_start_ts = new_start_ptm;
      src->cur_end_ts = new_end_ptm;
      src->cur_vobu_base_ts = new_base_time;

      /* NAV packet is also a data block that needs sending */
      gst_buffer_unmap (src->alloc_buf, &mmap);
      src->next_buf = src->alloc_buf;
      src->alloc_buf = NULL;

      if (!src->have_pci || pci->hli.hl_gi.hli_ss != 2) {
        /* Store the nav packet for activation at the right moment
         * if we don't have a packet yet or the info has changed (hli_ss != 2)
         */
        if (pci->hli.hl_gi.hli_s_ptm != 0)
          new_start_ptm = MPEGTIME_TO_GSTTIME (pci->hli.hl_gi.hli_s_ptm);

        src->next_is_nav_block = TRUE;
        src->next_nav_ts = new_start_ptm;
        GST_LOG_OBJECT (src, "Storing NAV pack with TS %" GST_TIME_FORMAT,
            GST_TIME_ARGS (src->next_nav_ts));
      } else {
        src->next_is_nav_block = FALSE;
        src->next_nav_ts = GST_CLOCK_TIME_NONE;
      }
      break;
    }
    case DVDNAV_STOP:
      /* End of the disc. EOS */
      dvdnav_reset (src->dvdnav);
      ret = GST_FLOW_EOS;
      break;
    case DVDNAV_STILL_FRAME:
    {
      dvdnav_still_event_t *info = (dvdnav_still_event_t *) mmap.data;

      if (!have_dvd_lock) {
        /* At a still frame but can't block, handle it later */
        return GST_FLOW_WOULD_BLOCK;
      }

      if (!rsn_dvdsrc_do_still (src, info->length))
        goto internal_error;

      g_mutex_lock (&src->branch_lock);
      if (src->branching)
        goto branching;
      g_mutex_unlock (&src->branch_lock);
      break;
    }
    case DVDNAV_WAIT:
      /* Drain out the queues so that the info on the screen matches
       * the VM state */
      if (have_dvd_lock) {
        /* FIXME: Drain out the queues, by sleeping on the clock or something */
        GST_LOG_OBJECT (src, "****** FIXME: WAIT for queues to drain *****");
      }
      if (dvdnav_wait_skip (src->dvdnav) != DVDNAV_STATUS_OK)
        goto internal_error;
      break;
    case DVDNAV_CELL_CHANGE:{
      dvdnav_cell_change_event_t *event =
          (dvdnav_cell_change_event_t *) mmap.data;
      GstMessage *message;

      src->pgc_duration = MPEGTIME_TO_GSTTIME (event->pgc_length);
      /* event->cell_start has the wrong time - it doesn't handle
       * multi-angle correctly (as of libdvdnav 4.1.3). The current_time()
       * calculates it correctly. */
      src->cur_position =
          MPEGTIME_TO_GSTTIME (dvdnav_get_current_time (src->dvdnav));

      GST_DEBUG_OBJECT (src,
          "CELL change dur now %" GST_TIME_FORMAT " position now %"
          GST_TIME_FORMAT, GST_TIME_ARGS (src->pgc_duration),
          GST_TIME_ARGS (src->cur_position));

      message = gst_message_new_duration_changed (GST_OBJECT (src));
      gst_element_post_message (GST_ELEMENT (src), message);

      rsn_dvdsrc_prepare_streamsinfo_event (src);
      src->need_tag_update = TRUE;

      break;
    }
    case DVDNAV_SPU_CLUT_CHANGE:
      rsn_dvdsrc_prepare_clut_change_event (src, (const guint32 *) mmap.data);
      break;
    case DVDNAV_VTS_CHANGE:{
      dvdnav_vts_change_event_t *event =
          (dvdnav_vts_change_event_t *) mmap.data;

      if (dvdnav_is_domain_vmgm (src->dvdnav)) {
        src->vts_n = 0;
      } else {
        src->vts_n = event->new_vtsN;
        if (src->vts_file) {
          ifoClose (src->vts_file);
          src->vts_file = NULL;
        }
        src->vts_file = ifoOpen (src->dvdread, src->vts_n);
      }

      src->in_menu = !dvdnav_is_domain_vts (src->dvdnav);

      break;
    }
    case DVDNAV_AUDIO_STREAM_CHANGE:{
      dvdnav_audio_stream_change_event_t *event =
          (dvdnav_audio_stream_change_event_t *) mmap.data;

      rsn_dvdsrc_prepare_audio_stream_event (src,
          event->logical, event->physical);
      GST_DEBUG_OBJECT (src, "  physical: %d", event->physical);
      GST_DEBUG_OBJECT (src, "  logical: %d", event->logical);

      break;
    }
    case DVDNAV_SPU_STREAM_CHANGE:{
      dvdnav_spu_stream_change_event_t *event =
          (dvdnav_spu_stream_change_event_t *) mmap.data;
      gint phys_track = event->physical_wide & 0x1f;
      gboolean forced_only = (event->physical_wide & 0x80) ? TRUE : FALSE;

      rsn_dvdsrc_prepare_spu_stream_event (src, event->logical, phys_track,
          forced_only);

      GST_DEBUG_OBJECT (src, "  physical_wide: %x", event->physical_wide);
      GST_DEBUG_OBJECT (src, "  physical_letterbox: %x",
          event->physical_letterbox);
      GST_DEBUG_OBJECT (src, "  physical_pan_scan: %x",
          event->physical_pan_scan);
      GST_DEBUG_OBJECT (src, "  logical: %x", event->logical);
      break;
    }
    case DVDNAV_HIGHLIGHT:{
      GST_DEBUG_OBJECT (src, "highlight change event, button %d",
          ((dvdnav_highlight_event_t *) mmap.data)->buttonN);
      rsn_dvdsrc_update_highlight (src);
      break;
    }
    case DVDNAV_HOP_CHANNEL:
      GST_DEBUG_OBJECT (src, "Channel hop - User action");
      src->need_segment = TRUE;
      break;
    case DVDNAV_NOP:
      break;
    default:
      GST_WARNING_OBJECT (src, "Unknown dvdnav event %d", event);
      break;
  }
  if (src->alloc_buf) {
    gst_buffer_unmap (src->alloc_buf, &mmap);
  }

  if (src->highlight_event && have_dvd_lock && src->in_playing) {
    GstEvent *hl_event = src->highlight_event;

    src->highlight_event = NULL;
    g_mutex_unlock (&src->dvd_lock);
    GST_DEBUG_OBJECT (src, "Sending highlight event - button %d",
        src->active_button);
    gst_pad_push_event (GST_BASE_SRC_PAD (src), hl_event);
    g_mutex_lock (&src->dvd_lock);
  }

  return ret;

/* ERRORS */
read_error:
  {
    gst_buffer_unmap (src->alloc_buf, &mmap);
    if (!rsn_descrambler_available ()) {
      GST_ELEMENT_ERROR (src, RESOURCE, READ,
          (_("Could not read DVD. This may be because the DVD is encrypted "
                  "and a DVD decryption library is not installed.")),
          ("Failed to read next DVD block. Error: %s",
              dvdnav_err_to_string (src->dvdnav)));
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, READ, (_("Could not read DVD.")),
          ("Failed to read next DVD block. Error: %s",
              dvdnav_err_to_string (src->dvdnav)));
    }
    return GST_FLOW_ERROR;
  }
internal_error:
  {
    gst_buffer_unmap (src->alloc_buf, &mmap);
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (_("Could not read DVD.")),
        ("Internal error processing DVD commands. Error: %s",
            dvdnav_err_to_string (src->dvdnav)));
    return GST_FLOW_ERROR;
  }
branching:
  {
    g_mutex_unlock (&src->branch_lock);
    gst_buffer_unmap (src->alloc_buf, &mmap);
    return GST_FLOW_FLUSHING;
  }
}

/* Send app a bus message that the available commands have changed */
static void
rsn_dvdsrc_send_commands_changed (resinDvdSrc * src)
{
  GstMessage *cmds_msg =
      gst_navigation_message_new_commands_changed (GST_OBJECT_CAST (src));
  gst_element_post_message (GST_ELEMENT_CAST (src), cmds_msg);
}

static gboolean
rsn_dvdsrc_handle_cmds_query (resinDvdSrc * src, GstQuery * query)
{
  /* Expand this array if we have more commands in the future: */
  GstNavigationCommand cmds[16];
  gint n_cmds = 0;

  /* Fill out the standard set of commands we support */
  cmds[n_cmds++] = GST_NAVIGATION_COMMAND_DVD_MENU;
  cmds[n_cmds++] = GST_NAVIGATION_COMMAND_DVD_TITLE_MENU;
  cmds[n_cmds++] = GST_NAVIGATION_COMMAND_DVD_ROOT_MENU;
  cmds[n_cmds++] = GST_NAVIGATION_COMMAND_DVD_SUBPICTURE_MENU;
  cmds[n_cmds++] = GST_NAVIGATION_COMMAND_DVD_AUDIO_MENU;
  cmds[n_cmds++] = GST_NAVIGATION_COMMAND_DVD_ANGLE_MENU;
  cmds[n_cmds++] = GST_NAVIGATION_COMMAND_DVD_CHAPTER_MENU;

  g_mutex_lock (&src->dvd_lock);

  /* Multiple angles available? */
  if (src->n_angles > 1) {
    cmds[n_cmds++] = GST_NAVIGATION_COMMAND_PREV_ANGLE;
    cmds[n_cmds++] = GST_NAVIGATION_COMMAND_NEXT_ANGLE;
  }

  /* Add button selection commands if we have them */
  if (src->active_button > 0) {
    /* We have a valid current button */
    cmds[n_cmds++] = GST_NAVIGATION_COMMAND_ACTIVATE;
  }
  /* Check for buttons in each direction */
  if (src->cur_btn_mask & RSN_BTN_LEFT)
    cmds[n_cmds++] = GST_NAVIGATION_COMMAND_LEFT;
  if (src->cur_btn_mask & RSN_BTN_RIGHT)
    cmds[n_cmds++] = GST_NAVIGATION_COMMAND_RIGHT;
  if (src->cur_btn_mask & RSN_BTN_UP)
    cmds[n_cmds++] = GST_NAVIGATION_COMMAND_UP;
  if (src->cur_btn_mask & RSN_BTN_DOWN)
    cmds[n_cmds++] = GST_NAVIGATION_COMMAND_DOWN;
  g_mutex_unlock (&src->dvd_lock);

  gst_navigation_query_set_commandsv (query, n_cmds, cmds);

  return TRUE;
}

static gboolean
rsn_dvdsrc_handle_angles_query (resinDvdSrc * src, GstQuery * query)
{
  gint cur_agl, n_angles;
  gboolean res = FALSE;

  g_mutex_lock (&src->dvd_lock);
  if (dvdnav_get_angle_info (src->dvdnav, &cur_agl,
          &n_angles) == DVDNAV_STATUS_OK) {
    gst_navigation_query_set_angles (query, cur_agl, n_angles);
    res = TRUE;
  }
  g_mutex_unlock (&src->dvd_lock);

  return res;
}

static gboolean
rsn_dvdsrc_handle_navigation_query (resinDvdSrc * src,
    GstNavigationQueryType nq_type, GstQuery * query)
{
  gboolean res;

  GST_LOG_OBJECT (src, "Have Navigation query of type %d", nq_type);

  switch (nq_type) {
    case GST_NAVIGATION_QUERY_COMMANDS:
      res = rsn_dvdsrc_handle_cmds_query (src, query);
      break;
    case GST_NAVIGATION_QUERY_ANGLES:
      res = rsn_dvdsrc_handle_angles_query (src, query);
      break;
    default:
      res = FALSE;
  }

  return res;
}

static GstFlowReturn
rsn_dvdsrc_prepare_next_block (resinDvdSrc * src, gboolean have_dvd_lock)
{
  GstFlowReturn ret;

  /* If buffer already ready, return */
  if (src->next_buf)
    return GST_FLOW_OK;

  do {
    ret = rsn_dvdsrc_step (src, have_dvd_lock);
  }
  while (ret == GST_FLOW_OK && src->next_buf == NULL);

  if (ret == GST_FLOW_WOULD_BLOCK)
    ret = GST_FLOW_OK;

  return ret;
}

static GstFlowReturn
rsn_dvdsrc_create (GstBaseSrc * bsrc, guint64 offset,
    guint length, GstBuffer ** outbuf)
{
  resinDvdSrc *src = RESINDVDSRC (bsrc);
  GstSegment *segment = &(GST_BASE_SRC (src)->segment);
  GstFlowReturn ret;
  GstEvent *streams_event = NULL;
  GstEvent *clut_event = NULL;
  GstEvent *spu_select_event = NULL;
  GstEvent *audio_select_event = NULL;
  GstEvent *highlight_event = NULL;
  GstMessage *angles_msg = NULL;
  GstTagList *tags = NULL;
  gboolean cmds_changed = FALSE;

  *outbuf = NULL;

  g_mutex_lock (&src->dvd_lock);
  ret = rsn_dvdsrc_prepare_next_block (src, TRUE);
  if (ret != GST_FLOW_OK) {
    g_mutex_unlock (&src->dvd_lock);
    return ret;
  }

  streams_event = src->streams_event;
  src->streams_event = NULL;

  spu_select_event = src->spu_select_event;
  src->spu_select_event = NULL;

  audio_select_event = src->audio_select_event;
  src->audio_select_event = NULL;

  clut_event = src->clut_event;
  src->clut_event = NULL;

  if (src->angles_changed) {
    gint cur, agls;
    if (dvdnav_get_angle_info (src->dvdnav, &cur, &agls) == DVDNAV_STATUS_OK) {

      angles_msg =
          gst_navigation_message_new_angles_changed (GST_OBJECT_CAST (src),
          cur, agls);
    }
    src->angles_changed = FALSE;
  }

  cmds_changed = src->commands_changed;
  src->commands_changed = FALSE;

  if (src->need_tag_update) {
    tags = update_title_info (src, FALSE);
    src->need_tag_update = FALSE;
  }

  g_mutex_unlock (&src->dvd_lock);

  /* Push in-band events now that we've dropped the dvd_lock, before
   * we change segment */
  if (streams_event) {
    GST_LOG_OBJECT (src, "Pushing stream layout event");
    gst_pad_push_event (GST_BASE_SRC_PAD (src), streams_event);
  }
  if (clut_event) {
    GST_LOG_OBJECT (src, "Pushing clut event");
    gst_pad_push_event (GST_BASE_SRC_PAD (src), clut_event);
  }
  /* Out of band events */
  if (spu_select_event) {
    GST_LOG_OBJECT (src, "Pushing spu_select event");
    gst_pad_push_event (GST_BASE_SRC_PAD (src), spu_select_event);
  }
  if (audio_select_event) {
    GST_LOG_OBJECT (src, "Pushing audio_select event");
    gst_pad_push_event (GST_BASE_SRC_PAD (src), audio_select_event);
  }

  if (src->need_segment) {
    /* Seamless segment update */
    GstClockTime elapsed_time = 0;

    if (src->cur_position != GST_CLOCK_TIME_NONE)
      elapsed_time += src->cur_position;
    if (src->cur_vobu_base_ts != GST_CLOCK_TIME_NONE)
      elapsed_time += src->cur_vobu_base_ts;

    GST_DEBUG_OBJECT (src,
        "Starting seamless segment update to %" GST_TIME_FORMAT " -> %"
        GST_TIME_FORMAT " VOBU %" GST_TIME_FORMAT " time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (src->cur_start_ts), GST_TIME_ARGS (src->cur_end_ts),
        GST_TIME_ARGS (src->cur_vobu_base_ts), GST_TIME_ARGS (elapsed_time));

    gst_base_src_new_seamless_segment (GST_BASE_SRC (src),
        src->cur_start_ts, -1, elapsed_time);

    src->need_segment = FALSE;
  }

  if (src->cur_end_ts != GST_CLOCK_TIME_NONE) {
    segment->position = src->cur_end_ts;
    if (segment->stop != -1 && segment->position > segment->stop)
      segment->stop = segment->position;

    GST_LOG_OBJECT (src, "Segment position now %" GST_TIME_FORMAT,
        GST_TIME_ARGS (segment->position));
  }

  if (tags) {
    GstEvent *tag_event = gst_event_new_tag (tags);
    gst_pad_push_event (GST_BASE_SRC_PAD (src), tag_event);
    tags = NULL;
  }
  g_mutex_lock (&src->dvd_lock);

  if (src->next_buf != NULL) {
    /* Now that we're in the new segment, we can enqueue any nav packet
     * correctly */
    if (src->next_is_nav_block) {
      rsn_dvdsrc_enqueue_nav_block (src, src->next_buf, src->next_nav_ts);
      src->next_is_nav_block = FALSE;
    }

    *outbuf = src->next_buf;
    src->next_buf = NULL;

    if (src->discont) {
      GST_LOG_OBJECT (src, "Marking discont buffer");
      GST_BUFFER_FLAG_SET (*outbuf, GST_BUFFER_FLAG_DISCONT);
      src->discont = FALSE;
    }
  }

  if (src->in_playing) {
    highlight_event = src->highlight_event;
    src->highlight_event = NULL;
  } else {
    highlight_event = NULL;
  }

  /* Schedule a clock callback for the any pending nav packet */
  rsn_dvdsrc_check_nav_blocks (src);

  g_mutex_unlock (&src->dvd_lock);

  if (highlight_event) {
    GST_LOG_OBJECT (src, "Pushing highlight event with TS %" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_EVENT_TIMESTAMP (highlight_event)));
    gst_pad_push_event (GST_BASE_SRC_PAD (src), highlight_event);
  }

  if (angles_msg) {
    gst_element_post_message (GST_ELEMENT_CAST (src), angles_msg);
  }

  if (cmds_changed)
    rsn_dvdsrc_send_commands_changed (src);

  return ret;
}

static RsnNavResult
rsn_dvdsrc_perform_button_action (resinDvdSrc * src,
    GstNavigationCommand action)
{
  pci_t *pci;
  RsnNavResult result = RSN_NAV_RESULT_NONE;
  int button = 0;
  btni_t *btn_info;

  if (!src->have_pci)
    return RSN_NAV_RESULT_NONE;
  pci = &src->cur_pci;

  if (pci->hli.hl_gi.hli_ss == 0)
    return RSN_NAV_RESULT_NONE; /* No buttons at the moment */

  dvdnav_get_current_highlight (src->dvdnav, &button);

  if (button > pci->hli.hl_gi.btn_ns || button < 1)
    return RSN_NAV_RESULT_NONE; /* No valid button */

  btn_info = pci->hli.btnit + button - 1;

  switch (action) {
    case GST_NAVIGATION_COMMAND_ACTIVATE:
      if (dvdnav_button_activate (src->dvdnav, pci) == DVDNAV_STATUS_OK)
        result = RSN_NAV_RESULT_BRANCH_AND_HIGHLIGHT;
      break;
    case GST_NAVIGATION_COMMAND_LEFT:
      if (dvdnav_left_button_select (src->dvdnav, pci) == DVDNAV_STATUS_OK) {
        if (btn_info->left &&
            pci->hli.btnit[btn_info->left - 1].auto_action_mode)
          result = RSN_NAV_RESULT_BRANCH_AND_HIGHLIGHT;
        else
          result = RSN_NAV_RESULT_HIGHLIGHT;
      }
      break;
    case GST_NAVIGATION_COMMAND_RIGHT:
      if (dvdnav_right_button_select (src->dvdnav, pci) == DVDNAV_STATUS_OK) {
        if (btn_info->right &&
            pci->hli.btnit[btn_info->right - 1].auto_action_mode)
          result = RSN_NAV_RESULT_BRANCH_AND_HIGHLIGHT;
        else
          result = RSN_NAV_RESULT_HIGHLIGHT;
      }
      break;
    case GST_NAVIGATION_COMMAND_DOWN:
      if (dvdnav_lower_button_select (src->dvdnav, pci) == DVDNAV_STATUS_OK) {
        if (btn_info->down &&
            pci->hli.btnit[btn_info->down - 1].auto_action_mode)
          result = RSN_NAV_RESULT_BRANCH_AND_HIGHLIGHT;
        else
          result = RSN_NAV_RESULT_HIGHLIGHT;
      }
      break;
    case GST_NAVIGATION_COMMAND_UP:
      if (dvdnav_upper_button_select (src->dvdnav, pci) == DVDNAV_STATUS_OK) {
        if (btn_info->up && pci->hli.btnit[btn_info->up - 1].auto_action_mode)
          result = RSN_NAV_RESULT_BRANCH_AND_HIGHLIGHT;
        else
          result = RSN_NAV_RESULT_HIGHLIGHT;
      }
      break;
    default:
      break;
  }

  if (result == RSN_NAV_RESULT_HIGHLIGHT) {
    /* If we're *only* changing the highlight, wake up the still condition.
     * If we're branching, that will happen anyway */
    g_cond_broadcast (&src->still_cond);
  }

  return result;
}

static RsnNavResult
rsn_dvdsrc_do_command (resinDvdSrc * src, GstNavigationCommand command)
{
  RsnNavResult result = RSN_NAV_RESULT_NONE;

  switch (command) {
    case GST_NAVIGATION_COMMAND_DVD_MENU:
      if (dvdnav_menu_call (src->dvdnav, DVD_MENU_Escape) == DVDNAV_STATUS_OK)
        result = RSN_NAV_RESULT_BRANCH;
      break;
    case GST_NAVIGATION_COMMAND_DVD_TITLE_MENU:
      if (dvdnav_menu_call (src->dvdnav, DVD_MENU_Title) == DVDNAV_STATUS_OK)
        result = RSN_NAV_RESULT_BRANCH;
      break;
    case GST_NAVIGATION_COMMAND_DVD_ROOT_MENU:
      if (dvdnav_menu_call (src->dvdnav, DVD_MENU_Root) == DVDNAV_STATUS_OK)
        result = RSN_NAV_RESULT_BRANCH;
      break;
    case GST_NAVIGATION_COMMAND_DVD_SUBPICTURE_MENU:
      if (dvdnav_menu_call (src->dvdnav, DVD_MENU_Subpicture) ==
          DVDNAV_STATUS_OK)
        result = RSN_NAV_RESULT_BRANCH;
      break;
    case GST_NAVIGATION_COMMAND_DVD_AUDIO_MENU:
      if (dvdnav_menu_call (src->dvdnav, DVD_MENU_Audio) == DVDNAV_STATUS_OK)
        result = RSN_NAV_RESULT_BRANCH;
      break;
    case GST_NAVIGATION_COMMAND_DVD_ANGLE_MENU:
      if (dvdnav_menu_call (src->dvdnav, DVD_MENU_Angle) == DVDNAV_STATUS_OK)
        result = RSN_NAV_RESULT_BRANCH;
      break;
    case GST_NAVIGATION_COMMAND_DVD_CHAPTER_MENU:
      if (dvdnav_menu_call (src->dvdnav, DVD_MENU_Part) == DVDNAV_STATUS_OK)
        result = RSN_NAV_RESULT_BRANCH;
      break;
    case GST_NAVIGATION_COMMAND_LEFT:
    case GST_NAVIGATION_COMMAND_RIGHT:
    case GST_NAVIGATION_COMMAND_UP:
    case GST_NAVIGATION_COMMAND_DOWN:
    case GST_NAVIGATION_COMMAND_ACTIVATE:
      return rsn_dvdsrc_perform_button_action (src, command);

    case GST_NAVIGATION_COMMAND_PREV_ANGLE:{
      gint32 cur, agls;
      gint new_angle = 0;
      if (dvdnav_get_angle_info (src->dvdnav, &cur, &agls) == DVDNAV_STATUS_OK) {
        if (cur > 0 &&
            dvdnav_angle_change (src->dvdnav, cur - 1) == DVDNAV_STATUS_OK) {
          new_angle = cur - 1;
        } else if (cur == 1 &&
            dvdnav_angle_change (src->dvdnav, agls) == DVDNAV_STATUS_OK) {
          new_angle = agls;
        }
        /* Angle switches are seamless and involve no branching */
        if (new_angle) {
          src->angles_changed = TRUE;
          GST_INFO_OBJECT (src, "Switched to angle %d", new_angle);
        }
      }
      break;
    }
    case GST_NAVIGATION_COMMAND_NEXT_ANGLE:{
      gint32 cur, agls;
      gint new_angle = 0;
      if (dvdnav_get_angle_info (src->dvdnav, &cur, &agls) == DVDNAV_STATUS_OK) {
        if (cur < agls
            && dvdnav_angle_change (src->dvdnav, cur + 1) == DVDNAV_STATUS_OK) {
          new_angle = cur + 1;
        } else if (cur == agls
            && dvdnav_angle_change (src->dvdnav, 1) == DVDNAV_STATUS_OK) {
          new_angle = 1;
        }
        /* Angle switches are seamless and involve no branching */
        if (new_angle) {
          src->angles_changed = TRUE;
          GST_INFO_OBJECT (src, "Switched to angle %d", new_angle);
        }
      }
      break;
    }
    default:
      break;
  }

  return result;
}

static gboolean
rsn_dvdsrc_handle_navigation_event (resinDvdSrc * src, GstEvent * event)
{
  gboolean have_lock = FALSE;
  GstEvent *hl_event = NULL;
  RsnNavResult nav_res = RSN_NAV_RESULT_NONE;
  GstNavigationEventType etype = gst_navigation_event_get_type (event);
  GstMessage *mouse_over_msg = NULL;
  GstMessage *angles_msg = NULL;

  switch (etype) {
    case GST_NAVIGATION_EVENT_KEY_PRESS:{
      const gchar *key;
      if (!gst_navigation_event_parse_key_event (event, &key))
        return FALSE;

      GST_DEBUG ("dvdnavsrc got a keypress: %s", key);

      g_mutex_lock (&src->dvd_lock);
      have_lock = TRUE;
      if (!src->running)
        goto not_running;

      if (g_str_equal (key, "Return")) {
        nav_res = rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_ACTIVATE);
      } else if (g_str_equal (key, "Left")) {
        nav_res = rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_LEFT);
      } else if (g_str_equal (key, "Right")) {
        nav_res = rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_RIGHT);
      } else if (g_str_equal (key, "Up")) {
        nav_res = rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_UP);
      } else if (g_str_equal (key, "Down")) {
        nav_res = rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_DOWN);
      } else if (g_str_equal (key, "m")) {
        nav_res = rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_DVD_MENU);
      } else if (g_str_equal (key, "t")) {
        nav_res =
            rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_DVD_TITLE_MENU);
      } else if (g_str_equal (key, "r")) {
        nav_res =
            rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_DVD_ROOT_MENU);
      } else if (g_str_equal (key, "comma")) {
        gint title = 0;
        gint part = 0;

        if (dvdnav_current_title_info (src->dvdnav, &title, &part)) {
          if (title > 0 && part > 1) {
            dvdnav_prev_pg_search (src->dvdnav);
            nav_res = RSN_NAV_RESULT_BRANCH;
          }
        }
      } else if (g_str_equal (key, "period")) {
        dvdnav_next_pg_search (src->dvdnav);
        nav_res = RSN_NAV_RESULT_BRANCH;
      } else if (g_str_equal (key, "bracketleft")) {
        nav_res =
            rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_PREV_ANGLE);
      } else if (g_str_equal (key, "bracketright")) {
        nav_res =
            rsn_dvdsrc_do_command (src, GST_NAVIGATION_COMMAND_NEXT_ANGLE);
      } else if (key && key[0] >= '1' && key[0] <= '8') {
        gint new_stream = key[0] - '1';
        GST_INFO_OBJECT (src, "Selecting audio stream %d", new_stream);
        rsn_dvdsrc_prepare_audio_stream_event (src, new_stream, new_stream);
      }
      break;
    }
    case GST_NAVIGATION_EVENT_MOUSE_MOVE:{
      gdouble x, y;

      if (!gst_navigation_event_parse_mouse_move_event (event, &x, &y))
        return FALSE;

      g_mutex_lock (&src->dvd_lock);
      have_lock = TRUE;
      if (!src->running)
        goto not_running;

      if (src->have_pci &&
          dvdnav_mouse_select (src->dvdnav, &src->cur_pci, (int) x, (int) y) ==
          DVDNAV_STATUS_OK) {
        nav_res = RSN_NAV_RESULT_HIGHLIGHT;
        if (!src->was_mouse_over) {
          GST_DEBUG_OBJECT (src, "Mouse moved onto a button");
          mouse_over_msg =
              gst_navigation_message_new_mouse_over ((GstObject *) src, TRUE);
          src->was_mouse_over = TRUE;
        }
      } else if (src->was_mouse_over) {
        GST_DEBUG_OBJECT (src, "Mouse moved out of a button");
        mouse_over_msg =
            gst_navigation_message_new_mouse_over ((GstObject *) src, FALSE);
        src->was_mouse_over = FALSE;
      }
      break;
    }
    case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:{
      gdouble x, y;
      gint button;

      if (!gst_navigation_event_parse_mouse_button_event (event, &button, &x,
              &y))
        return FALSE;
      if (button != 1)
        return FALSE;

      GST_DEBUG_OBJECT (src, "Got click at %g, %g", x, y);

      g_mutex_lock (&src->dvd_lock);
      have_lock = TRUE;
      if (!src->running)
        goto not_running;

      if (src->have_pci && dvdnav_mouse_activate (src->dvdnav, &src->cur_pci,
              (int) x, (int) y) == DVDNAV_STATUS_OK) {
        nav_res = RSN_NAV_RESULT_BRANCH_AND_HIGHLIGHT;
      }
      break;
    }
    case GST_NAVIGATION_EVENT_COMMAND:{
      GstNavigationCommand command;

      if (!gst_navigation_event_parse_command (event, &command))
        return FALSE;
      if (command == GST_NAVIGATION_COMMAND_INVALID)
        return FALSE;

      g_mutex_lock (&src->dvd_lock);
      have_lock = TRUE;
      if (!src->running)
        goto not_running;

      GST_LOG_OBJECT (src, "handling navigation command %d", command);
      nav_res = rsn_dvdsrc_do_command (src, command);
      break;
    }
    default:
      return TRUE;
  }

  if (have_lock) {
    gboolean channel_hop = FALSE;
    gboolean cmds_changed;

    if (nav_res != RSN_NAV_RESULT_NONE) {
      if (nav_res == RSN_NAV_RESULT_BRANCH) {
        channel_hop = TRUE;
      } else if (nav_res == RSN_NAV_RESULT_BRANCH_AND_HIGHLIGHT) {
        src->active_highlight = TRUE;
        channel_hop = TRUE;
      }

      rsn_dvdsrc_update_highlight (src);
    }

    if (channel_hop) {
      GstEvent *seek;

      GST_DEBUG_OBJECT (src, "Processing flush and jump");
      g_mutex_lock (&src->branch_lock);
      src->branching = TRUE;
      g_cond_broadcast (&src->still_cond);
      g_mutex_unlock (&src->branch_lock);

      hl_event = src->highlight_event;
      src->highlight_event = NULL;
      src->active_highlight = FALSE;

      g_mutex_unlock (&src->dvd_lock);

      if (hl_event) {
        GST_DEBUG_OBJECT (src, "Sending highlight change event - button: %d",
            src->active_button);
        gst_pad_push_event (GST_BASE_SRC_PAD (src), hl_event);
      }

      /* Send ourselves a seek event to wake everything up and flush */
      seek = gst_event_new_seek (1.0, rsndvd_format, GST_SEEK_FLAG_FLUSH,
          GST_SEEK_TYPE_NONE, -1, GST_SEEK_TYPE_NONE, -1);
      src->flushing_seek = TRUE;
      gst_element_send_event (GST_ELEMENT (src), seek);

      g_mutex_lock (&src->dvd_lock);

      rsn_dvdsrc_update_highlight (src);
    }

    hl_event = src->highlight_event;
    src->highlight_event = NULL;

    if (src->angles_changed) {
      gint cur, agls;
      if (dvdnav_get_angle_info (src->dvdnav, &cur, &agls) == DVDNAV_STATUS_OK) {

        angles_msg =
            gst_navigation_message_new_angles_changed (GST_OBJECT_CAST (src),
            cur, agls);
      }
      src->angles_changed = FALSE;

      src->need_tag_update = TRUE;
    }

    cmds_changed = src->commands_changed;
    src->commands_changed = FALSE;

    g_mutex_unlock (&src->dvd_lock);

    if (hl_event) {
      GST_DEBUG_OBJECT (src, "Sending highlight change event - button: %d",
          src->active_button);
      gst_pad_push_event (GST_BASE_SRC_PAD (src), hl_event);
    }

    if (cmds_changed)
      rsn_dvdsrc_send_commands_changed (src);
  }

  if (mouse_over_msg) {
    gst_element_post_message (GST_ELEMENT_CAST (src), mouse_over_msg);
  }

  if (angles_msg) {
    gst_element_post_message (GST_ELEMENT_CAST (src), angles_msg);
  }

  return TRUE;
not_running:
  if (have_lock)
    g_mutex_unlock (&src->dvd_lock);
  GST_DEBUG_OBJECT (src, "Element not started. Ignoring navigation event");
  return FALSE;
}

static void
rsn_dvdsrc_prepare_audio_stream_event (resinDvdSrc * src, guint8 logical_stream,
    guint8 phys_stream)
{
  GstStructure *s;
  GstEvent *e;

  if (phys_stream == src->cur_audio_phys_stream)
    return;
  src->cur_audio_phys_stream = phys_stream;

  GST_DEBUG_OBJECT (src, "Preparing audio change, phys %d", phys_stream);

  s = gst_structure_new ("application/x-gst-dvd",
      "event", G_TYPE_STRING, "dvd-set-audio-track",
      "logical-id", G_TYPE_INT, (gint) logical_stream,
      "physical-id", G_TYPE_INT, (gint) phys_stream, NULL);

  e = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s);

  if (src->audio_select_event)
    gst_event_unref (src->audio_select_event);
  src->audio_select_event = e;
}

static void
rsn_dvdsrc_prepare_spu_stream_event (resinDvdSrc * src, guint8 logical_stream,
    guint8 phys_stream, gboolean forced_only)
{
  GstStructure *s;
  GstEvent *e;

  if (phys_stream == src->cur_spu_phys_stream &&
      forced_only == src->cur_spu_forced_only) {
    return;
  }
  src->cur_spu_phys_stream = phys_stream;
  src->cur_spu_forced_only = forced_only;

  GST_DEBUG_OBJECT (src, "Preparing SPU change, log %d phys %d forced %d",
      logical_stream, phys_stream, forced_only);

  s = gst_structure_new ("application/x-gst-dvd",
      "event", G_TYPE_STRING, "dvd-set-subpicture-track",
      "logical-id", G_TYPE_INT, (gint) logical_stream,
      "physical-id", G_TYPE_INT, (gint) phys_stream,
      "forced-only", G_TYPE_BOOLEAN, forced_only, NULL);

  e = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s);

  if (src->spu_select_event)
    gst_event_unref (src->spu_select_event);
  src->spu_select_event = e;
}

static gboolean
rsn_dvdsrc_prepare_streamsinfo_event (resinDvdSrc * src)
{
  vtsi_mat_t *vts_attr;
  video_attr_t *v_attr;
  audio_attr_t *a_attrs;
  subp_attr_t *s_attrs;
  gint n_audio, n_subp;
  int8_t cur_audio;
  GstStructure *s;
  GstEvent *e;
  gint i;
  gchar lang_code[3] = { '\0', '\0', '\0' };
  gchar *t;
  gboolean is_widescreen;
  gboolean have_audio;
  gboolean have_subp;

  if (src->vts_n == 0 || src->vts_attrs == NULL) {
    /* VMGM info */
    vts_attr = NULL;
    v_attr = &src->vmgm_attr.vmgm_video_attr;
    a_attrs = &src->vmgm_attr.vmgm_audio_attr;
    n_audio = MIN (1, src->vmgm_attr.nr_of_vmgm_audio_streams);
    s_attrs = &src->vmgm_attr.vmgm_subp_attr;
    n_subp = MIN (1, src->vmgm_attr.nr_of_vmgm_subp_streams);
  } else if (src->in_menu) {
    /* VTSM attrs */
    vts_attr = get_vts_attr (src, src->vts_n);
    v_attr = &vts_attr->vtsm_video_attr;
    a_attrs = &vts_attr->vtsm_audio_attr;
    n_audio = MAX (1, vts_attr->nr_of_vtsm_audio_streams);
    s_attrs = &vts_attr->vtsm_subp_attr;
    n_subp = MAX (1, vts_attr->nr_of_vtsm_subp_streams);
  } else {
    /* VTS domain */
    vts_attr = get_vts_attr (src, src->vts_n);
    v_attr = &vts_attr->vts_video_attr;
    a_attrs = vts_attr->vts_audio_attr;
    n_audio = vts_attr->nr_of_vts_audio_streams;
    s_attrs = vts_attr->vts_subp_attr;
    n_subp = vts_attr->nr_of_vts_subp_streams;
  }

  if (src->vts_n > 0 && vts_attr == NULL)
    return FALSE;

  GST_DEBUG_OBJECT (src, "Preparing streamsinfo for %d audio and "
      "%d subpicture streams", n_audio, n_subp);

  /* build event */
  s = gst_structure_new ("application/x-gst-dvd",
      "event", G_TYPE_STRING, "dvd-lang-codes", NULL);
  e = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s);

  /* video */
  is_widescreen = (v_attr->display_aspect_ratio != 0);
  gst_structure_set (s, "video-pal-format", G_TYPE_BOOLEAN,
      (v_attr->video_format != 0), NULL);
  gst_structure_set (s, "video-widescreen", G_TYPE_BOOLEAN, is_widescreen,
      NULL);

  /* audio */
  cur_audio = dvdnav_get_active_audio_stream (src->dvdnav);

  have_audio = FALSE;
  for (i = 0; i < n_audio; i++) {
    const audio_attr_t *a = a_attrs + i;
    gint phys_id = dvdnav_get_audio_logical_stream (src->dvdnav, (guint) i);

    if (phys_id == -1) {
      GST_DEBUG_OBJECT (src, "No substream ID in map for audio %d. Skipping.",
          i);
      continue;
    }

    GST_DEBUG_OBJECT (src, "mapped logical audio %d to MPEG substream %d",
        i, phys_id);
    /* Force audio stream reselection in case format changed ... */
    if (i == cur_audio) {
      src->cur_audio_phys_stream = -1;
      rsn_dvdsrc_prepare_audio_stream_event (src, i, phys_id);
    }
#if 0
    /* Old test code: Only output A52 streams */
    if (a->audio_format != 0) {
      GST_DEBUG_OBJECT (src, "Ignoring non-A52 stream %d, format %d", i,
          (int) a->audio_format);
      continue;
    }
    if (a->audio_format == 0)
      have_audio = TRUE;
#else
    have_audio = TRUE;
#endif

    GST_DEBUG_OBJECT (src, "Audio stream %d is format %d, substream %d", i,
        (int) a->audio_format, phys_id);

    t = g_strdup_printf ("audio-%d-stream", i);
    gst_structure_set (s, t, G_TYPE_INT, phys_id, NULL);
    g_free (t);

    t = g_strdup_printf ("audio-%d-format", i);
    gst_structure_set (s, t, G_TYPE_INT, (int) a->audio_format, NULL);
    g_free (t);

    /* Check that the language code is flagged and at least somewhat valid
     * before putting it in the output structure */
    if (a->lang_type && a->lang_code > 0x100) {
      t = g_strdup_printf ("audio-%d-language", i);
      lang_code[0] = (a->lang_code >> 8) & 0xff;
      lang_code[1] = a->lang_code & 0xff;
      gst_structure_set (s, t, G_TYPE_STRING, lang_code, NULL);
      g_free (t);

      GST_DEBUG_OBJECT (src, "Audio stream %d is language %s", i, lang_code);
    } else
      GST_DEBUG_OBJECT (src, "Audio stream %d - no language", i);
  }

  if (have_audio == FALSE) {
    /* Always create at least one audio stream of the required type */
    gst_structure_set (s, "audio-0-format", G_TYPE_INT, (int) 0,
        "audio-0-stream", G_TYPE_INT, (int) 0, NULL);
  }

  /* subpictures */
  have_subp = FALSE;
  for (i = 0; i < n_subp; i++) {
    const subp_attr_t *u = s_attrs + i;
    gint phys_id = dvdnav_get_spu_logical_stream (src->dvdnav, (guint) i);

    if (phys_id == -1) {
      GST_DEBUG_OBJECT (src, "No substream ID in map for subpicture %d. "
          "Skipping", i);
      continue;
    }
    have_subp = TRUE;

    GST_DEBUG_OBJECT (src, "mapped logical subpicture %d to MPEG substream %d",
        i, phys_id);

    t = g_strdup_printf ("subpicture-%d-stream", i);
    gst_structure_set (s, t, G_TYPE_INT, (int) phys_id, NULL);
    g_free (t);

    t = g_strdup_printf ("subpicture-%d-format", i);
    gst_structure_set (s, t, G_TYPE_INT, (int) 0, NULL);
    g_free (t);

    t = g_strdup_printf ("subpicture-%d-language", i);
    if (u->type && u->lang_code > 0x100) {
      lang_code[0] = (u->lang_code >> 8) & 0xff;
      lang_code[1] = u->lang_code & 0xff;
      gst_structure_set (s, t, G_TYPE_STRING, lang_code, NULL);
    } else {
      gst_structure_set (s, t, G_TYPE_STRING, "MENU", NULL);
    }
    g_free (t);

    GST_DEBUG_OBJECT (src, "Subpicture stream %d is language %s", i,
        lang_code[0] ? lang_code : "NONE");
  }
  if (!have_subp) {
    /* Always create at least one subpicture stream */
    gst_structure_set (s, "subpicture-0-format", G_TYPE_INT, (int) 0,
        "subpicture-0-language", G_TYPE_STRING, "MENU",
        "subpicture-0-stream", G_TYPE_INT, (int) 0, NULL);
  }

  if (src->streams_event)
    gst_event_unref (src->streams_event);
  src->streams_event = e;

  return TRUE;
}

static void
rsn_dvdsrc_prepare_clut_change_event (resinDvdSrc * src, const guint32 * clut)
{
  GstEvent *event;
  GstStructure *structure;
  gchar name[16];
  int i;

  if (memcmp (src->cur_clut, clut, sizeof (guint32) * 16) == 0)
    return;
  memcpy (src->cur_clut, clut, sizeof (guint32) * 16);

  structure = gst_structure_new ("application/x-gst-dvd",
      "event", G_TYPE_STRING, "dvd-spu-clut-change", NULL);

  /* Create a separate field for each value in the table. */
  for (i = 0; i < 16; i++) {
    sprintf (name, "clut%02d", i);
    gst_structure_set (structure, name, G_TYPE_INT, (int) clut[i], NULL);
  }

  /* Create the DVD event and put the structure into it. */
  event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, structure);

  GST_LOG_OBJECT (src, "preparing clut change event %" GST_PTR_FORMAT, event);

  if (src->clut_event)
    gst_event_unref (src->clut_event);
  src->clut_event = event;
}

/*
 * Check for a new highlighted area, and prepare an spu highlight event if
 * necessary.
 */
static void
rsn_dvdsrc_update_highlight (resinDvdSrc * src)
{
  int button = 0;
  pci_t *pci = &src->cur_pci;
  dvdnav_highlight_area_t area;
  int mode = src->active_highlight ? 1 : 0;
  GstEvent *event = NULL;
  GstStructure *s;

  if (src->have_pci) {
    if (dvdnav_get_current_highlight (src->dvdnav, &button) == DVDNAV_STATUS_OK) {
      GST_LOG_OBJECT (src, "current dvdnav button is %d, we have %d",
          button, src->active_button);
    }

    if (pci->hli.hl_gi.hli_ss == 0 || button < 0) {
      button = 0;
    } else if (button > pci->hli.hl_gi.btn_ns) {
      /* button is out of the range of possible buttons. */
      button = pci->hli.hl_gi.btn_ns;
      dvdnav_button_select (src->dvdnav, &src->cur_pci, button);
    }

    if (button > 0 && dvdnav_get_highlight_area (pci, button, mode,
            &area) != DVDNAV_STATUS_OK) {
      button = 0;
    }
  }

  if (button == 0) {
    /* No highlight available, or no button selected - clear the SPU */
    if (src->active_button != 0) {
      src->active_button = 0;

      s = gst_structure_new ("application/x-gst-dvd", "event",
          G_TYPE_STRING, "dvd-spu-reset-highlight", NULL);
      event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB, s);
      if (src->highlight_event)
        gst_event_unref (src->highlight_event);
      src->highlight_event = event;
      if (src->cur_btn_mask != RSN_BTN_NONE) {
        src->cur_btn_mask = RSN_BTN_NONE;
        src->commands_changed = TRUE;
      }
    }
    return;
  }

  /* Check if we have a new button number, or a new highlight region. */
  if (button != src->active_button ||
      area.sx != src->area.sx || area.sy != src->area.sy ||
      area.ex != src->area.ex || area.ey != src->area.ey ||
      area.palette != src->area.palette) {
    btni_t *btn_info = pci->hli.btnit + button - 1;
    guint32 btn_mask;

    GST_DEBUG_OBJECT (src, "Setting highlight. Button %d @ %d,%d,%d,%d "
        "active %d palette 0x%x (from button %d @ %d,%d,%d,%d palette 0x%x)",
        button, area.sx, area.sy, area.ex, area.ey,
        mode, area.palette,
        src->active_button, src->area.sx, src->area.sy, src->area.ex,
        src->area.ey, src->area.palette);

    memcpy (&(src->area), &area, sizeof (dvdnav_highlight_area_t));

    s = gst_structure_new ("application/x-gst-dvd", "event",
        G_TYPE_STRING, "dvd-spu-highlight",
        "button", G_TYPE_INT, (gint) button,
        "palette", G_TYPE_INT, (gint) area.palette,
        "sx", G_TYPE_INT, (gint) area.sx,
        "sy", G_TYPE_INT, (gint) area.sy,
        "ex", G_TYPE_INT, (gint) area.ex,
        "ey", G_TYPE_INT, (gint) area.ey, NULL);

    event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB, s);

    if (src->active_button < 1) {
      /* When setting the button for the first time, take the
         timestamp into account. */
      gst_structure_set (s, "ts", GST_TYPE_CLOCK_TIME,
          MPEGTIME_TO_GSTTIME (area.pts), NULL);
    }

    src->active_button = button;

    if (src->highlight_event)
      gst_event_unref (src->highlight_event);
    src->highlight_event = event;

    /* Calculate whether the available set of button motions is changed */
    btn_mask = 0;
    if (btn_info->left && btn_info->left != button)
      btn_mask |= RSN_BTN_LEFT;
    if (btn_info->right && btn_info->right != button)
      btn_mask |= RSN_BTN_RIGHT;
    if (btn_info->up && btn_info->up != button)
      btn_mask |= RSN_BTN_UP;
    if (btn_info->down && btn_info->down != button)
      btn_mask |= RSN_BTN_DOWN;

    if (btn_mask != src->cur_btn_mask) {
      src->cur_btn_mask = btn_mask;
      src->commands_changed = TRUE;
    }
  }
}

static void
rsn_dvdsrc_enqueue_nav_block (resinDvdSrc * src, GstBuffer * nav_buf,
    GstClockTime ts)
{
  RsnDvdPendingNav *pend_nav = g_new0 (RsnDvdPendingNav, 1);
  GstSegment *seg = &(GST_BASE_SRC (src)->segment);

  pend_nav->buffer = gst_buffer_ref (nav_buf);
  pend_nav->ts = ts;
  pend_nav->running_ts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, ts);

  if (src->pending_nav_blocks == NULL) {
    src->pending_nav_blocks = src->pending_nav_blocks_end =
        g_slist_append (src->pending_nav_blocks_end, pend_nav);
  } else {
    src->pending_nav_blocks_end =
        g_slist_append (src->pending_nav_blocks_end, pend_nav);
    src->pending_nav_blocks_end = g_slist_next (src->pending_nav_blocks_end);
  }

  GST_LOG_OBJECT (src, "Enqueued nav with TS %" GST_TIME_FORMAT
      " with run ts %" GST_TIME_FORMAT ". %d packs pending",
      GST_TIME_ARGS (ts), GST_TIME_ARGS (pend_nav->running_ts),
      g_slist_length (src->pending_nav_blocks));
}

static void
rsn_dvdsrc_activate_nav_block (resinDvdSrc * src, GstBuffer * nav_buf)
{
  int32_t forced_button;

  {
    GstMapInfo mmap;
    gst_buffer_map (nav_buf, &mmap, GST_MAP_READ);

    navRead_PCI (&src->cur_pci, mmap.data + 0x2d);

    gst_buffer_unmap (nav_buf, &mmap);
  }

  src->have_pci = TRUE;

  forced_button = src->cur_pci.hli.hl_gi.fosl_btnn & 0x3f;
  if (forced_button != 0) {
    GST_DEBUG_OBJECT (src, "Selecting button %d based on nav packet command",
        forced_button);
    dvdnav_button_select (src->dvdnav, &src->cur_pci, forced_button);
  }
  /* highlight might change, let's check */
  rsn_dvdsrc_update_highlight (src);

  if (src->highlight_event && src->in_still_state) {
    GST_LOG_OBJECT (src, "Signalling still condition due to highlight change");
    g_cond_broadcast (&src->still_cond);
  }
}

static void
rsn_dvdsrc_clear_nav_blocks (resinDvdSrc * src)
{
  GST_DEBUG_OBJECT (src, "Clearing %d pending navpacks",
      g_slist_length (src->pending_nav_blocks));

  while (src->pending_nav_blocks) {
    RsnDvdPendingNav *cur = (RsnDvdPendingNav *) src->pending_nav_blocks->data;

    gst_buffer_unref (cur->buffer);
    g_free (cur);

    src->pending_nav_blocks =
        g_slist_delete_link (src->pending_nav_blocks, src->pending_nav_blocks);
  }

  src->pending_nav_blocks_end = NULL;
}

static gboolean
rsn_dvdsrc_nav_clock_cb (GstClock * clock, GstClockTime time, GstClockID id,
    gpointer user_data)
{
  resinDvdSrc *src = (resinDvdSrc *) user_data;
  GstClockTime base_time = gst_element_get_base_time (GST_ELEMENT (src));

  GST_LOG_OBJECT (src, "NAV pack callback for TS %" GST_TIME_FORMAT " at ts %"
      GST_TIME_FORMAT, GST_TIME_ARGS (time),
      GST_TIME_ARGS (gst_clock_get_time (clock) - base_time));

  g_mutex_lock (&src->dvd_lock);

  /* Destroy the clock id that caused this callback */
  if (src->nav_clock_id) {
    gst_clock_id_unref (src->nav_clock_id);
    src->nav_clock_id = NULL;
  }

  while (src->pending_nav_blocks) {
    RsnDvdPendingNav *cur = (RsnDvdPendingNav *) src->pending_nav_blocks->data;

    if (time < base_time + cur->running_ts)
      break;                    /* Next NAV is in the future */

    GST_DEBUG_OBJECT (src, "Activating nav pack with TS %" GST_TIME_FORMAT
        " at running TS %" GST_TIME_FORMAT, GST_TIME_ARGS (cur->ts),
        GST_TIME_ARGS (cur->running_ts));
    rsn_dvdsrc_activate_nav_block (src, cur->buffer);

    gst_buffer_unref (cur->buffer);
    g_free (cur);

    src->pending_nav_blocks =
        g_slist_delete_link (src->pending_nav_blocks, src->pending_nav_blocks);
  }

  if (src->pending_nav_blocks == NULL)
    src->pending_nav_blocks_end = NULL;
  else {
    /* Schedule a next packet, if any */
    RsnDvdPendingNav *next_nav =
        (RsnDvdPendingNav *) src->pending_nav_blocks->data;
    rsn_dvdsrc_schedule_nav_cb (src, next_nav);
  }

  g_mutex_unlock (&src->dvd_lock);

  return TRUE;
}

/* Called with dvd_lock held. NOTE: Releases dvd_lock briefly */
static void
rsn_dvdsrc_schedule_nav_cb (resinDvdSrc * src, RsnDvdPendingNav * next_nav)
{
  GstClock *clock;
  GstClockTime base_ts;

  if (!src->in_playing) {
    GST_LOG_OBJECT (src, "Not scheduling NAV block - state != PLAYING");
    return;                     /* Not in playing state yet */
  }

  GST_OBJECT_LOCK (src);
  clock = GST_ELEMENT_CLOCK (src);
  base_ts = GST_ELEMENT (src)->base_time;

  if (clock == NULL) {
    GST_LOG_OBJECT (src, "Not scheduling NAV block - no clock yet");
    GST_OBJECT_UNLOCK (src);
    return;
  }
  gst_object_ref (clock);

  src->nav_clock_id = gst_clock_new_single_shot_id (clock,
      base_ts + next_nav->running_ts);

  GST_OBJECT_UNLOCK (src);

  GST_LOG_OBJECT (src, "Schedule nav pack for running TS %" GST_TIME_FORMAT,
      GST_TIME_ARGS (next_nav->running_ts));

  g_mutex_unlock (&src->dvd_lock);
  gst_clock_id_wait_async (src->nav_clock_id, rsn_dvdsrc_nav_clock_cb, src,
      NULL);
  gst_object_unref (clock);
  g_mutex_lock (&src->dvd_lock);
}

/* Called with dvd_lock held */
static void
rsn_dvdsrc_check_nav_blocks (resinDvdSrc * src)
{
  RsnDvdPendingNav *next_nav;

  /* Make sure a callback is scheduled for the first nav packet */
  if (src->nav_clock_id != NULL) {
    return;                     /* Something already scheduled */
  }
  if (src->pending_nav_blocks == NULL) {
    return;                     /* No nav blocks available yet */
  }
  if (!src->in_playing)
    return;                     /* Not in playing state yet */

  GST_LOG_OBJECT (src, "Installing NAV callback");
  next_nav = (RsnDvdPendingNav *) src->pending_nav_blocks->data;

  rsn_dvdsrc_schedule_nav_cb (src, next_nav);
}

static gboolean
rsn_dvdsrc_src_event (GstBaseSrc * basesrc, GstEvent * event)
{
  resinDvdSrc *src = RESINDVDSRC (basesrc);
  gboolean res;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NAVIGATION:
      res = rsn_dvdsrc_handle_navigation_event (src, event);
      break;
    case GST_EVENT_SEEK:{
      GstSeekFlags flags;

      GST_LOG_OBJECT (src, "handling seek event");

      gst_event_parse_seek (event, NULL, NULL, &flags, NULL, NULL, NULL, NULL);
      src->flushing_seek = !!(flags & GST_SEEK_FLAG_FLUSH);
      GST_DEBUG_OBJECT (src, "%s seek event",
          src->flushing_seek ? "flushing" : "non-flushing");

      res = GST_BASE_SRC_CLASS (parent_class)->event (basesrc, event);
      break;
    }
    default:
      GST_LOG_OBJECT (src, "handling %s event", GST_EVENT_TYPE_NAME (event));

      res = GST_BASE_SRC_CLASS (parent_class)->event (basesrc, event);
      break;
  }

  return res;
}

static void
rsn_dvdsrc_post_title_info (GstElement * element)
{
  resinDvdSrc *src = RESINDVDSRC (element);
  GstMessage *message;
  GstStructure *s;
  int32_t n, ntitles;
  int res;
  GValue array = { 0 };

  res = dvdnav_get_number_of_titles (src->dvdnav, &ntitles);
  if (res != DVDNAV_STATUS_OK) {
    GST_WARNING_OBJECT (src, "Failed to get number of titles: %d", res);
    return;
  }

  g_value_init (&array, GST_TYPE_ARRAY);

  s = gst_structure_new ("application/x-gst-dvd", "event",
      G_TYPE_STRING, "dvd-title-info", NULL);

  for (n = 0; n < ntitles; ++n) {
    uint64_t *times, duration;
    uint32_t nchapters;
    GValue item = { 0 };

    g_value_init (&item, G_TYPE_UINT64);

    nchapters =
        dvdnav_describe_title_chapters (src->dvdnav, n, &times, &duration);
    if (nchapters == 0) {
      GST_WARNING_OBJECT (src, "Failed to get title %d info", n);
      g_value_set_uint64 (&item, GST_CLOCK_TIME_NONE);
    } else {
      g_value_set_uint64 (&item, gst_util_uint64_scale (duration, GST_SECOND,
              90000));
      free (times);
    }
    gst_value_array_append_value (&array, &item);
    g_value_unset (&item);
  }
  gst_structure_set_value (s, "title-durations", &array);
  g_value_unset (&array);

  message = gst_message_new_element (GST_OBJECT (src), s);
  gst_element_post_message (GST_ELEMENT_CAST (src), message);
}

static GstStateChangeReturn
rsn_dvdsrc_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  resinDvdSrc *src = RESINDVDSRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      GST_DEBUG_OBJECT (element, "Switching to PAUSED");
      /* Unschedule any NAV packet callback */
      g_mutex_lock (&src->dvd_lock);
      src->in_playing = FALSE;
      if (src->nav_clock_id) {
        gst_clock_id_unschedule (src->nav_clock_id);
        gst_clock_id_unref (src->nav_clock_id);
        src->nav_clock_id = NULL;
      }
      g_mutex_unlock (&src->dvd_lock);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_DEBUG_OBJECT (element, "Switching to PLAYING");
      /* Kick off the NAV packet callback if needed */
      g_mutex_lock (&src->dvd_lock);
      src->in_playing = TRUE;
      rsn_dvdsrc_check_nav_blocks (src);
      g_mutex_unlock (&src->dvd_lock);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      rsn_dvdsrc_post_title_info (element);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
rsn_dvdsrc_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  resinDvdSrc *src = RESINDVDSRC (basesrc);
  gboolean res = FALSE;
  GstFormat format;
  gint64 val;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
      gst_query_parse_duration (query, &format, NULL);
      g_mutex_lock (&src->dvd_lock);
      if (!src->running) {
        g_mutex_unlock (&src->dvd_lock);
        break;
      }

      if (format == GST_FORMAT_TIME) {
        if (src->pgc_duration != GST_CLOCK_TIME_NONE) {
          val = src->pgc_duration;

          GST_DEBUG_OBJECT (src, "duration : %" GST_TIME_FORMAT,
              GST_TIME_ARGS (val));
          gst_query_set_duration (query, format, val);
          res = TRUE;
        }
      } else if (format == title_format) {
        gint32 titles;

        if (dvdnav_get_number_of_titles (src->dvdnav,
                &titles) == DVDNAV_STATUS_OK) {
          val = titles;
          gst_query_set_duration (query, format, val);
          res = TRUE;
        }
      } else if (format == chapter_format) {
        gint32 title, chapters, x;

        if (dvdnav_current_title_info (src->dvdnav, &title,
                &x) == DVDNAV_STATUS_OK) {
          if (dvdnav_get_number_of_parts (src->dvdnav, title,
                  &chapters) == DVDNAV_STATUS_OK) {
            val = chapters;
            gst_query_set_duration (query, format, val);
            res = TRUE;
          }
        }
      }
      g_mutex_unlock (&src->dvd_lock);
      break;
    case GST_QUERY_POSITION:
      gst_query_parse_position (query, &format, NULL);

      g_mutex_lock (&src->dvd_lock);
      if (!src->running) {
        g_mutex_unlock (&src->dvd_lock);
        break;
      }
      if (format == title_format) {
        gint32 title, chapter;

        if (dvdnav_current_title_info (src->dvdnav, &title,
                &chapter) == DVDNAV_STATUS_OK) {
          val = title;
          gst_query_set_position (query, format, val);
          res = TRUE;
        }
      } else if (format == chapter_format) {
        gint32 title, chapter = -1;

        if (dvdnav_current_title_info (src->dvdnav, &title,
                &chapter) == DVDNAV_STATUS_OK) {
          val = chapter;
          gst_query_set_position (query, format, val);
          res = TRUE;
        }
      }
      g_mutex_unlock (&src->dvd_lock);
      break;
    case GST_QUERY_CUSTOM:
    {
      GstNavigationQueryType nq_type = gst_navigation_query_get_type (query);
      if (nq_type != GST_NAVIGATION_QUERY_INVALID)
        res = rsn_dvdsrc_handle_navigation_query (src, nq_type, query);
      else
        res = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);
      break;
    }
    case GST_QUERY_SCHEDULING:
    {
      /* Make sure we operate in pull mode */
      gst_query_set_scheduling (query, GST_SCHEDULING_FLAG_SEQUENTIAL, 1, -1,
          0);
      gst_query_add_scheduling_mode (query, GST_PAD_MODE_PUSH);

      res = TRUE;
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);
      break;
  }

  return res;
}

static gboolean
rsn_dvdsrc_is_seekable (GstBaseSrc * bsrc)
{
  return TRUE;
}

static gboolean
rsn_dvdsrc_prepare_seek (GstBaseSrc * bsrc, GstEvent * event,
    GstSegment * segment)
{
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  GstSeekFlags flags;
  GstFormat seek_format;
  gdouble rate;
  gboolean update;
  gboolean ret;

  gst_event_parse_seek (event, &rate, &seek_format, &flags,
      &cur_type, &cur, &stop_type, &stop);

  /* Don't allow bytes seeks - angle, time, chapter, title only is the plan */
  if (seek_format == GST_FORMAT_BYTES)
    return FALSE;

  if (seek_format == rsndvd_format || seek_format == title_format ||
      seek_format == chapter_format) {
    /* Seeks in our internal formats are passed directly through to the do_seek
     * method. */
    gst_segment_init (segment, seek_format);
    gst_segment_do_seek (segment, rate, seek_format, flags, cur_type, cur,
        stop_type, stop, &update);

    return TRUE;
  }

  /* Let basesrc handle other formats */
  ret = GST_BASE_SRC_CLASS (parent_class)->prepare_seek_segment (bsrc,
      event, segment);

  return ret;
}

/* Find sector from time using time map if available */
static gint
rsn_dvdsrc_get_sector_from_time_tmap (resinDvdSrc * src, GstClockTime ts)
{
  vts_tmapt_t *vts_tmapt;
  vts_tmap_t *title_tmap;
  gint32 title, part, vts_ttn;
  guint32 entry, sector, logical_sector;
  gint cell_n;
  pgc_t *pgc;

  if (ts == 0)
    return 0;

  if (src->vts_file == NULL)
    return -1;

  if (dvdnav_current_title_info (src->dvdnav, &title, &part) !=
      DVDNAV_STATUS_OK)
    return -1;

  vts_tmapt = src->vts_file->vts_tmapt;
  if (vts_tmapt == NULL)
    return -1;

  /* To find the right tmap, we need the title number within this VTS (vts_ttn)
   * from the VMG tt_srpt table... */
  if (title < 1 || title > src->vmg_file->tt_srpt->nr_of_srpts)
    return -1;

  /* We must be in the correct VTS for any of this to succeed... */
  if (src->vts_n != src->vmg_file->tt_srpt->title[title - 1].title_set_nr)
    return -1;

  /* We must also be in the VTS domain to use the tmap table */
  if (src->vts_n == 0 || src->in_menu)
    return -1;

  vts_ttn = src->vmg_file->tt_srpt->title[title - 1].vts_ttn;

  GST_DEBUG_OBJECT (src, "Seek to time %" GST_TIME_FORMAT
      " in VTS %d title %d (vts_ttn %d of %d)",
      GST_TIME_ARGS (ts), src->vts_n, title, vts_ttn, vts_tmapt->nr_of_tmaps);

  if (vts_ttn < 1 || vts_ttn > vts_tmapt->nr_of_tmaps)
    return -1;

  pgc = get_current_pgc (src);
  if (pgc == NULL)
    return -1;

  /* Get the time map */
  title_tmap = vts_tmapt->tmap + vts_ttn - 1;
  if (title_tmap->tmu == 0)
    return -1;

  entry = ts / (title_tmap->tmu * GST_SECOND);
  if (entry == 0)
    return 0;

  if (entry < 1 || entry > title_tmap->nr_of_entries)
    return -1;

  sector = title_tmap->map_ent[entry - 1] & 0x7fffffff;

  GST_LOG_OBJECT (src, "Got sector %u for time seek (entry %d of %d)",
      sector, entry, title_tmap->nr_of_entries);

  /* Sector is now an absolute sector within the current VTS, but
   * dvdnav_sector_search expects a logical sector within the current PGC...
   * which means iterating over the cells of the current PGC until we find
   * the cell that contains the time and sector we want, accumulating
   * the logical sector offsets until we find it
   */
  logical_sector = 0;
  for (cell_n = 0; cell_n < pgc->nr_of_cells; cell_n++) {
    cell_playback_t *cell = pgc->cell_playback + cell_n;

    /* This matches how libdvdnav calculates the logical sector
     * in dvdnav_sector_search(): */

    if (sector >= cell->first_sector && sector <= cell->last_sector) {
      logical_sector += sector - cell->first_sector;
      break;
    }

    if (cell->block_type == BLOCK_TYPE_ANGLE_BLOCK &&
        cell->block_mode != BLOCK_MODE_FIRST_CELL)
      continue;

    logical_sector += (cell->last_sector - cell->first_sector + 1);
  }

  GST_DEBUG_OBJECT (src, "Mapped sector %u onto PGC relative sector %u",
      sector, logical_sector);

  return logical_sector;
}

/* call with DVD lock held */
static gboolean
rsn_dvdsrc_seek_to_time (resinDvdSrc * src, GstClockTime ts)
{
  gint sector;
  dvdnav_status_t res;

  GST_DEBUG_OBJECT (src, "Time seek requested to ts %" GST_TIME_FORMAT,
      GST_TIME_ARGS (ts));

  sector = rsn_dvdsrc_get_sector_from_time_tmap (src, ts);
  if (sector < 0)
    return FALSE;

  src->discont = TRUE;
  res = dvdnav_sector_search (src->dvdnav, sector, SEEK_SET);

  if (res != DVDNAV_STATUS_OK)
    return FALSE;

  return TRUE;
}

static gboolean
rsn_dvdsrc_do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  resinDvdSrc *src = RESINDVDSRC (bsrc);
  gboolean ret = FALSE;

  if (segment->format == rsndvd_format || src->first_seek) {
    /* The internal format has already served its purpose of waking
     * everything up and flushing, we just need to step to the next
     * data block (below) so we know our new position */
    ret = TRUE;
    /* HACK to make initial seek work: */
    src->first_seek = FALSE;
  } else {
    /* Handle other formats: Time, title, chapter, angle */
    if (segment->format == GST_FORMAT_TIME) {
      g_mutex_lock (&src->dvd_lock);
      src->discont = TRUE;
      ret = rsn_dvdsrc_seek_to_time (src, segment->start);
      g_mutex_unlock (&src->dvd_lock);
    } else if (segment->format == title_format) {
      gint titles;

      g_mutex_lock (&src->dvd_lock);
      if (src->running &&
          dvdnav_get_number_of_titles (src->dvdnav,
              &titles) == DVDNAV_STATUS_OK) {
        if (segment->start > 0 && segment->start <= titles) {
          dvdnav_title_play (src->dvdnav, segment->start);
          ret = TRUE;
          src->discont = TRUE;
        }
      }
      g_mutex_unlock (&src->dvd_lock);
    } else if (segment->format == chapter_format) {
      g_mutex_lock (&src->dvd_lock);
      if (src->running) {
        gint32 title, chapters, x;
        if (dvdnav_current_title_info (src->dvdnav, &title, &x) ==
            DVDNAV_STATUS_OK) {
          if (segment->start + 1 == x) {
            /* if already on the first part, don't try to get before it */
            if (segment->start == 0) {
              dvdnav_part_play (src->dvdnav, title, 1);
            } else {
              dvdnav_prev_pg_search (src->dvdnav);
            }
            ret = TRUE;
            src->discont = TRUE;
          } else if (segment->start == x + 1) {
            dvdnav_next_pg_search (src->dvdnav);
            ret = TRUE;
            src->discont = TRUE;
          } else if (dvdnav_get_number_of_parts (src->dvdnav, title,
                  &chapters) == DVDNAV_STATUS_OK) {
            if (segment->start > 0 && segment->start <= chapters) {
              dvdnav_part_play (src->dvdnav, title, segment->start);
              ret = TRUE;
              src->discont = TRUE;
            }
          }
        }
      }
      g_mutex_unlock (&src->dvd_lock);
    }
  }

  if (ret) {
    /* Force a highlight update */
    src->active_button = -1;

    if (src->flushing_seek) {
      GstMessage *mouse_over_msg = NULL;
      g_mutex_lock (&src->dvd_lock);
      src->flushing_seek = FALSE;

      gst_buffer_replace (&src->next_buf, NULL);
      src->cur_start_ts = GST_CLOCK_TIME_NONE;
      src->cur_end_ts = GST_CLOCK_TIME_NONE;
      src->cur_vobu_base_ts = GST_CLOCK_TIME_NONE;
      src->have_pci = FALSE;
      if (src->nav_clock_id) {
        gst_clock_id_unschedule (src->nav_clock_id);
        gst_clock_id_unref (src->nav_clock_id);
        src->nav_clock_id = NULL;
      }
      rsn_dvdsrc_clear_nav_blocks (src);
      if (src->was_mouse_over) {
        mouse_over_msg =
            gst_navigation_message_new_mouse_over ((GstObject *) src, FALSE);
        src->was_mouse_over = FALSE;
      }
      g_mutex_unlock (&src->dvd_lock);

      if (mouse_over_msg)
        gst_element_post_message (GST_ELEMENT_CAST (src), mouse_over_msg);
    }

    GST_LOG_OBJECT (src, "Entering prepare_next_block after seek."
        " Flushing = %d", src->flushing_seek);
    while (src->cur_start_ts == GST_CLOCK_TIME_NONE) {
      if (rsn_dvdsrc_prepare_next_block (src, FALSE) != GST_FLOW_OK)
        goto fail;
      if (src->cur_start_ts == GST_CLOCK_TIME_NONE)
        gst_buffer_replace (&src->next_buf, NULL);
    }
    GST_LOG_OBJECT (src, "prepare_next_block after seek done");

    segment->format = GST_FORMAT_TIME;
    /* The first TS output: */
    segment->position = segment->start = src->cur_start_ts;
    GST_LOG_OBJECT (src, "Segment position now %" GST_TIME_FORMAT,
        GST_TIME_ARGS (segment->position));

    /* time field = position is the 'logical' stream time here: */
    segment->time = 0;
    if (src->cur_position != GST_CLOCK_TIME_NONE)
      segment->time += src->cur_position;
    if (src->cur_vobu_base_ts != GST_CLOCK_TIME_NONE)
      segment->time += src->cur_vobu_base_ts;

    segment->stop = -1;
    segment->duration = -1;

    GST_DEBUG_OBJECT (src, "seek completed. New start TS %" GST_TIME_FORMAT
        " pos %" GST_TIME_FORMAT " (offset %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (segment->start), GST_TIME_ARGS (segment->time),
        GST_TIME_ARGS ((GstClockTimeDiff) (segment->start - segment->time)));

    src->need_segment = FALSE;
  }

  return ret;
fail:
  GST_DEBUG_OBJECT (src, "Seek in format %d failed", segment->format);
  return FALSE;
}
