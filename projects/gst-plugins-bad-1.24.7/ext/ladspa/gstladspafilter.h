/* GStreamer 
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
 *
 * gstladspafilter.h: Header for LADSPA filter 
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

#ifndef __GST_LADSPA_FILTER_H__
#define __GST_LADSPA_FILTER_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiofilter.h>
#include "gstladspautils.h"

G_BEGIN_DECLS

#define GST_TYPE_LADSPA_FILTER              (gst_ladspa_filter_get_type())
#define GST_LADSPA_FILTER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_LADSPA_FILTER,GstLADSPAFilter))
#define GST_LADSPA_FILTER_CAST(obj)         ((GstLADSPAFilter *) (obj))
#define GST_LADSPA_FILTER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_LADSPA_FILTER,GstLADSPAFilterClass))
#define GST_LADSPA_FILTER_CLASS_CAST(klass) ((GstLADSPAFilterClass *) (klass))
#define GST_LADSPA_FILTER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_LADSPA_FILTER,GstLADSPAFilterClass))
#define GST_IS_LADSPA_FILTER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_LADSPA_FILTER))
#define GST_IS_LADSPA_FILTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_LADSPA_FILTER))

typedef struct _GstLADSPAFilter GstLADSPAFilter;

typedef struct _GstLADSPAFilterClass GstLADSPAFilterClass;

struct _GstLADSPAFilter
{
  GstAudioFilter parent;

  GstLADSPA ladspa;
};

struct _GstLADSPAFilterClass
{
  GstAudioFilterClass parent_class;

  GstLADSPAClass ladspa;
};

GType
gst_ladspa_filter_get_type (void);

void
ladspa_register_filter_element (GstPlugin * plugin, GstStructure *ladspa_meta);

void
gst_my_audio_filter_class_add_pad_templates (GstAudioFilterClass * audio_class,
    GstCaps * srccaps, GstCaps * sinkcaps);

G_END_DECLS

#endif /* __GST_LADSPA_FILTER_H__ */
