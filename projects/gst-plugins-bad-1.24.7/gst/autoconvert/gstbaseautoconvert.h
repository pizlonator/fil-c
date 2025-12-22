/* GStreamer
 *
 *  Copyright 2007 Collabora Ltd
 *   @author: Olivier Crete <olivier.crete@collabora.co.uk>
 *  Copyright 2007-2008 Nokia
 *  Copyright 2023 Igalia S.L.
 *   @author: Thibault Saunier <tsaunier@igalia.com>
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

#pragma once

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_BASE_AUTO_CONVERT         (gst_base_auto_convert_get_type())
#define GST_BASE_AUTO_CONVERT(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_BASE_AUTO_CONVERT,GstBaseAutoConvert))
#define GST_BASE_AUTO_CONVERT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_BASE_AUTO_CONVERT,GstBaseAutoConvertClass))
#define GST_BASE_AUTO_CONVERT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_BASE_AUTO_CONCERT, GstBaseAutoConvertClass))
#define GST_IS_BASE_AUTO_CONVERT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_BASE_AUTO_CONVERT))
#define GST_IS_BASE_AUTO_CONVERT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_BASE_AUTO_CONVERT))
typedef struct _GstBaseAutoConvert GstBaseAutoConvert;
typedef struct _GstBaseAutoConvertClass GstBaseAutoConvertClass;

struct _GstBaseAutoConvert
{
  /*< private >*/
  GstBin bin;                   /* we extend GstBin */

  GList *factories;
  GList *filters_info;

  GstPad *sinkpad;
  GstPad *srcpad;

  /* Have to be set all at once
   * Protected by the object lock and the stream lock
   * Both must be held to modify these
   */
  GstElement *current_subelement;
  GstPad *current_internal_srcpad;
  GstPad *current_internal_sinkpad;

  GHashTable *elements;
};

/* This structure is used to allow handling random bin from their description
   without needing to register a factory. The data it contains is pretty similar
   but is specific for filters (1sinkpad and 1 srcpad).
*/
typedef struct
{
  /* Name of the filter, each instance of should have that name */
  gchar *name;
  gchar *bindesc;
  GstRank rank;
  GstCaps *sink_caps;
  GstCaps *src_caps;
  GstElement *subbin;
} GstAutoConvertFilterInfo;

struct _GstBaseAutoConvertClass
{
  GstBinClass parent_class;

  gboolean registers_filters;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstBaseAutoConvert, gst_object_unref)
GType gst_base_auto_convert_get_type (void);

gboolean
gst_base_auto_convert_register_filter(GstBaseAutoConvert *self, gchar *name,
    gchar * bindesc, GstRank rank);
void gst_base_auto_convert_reset_filters (GstBaseAutoConvert * self);

G_END_DECLS
