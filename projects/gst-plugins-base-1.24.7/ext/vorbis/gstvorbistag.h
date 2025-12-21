/* -*- c-basic-offset: 2 -*-
 * GStreamer
 * Copyright (C) <2006> James Livingston <doclivingston@gmail.com>
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


#ifndef __GST_VORBIS_TAG_H__
#define __GST_VORBIS_TAG_H__

#include "gstvorbisparse.h"


G_BEGIN_DECLS


#define GST_TYPE_VORBIS_TAG \
  (gst_vorbis_tag_get_type())
#define GST_VORBIS_TAG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VORBIS_TAG,GstVorbisTag))
#define GST_VORBIS_TAG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VORBIS_TAG,GstVorbisTagClass))
#define GST_IS_VORBIS_TAG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VORBIS_TAG))
#define GST_IS_VORBIS_TAG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VORBIS_TAG))


typedef struct _GstVorbisTag GstVorbisTag;
typedef struct _GstVorbisTagClass GstVorbisTagClass;

/**
 * GstVorbisTag:
 *
 * Opaque data structure.
 */
struct _GstVorbisTag {
  GstVorbisParse parse;
};

struct _GstVorbisTagClass {
  GstVorbisParseClass parent_class;
};

GType gst_vorbis_tag_get_type(void);

G_END_DECLS

#endif /* __GST_VORBIS_TAG_H__ */
