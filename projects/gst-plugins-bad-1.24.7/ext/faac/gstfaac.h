/* GStreamer FAAC (Free AAC Encoder) plugin
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
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

#ifndef __GST_FAAC_H__
#define __GST_FAAC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudioencoder.h>

#include <faac.h>

G_BEGIN_DECLS

#define GST_TYPE_FAAC \
  (gst_faac_get_type ())
#define GST_FAAC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_FAAC, GstFaac))
#define GST_FAAC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_FAAC, GstFaacClass))
#define GST_IS_FAAC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_FAAC))
#define GST_IS_FAAC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_FAAC))

typedef struct _GstFaac GstFaac;
typedef struct _GstFaacClass GstFaacClass;

struct _GstFaac {
  GstAudioEncoder element;

  /* input format */
  gint format;
  /* input frame size */
  gint samples;
  /* required output buffer size */
  gint bytes;

  /* negotiated */
  gint mpegversion, outputformat;

  /* properties */
  gint bitrate,
       profile,
       quality,
       brtype,
       shortctl;
  gboolean tns,
           midside;

  /* FAAC object */
  faacEncHandle handle;
};

struct _GstFaacClass {
  GstAudioEncoderClass parent_class;
};

GType gst_faac_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (faac);

G_END_DECLS

#endif /* __GST_FAAC_H__ */
