/* GStreamer 
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2005 Stefan Kost <ensonic@users.sf.net> (audiotestsrc)
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
 *
 * gstladspasource.h: Header for LADSPA source 
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

#ifndef __GST_LADSPA_SOURCE_H__
#define __GST_LADSPA_SOURCE_H__

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstladspautils.h"

G_BEGIN_DECLS

#define GST_TYPE_LADSPA_SOURCE              (gst_ladspa_source_get_type())
#define GST_LADSPA_SOURCE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_LADSPA_SOURCE,GstLADSPASource))
#define GST_LADSPA_SOURCE_CAST(obj)         ((GstLADSPASource *) (obj))
#define GST_LADSPA_SOURCE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_LADSPA_SOURCE,GstLADSPASourceClass))
#define GST_LADSPA_SOURCE_CLASS_CAST(klass) ((GstLADSPASourceClass *) (klass))
#define GST_LADSPA_SOURCE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_LADSPA_SOURCE,GstLADSPASourceClass))
#define GST_IS_LADSPA_SOURCE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_LADSPA_SOURCE))
#define GST_IS_LADSPA_SOURCE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_LADSPA_SOURCE))

typedef struct _GstLADSPASource GstLADSPASource;

typedef struct _GstLADSPASourceClass GstLADSPASourceClass;

struct _GstLADSPASource
{
  GstBaseSrc parent;

  GstLADSPA ladspa;

  /* audio parameters */
  GstAudioInfo info;
  gint samples_per_buffer;

  /*< private > */
  gboolean tags_pushed;              /* send tags just once ? */
  GstClockTimeDiff timestamp_offset; /* base offset */
  GstClockTime next_time;            /* next timestamp */
  gint64 next_sample;                /* next sample to send */
  gint64 next_byte;                  /* next byte to send */
  gint64 sample_stop;
  gboolean check_seek_stop;
  gboolean eos_reached;
  gint generate_samples_per_buffer;  /* used to generate a partial buffer */
  gboolean can_activate_pull;
  gboolean reverse;                  /* play backwards */
};

struct _GstLADSPASourceClass
{
  GstBaseSrcClass parent_class;

  GstLADSPAClass ladspa;
};

GType
gst_ladspa_source_get_type (void);

void
ladspa_register_source_element (GstPlugin * plugin, GstStructure *ladspa_meta);

void
gst_my_base_source_class_add_pad_template (GstBaseSrcClass * base_class,
    GstCaps * srccaps);

G_END_DECLS

#endif /* __GST_LADSPA_SOURCE_H__ */
