/*
 * (C) 2011 Collabora Ltd.
 *  Contact: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GST_SPANDSP_H__
#define __GST_SPANDSP_H__

#include <gst/gst.h>

#include <spandsp.h>

G_BEGIN_DECLS

#define GST_TYPE_SPAN_PLC (gst_span_plc_get_type())
#define GST_SPAN_PLC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SPAN_PLC,GstSpanPlc))
#define GST_SPAN_PLC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SPAN_PLC,GstSpanPlcClass))
#define GST_IS_SPAN_PLC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SPAN_PLC))
#define GST_IS_SPAN_PLC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SPAN_PLC))

typedef struct _GstSpanPlc GstSpanPlc;
typedef struct _GstSpanPlcClass GstSpanPlcClass;

struct _GstSpanPlc
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *srcpad;

  /* <private> */
  plc_state_t *plc_state;
  gint sample_rate;

  /* Used to generate the 'stats' property. Protected by object lock */
  guint64 num_pushed;
  guint64 num_gap;
  guint64 plc_num_samples;
  guint64 plc_duration;
};

struct _GstSpanPlcClass
{
  GstElementClass parent_class;
};

GType gst_span_plc_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (spanplc);

G_END_DECLS

#endif
