/* GStreamer
 * Copyright (C) 2008 Jan Schmidt <thaytan@noraisin.net>
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
#ifndef __RESINDVDSRC_H__
#define __RESINDVDSRC_H__

#include <gst/gst.h>

#include <gst/base/gstbasesrc.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <dvdnav/dvdnav.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>

G_BEGIN_DECLS

#define RESIN_TYPE_DVDSRC (rsn_dvdsrc_get_type())
#define RESINDVDSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),RESIN_TYPE_DVDSRC,resinDvdSrc))
#define RESINDVDSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),RESIN_TYPE_DVDSRC,resinDvdSrcClass))
#define IS_RESINDVDSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),RESIN_TYPE_DVDSRC))
#define IS_RESINDVDSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),RESIN_TYPE_DVDSRC))

typedef struct _resinDvdSrc      resinDvdSrc;
typedef struct _resinDvdSrcClass resinDvdSrcClass;

struct _resinDvdSrc
{
  GstBaseSrc parent;

  gboolean	faststart;

  GMutex	dvd_lock;
  GCond		still_cond;
  GMutex	branch_lock;
  gboolean	branching;

  gchar		*device;
  dvdnav_t	*dvdnav;

  gchar     *disc_name;

  /* dvd_reader instance is used to load and cache VTS/VMG ifo info */
  dvd_reader_t  *dvdread;

  /* vmgi_mat_t from the VMG ifo: */
  vmgi_mat_t                vmgm_attr;        /* VMGM domain info             */
  /* Array of cached vtsi_mat_t strctures from each IFO: */
  GArray                   *vts_attrs;

  ifo_handle_t              *vmg_file;
  ifo_handle_t              *vts_file;

  /* Current playback location: VTS 0 = VMG, plus in_menu or not */
  gint		vts_n;
  gboolean	in_menu;
  gint          title_n; /* Title num */
  gint          part_n; /* Part num */
  gint          n_angles; /* number of angles */
  gint          cur_angle; /* current angle */

  gboolean	running;
  gboolean	discont;
  gboolean	first_seek;
  gboolean	flushing_seek;
  gboolean	need_segment;
  gboolean	need_tag_update;
  gboolean	active_highlight;
  gboolean      in_still_state;
  gboolean      in_playing;

  gboolean      was_mouse_over;

  /* Remaining time to wait in a timed still: */
  GstClockTime  still_time_remaining;

  GstBuffer	*alloc_buf;
  GstBuffer	*next_buf;
  /* TRUE if the next_buf is a nav block that needs enqueueing */
  gboolean      next_is_nav_block;
  /* PTS for activating the pending nav block in next_buf */
  GstClockTime  next_nav_ts;
  /* Track accumulated segment position, cleared by flushing */
  GstSegment    src_segment;

  /* Start timestamp of the previous NAV block */
  GstClockTime  cur_start_ts;
  /* End timestamp of the previous NAV block */
  GstClockTime  cur_end_ts;
  /* base ts is cur_start_ts - cell_time for each VOBU */
  GstClockTime  cur_vobu_base_ts;
  /* Position info of the previous NAV block */
  GstClockTime  cur_position;
  /* Duration of the current PGC */
  GstClockTime  pgc_duration;

  gint          active_button;
  dvdnav_highlight_area_t area;

  /* Pending events to output */
  GstEvent	*streams_event;
  GstEvent	*clut_event;
  GstEvent	*spu_select_event;
  GstEvent	*audio_select_event;
  GstEvent	*highlight_event;

  gboolean      angles_changed;
  gboolean      commands_changed;

  /* GList of NAV packets awaiting activation, and the
   * running times to activate them. */
  GSList *pending_nav_blocks;
  GSList *pending_nav_blocks_end;

  GstClockID nav_clock_id;

  gboolean have_pci;
  pci_t cur_pci;

  /* Current state tracking */
  gint8         cur_audio_phys_stream;
  gint8         cur_spu_phys_stream;
  gboolean      cur_spu_forced_only;
  guint32       cur_clut[16];

  guint32       cur_btn_mask;
};

struct _resinDvdSrcClass 
{
  GstBaseSrcClass parent_class;
};

GType rsn_dvdsrc_get_type (void);

G_END_DECLS

#endif /* __RESINDVDSRC_H__ */
