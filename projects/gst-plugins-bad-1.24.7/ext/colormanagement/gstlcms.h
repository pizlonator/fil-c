/*
 * GStreamer gstreamer-lcms
 * Copyright (C) 2016 Andreas Frisch <fraxinas@dreambox.guru>
 *
 * gstlcms.h
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

#ifndef __GST_LCMS_H__
#define __GST_LCMS_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <lcms2.h>

G_BEGIN_DECLS

typedef enum
{
  GST_LCMS_INTENT_PERCEPTUAL = 0,
  GST_LCMS_INTENT_RELATIVE_COLORIMETRIC,
  GST_LCMS_INTENT_SATURATION,
  GST_LCMS_INTENT_ABSOLUTE_COLORIMETRIC,
} GstLcmsIntent;

#define GST_TYPE_LCMS_INTENT (gst_lcms_intent_get_type ())

typedef enum
{
  GST_LCMS_LOOKUP_METHOD_UNCACHED = 0,
  GST_LCMS_LOOKUP_METHOD_PRECALCULATED,
  GST_LCMS_LOOKUP_METHOD_CACHED,
  GST_LCMS_LOOKUP_METHOD_FILE,
} GstLcmsLookupMethod;

#define GST_TYPE_LCMS_LOOKUP_METHOD (gst_lcms_lookup_method_get_type ())

#define GST_TYPE_LCMS            (gst_lcms_get_type())
#define GST_LCMS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_LCMS,GstLcms))
#define GST_LCMS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_LCMS,GstLcmsClass))
#define GST_LCMS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_LCMS,GstLcmsClass))
#define GST_IS_LCMS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_LCMS))
#define GST_IS_LCMS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_LCMS))

typedef struct _GstLcms GstLcms;
typedef struct _GstLcmsClass GstLcmsClass;

/**
 * GstLcms:
 *
 * Opaque data structure.
 */
struct _GstLcms
{
  GstVideoFilter videofilter;

  /* < private > */
  gboolean embeddedprofiles;
  GstLcmsIntent intent;
  GstLcmsLookupMethod lookup_method;

  cmsHPROFILE cms_inp_profile, cms_dst_profile;
  cmsHTRANSFORM cms_transform;
  cmsUInt32Number cms_inp_format, cms_dst_format;

  gchar *inp_profile_filename;
  gchar *dst_profile_filename;

  guint32 *color_lut;

  gboolean preserve_black;

  void (*process) (GstLcms * lcms, GstVideoFrame * inframe,
      GstVideoFrame * outframe);
};

struct _GstLcmsClass
{
  GstVideoFilterClass parent_class;
};

G_GNUC_INTERNAL GType gst_lcms_get_type (void);
G_GNUC_INTERNAL GType gst_lcms_intent_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (lcms);

G_END_DECLS
#endif /* __GST_LCMS_H__ */
