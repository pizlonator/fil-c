/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#ifndef __GST_MF_SOURCE_OBJECT_H__
#define __GST_MF_SOURCE_OBJECT_H__

#include <gst/gst.h>
#include "gstmfutils.h"

G_BEGIN_DECLS

#define GST_TYPE_MF_SOURCE_OBJECT             (gst_mf_source_object_get_type())
#define GST_MF_SOURCE_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_MF_SOURCE_OBJECT, GstMFSourceObject))
#define GST_MF_SOURCE_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_MF_SOURCE_OBJECT, GstMFSourceObjectClass))
#define GST_IS_MF_SOURCE_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_MF_SOURCE_OBJECT))
#define GST_IS_MF_SOURCE_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_MF_SOURCE_OBJECT))
#define GST_MF_SOURCE_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_MF_SOURCE_OBJECT, GstMFSourceObjectClass))

typedef struct _GstMFSourceObject        GstMFSourceObject;
typedef struct _GstMFSourceObjectClass   GstMFSourceObjectClass;

typedef enum
{
  GST_MF_SOURCE_TYPE_VIDEO,
} GstMFSourceType;

typedef enum
{
  GST_MF_OK,
  GST_MF_DEVICE_NOT_FOUND,
  GST_MF_ACTIVATION_FAILED,
} GstMFSourceResult;

#define GST_TYPE_MF_SOURCE_TYPE (gst_mf_source_type_get_type())
GType gst_mf_source_type_get_type (void);

struct _GstMFSourceObject
{
  GstObject parent;

  GstMFSourceResult source_state;

  GstMFSourceType source_type;
  gchar *device_path;
  gchar *device_name;
  gint device_index;

  GWeakRef client;
};

struct _GstMFSourceObjectClass
{
  GstObjectClass parent_class;

  gboolean      (*start)       (GstMFSourceObject * object);

  gboolean      (*stop)        (GstMFSourceObject * object);

  GstFlowReturn (*fill)        (GstMFSourceObject * object,
                                GstBuffer * buffer);

  GstFlowReturn (*create)      (GstMFSourceObject * object,
                                GstBuffer ** buffer);

  GstFlowReturn (*get_sample)  (GstMFSourceObject * object,
                                GstSample ** sample);

  gboolean      (*unlock)      (GstMFSourceObject * object);

  gboolean      (*unlock_stop) (GstMFSourceObject * object);

  GstCaps *     (*get_caps)    (GstMFSourceObject * object);

  gboolean      (*set_caps)    (GstMFSourceObject * object,
                                GstCaps * caps);
};

GType           gst_mf_source_object_get_type     (void);

gboolean        gst_mf_source_object_start        (GstMFSourceObject * object);

gboolean        gst_mf_source_object_stop         (GstMFSourceObject * object);

/* Used for raw format */
GstFlowReturn   gst_mf_source_object_fill         (GstMFSourceObject * object,
                                                   GstBuffer * buffer);

/* Used for compressed/raw format */
GstFlowReturn   gst_mf_source_object_create       (GstMFSourceObject * object,
                                                   GstBuffer ** buffer);

/* DirectShow filter */
GstFlowReturn   gst_mf_source_object_get_sample   (GstMFSourceObject * object,
                                                   GstSample ** sample);

void            gst_mf_source_object_set_flushing (GstMFSourceObject * object,
                                                   gboolean flushing);

GstCaps *       gst_mf_source_object_get_caps     (GstMFSourceObject * object);

gboolean        gst_mf_source_object_set_caps     (GstMFSourceObject * object,
                                                   GstCaps * caps);

gboolean        gst_mf_source_object_set_client   (GstMFSourceObject * object,
                                                   GstElement * element);

GstClockTime    gst_mf_source_object_get_running_time (GstMFSourceObject * object);

/* A factory method for subclass impl. selection */
GstMFSourceObject * gst_mf_source_object_new      (GstMFSourceType type,
                                                   gint device_index,
                                                   const gchar * device_name,
                                                   const gchar * device_path,
                                                   gpointer dispatcher);

GstMFSourceResult   gst_mf_source_object_enumerate (gint device_index,
                                                    GstMFSourceObject ** object);

/* Utility methods */
gint            gst_mf_source_object_caps_compare (GstCaps * caps1,
                                                   GstCaps * caps2);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstMFSourceObject, gst_object_unref)

G_END_DECLS

#endif /* __GST_MF_SOURCE_OBJECT_H__ */