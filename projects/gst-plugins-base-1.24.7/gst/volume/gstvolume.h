/* -*- c-basic-offset: 2 -*-
 * vi:si:et:sw=2:sts=8:ts=8:expandtab
 *
 * GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_VOLUME_H__
#define __GST_VOLUME_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/audio/streamvolume.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_VOLUME (gst_volume_get_type())
G_DECLARE_FINAL_TYPE (GstVolume, gst_volume, GST, VOLUME, GstAudioFilter)

/**
 * GstVolume:
 *
 * Opaque data structure.
 */
struct _GstVolume {
  GstAudioFilter element;

  void (*process)(GstVolume*, gpointer, guint);
  void (*process_controlled)(GstVolume*, gpointer, gdouble *, guint, guint);

  gboolean mute;
  gfloat volume;

  gboolean current_mute;
  gdouble current_volume;

  gint64 current_vol_i32;
  gint64 current_vol_i24; /* the _i(nt) values get synchronized with the */
  gint64 current_vol_i16; /* the _i(nt) values get synchronized with the */
  gint64 current_vol_i8;  /* the _i(nt) values get synchronized with the */

  GList *tracklist;
  gboolean negotiated;

  gboolean *mutes;
  guint mutes_count;
  gdouble *volumes;
  guint volumes_count;
};

GST_ELEMENT_REGISTER_DECLARE (volume);

G_END_DECLS

#endif /* __GST_VOLUME_H__ */
