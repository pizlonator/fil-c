/*
 * GStreamer - DTMF Detection
 *
 *  Copyright 2009 Nokia Corporation
 *  Copyright 2009 Collabora Ltd,
 *   @author: Olivier Crete <olivier.crete@collabora.co.uk>
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
 *
 */

#ifndef __GST_DTMF_DETECT_H__
#define __GST_DTMF_DETECT_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include <spandsp.h>

G_BEGIN_DECLS

/* #define's don't like whitespacey bits */
#define GST_TYPE_DTMF_DETECT \
  (gst_dtmf_detect_get_type())
#define GST_DTMF_DETECT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  GST_TYPE_DTMF_DETECT,GstDtmfDetect))
#define GST_DTMF_DETECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  GST_TYPE_DTMF_DETECT,GstDtmfDetectClass))
#define GST_IS_DTMF_DETECT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DTMF_DETECT))
#define GST_IS_DTMF_DETECT_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DTMF_DETECT))

typedef struct _GstDtmfDetect GstDtmfDetect;
typedef struct _GstDtmfDetectClass GstDtmfDetectClass;
typedef struct _GstDtmfDetectPrivate GstDtmfDetectPrivate;

struct _GstDtmfDetect
{
  GstBaseTransform parent;

  dtmf_rx_state_t *dtmf_state;
};

struct _GstDtmfDetectClass
{
  GstBaseTransformClass parent_class;
};

GType gst_dtmf_detect_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (dtmfdetect);

G_END_DECLS

#endif /* __GST_DTMF_DETECT_H__ */
