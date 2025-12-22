/* GStreamer AccurateRip (TM) audio checksumming element
 *
 * Copyright (C) 2012 Christophe Fergeau <teuf@gnome.org>
 *
 * Based on the GStreamer chromaprint audio fingerprinting element
 * Copyright (C) 2006 M. Derezynski
 * Copyright (C) 2008 Eric Buehl
 * Copyright (C) 2008 Sebastian Dröge <slomo@circular-chaos.org>
 * Copyright (C) 2011 Lukáš Lalinský <<user@hostname.org>>
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

#ifndef __GST_ACCURIP_H__
#define __GST_ACCURIP_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/audio/gstaudiofilter.h>
#include <gst/audio/audio.h>

G_BEGIN_DECLS

#define GST_TYPE_ACCURIP \
  (gst_accurip_get_type())
#define GST_ACCURIP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ACCURIP,GstAccurip))
#define GST_ACCURIP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ACCURIP,GstAccuripClass))
#define GST_IS_ACCURIP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ACCURIP))
#define GST_IS_ACCURIP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ACCURIP))

#define GST_TAG_ACCURIP_CRC    "accurip-crc"
#define GST_TAG_ACCURIP_CRC_V2 "accurip-crcv2"

typedef struct _GstAccurip      GstAccurip;
typedef struct _GstAccuripClass GstAccuripClass;

/**
 * GstAccurip:
 *
 * Opaque #GstAccurip element structure
 */
struct _GstAccurip
{
  GstAudioFilter element;

  /*< private >*/
  guint32              crc;
  guint32              crc_v2;
  guint64              num_samples;
  gboolean             is_first;
  gboolean             is_last;

  /* Needed when 'is_last' is true */
  guint32             *crcs_ring;
  guint32             *crcs_v2_ring;
  guint64              ring_samples;
};

struct _GstAccuripClass
{
  GstAudioFilterClass parent_class;
};

GType gst_accurip_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (accurip);

G_END_DECLS

#endif /* __GST_ACCURIP_H__ */
