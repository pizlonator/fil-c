/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
 * gstspectrascope.h: simple oscilloscope
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

#ifndef __GST_SPECTRA_SCOPE_H__
#define __GST_SPECTRA_SCOPE_H__

#include "gst/pbutils/gstaudiovisualizer.h"
#include <gst/fft/gstffts16.h>

G_BEGIN_DECLS
#define GST_TYPE_SPECTRA_SCOPE            (gst_spectra_scope_get_type())
#define GST_SPECTRA_SCOPE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SPECTRA_SCOPE,GstSpectraScope))
#define GST_SPECTRA_SCOPE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SPECTRA_SCOPE,GstSpectraScopeClass))
#define GST_IS_SPECTRA_SCOPE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SPECTRA_SCOPE))
#define GST_IS_SPECTRA_SCOPE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SPECTRA_SCOPE))
typedef struct _GstSpectraScope GstSpectraScope;
typedef struct _GstSpectraScopeClass GstSpectraScopeClass;

struct _GstSpectraScope
{
  GstAudioVisualizer parent;

  GstFFTS16 *fft_ctx;
  GstFFTS16Complex *freq_data;
};

struct _GstSpectraScopeClass
{
  GstAudioVisualizerClass parent_class;
};

GType gst_spectra_scope_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (spectrascope);

G_END_DECLS
#endif /* __GST_SPECTRA_SCOPE_H__ */
