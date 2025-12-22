/* GStreamer chromaprint audio fingerprinting element
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

#ifndef __GST_CHROMAPRINT_H__
#define __GST_CHROMAPRINT_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/audio/gstaudiofilter.h>
#include <gst/audio/audio.h>
#include <chromaprint.h>

G_BEGIN_DECLS

#define GST_TYPE_CHROMAPRINT \
  (gst_chromaprint_get_type())
#define GST_CHROMAPRINT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CHROMAPRINT,GstChromaprint))
#define GST_CHROMAPRINT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CHROMAPRINT,GstChromaprintClass))
#define GST_IS_CHROMAPRINT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CHROMAPRINT))
#define GST_IS_CHROMAPRINT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CHROMAPRINT))

#define GST_TAG_CHROMAPRINT_FINGERPRINT "chromaprint-fingerprint"

typedef struct _GstChromaprint      GstChromaprint;
typedef struct _GstChromaprintClass GstChromaprintClass;

/**
 * GstChromaprint:
 *
 * Opaque #GstChromaprint element structure
 */
struct _GstChromaprint
{
  GstAudioFilter element;

  /*< private >*/
  ChromaprintContext * context;
  char               * fingerprint;
  gboolean             record;
  guint64              nsamples;
  guint                duration;
  guint                max_duration;
};

struct _GstChromaprintClass
{
  GstAudioFilterClass parent_class;
};

GType gst_chromaprint_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (chromaprint);

G_END_DECLS

#endif /* __GST_CHROMAPRINT_H__ */
