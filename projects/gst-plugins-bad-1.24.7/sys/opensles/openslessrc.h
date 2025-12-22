/* GStreamer
 * Copyright (C) 2012 Fluendo S.A. <support@fluendo.com>
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

#ifndef __OPENSLESSRC_H__
#define __OPENSLESSRC_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include "openslesringbuffer.h"

G_BEGIN_DECLS

#define GST_TYPE_OPENSLES_SRC \
  (gst_opensles_src_get_type())
#define GST_OPENSLES_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENSLES_SRC,GstOpenSLESSrc))
#define GST_OPENSLES_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENSLES_SRC,GstOpenSLESSrcClass))

typedef struct _GstOpenSLESSrc GstOpenSLESSrc;
typedef struct _GstOpenSLESSrcClass GstOpenSLESSrcClass;

struct _GstOpenSLESSrc
{
  GstAudioBaseSrc src;
  GstOpenSLESRecordingPreset preset;
};

struct _GstOpenSLESSrcClass
{
  GstAudioBaseSrcClass parent_class;
};

GType gst_opensles_src_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (openslessrc);

G_END_DECLS
#endif /* __OPENSLESSRC_H__ */
